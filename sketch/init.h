//#include <stdint.h>
//#include <stddef.h>
//#include <string.h>
//#include <vector>
//#include <cmath>
#include "Arduino_RouterBridge.h"
#include <Wire.h>
#include "LibPrintf.h"

/**************************************
 *     BNO055                          *
 *************************************/
#include <Adafruit_Sensor.h>
#include <Adafruit_BNO055.h>
#include <utility/imumaths.h>

//Adafruit BNO055 Config
/* Set the delay between fresh samples */
uint16_t BNO055_SAMPLERATE_DELAY_MS = 50;

// Check I2C device address and correct line below (by default address is 0x29 or 0x28)
//                                   id, address
Adafruit_BNO055 bno = Adafruit_BNO055(55, 0x28, &Wire1);
adafruit_bno055_offsets_t storedOffsets;
const double MAGNETIC_DECLINATION_DEGS = 12.67; //For Flushing NY


/**************************************
 *     Arduino Motor Shield                      *
 *************************************/
/*
Channel A:
D12 - Direction
D3 - PWM (work duty)
D9 - Brake
A0 - current sensing.

Channel B:
D13 - Direction
D11 - PWM (work duty)
D8 - Brake
A1 - current sensing.
*/

// --- Arduino Motor Shield Rev3 Pin Definitions ---
const int DIR_A = 12;
const int PWM_A = 3;
const int BRAKE_A = 9;
const int CURR_A = A0;

const int DIR_B = 13;
const int PWM_B = 11;
const int BRAKE_B = 8;
const int CURR_B = A1;

/**************************************
 *     VL53L5CX                          *
 *************************************/
#include <SparkFun_VL53L5CX_Library.h> // http://librarymanager/All#SparkFun_VL53L5CX
#include <array>
#include <Arduino_JSON.h>

SparkFun_VL53L5CX myImager;
VL53L5CX_ResultsData measurementData; 


// --- CONFIGURATION FLAGS for VL53L5cx ---
const bool IS_SENSOR_ROTATED = true; // Set to true if sideways, false if normal mounting
int resolution = 8;


/**************************************
 * Thread define Macro
 **************************************/
