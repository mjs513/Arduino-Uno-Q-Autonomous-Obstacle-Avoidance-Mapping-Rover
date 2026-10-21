import time
from arduino.app_utils import Bridge
import joystickbridge as joystick

# Mappings
BUTTON_NAMES = {
    305: "cross", 306: "circle", 307: "triangle", 304: "square",
    308: "l1/Y", 309: "r1", 310: "l2_btn", 311: "r2_btn",
    312: "share", 313: "options", 316: "ps", 314: "l3", 315: "r3/options",
    317: "touchpad/Stl Lt Bth", 318: "Stick Rt Btn",
    158: "share", 172: "guide",
}

AXIS_NAMES = {
    0: "lx", 1: "ly", 2: "rx", 5: "ry",
    3: "L2 Analog", 4: "R2 Analog", 9: "R2 Analog", 10: "L2 Analog",
    16: "dpad_x", 17: "dpad_y",
}


class JoystickHandler:
    def __init__(self, debug_output=False):
        self.debug_output = debug_output
        self.connected = False
        self.name = "Unknown"
        self.num_axes = 0
        self.num_buttons = 0
        self.axis_map = []
        self.button_map = []
        self.axis_ranges = {}
        self.last_known_axes = {}
        self.last_known_buttons = {}

    def setup(self) -> bool:
        """Initializes hardware link and populates metadata."""
        print("Initializing Bluetooth & input device node on the brick framework...")
        init_status = joystick.startJoystick()
        print("Bridge Response:", init_status)

        if init_status.get("status") == "error":
            print("❌ Could not stand up Joystick hardware link.")
            return False

        print(">> Joystick connected")
        self.connected = True

        try:
            self.name = joystick.getName().get("name", "Unknown Wireless Controller")
            self.num_axes = joystick.getNumAxes().get("num_axes", 0)
            self.num_buttons = joystick.getNumButtons().get("num_buttons", 0)
            self.axis_map = joystick.getAxisMap().get("axis_map", [])
            self.button_map = joystick.getButtonMap().get("button_map", [])

            response = joystick.getAxisRanges() or {}
            self.axis_ranges = response.get("axis_ranges", response)

            if self.debug_output:
                print(f"Name: {self.name}, Axes: {self.num_axes}, Buttons: {self.num_buttons}")
                self._print_axis_ranges()

        except Exception as e:
            print("Error reading joystick metadata:", e)
            return False

        #Bridge.notify("joy_connected")
        return True

    
    def _print_axis_ranges(self):
        print("\n=== Axis Range Details ===")
        print(f"{'Axis Name':<12} {'Code':<6} {'Min':<8} {'Max':<8} {'Fuzz':<6} {'Flat':<6}")
        print("-" * 52)
        for code in self.axis_map:
            axis_lbl = AXIS_NAMES.get(code, f"axis_{code}")
            rng = self.axis_ranges.get(str(code)) or self.axis_ranges.get(code)
            if rng:
                axis_min, axis_max, fuzz, flat = self._parse_range(rng, code)
                print(f"{axis_lbl:<12} {code:<6} {axis_min:<8} {axis_max:<8} {fuzz:<6} {flat:<6}")

    def _parse_range(self, rng, code):
        if isinstance(rng, dict):
            axis_min = rng.get('min', 0)
            axis_max = rng.get('max', 255)
            fuzz = rng.get('fuzz', 0)
            flat = rng.get('flat', 15 if code in [0, 1, 2, 3, 4, 5] else 0)
        elif isinstance(rng, (list, tuple)):
            axis_min = rng[0]
            axis_max = rng[1]
            fuzz = rng[2] if len(rng) > 2 else 0
            flat = rng[3] if len(rng) > 3 else (15 if code in [0, 1, 2, 3, 4, 5] else 0)
        else:
            axis_min, axis_max, fuzz, flat = 0, 255, 0, 15

        if flat == 0 and code in [0, 1, 2, 3, 4, 5]:
            flat = 15
        return axis_min, axis_max, fuzz, flat

    
    def poll(self):
        """Polls joystick state and notifies Bridge when inputs change."""
        try:
            state = joystick.getState()

            # Process Axes
            if state.get("axes_changed"):
                raw_axes = {int(k): v for k, v in state.get("axes", {}).items()}
                axis_motion_list = []

                for axis_code in self.axis_map:
                    raw_val = raw_axes.get(axis_code, 127)
                    rng = self.axis_ranges.get(str(axis_code)) or self.axis_ranges.get(axis_code) or {}
                    _, _, fuzz, flat = self._parse_range(rng, axis_code)

                    center = 127 if axis_code in [0, 1, 2, 3, 4, 5] else 0
                    processed_val = raw_val
                    if center == 127 and abs(raw_val - center) <= flat:
                        processed_val = center

                    prev_val = self.last_known_axes.get(axis_code, 127)
                    signal_change = abs(processed_val - prev_val)

                    if signal_change > fuzz or (processed_val == center and prev_val != center):
                        axis_motion_list.extend([axis_code, processed_val])
                        self.last_known_axes[axis_code] = processed_val

                if axis_motion_list:
                    if self.debug_output:
                        print(f"Sending Calibrated Axis Updates: {axis_motion_list}")
                    Bridge.notify("joy_axis_motion", axis_motion_list)

            # Process Buttons
            if state.get("buttons_changed"):
                buttons = {int(k): v for k, v in state.get("buttons", {}).items()}
                for btn_code in self.button_map:
                    current_val = buttons.get(btn_code, 0)
                    if self.last_known_buttons.get(btn_code, 0) != current_val:
                        self.last_known_buttons[btn_code] = current_val
                        if self.debug_output:
                            btn_name = BUTTON_NAMES.get(btn_code, 'unknown')
                            print(f"BTN: {btn_code} ({btn_name}) -> {current_val}")

                        event_type = "joy_button_down" if current_val != 0 else "joy_button_up"
                        Bridge.notify(event_type, btn_code)

        except Exception as e:
            if self.debug_output:
                print("Error reading joystick state:", e)

    
    def get_axis_range(self, code: int) -> list:
        try:
            resp = joystick.getAxisRange(code) or {}
            if isinstance(resp, list):
                return resp
            if isinstance(resp, dict):
                if "range" in resp:
                    inner = resp["range"]
                    if isinstance(inner, dict):
                        return [inner.get("min", 0), inner.get("max", 255)]
                    elif isinstance(inner, (list, tuple)):
                        return [inner[0], inner[1]]
                if "min" in resp:
                    return [resp.get("min", 0), resp.get("max", 255)]
        except Exception as e:
            print(f"Error handling get_axis_range for code {code}: {e}")
        return [0, 255]