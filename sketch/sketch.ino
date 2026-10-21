

#include "init.h"
#include "lidar_init.h"

#include <elapsedMillis.h>
elapsedMillis printTimer;
long printTime = 1000;

elapsedMillis currentTimer;
long currentSample = 10;


//#define DEBUG_JOYSTICK
//#define DEBUG_ODO
//#define DEBUG_MOTOR


// Thread Configuration for Odometry and IMU
static struct odom_state current_odom = {0.0f, 0.0f, 0.0f};
K_MUTEX_DEFINE(odom_mutex);

/********************************************/
/*
 * Producer Thread:
 * K_MSGQ_DEFINE:
 * Parameters
 *   q_name     Name of the message queue.
 *   q_msg_size     Message size (in bytes).
 *   q_max_msgs     Maximum number of messages that can be queued.
 *   q_align     Alignment of the message queue's ring buffer (power of 2).
 ************************************************/
K_MSGQ_DEFINE(bno055_msgq, MSG_SIZE, MSGQ_MAX_MSGS, 4);

void bno055_thread(void *dummy1, void *dummy2, void *dummy3)
{
    while (1) {
      struct sensor_msg msg;

      imu::Vector<3> mag = bno.getVector(Adafruit_BNO055::VECTOR_MAGNETOMETER);
      
      // Quaternion data
      imu::Quaternion quat = bno.getQuat();

      w = quat.w();
      x = quat.x();
      y = quat.y();
      z = quat.z();
      
      /* ZYX ROTATION */
      sqw = w * w;
      sqx = x * x;
      sqy = y * y;
      sqz = z * z;
  
      yaw = atan2(2.0 * (x * y + z * w), (sqx - sqy - sqz + sqw));
      roll = asin(-2.0 * (x * z - y * w) / (sqx + sqy + sqz + sqw));
      pitch = atan2(2.0 * (y * z + x * w), (-sqx - sqy + sqz + sqw));

      // --- APPLY MAGNETIC DECLINATION ---
      // Convert raw yaw to degrees
      double true_heading = (yaw * 180.0 / M_PI) + MAGNETIC_DECLINATION_DEGS;
  
      // Wrap the heading so it stays strictly within -180.0 to +180.0
      if (true_heading > 180.0) {
          true_heading -= 360.0;
      } else if (true_heading < -180.0) {
          true_heading += 360.0;
      }

      msg.timestamp = k_uptime_get_32();
      msg.yaw = -true_heading;
      msg.roll = roll * 180./M_PI;
      msg.pitch = pitch * 180./M_PI;

      int ret = k_msgq_put(&bno055_msgq, &msg, K_NO_WAIT);
      if (ret != 0) {
        Serial.println("Message queue full, dropping message");
      }

      delay(BNO055_SAMPLERATE_DELAY_MS);
    }
}

/* ----------------------------------------------------------------------- */

int stasis(float wheel_drv, float imu_drv)
{

        if (mDirection == mFORWARD || mDirection == mREVERSE) { 
          if(wheel_drv > ARE_TURNING) {
                stasis_err++;
                //Serial.print("FR: "); Serial.print(stasis_err); Serial.print(", "); Serial.println(stasis_alarm);
          } else {
                if (stasis_err) stasis_err--;
                                  
                else stasis_alarm = 0;
          }
          
        } else if(mDirection == LEFT || mDirection == RIGHT) { 
          if(wheel_drv > ARE_TURNING_LR) {
                stasis_err++;
          } else {
                if (stasis_err) stasis_err--;
                else stasis_alarm = 0;
          }
        }
                
        if (stasis_err > GLOBAL_STASIS_ERR) {
                stasis_alarm = 1;
                stasis_err = 0;
        } 
  
  
        return 0;
}

// Add a global variable to store the baseline heading
static float heading_offset_deg = 0.0f;


