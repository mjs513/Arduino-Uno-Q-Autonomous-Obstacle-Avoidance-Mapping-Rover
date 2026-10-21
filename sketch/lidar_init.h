#include <PID_v1.h>
#include <elapsedMillis.h>
#include <vector>
#include "common_packets.h"

elapsedMillis  lidar_timer;


//-----------------------------------------------------------------------------
// Copy of the Arduino Zephyr analog code - to map from Pin Number to zephyr
// object and channel
//-----------------------------------------------------------------------------
#include "wiring_private.h"
#define PWM_DT_SPEC(n, p, i) PWM_DT_SPEC_GET_BY_IDX(n, i),
#define PWM_PINS(n, p, i) \
  DIGITAL_PIN_GPIOS_FIND_PIN(DT_REG_ADDR(DT_PHANDLE_BY_IDX(DT_PATH(zephyr_user), p, i)), \
                             DT_PHA_BY_IDX(DT_PATH(zephyr_user), p, i, pin)),


const struct pwm_dt_spec arduino_pwm[] = {
  DT_FOREACH_PROP_ELEM(DT_PATH(zephyr_user), pwms, PWM_DT_SPEC)
};

/* pwm-pins node provides a mapping digital pin numbers to pwm channels */
const pin_size_t arduino_pwm_pins[] = {
  DT_FOREACH_PROP_ELEM(DT_PATH(zephyr_user), pwm_pin_gpios, PWM_PINS)
};

size_t pwm_pin_index(pin_size_t pinNumber) {
  for (size_t i = 0; i < ARRAY_SIZE(arduino_pwm_pins); i++) {
    if (arduino_pwm_pins[i] == pinNumber) {
      return i;
    }
  }
  return (size_t)-1;
}

void analogWriteFrequency(int pin, float freq)
{
  float duty = 50;
  //Step 1. calculate period
  float T = 1.0/freq;
  //Step 2 convert to nanoseconds
  int Tns = (int)(T * 1000000000.0);
  //Serial.println(Tns);
  //Step 3. Calculate Pulse width
  int PW = (int) ((duty / 100.0) * Tns);
  //Serial.println(PW);

  // Quick and dirty to set the period and pulse in nano seconds
  size_t idx = pwm_pin_index(pin);
  pwm_set(arduino_pwm[idx].dev, arduino_pwm[idx].channel, Tns, PW, PWM_POLARITY_NORMAL);
}

/****************************************************************/

const int N_ANGLES = 360;                // # of angles (0..359)
#define SerialX Serial3


struct Config {
  byte id;
  char version[6];
  int motor_pwm_pin;            // pin connected to mosfet for motor speed control
  double rpm_setpoint;          // desired RPM (uses double to be compatible with PID library)
  double rpm_min;
  double rpm_max;
  double pwm_max;               // max analog value
  double pwm_min;               // min analog pulse value to spin the motor
  int sample_time;              // how often to calculate the PID values

  // PID tuning values
  double Kp;
  double Ki;
  double Kd;

  boolean motor_enable;         // to spin the laser or not
  boolean raw_data;             // to retransmit the serial data to the USB port
  boolean show_dist;            // controlled by ShowDist and HideDist commands
  boolean show_rpm;             // controlled by ShowRPM and HideRPM commands
  boolean show_interval;        // true = show time interval, once per revolution, at angle=0
  boolean show_errors;          // Show CRC, signal strength and invalid data errors
  boolean aryAngles[N_ANGLES];  // array of angles to display
}
xv_config;

// Double buffering
struct LidarScan {
  uint8_t  angle[360];
  uint8_t  flag[360];
  uint16_t quality[360];
  uint16_t range[360];
};

LidarScan scanA;
LidarScan scanB;

LidarScan* buildScan = &scanA;   // XV11 thread writes here
LidarScan* readyScan = &scanB;   // transmitter reads here

