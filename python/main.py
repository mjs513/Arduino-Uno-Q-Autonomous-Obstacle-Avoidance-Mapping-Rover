import time
from arduino.app_utils import App, Bridge
import config as config
from planner import PotentialFieldPlanner
from joystick_handler import JoystickHandler
from vl53_handler import VL53Sensor

debug_output = False

# Initialize Joystick and VL53 modules
joy = JoystickHandler(debug_output=debug_output)
sensor = VL53Sensor()

# Initialize Steering Planner
sensor_height_m = config.TOF_BOARD.world_position[1]
planner = PotentialFieldPlanner(
    safe_distance_m=0.50,
    min_height_m=-(sensor_height_m - 0.03),
    max_height_m=0.20,
    k_rep=0.01,
)


def set_debug_output(enable: int):
    global debug_output
    debug_output = bool(enable)
    joy.debug_output = debug_output

# Used only for sync purposes.
def linuxStarted():
  return True
    
def main():   
    print("=== Rover Test Bed ===")
    print(">> Waiting for Joystick")

    if not joy.setup():
        print("❌ Joystick setup failed. Exiting.")
        return

    print("\n=== Live Rover Control Loop ===")
    Bridge.notify("joy_connected")

    while True:
        # 1. Update Joystick State
        joy.poll()

        # 2. Process Sensor Data and Compute Navigation Commands
        points_oriented, valid_count = sensor.process_frame()

        if points_oriented is not None:
            cmd = planner.compute_steering(points_oriented)
            #print(
            #    f"Min Distance (mm) : {cmd.min_distance_m * 1000:.2f}\n", 
            #    f"Steering Angle (deg): {-cmd.steering_angle_deg:.2f}\n", 
            #    f"Valid Count        : {int(valid_count)}"
            #)

            if cmd.obstacle_detected and abs(cmd.steering_angle_deg) > 1.5:
                #print(
                #    f"Status: 🚨 Obstacle Detected | ",
                #    f"Closest: {cmd.min_distance_m * 1000:.0f} mm | ",
                #    f"Steer: {-cmd.steering_angle_deg:.1f}° | ",
                #    f"Valid Points: {valid_count}/64"
                #)
                Bridge.notify(
                    "get_steering",
                    float(cmd.min_distance_m * 1000),  # int32 (mm)
                    float(-cmd.steering_angle_deg),  # float
                    int(valid_count),  # int32
                )
            else:
                #print(
                #    f"Status: 🟢 Clear Path Ahead | "
                #    f"Steer: 0.0° | "
                #    f"Valid Points: {valid_count}/64"
                #)
                Bridge.notify(
                    "get_steering",
                    float(cmd.min_distance_m * 1000),  # int32 (mm)
                    float(-cmd.steering_angle_deg),  # float
                    int(valid_count),  # int32
                ) 

        time.sleep(0.025)


# Register Bridge Callbacks
Bridge.provide("linux_started", linuxStarted)
Bridge.provide("debug_output", set_debug_output)
Bridge.provide("get_axis_range", lambda code: joy.get_axis_range(code))


if __name__ == "__main__":   
    main()

App.run()