void odometry_thread(void *dummy1, void *dummy2, void *dummy3)
{
    struct sensor_msg msg;
    long oldLeft = 0;
    long oldRight = 0;
    bool heading_initialized = false; // Tracking flag

    while (1) {
        int ret = k_msgq_get(&bno055_msgq, &msg, K_FOREVER);
        if (ret == 0) {
            // --- ZERO HEADING ON FIRST VALID SAMPLE ---
            if (!heading_initialized) {
                heading_offset_deg = msg.yaw; // Grab initial raw yaw
                heading_initialized = true;
                Serial.print("Heading zeroed at: ");
                Serial.println(heading_offset_deg);
            }
          
            // 1. Read Encoders
            long newLeft = leftEnc.read();  //using the motor shiels for some reason alway neg going fwd
            long newRight = rightEnc.read();

            long deltaLeft = newLeft - oldLeft;
            long deltaRight = newRight - oldRight;

            // 2. Convert to distance (meters)
            float distLeft = deltaLeft * METERS_PER_TICK;
            float distRight = deltaRight * METERS_PER_TICK;
            float deltaDistance = (distLeft + distRight) / 2.0;

            // 3. Read Heading safely from the message payload (Convert degrees back to radians)
            // for odometry (-) is clockwise and (+) is counter-clockwise
            // Subtract starting offset and invert sign for CCW (+) math using ROS2 nomenclature
            float relative_yaw = msg.yaw - heading_offset_deg;
            float currentHeading = -relative_yaw * (M_PI / 180.0); 

            //if(currentHeading < 0) currentHeading += 2.0f * M_PI;
            // calculate rotation rate-of-change for stasis detector
            float wheel_theta = (distLeft - distRight) / WHEEL_BASE;
            float last_theta = currentHeading;
            float imu_theta = currentHeading - last_theta;

            // Serial.print(wheel_theta); Serial.print(", "); Serial.println(imu_theta);
			//  Statis Detector
            stasis(fabs(wheel_theta),fabs(imu_theta));

            // 4. Update Absolute X and Y position
            X_position += deltaDistance * cos(currentHeading);
            Y_position += deltaDistance * sin(currentHeading);
            
            // 5. Safely share odometry updates via the mutex
            if (k_mutex_lock(&odom_mutex, K_MSEC(5)) == 0) {
                current_odom.x = X_position;
                current_odom.y = Y_position;
                current_odom.pitch = msg.pitch;
                current_odom.roll = msg.roll;
                current_odom.heading = -relative_yaw;
                current_odom.left_enc = newLeft;
                current_odom.right_enc = newRight;
                k_mutex_unlock(&odom_mutex);
            }

            // 6. Save states for next loop
            oldLeft = newLeft;
            oldRight = newRight;
        }
      delay(1);
    }
}


/*
void odometry_thread(void *dummy1, void *dummy2, void *dummy3)
{
    struct sensor_msg msg;
    long oldLeft = leftEnc.read();   // Initialize to current reading to avoid startup jump
    long oldRight = rightEnc.read();
    bool heading_initialized = false; 
    
    static float last_heading_rad = 0.0f; // Persists across loop iterations

    while (1) {
        int ret = k_msgq_get(&bno055_msgq, &msg, K_FOREVER);
        if (ret == 0) {
            // --- ZERO HEADING ON FIRST VALID SAMPLE ---
            if (!heading_initialized) {
                heading_offset_deg = msg.yaw; 
                heading_initialized = true;
                Serial.print("Heading zeroed at: ");
                Serial.println(heading_offset_deg);
            }
          
            // 1. Read Encoders
            long newLeft = leftEnc.read();  
            long newRight = rightEnc.read();

            long deltaLeft = newLeft - oldLeft;
            long deltaRight = newRight - oldRight;

            // 2. Convert to distance (meters)
            float distLeft = deltaLeft * METERS_PER_TICK;
            float distRight = deltaRight * METERS_PER_TICK;
            float deltaDistance = (distLeft + distRight) / 2.0f;

            // 3. Read Heading (Convert degrees to radians, normalize orientation)
            float relative_yaw = msg.yaw - heading_offset_deg;
            float currentHeading = -relative_yaw * (M_PI / 180.0f);  

            // 4. Fixed IMU delta calculation (using persistent last heading)
            float imu_theta = currentHeading - last_heading_rad;
            last_heading_rad = currentHeading;

            // Optional stasis check can use imu_theta now that it works
            float wheel_theta = (distLeft - distRight) / WHEEL_BASE;
            //Serial.print(wheel_theta,4); Serial.print(", "); Serial.println(imu_theta,4);
            stasis(fabs(wheel_theta), fabs(imu_theta));

            // 5. Update Absolute X and Y position using average heading of the step
            // (Using midpoint heading reduces trajectory integration error)
            float midHeading = currentHeading - (imu_theta / 2.0f);
            X_position += deltaDistance * cos(midHeading);
            Y_position += deltaDistance * sin(midHeading);
          
            // 6. Safely share odometry updates via the mutex
            if (k_mutex_lock(&odom_mutex, K_MSEC(5)) == 0) {
                current_odom.x = X_position;
                current_odom.y = Y_position;
                current_odom.pitch = msg.pitch;
                current_odom.roll = msg.roll;
                current_odom.heading = -relative_yaw;
                current_odom.left_enc = newLeft;
                current_odom.right_enc = newRight;
                k_mutex_unlock(&odom_mutex);
            }

            // 7. Save states for next loop
            oldLeft = newLeft;
            oldRight = newRight;
        }
    }
}
*/