volatile bool full_scan_ready = false;
/*******************************************************/

// --- Threading primitives for lidar data ---
K_MUTEX_DEFINE(lidar_mutex);

#define LIDAR_STACK_SIZE 2048
#define LIDAR_THREAD_PRIORITY 6

K_THREAD_STACK_DEFINE(lidar_stack, LIDAR_STACK_SIZE);
struct k_thread lidar_thread;

uint8_t packetCnt = 0;

double pwm_val = 200;
double pwm_last;
double motor_rpm;
unsigned long now;
unsigned long motor_check_timer = millis();
unsigned long motor_check_interval = 200;
unsigned int rpm_err_thresh = 10;
unsigned int rpm_err = 0;
unsigned long curMillis;
unsigned long lastMillis = millis();

const unsigned char COMMAND = 0xFA;        // Start of new packet
const int INDEX_LO = 0xA0;                 // lowest index value
const int INDEX_HI = 0xF9;                 // highest index value

const int N_DATA_QUADS = 4;                // there are 4 groups of data elements
const int N_ELEMENTS_PER_QUAD = 4;

// Offsets to bytes within 'Packet'
const int OFFSET_TO_START = 0;
const int OFFSET_TO_INDEX = OFFSET_TO_START + 1;
const int OFFSET_TO_SPEED_LSB = OFFSET_TO_INDEX + 1;
const int OFFSET_TO_SPEED_MSB = OFFSET_TO_SPEED_LSB + 1;
const int OFFSET_TO_4_DATA_READINGS = OFFSET_TO_SPEED_MSB + 1;
const int OFFSET_TO_CRC_L = OFFSET_TO_4_DATA_READINGS + (N_DATA_QUADS * N_ELEMENTS_PER_QUAD);
const int OFFSET_TO_CRC_M = OFFSET_TO_CRC_L + 1;
const int PACKET_LENGTH = OFFSET_TO_CRC_M + 1;

const int OFFSET_DATA_DISTANCE_LSB = 0;
const int OFFSET_DATA_DISTANCE_MSB = OFFSET_DATA_DISTANCE_LSB + 1;
const int OFFSET_DATA_SIGNAL_LSB = OFFSET_DATA_DISTANCE_MSB + 1;
const int OFFSET_DATA_SIGNAL_MSB = OFFSET_DATA_SIGNAL_LSB + 1;

int Packet[PACKET_LENGTH];
int ixPacket = 0;
const int VALID_PACKET = 0;
const int INVALID_PACKET = VALID_PACKET + 1;
const byte INVALID_DATA_FLAG = (1 << 7);
const byte STRENGTH_WARNING_FLAG = (1 << 6);
const byte BAD_DATA_MASK = (INVALID_DATA_FLAG | STRENGTH_WARNING_FLAG);

const byte eState_Find_COMMAND = 0;
const byte eState_Build_Packet = eState_Find_COMMAND + 1;
int eState = eState_Find_COMMAND;
PID rpmPID(&motor_rpm, &pwm_val, &xv_config.rpm_setpoint, xv_config.Kp, xv_config.Ki, xv_config.Kd, DIRECT);

uint8_t inByte = 0;
uint8_t motor_rph_high_byte = 0;
uint8_t motor_rph_low_byte = 0;
uint16_t aryDist[N_DATA_QUADS] = {0, 0, 0, 0};
uint16_t aryQuality[N_DATA_QUADS] = {0, 0, 0, 0};
uint16_t motor_rph = 0;
uint16_t startingAngle = 0;


