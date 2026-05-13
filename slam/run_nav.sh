#!/bin/bash

# 1. 기존 ROS 2 관련 프로세스 종료
echo "Cleaning up existing ROS 2 processes..."
# 특정 노드와 런치 프로세스들을 강제 종료합니다.
sudo pkill -9 -f "ros2"
sudo pkill -9 -f "ld08"
sudo pkill -9 -f "mpu6050"
sudo pkill -9 -f "nav2"
sleep 2

# 2. ROS 2 및 워크스페이스 환경 설정 (Jazzy 버전으로 수정됨)
echo "Setting up ROS 2 Jazzy environment..."
source /opt/ros/jazzy/setup.bash
# 유저 계정(ubuntupi)에 맞게 경로 확인이 필요합니다. 
# 만약 워크스페이스가 /home/ubuntupi/ros2_ws 라면 아래와 같이 수정하세요.
source ~/ros2_ws/install/setup.bash

# 3. 라이다(LiDAR) 센서 드라이버 실행
echo "Launching LD08 LiDAR..."
ros2 launch ld08_driver ld08.launch.py &
sleep 5

# 4. MPU6050 센서 초기화 및 노드 실행
echo "Initializing MPU6050 (I2C)..."
sudo i2cset -y 1 0x68 0x6B 0x00
echo "Starting MPU6050 Node..."
ros2 run ros2_mpu6050 ros2_mpu6050_node &
sleep 2

# 5. RF2O 레이저 오도메트리 및 가상 TF 실행
echo "Starting RF2O Laser Odometry..."
ros2 launch rf2o_laser_odometry rf2o_laser_odometry.launch.py &
sleep 2

echo "Launching Virtual TF..."
ros2 launch slam_back.launch.py &
sleep 2

# 6. Nav2 Bringup 실행 (Jazzy 파라미터 적용)
echo "Starting Nav2 Bringup with Map..."
# 파일 경로(/home/lee 또는 /home/ubuntupi)를 실제 파일이 있는 곳으로 맞춰주세요.
ros2 launch nav2_bringup bringup_launch.py \
    map:=/home/lee/map_last.yaml \
    use_sim_time:=False \
    params_file:=/home/lee/nav2_slam_params.yaml

# 스크립트 종료 시 모든 백그라운드 프로세스 종료
trap "kill 0" EXIT
