import json
import time
import numpy as np
from arduino.app_utils import Bridge
from geometry import CoordinateMethod, compute_zone_angles, distances_to_points


class VL53Sensor:
    def __init__(self):
        self.zone_angles = compute_zone_angles()

    def fetch_telemetry(self):
        """Retrieves and parses raw telemetry over Bridge."""
        try:
            raw = Bridge.call("get_sensor_telemetry")
            data = json.loads(raw)

            ranges = np.array(data["ranges"], dtype=np.int32)
            statuses = np.array(data["statuses"], dtype=np.int8)
            intensities = np.array(data["intensities"], dtype=np.float32)
            
            # Extract pitch and roll sent from the BNO055 via Arduino
            #pitch = float(data.get("pitch", 0.0))
            #roll = float(data.get("roll", 0.0))
            pitch = 0.0
            roll = 0.0
   
            return ranges, statuses, intensities, pitch, roll
        except Exception as e:
            print("fetch_sensor_data error:", e)
            return None, None, None, 0.0, 0.0

    @staticmethod
    def apply_pitch_roll(points: np.ndarray, pitch_deg: float, roll_deg: float) -> np.ndarray:
        if points is None or len(points) == 0:
            return points
        p_rad, r_rad = np.radians(pitch_deg), np.radians(roll_deg)
        Rx = np.array([
            [1.0, 0.0, 0.0],
            [0.0, np.cos(p_rad), -np.sin(p_rad)],
            [0.0, np.sin(p_rad), np.cos(p_rad)],
        ])
        Rz = np.array([
            [np.cos(r_rad), -np.sin(r_rad), 0.0],
            [np.sin(r_rad), np.cos(r_rad), 0.0],
            [0.0, 0.0, 1.0],
        ])
        return points @ (Rz @ Rx).T

    def process_frame(self, max_retries=5, retry_delay=0.25):
        """Fetches telemetry, filters noise, and computes oriented 3D point cloud."""
        for attempt in range(max_retries):
                ranges, statuses, intensities, imu_pitch, imu_roll = (
                    self.fetch_telemetry()
                )
        
                if ranges is not None and len(ranges) == 64:
                    break  # Successfully fetched valid telemetry
        
                if attempt < max_retries - 1:
                    time.sleep(retry_delay)
                else:
                    # All retries failed
                    return None, 0

        # Confidence Masking (VL53L5CX valid target status codes: 5, 9, 12, 13)
        status_mask = (
            (statuses == 5) | (statuses == 9) | 
            (statuses == 12) | (statuses == 13)
        )
        intensity_mask = intensities >= 10
        valid_mask = status_mask & intensity_mask

        filtered_ranges = ranges.copy()
        filtered_ranges[~valid_mask] = 0

        # Transform to local space and apply orientation
        points_local = distances_to_points(
            filtered_ranges, self.zone_angles, CoordinateMethod.ST_LOOKUP
        )
        points_oriented = self.apply_pitch_roll(
            points_local, pitch_deg=imu_pitch, roll_deg=imu_roll
        )

        return points_oriented, np.sum(valid_mask)