uint16_t processIndex() {
  uint16_t angle = 0;
  uint16_t data_4deg_index = Packet[OFFSET_TO_INDEX] - INDEX_LO;
  angle = data_4deg_index * N_DATA_QUADS;
  static bool first_rev = true;
  
  if (angle == 0)
  {
      if (!first_rev)
      {
          k_mutex_lock(&lidar_mutex, K_FOREVER);
  
          LidarScan* tmp = readyScan;
          readyScan = buildScan;
          buildScan = tmp;
  
          full_scan_ready = true;
  
          k_mutex_unlock(&lidar_mutex);
      }
  
    first_rev = false;
    
    if (xv_config.show_rpm) {
      Serial.print(F("R,"));
      Serial.print((int)motor_rpm);
      Serial.print(F(","));
      Serial.println((int)pwm_val);
    }

    curMillis = millis();
    if (xv_config.show_interval) {
      Serial.print(F("T,"));
      Serial.println(curMillis - lastMillis);
    }
    lastMillis = curMillis;
  }
  return angle;
}

void processSpeed() {
  motor_rph_low_byte = Packet[OFFSET_TO_SPEED_LSB];
  motor_rph_high_byte = Packet[OFFSET_TO_SPEED_MSB];
  motor_rph = (motor_rph_high_byte << 8) | motor_rph_low_byte;
  motor_rpm = float((motor_rph_high_byte << 8) | motor_rph_low_byte) / 64.0;
}

byte processDistance(int iQuad) {
  uint8_t dataL, dataM;
  aryDist[iQuad] = 0;
  int iOffset = OFFSET_TO_4_DATA_READINGS + (iQuad * N_DATA_QUADS) + OFFSET_DATA_DISTANCE_LSB;
  dataM = Packet[iOffset + 1];
  if (dataM & BAD_DATA_MASK)
    return dataM & BAD_DATA_MASK;
  dataL = Packet[iOffset];
  aryDist[iQuad] = dataL | ((dataM & 0x3F) << 8);
  return 0;
}

void processSignalStrength(int iQuad) {
  uint8_t dataL, dataM;
  aryQuality[iQuad] = 0;
  int iOffset = OFFSET_TO_4_DATA_READINGS + (iQuad * N_DATA_QUADS) + OFFSET_DATA_SIGNAL_LSB;
  dataL = Packet[iOffset];
  dataM = Packet[iOffset + 1];
  aryQuality[iQuad] = dataL | (dataM << 8);
}

byte eValidatePacket() {
  unsigned long chk32;
  unsigned long checksum;
  const int bytesToCheck = PACKET_LENGTH - 2;
  const int CalcCRC_Len = bytesToCheck / 2;
  unsigned int CalcCRC[CalcCRC_Len];

  byte b1a, b1b, b2a, b2b;
  int ix;

  for (int ix = 0; ix < CalcCRC_Len; ix++)
    CalcCRC[ix] = 0;

  for (ix = 0; ix < bytesToCheck; ix += 2)
    CalcCRC[ix / 2] = Packet[ix] + ((Packet[ix + 1]) << 8);

  chk32 = 0;
  for (ix = 0; ix < CalcCRC_Len; ix++)
    chk32 = (chk32 << 1) + CalcCRC[ix];
  checksum = (chk32 & 0x7FFF) + (chk32 >> 15);
  checksum &= 0x7FFF;
  b1a = checksum & 0xFF;
  b1b = Packet[OFFSET_TO_CRC_L];
  b2a = checksum >> 8;
  b2b = Packet[OFFSET_TO_CRC_M];
  if ((b1a == b1b) && (b2a == b2b))
    return VALID_PACKET;
  else
    return INVALID_PACKET;
}

void initConfig() {
  xv_config.motor_pwm_pin = 5;
  xv_config.rpm_setpoint = 210;
  xv_config.rpm_min = 100;
  xv_config.rpm_max = 300;
  xv_config.pwm_min = 100;
  xv_config.pwm_max = 1023;
  xv_config.sample_time = 10;
  xv_config.Kp = 2.0;
  xv_config.Ki = 1.0;
  xv_config.Kd = 0.0;

  xv_config.motor_enable = true;
  xv_config.raw_data = false;
  xv_config.show_dist = true;
  xv_config.show_rpm = false;
  xv_config.show_interval = false;
  xv_config.show_errors = false;
  for (int ix = 0; ix < N_ANGLES; ix++)
    xv_config.aryAngles[ix] = true;
}

