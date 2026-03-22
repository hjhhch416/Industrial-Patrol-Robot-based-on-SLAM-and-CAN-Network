-- my_carto.lua
include "map_builder.lua"
include "trajectory_builder.lua"

options = {
  map_builder = MAP_BUILDER,
  trajectory_builder = TRAJECTORY_BUILDER,
  map_frame = "map",
  tracking_frame = "base_link",        -- IMU 기준으로 추적
  published_frame = "odom",           -- EKF가 odom을 발행하므로 odom 프레임 기준
  odom_frame = "odom",
  provide_odom_frame = false,         -- EKF 노드가 odom TF를 발행하므로 false
  publish_frame_projected_to_2d = true,
  use_odometry = true,                -- rf2o/EKF 데이터 사용
  use_nav_sat = false,
  use_landmarks = false,
  num_laser_scans = 1,
  num_multi_echo_laser_scans = 0,
  num_subdivisions_per_laser_scan = 1,
  num_point_clouds = 0,
  lookup_transform_timeout_sec = 0.2,
  submap_publish_period_sec = 0.3,
  pose_publish_period_sec = 5e-3,
  trajectory_publish_period_sec = 30e-3,
  rangefinder_sampling_ratio = 1.,
  odometry_sampling_ratio = 1.,
  fixed_frame_pose_sampling_ratio = 1.,
  imu_sampling_ratio = 1.,
  landmarks_sampling_ratio = 1.,
}

MAP_BUILDER.use_trajectory_builder_2d = true

-- 2D SLAM 세부 최적화
TRAJECTORY_BUILDER_2D.use_imu_data = true      -- IMU 데이터 융합
TRAJECTORY_BUILDER_2D.min_range = 0.1
TRAJECTORY_BUILDER_2D.max_range = 8.0          -- 연산량 감소를 위해 8m로 제한
TRAJECTORY_BUILDER_2D.use_online_correlative_scan_matching = true -- 라이다 고정력 강화
TRAJECTORY_BUILDER_2D.motion_filter.max_angle_radians = math.rad(0.1)

-- 루프 클로징 최적화 (CPU 부하 감소)
POSE_GRAPH.optimize_every_n_nodes = 45         -- 연산 주기를 늘려 RPi5 안정화

return options