#define K_THREAD_CREATE_NAMED(name, stack_size, entry,        \
                              p1, p2, p3, priority, options,  \
                              delay, thread_name)             \
    static K_THREAD_STACK_DEFINE(name##_stack, stack_size);   \
    static struct k_thread name##_thread;                     \
    static k_tid_t name##_tid;                                \
    static void name##_start(void) {                          \
        name##_tid = k_thread_create(&name##_thread,          \
                        name##_stack,                         \
                        K_THREAD_STACK_SIZEOF(name##_stack),  \
                        entry, p1, p2, p3,                    \
                        priority, options, delay);            \
        k_thread_name_set(&name##_thread, thread_name);       \
        k_thread_start(&name##_thread);                       \
    }



/**************************************
 * Joystick Configuration and Setup
 **************************************/
uint32_t last_led_change_time_ms = 0;
char buffer[128];
std::vector<int32_t> joystick_range = {-128, 127};

typedef struct {
  uint16_t    value;
  const char  *name;  
} value_to_name_t;

// --- FIX 1: Map this to your actual raw evdev axis code ---
// Based on your python log, your left stick Y-axis is sending a raw evdev code of 1.
enum { JOY_LEFT_X = 0,
       JOY_LEFT_Y = 1,
       JOY_RIGHT_X = 2,
       JOY_LEFT_TRIGGER = 3,
       JOY_RIGHT_TRIGGER = 4,
       JOY_RIGHT_Y = 5,
       JOY_AXIS_COUNT
};

enum {
    JOY_BTN_CROSS = 305,
    JOY_BTN_CIRCLE = 306,
    JOY_BTN_TRIANGLE = 307,
    JOY_BTN_SQUARE = 304,
    JOY_BTN_L1 = 308,
    JOY_BTN_R1 = 309,
    JOY_BTN_L2 = 310,
    JOY_BTN_R2_ = 311,
    JOY_BTN_SHARE = 312,
    JOY_BTN_OPTIONS = 313,
    JOY_BTN_PS_XB = 316,
    JOY_BTN_VIEW = 314,
    JOY_BTN_MENU = 315,
    JOY_BTN_LTSTICK = 317,
    JOY_BTN_RTSTICK = 318,
    JOY_BTN_GUID = 172,
};

static const value_to_name_t axis_names[] = {
    {0, "lx"},
    {1, "ly"},
    {2, "rx"},
    {5, "ry"},
    {3, "L2"},
    {4, "R2"},
    {9, "R2/Rt"},
    {10, "L2/Lt"},
    {16, "dpad_x"},
    {17, "dpad_y"}
};

static const value_to_name_t button_names[] = {
    {305, "cross/B"},
    {306, "circle"},
    {307, "triangle/X"},
    {304, "square/A"},
    {308, "l1/Y"},
    {309, "r1/rs"},
    {310, "l2/ls"},
    {311, "r2_btn"},
    {312, "share"},
    {313, "options"},
    {316, "ps/XB"},
    {314, "l3/view"},
    {315, "r3/options"},
    {317, "touchpad/Stl Lt Bth"},
    {318, "Stick Rt Btn"},
    {158, "share"},
    {172, "guide"}
};

int joystick_axes[JOY_AXIS_COUNT] = {0};
int joystick_axes_mins[JOY_AXIS_COUNT] = {0};
int joystick_axes_maxs[JOY_AXIS_COUNT] = {0};
bool joystick_connected_called = false;

// Define your button codes based on your map_button_to_name mapping
// Adjust these numbers if your controller uses different IDs for Triangle/Cross
#define BTN_TRIANGLE  1 
#define BTN_CROSS     2

/**************************************
 * Motor control setup and configuration
 **************************************/
int16_t throttleLeft = 0;
int16_t throttleRight = 0;

volatile int16_t new_left_wheel_velocity = 0;
volatile int16_t new_right_wheel_velocity = 0;

enum DriveMode { TANK, LOCK_FORWARD, LOCK_BACKWARD };
DriveMode currentMode = TANK;

enum DriveDirection { STOP, mFORWARD, mREVERSE, LEFT, RIGHT };
DriveDirection mDirection = STOP;

// Returns 1.0 for positive x, -1.0 for negative x (including -0.0).
//double sign(double x) {
//    double sign = std::copysign(1.0, x); 
//    if (x == 0) sign = 0;
//    return sign; 
//}


/***********************************************
 * Wheeel Encoder setup and config for odometry
 ************************************************/
/*  Lets set up Wheel Encoders   */
#include <Encoder.h>  // Note this is my modified library to support R4 and Q
Encoder leftEnc(2, 4);  //was 8, 5
Encoder rightEnc(6, 7);


#define MSGQ_MAX_MSGS 20
#define MSG_SIZE sizeof(struct sensor_msg)

/*  Required for calculating yaw and heading   *
 *  from quaternions.  For the BNO055 rotation *
 *  is ZYX not XYZ.                            *
 */
double w, x, y, z;
double yaw, pitch, roll;
double sqw, sqx, sqy, sqz;

/***********************************************
 * BNO055 sensor message to the Odometry thread
 ************************************************/
/* BNO055 sensor message to the Odometry thread */
struct sensor_msg {
    uint32_t timestamp;
    double roll;
    double pitch;
    double yaw;
};


#define METERS_PER_TICK 0.00008268
#define WHEEL_BASE 0.152

float X_position, Y_position;

/********************************************
 * PATTERN A: Mutex-Protected Global State  *
 ********************************************/
struct odom_state {
    float x;
    float y;
    float pitch;
    float roll;
    float heading; // in radians
    long left_enc;
    long right_enc;
};

/****************************************
 * Steering Values
 ****************************************/
volatile float obs_dist;
volatile float steering_angle;
volatile int valid_points;

bool manualControl = true;
bool autoControl = false;

// Wheel control parameters
const int BASE_FORWARD_SPEED = 120; // Standard forward throttle (0-255)
const int MAX_STEER_SPEED   = 200; // Max speed for aggressive turns
const float Kp              = 2.5f; // Proportional gain for steering
const float ANGLE_DEADBAND  = 2.0f; // Degrees error to treat as straight



/* ----------------------------------------------------------------------- */
/*                              stasis detector                            */
/* ----------------------------------------------------------------------- */

#define NOT_TURNING 0.005
#define ARE_TURNING 0.012 //was 0.010 for straight or backward
#define ARE_TURNING_LR 0.50
#define GLOBAL_STASIS_ERR 60  // was 60

/* stasis detector globals */

int stasis_err;         /* number of times imu and wheels very different */
int stasis_alarm;       /* signal too many stasis errors */

