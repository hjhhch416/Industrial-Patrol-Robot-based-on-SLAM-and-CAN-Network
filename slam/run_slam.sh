#!/bin/bash

# 1. ROS2 워크스페이스 환경 설정
echo "Setting up ROS2 workspace..."
source /opt/ros/jazzy/setup.bash  # 설치된 버전에 따라 수정 가능 (예: foxy, humble)
source ~/ros2_ws/install/setup.bash

# 2. 라이다 센서 실행 (백그라운드)
echo "Launching LiDAR Sensor..."
ros2 launch ld08_driver ld08.launch.py &
sleep 3

# 3. MPU6050 센서 초기화 (권한 필요)
echo "Initializing MPU6050 via I2C..."
sudo i2cset -y 1 0x68 0x6B 0x00

# 4. MPU6050 노드 실행 (백그라운드)
echo "Starting MPU6050 Node..."
ros2 run ros2_mpu6050 ros2_mpu6050 ros2_mpu6050_node &
sleep 2

# 5. RF2O 레이저 오도메트리 실행 (백그라운드)
echo "Starting RF2O Laser Odometry..."
ros2 launch rf2o_laser_odometry rf2o_laser_odometry.launch.py &
sleep 2

# 6. 가상 TF 좌표계 실행 (백그라운드)
echo "Launching Virtual TF..."
ros2 launch slam_back.launch.py &
sleep 2

# 7. Cartographer SLAM 실행 (메인 프로세스)
echo "Launching Cartographer SLAM..."
ros2 launch carto_slam.launch.py

# 스크립트 종료 시 백그라운드 프로세스들 모두 종료
trap "kill 0" EXIT