void motorCheck() {
  now = millis();
  if (now - motor_check_timer > motor_check_interval) {
    if ((motor_rpm < xv_config.rpm_min or motor_rpm > xv_config.rpm_max) && pwm_val > 1000) {
      rpm_err++;
    }
    else {
      rpm_err = 0;
    }
    if (rpm_err > rpm_err_thresh) {
      xv_config.motor_enable =false;
    }
    motor_check_timer = millis();
  }
}

// --- Lidar thread function ---
void xv11_thread(void *dummy1, void *dummy2, void *dummy3){
  while (true) {
    byte aryInvalidDataFlag[N_DATA_QUADS] = {0, 0, 0, 0};

    while (SerialX.available() > 0) {
      inByte = SerialX.read();
      if (xv_config.raw_data)
        Serial.write(inByte);

      if (eState == eState_Find_COMMAND) {
        if (inByte == COMMAND) {
          eState++;
          Packet[ixPacket++] = inByte;
        }
      }
      else {
        Packet[ixPacket++] = inByte;
        if (ixPacket == PACKET_LENGTH) {
          if (eValidatePacket() == VALID_PACKET) {
            startingAngle = processIndex();
            processSpeed();
            for (int ix = 0; ix < N_DATA_QUADS; ix++)
              aryInvalidDataFlag[ix] = processDistance(ix);
            for (int ix = 0; ix < N_DATA_QUADS; ix++) {
              aryQuality[ix] = 0;
              if (aryInvalidDataFlag[ix] == 0)
                processSignalStrength(ix);
            }

            if (xv_config.show_dist) {
              k_mutex_lock(&lidar_mutex, K_FOREVER);
              for (int ix = 0; ix < N_DATA_QUADS; ix++) {
                if (xv_config.aryAngles[startingAngle + ix]) {
                  if (aryInvalidDataFlag[ix] & BAD_DATA_MASK) {
                    buildScan->flag[startingAngle + ix] = 0;
                    if (aryInvalidDataFlag[ix] & INVALID_DATA_FLAG){
                      buildScan->flag[startingAngle + ix] = 1;
                    }
                    if (aryInvalidDataFlag[ix] & STRENGTH_WARNING_FLAG){
                      buildScan->flag[startingAngle + ix] = 2;
                    }
                    buildScan->angle[startingAngle + ix] = startingAngle + ix;
                    buildScan->range[startingAngle + ix] = 0;
                    buildScan->quality[startingAngle + ix] = 0;
                  }
                  else {
                    buildScan->angle[startingAngle + ix] = startingAngle + ix;
                    buildScan->range[startingAngle + ix] = int(aryDist[ix]);
                    buildScan->quality[startingAngle + ix] = aryQuality[ix];
                    buildScan->flag[startingAngle + ix]= 0;
                  }
                }
              }
              k_mutex_unlock(&lidar_mutex);
            }
          }
          else if (xv_config.show_errors) {
            Serial.println(F("C,CRC"));
          }

          for (int ix = 0; ix < N_DATA_QUADS; ix++) {
            aryDist[ix] = 0;
            aryQuality[ix] = 0;
            aryInvalidDataFlag[ix] = 0;
          }
          for (ixPacket = 0; ixPacket < PACKET_LENGTH; ixPacket++)
            Packet[ixPacket] = 0;
          ixPacket = 0;
          eState = eState_Find_COMMAND;
        }
      }
    }
    k_msleep(1);
  }
}


K_THREAD_CREATE_NAMED(xv11_thread, LIDAR_STACK_SIZE, xv11_thread,
                      NULL, NULL, NULL, LIDAR_THREAD_PRIORITY, 0,
                      K_FOREVER, "xv11_thread");