void telem_thread(void *dummy1, void *dummy2, void *dummy3) {
    while (1) {
        struct odom_state local_odom;

        // Safely snapshot the odometry state
        if (k_mutex_lock(&odom_mutex, K_MSEC(5)) == 0) {
            local_odom = current_odom;
            k_mutex_unlock(&odom_mutex);

#if defined(DEBUG_ODO)
            printf("[ODO] X: %f, Y: %f, Heading: %f deg, Pitch: %f deg, Roll: %f deg, EncLeft: %d, EncRight: %d\r\n", 
                   local_odom.x, local_odom.y, local_odom.heading, local_odom.pitch, local_odom.roll, 
                   local_odom.left_enc, local_odom.right_enc);
#else
            if (printTimer > printTime) {

              /* Write LIDAR and ODOM PACKET  */            
              if (full_scan_ready)
              {
                  full_scan_ready = false;

                  /* Send ODOMETRY PACKET */
                  // Populate framed odometry packet
                  OdomPacket pkt;
                  pkt.magic1   = MAGIC_1;
                  pkt.magic2   = MAGIC_2;
                  pkt.pkt_type = PKT_ODOM;
                  pkt.x        = local_odom.x;
                  pkt.y        = local_odom.y;
                  pkt.theta    = local_odom.heading * (M_PI / 180.0); // convert deg -> rad
      
                  // Write the entire packed structure over serial in one operation
                  Serial1.write((uint8_t*)&pkt, sizeof(pkt));

                 /* Send LIDAR PACKET */
                  k_mutex_lock(&lidar_mutex, K_FOREVER);
                  memcpy(
                      LidarPackets.distance,
                      readyScan->range,
                      sizeof(LidarPackets.distance)
                  );        
                  k_mutex_unlock(&lidar_mutex);

                  //for (int i = 0; i < N_ANGLES; i++) {
                  //  Serial.print(i); Serial.print(": "); Serial.println( LidarPackets.distance[i]);
                  //} Serial.println("--------------------------------------");

                  LidarPackets.magic1 = MAGIC_1;
                  LidarPackets.magic2 = MAGIC_2;
                  LidarPackets.pkt_type = PKT_LIDAR;
                
                  Serial1.write((uint8_t*)&LidarPackets, sizeof(LidarPackets));  
              }
             
              printTimer = 0;
            }
#endif
        }

      // 50ms polling loop (~20Hz rate)
      k_msleep(1);
    }
}


/* Thread declarations using macro */
K_THREAD_CREATE_NAMED(bno055_thread, 1024, bno055_thread,
                      NULL, NULL, NULL,7, 0,
                      K_FOREVER,"bno055_thread");

K_THREAD_CREATE_NAMED(odometry_thread, 1024, odometry_thread,
                      NULL, NULL, NULL, 5, 0,
                      K_FOREVER,"odometry_thread");

K_THREAD_CREATE_NAMED(telem_thread,1024, telem_thread,
                      NULL, NULL, NULL, 5, 0,
                      K_FOREVER, "telem_thread");


void get_steering(float _obs_dist, float _steering_angle, int _valid_points ) {
  obs_dist = _obs_dist;
  steering_angle = -(_steering_angle);
  valid_points = _valid_points;

  //printf("[Steering] %f, %f, %d\r\n", _obs_dist, _steering_angle, _valid_points);

}
											

void joystickController() {
 
  if (new_left_wheel_velocity != throttleLeft) {
      throttleLeft = new_left_wheel_velocity;
      motorController();
  }
  if (new_right_wheel_velocity != throttleRight) {
      throttleRight = new_right_wheel_velocity;
      motorController();
  }  
}

