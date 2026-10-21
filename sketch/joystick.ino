
const char *map_button_to_name(int btn) {
  for(size_t i = 0; i < sizeof(button_names)/sizeof(button_names[0]); i++) {
    if (button_names[i].value == btn) return button_names[i].name;   
  }
  return nullptr;
}

void joy_connected() {
  joystick_connected_called = true;
}

void joy_button_down(int btn) {
  sprintf(buffer, "BDN: %d", btn);
  Serial.print(buffer);
  
  if (btn == JOY_BTN_L1) {
    if (currentMode == LOCK_FORWARD) {
      currentMode = TANK; // Toggle off if already on
      Serial.println(" -> Mode: Normal Tank");
    } else {
      currentMode = LOCK_FORWARD;
      Serial.println(" -> Mode: LOCKED FORWARD");
    }
  } 
  else if (btn == JOY_BTN_R1) {
    if (currentMode == LOCK_BACKWARD) {
      currentMode = TANK; // Toggle off if already on
      Serial.println(" -> Mode: Normal Tank");
    } else {
      currentMode = LOCK_BACKWARD;
      Serial.println(" -> Mode: LOCKED BACKWARD");
    }
  }
  else if (btn == JOY_BTN_CROSS) {
    Serial.println("Reset Encoders");
    rightEnc.write(0);
    leftEnc.write(0);
  }
  else if (btn == JOY_BTN_SQUARE) {
    autoControl = false;
    manualControl = true;
    Serial.println("MANUAL CONTROL SET");
  }
  else if (btn == JOY_BTN_CIRCLE) {
    manualControl = false;
    autoControl = true;
    Serial.println("AUTO CONTROL SET");
  }

  // Your existing name mapping print...
  const char *btn_name = map_button_to_name(btn);
  if (btn_name != nullptr) {
    Serial.print(" ("); Serial.print(btn_name); Serial.print(")");
  }
  Serial.println();
}

void joy_button_up(int btn) {
  // --- FIX 2: Removed rogue '\n' inside sprintf string to prevent broken double newline spacing ---
  sprintf(buffer, "BUP: %d", btn); 
  Serial.print(buffer);
  const char *btn_name = map_button_to_name(btn);
  if (btn_name != nullptr) {
    Serial.print(" (");
    Serial.print(btn_name);
    Serial.print(")");
  }
  Serial.println();
}

const char *map_axis_to_name(int axis) {
  for(size_t i = 0; i < sizeof(axis_names)/sizeof(axis_names[0]); i++) {
    if (axis_names[i].value == axis) return axis_names[i].name;   
  }
  return nullptr;
}

void joy_axis_motion(std::vector<int> motions) {
  // Only print if we actually received data pairs
  if (motions.size() >= 2) {
    //MSerial.print("Axis Updated ->");
    
    for (size_t i = 0; i < motions.size(); i += 2) {
      int axis_code = motions[i];
      int axis_val = motions[i + 1];
      
      const char *axis_name = map_axis_to_name(axis_code);
      //MSerial.print(" ");
      //if (axis_name != nullptr) {
      //  MSerial.print(axis_name);
      //} else {
      //  MSerial.print(axis_code);
      //}
      //MSerial.print(":");
      //MSerial.print(axis_val);

      // Process our specific drive axis
      if (axis_code == JOY_LEFT_Y) {
        // If your value scales 0-255 with 128 as center:
        if (axis_val >= joystick_axes_mins[JOY_LEFT_Y] && axis_val <= joystick_axes_maxs[JOY_LEFT_Y]) {
          new_left_wheel_velocity = (joystick_axes[JOY_LEFT_Y] - axis_val) * 100 / joystick_axes[JOY_LEFT_Y]; 
        } else {
          // If fallback raw signed 16-bit
          new_left_wheel_velocity = axis_val / 32;
        }
      }
      // Process our specific right drive axis
      if (axis_code == JOY_RIGHT_Y) {
        // If your value scales 0-255 with 128 as center:
        if (axis_val >= joystick_axes_mins[JOY_RIGHT_Y] && axis_val <= joystick_axes_maxs[JOY_RIGHT_Y]) {
          new_right_wheel_velocity = (joystick_axes[JOY_RIGHT_Y] - axis_val) * 100 / joystick_axes[JOY_RIGHT_Y]; 
        } else {
          // If fallback raw signed 16-bit
          new_right_wheel_velocity = axis_val / 32;
        }
      }
    }
    //MSerial.println();
  }
}


void init_joystick_axis(int axis) {
    while(!Bridge.call("get_axis_range", (int)axis).result(joystick_range)) {
      Serial.print("get_axis_range call failed:");
      Serial.println(axis);
    } 
  
      Serial.print("Joystick Axis(");
      Serial.print(axis);
      Serial.print(")Range: ");
      Serial.print(joystick_range[0]);
      Serial.print(",");
      Serial.println(joystick_range[1]);
      
      joystick_axes_mins[axis] = joystick_range[0];
      joystick_axes_maxs[axis] = joystick_range[1];
      joystick_axes[axis] = (joystick_range[0] + joystick_range[1]) / 2;

}


//void joy_hat_motion(int x, int y) {
//  sprintf(buffer, "Hat: %d %d\n", x, y);
//  Serial.print(buffer);
//}