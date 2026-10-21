/*********************************************************
 * VL53L5CX TOF functions
 **********************************************************/
// --- HELPER FUNCTION: ROT90 ---
// Simulates Python's np.rot90(matrix, k=1) on a flattened 1D array.
// Takes raw ST sensor data and maps it to a rotated 8x8 layout.
int getRotatedIndex(int r, int c) {
    // Original coordinates before 90 deg counter-clockwise rotation:
    // new_row = 7 - col
    // new_col = row
    int orig_r = c;
    int orig_c = 7 - r;
    return (orig_r * 8) + orig_c;
}


int getMatrixIndex(int r, int c) {
    if (IS_SENSOR_ROTATED) {
        // Sideways mounting: Simulates Python's np.rot90(matrix, k=1)
        int orig_r = c;
        int orig_c = 7 - r;
        return (orig_r * 8) + orig_c;
    } else {
        // Standard mounting: Direct linear index calculation
        return (r * 8) + c;
    }
}


// RPC Handler returning all 3 arrays as a JSON object
String get_sensor_telemetry() {
  JSONVar root;
  JSONVar ranges;
  JSONVar statuses;
  JSONVar intensities;

  if (myImager.isDataReady() == true)
  {
    if (myImager.getRangingData(&measurementData))
    {
      // 1. Process 8x8 Matrix using your smart index helper
      for (int r = 0; r < 8; r++) {
        for (int c = 0; c < 8; c++) {
            int rotIndex = (r * 8) + c;
            int targetIndex = getMatrixIndex(r, c); // Decides whether to rotate or not
          
            ranges[targetIndex] = measurementData.distance_mm[targetIndex];
            statuses[targetIndex] = measurementData.target_status[targetIndex];
            if(measurementData.target_status[targetIndex] == 255) statuses[targetIndex] = 14;
            // signal_per_spad is returned in count rate per SPAD
            intensities[targetIndex] = (float)measurementData.signal_per_spad[targetIndex]; 
        }
      }
            
      root["ranges"] = ranges;
      root["statuses"] = statuses;
      root["intensities"] = intensities;

      // 2. Safely read IMU Pitch and Roll from Odometry thread
      float current_pitch = 0.0f;
      float current_roll = 0.0f;

      if (k_mutex_lock(&odom_mutex, K_MSEC(5)) == 0) {
        current_pitch = current_odom.pitch;
        current_roll = current_odom.roll;
        k_mutex_unlock(&odom_mutex);
      }

      // 3. Attach pitch and roll to JSON response
      root["pitch"] = current_pitch;
      root["roll"] = current_roll;

      //Serial.println(JSON.stringify(root));

      // Send back over RPC Bridge
      return JSON.stringify(root);
    }
  }

  // Return empty JSON object if data isn't ready
  return "{}";
}