void autoController() {
  // steering_angle comes from Python:
  // Positive = steer Left (+), Negative = steer Right (-)

 if (abs(steering_angle) < ANGLE_DEADBAND) {
      // Drive straight
      throttleLeft  = BASE_FORWARD_SPEED;
      throttleRight = BASE_FORWARD_SPEED;
      mForwardPWM(throttleLeft, throttleRight);
      return;
  }

  // Calculate differential motor adjustments based on required turn
  int turn_effort = (int)(steering_angle * Kp);

  if (steering_angle > 0) {
      // Turn Left (+ angle): Slow down left wheel, speed up right wheel
      throttleLeft  = constrain(BASE_FORWARD_SPEED - turn_effort, -MAX_STEER_SPEED, MAX_STEER_SPEED);
      throttleRight = constrain(BASE_FORWARD_SPEED + turn_effort, -MAX_STEER_SPEED, MAX_STEER_SPEED);
  } else {
      // Turn Right (- angle): Slow down right wheel, speed up left wheel
      throttleLeft  = constrain(BASE_FORWARD_SPEED + abs(turn_effort), -MAX_STEER_SPEED, MAX_STEER_SPEED);
      throttleRight = constrain(BASE_FORWARD_SPEED - abs(turn_effort), -MAX_STEER_SPEED, MAX_STEER_SPEED);
  }

  // Apply motor directions and speeds
  mForwardPWM(throttleLeft, throttleRight);
  
}

// https://forum.arduino.cc/t/arduino-uno-q-and-bridge-tool-any-samples-and-documentation/1410551/22
bool waitForLinux(int intervalMs = 200, int timeoutMs = 30000)
{
  // Wait until Linux has started, or timeout.
  boolean started = false;
  unsigned long startTimeMs = millis();

  while (true)
  {
    if (Bridge.call("linux_started").result(started))
    {
      // The Linux side has started.
      // (It doesn't seem necessary to check the 'started' value, but we're using the chained '.result()' to test for call success.)
      Serial.println("Bridge.call('linux_started') succeeded");
      started = true;
      break;
    }

    delay(intervalMs);

    if (millis() >= startTimeMs + timeoutMs)
    {
      // Timeout - stop waiting.
      break;
    }
  }

  int elapsedMs = millis() - startTimeMs;
  
  if (started)
    Serial.println("Linux started after " + String(elapsedMs) + " ms");
  else
    Serial.println("TIMEOUT - Linux NOT started after " + String(elapsedMs) + " ms");

  return started;
}

