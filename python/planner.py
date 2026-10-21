"""Potential Field Obstacle Avoidance Planner.

Consumes 3D point clouds (in meters) and calculates steering vectors.
"""

from dataclasses import dataclass
import numpy as np


@dataclass
class SteeringCommand:
    """Steering calculation outputs."""

    f_x: float  # Lateral force (Meters / force units)
    f_z: float  # Forward force
    steering_angle_deg: float  # Steering angle relative to robot heading (-180 to 180)
    obstacle_detected: bool  # True if points fall within safety threshold
    min_distance_m: float  # Distance to closest obstacle point in meters


class PotentialFieldPlanner:

    def __init__(
        self,
        safe_distance_m: float = 0.200,  # 60 cm safety radius
        min_height_m: float = -0.05,  # Floor cut-off relative to sensor
        max_height_m: float = 0.30,  # Ceiling cut-off relative to sensor
        k_rep: float = 0.05,  # Repulsive force scaling gain
        forward_attraction: float = 1.0,  # Nominal forward drive force
    ):
        self.safe_distance_m = safe_distance_m
        self.min_height_m = min_height_m
        self.max_height_m = max_height_m
        self.k_rep = k_rep
        self.forward_attraction = forward_attraction

    def compute_steering(self, points_m: np.ndarray) -> SteeringCommand:
        """Computes steering vector from (N, 3) XYZ point cloud in meters."""
        if points_m is None or len(points_m) == 0:
            return SteeringCommand(
                f_x=0.0,
                f_z=self.forward_attraction,
                steering_angle_deg=0.0,
                obstacle_detected=False,
                min_distance_m=float("inf"),
            )
    
        # 1. Height Filtering (Drop floor plane and ceiling noise)
        y_pts = points_m[:, 1]
        valid_height_mask = (y_pts >= self.min_height_m) & (
            y_pts <= self.max_height_m
        )
        filtered_points = points_m[valid_height_mask]
    
        if len(filtered_points) == 0:
            return SteeringCommand(
                f_x=0.0,
                f_z=self.forward_attraction,
                steering_angle_deg=0.0,
                obstacle_detected=False,
                min_distance_m=float("inf"),
            )
    
        # 2. Extract Horizontal Projection (X = Lateral, Z = Forward Depth)
        x_pts = filtered_points[:, 0]
        z_pts = filtered_points[:, 2]
    
        # 2D Euclidean Distance on horizontal plane
        d_i = np.sqrt(x_pts**2 + z_pts**2)
    
        # Filter out points farther than safe_distance_m AND zero/invalid points (d_i > 0.01m)
        in_range_mask = (d_i < self.safe_distance_m) & (d_i > 0.01)
        if not np.any(in_range_mask):
            return SteeringCommand(
                f_x=0.0,
                f_z=self.forward_attraction,
                steering_angle_deg=0.0,
                obstacle_detected=False,
                min_distance_m=float(np.min(d_i)) if len(d_i) > 0 else float("inf"),
            )
    
        d_i = d_i[in_range_mask]
        x_pts = x_pts[in_range_mask]
        z_pts = z_pts[in_range_mask]
    
        # 3. Compute Potential Field Forces
        # F_rep = k_rep * (1/d - 1/R_safe) * (1 / d^2)
        rep_magnitude = (
            self.k_rep
            * ((1.0 / d_i) - (1.0 / self.safe_distance_m))
            * (1.0 / (d_i**2))
        )
    
        # Epsilon prevents division by zero if a point is extremely close to origin
        eps = 1e-6
        unit_x = -x_pts / (d_i + eps)
        unit_z = -z_pts / (d_i + eps)
    
        # Force vectors push AWAY from obstacle points toward origin
        f_rep_x = np.sum(rep_magnitude * unit_x)
        f_rep_z = np.sum(rep_magnitude * unit_z)
    
        # Add forward constant attraction
        total_f_x = f_rep_x
        total_f_z = f_rep_z + self.forward_attraction
    
        # 4. Calculate Angle (-deg = Left, +deg = Right)
        steering_angle_rad = np.arctan2(total_f_x, total_f_z)
        steering_angle_deg = np.degrees(steering_angle_rad)
    
        return SteeringCommand(
            f_x=float(total_f_x),
            f_z=float(total_f_z),
            steering_angle_deg=float(steering_angle_deg),
            obstacle_detected=True,
            min_distance_m=float(np.min(d_i)),
        )