void initIMU() {
  /* Initialize sensor */
   Serial.println("Orientation Sensor Test"); Serial.println("");

    /* Initialise the sensor */
    while (!bno.begin())
    {
        /* There was a problem detecting the BNO055 ... check your connections */
        Serial.print("Ooops, no BNO055 detected ... Check your wiring or I2C ADDR!");
        delay(100);
    }

    //  Restore  offsets
    storedOffsets.accel_offset_x = -18;
    storedOffsets.accel_offset_y = -48;
    storedOffsets.accel_offset_z = -25;
    storedOffsets.mag_offset_x   = -199;
    storedOffsets.mag_offset_y   = 250;
    storedOffsets.mag_offset_z   = -48;
    storedOffsets.gyro_offset_x  = 0;
    storedOffsets.gyro_offset_y  = 0;
    storedOffsets.gyro_offset_z  = -1;
    storedOffsets.accel_radius   = 1000;
    storedOffsets.mag_radius     = 623;
  
    bno.setSensorOffsets(storedOffsets);

    bno.setExtCrystalUse(true);

    Serial.println("Offsets restored.");
    
    delay(100);  /* Initialize sensor */
  
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);

  Bridge.begin();
  Serial.begin(115200);
  Serial1.begin(115200);
  Serial3.begin(115200);

  //delay(5000);
  printf_init(Serial1);
  
  // Wait until Linux has started.
  while (!waitForLinux())
    delay(500);
  
  
  Serial.println("\n*** Joystick test program starting ***");
  Serial.flush();

  Serial.println("Initializing the Arduino Motor Shield!");
  setupMotors();
  Serial.println("Motor Shield initialized.");
  mStop();
	
	/*  Configure VL53L5CX Sensor   */
  //Configure vl53
  Serial.print("vl53: thread started \n");

  Wire1.begin();
  Wire1.setClock(400000); // 400kHz Fast I2C
 
  
  while (myImager.begin(0x29, Wire1) == false)
  {
      Serial.println(F("VL53 not found! Freezing..."));
      delay(250);
  }

  myImager.setResolution(resolution * resolution); 
  myImager.setRangingFrequency(15);
  myImager.startRanging();

  delay(250);

  initIMU();
  
  /*  Send VL53 data to the MPU  */
  Bridge.provide_safe("get_sensor_telemetry", get_sensor_telemetry);

  /*  Get Joystick data */  
  Bridge.provide_safe("joy_connected", joy_connected);
  Bridge.provide_safe("joy_button_down", joy_button_down);
  Bridge.provide_safe("joy_button_up", joy_button_up);
  Bridge.provide_safe("joy_axis_motion", joy_axis_motion);
  //Bridge.provide_safe("joy_hat_motion", joy_hat_motion);

  printTimer = 0;

  /* Initialize XV-11 Lidar */
  initConfig();

  analogReadResolution(10);
  analogWriteFrequency(xv_config.motor_pwm_pin, 4000);

  rpmPID.SetOutputLimits(xv_config.pwm_min, xv_config.pwm_max);
  rpmPID.SetSampleTime(xv_config.sample_time);
  rpmPID.SetTunings(xv_config.Kp, xv_config.Ki, xv_config.Kd);
  rpmPID.SetMode(AUTOMATIC);

  eState = eState_Find_COMMAND;
  for (ixPacket = 0; ixPacket < PACKET_LENGTH; ixPacket++)
    Packet[ixPacket] = 0;
  ixPacket = 0;

  delay(100);

  bno055_thread_start();
  odometry_thread_start();
  telem_thread_start();
#if !defined(DEBUG_ODO)
  xv11_thread_start();
#endif

  
  Serial1.println("Connected");

  Bridge.provide_safe("get_steering", get_steering);

  mStop();
  
  delay(2000);
}


void loop() {
  if ((millis() - last_led_change_time_ms) > 500) {
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
    last_led_change_time_ms = millis();
  }

  if (joystick_connected_called) {
    Serial.println("Getting Axes!!");
    init_joystick_axis(JOY_LEFT_X); 
    init_joystick_axis(JOY_LEFT_Y);
    init_joystick_axis(JOY_RIGHT_X);
    init_joystick_axis(JOY_RIGHT_Y);
    init_joystick_axis(JOY_RIGHT_TRIGGER);
    //init_joystick_axis(JOY_LEFT_TRIGGER);
    joystick_connected_called = false;
  }

  /* Control Loop Selection */
  if(manualControl) {
    //mStop();
    autoControl == false;
    joystickController();
  }
  if(autoControl) {
    //mStop();
    manualControl = false;
    autoController();
  }

  /* LIDAR PID Control */
  if (xv_config.motor_enable) {
    rpmPID.Compute();
    if (pwm_val != pwm_last) {
      analogWrite(xv_config.motor_pwm_pin, pwm_val);
      pwm_last = pwm_val;
    }
    motorCheck();
  }

  /* Stasis Check */
  if(stasis_alarm) {
      autoControl = false;
      manualControl = true;
      currentMode = TANK;
      stasis_alarm = 0;
      mStop();
      //Serial.println("Stasis Alarm......");
  }

  if(currentTimer > currentSample) {
    /* References
      https://robojax.com/tutorial_view.php?lang=en&id=120
      https://dustynrobots.com/academia/research/digital-signal-processing-and-filtering-motor-current-sensing/
    */
    const double currentFactor = 2/3.3;// 3.3V per 2A (in video at 04:35)
    double currentVL = map(analogRead(A0), 0, 1024, 0, 5000)/1000;
    double currentL = currentVL * currentFactor; //(in video at 04:44)
    double currentVR = map(analogRead(A1), 0, 1024, 0, 5000)/1000;
    double currentR = currentVR * currentFactor; //(in video at 04:44)

    if(currentL > 1.2 || currentR > 1.2) {
      autoControl = false;
      manualControl = true;
      currentMode = TANK;
      mStop();
    }
    
    currentSample = 0;
    
  }
  
  delayMicroseconds(1);
  
}

