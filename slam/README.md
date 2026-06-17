
🚀 ROS 2 Cartographer SLAM & Nav2 Project for RPi 5
이 프로젝트는 Raspberry Pi 5 환경에서 ROS 2 (Jazzy)를 기반으로 LD08 라이다와 MPU6050 IMU 센서를 융합하여 2D Cartographer SLAM을 수행하고, 생성된 지도를 바탕으로 Navigation2(Nav2) 자율주행을 구현한 종합 패키지입니다.
🛠 System Overview & Requirements
OS: Ubuntu (Raspberry Pi 5)
ROS Version: ROS 2 Jazzy
Sensors:
LiDAR: LD08 (ld08_driver 패키지 사용)
IMU: MPU6050 (I2C 통신, ros2_mpu6050 노드 사용)
Key Algorithms: Cartographer (2D SLAM), RF2O Laser Odometry, Robot Localization (EKF), Navigation2
📂 Key Features & Node Configuration
1. Cartographer 2D SLAM (my_carto.lua)
Raspberry Pi 5의 하드웨어 리소스 한계를 고려하여 최적화된 파라미터가 적용되었습니다.
프레임 설정: tracking_frame은 base_link(IMU 기준)를 사용하며, EKF 노드가 Odom을 발행하므로 provide_odom_frame은 false로 설정하여 TF 충돌을 방지했습니다
.
IMU 융합: TRAJECTORY_BUILDER_2D.use_imu_data = true로 설정하여 IMU 데이터를 궤적 생성에 적극 반영합니다
.
연산량 최적화:
라이다 스캔의 최대 거리를 8.0m (max_range = 8.0)로 제한하여 불필요한 연산을 줄였습니다
.
루프 클로징 최적화 주기(optimize_every_n_nodes)를 45로 설정하여 CPU 부하를 대폭 감소시키고 시스템 안정성을 확보했습니다
.
2. Sensor Fusion & Odometry (my_ekf.yaml)
라이다 기반의 오도메트리와 IMU 데이터를 융합하여 신뢰성 있는 로봇의 위치(Odometry)를 추정합니다.
RF2O Laser Odometry: 라이다의 스캔 데이터를 바탕으로 실시간 2D 오도메트리를 계산합니다
.
EKF Filter (robot_localization): two_d_mode: true 및 publish_tf: true 로 설정되어 2D 평면 상의 최종 odom 좌표계를 시스템에 발행합니다
.
3. Navigation2 (nav2_slam_params.yaml)
안정적인 자율주행을 위한 Nav2 파라미터 세부 설정입니다.
Controller Server: 로봇이 주행 중 경로를 이탈하거나 제어가 지연될 때 발생하는 Aborted 오류를 방지하기 위해 failure_tolerance를 10000.0초로 넉넉하게 설정했습니다
.
Costmap 튜닝:
로봇의 반지름(robot_radius)은 0.22m로 설정되었습니다
.
Local/Global Costmap 모두 static_layer, obstacle_layer, inflation_layer를 활성화하였습니다
.
장애물 인식의 최대 범위(obstacle_max_range)는 2.5m, 레이트레이싱(raytrace_max_range)은 3.0m로 튜닝되었습니다
.
Planner Server: A* 알고리즘 대신 NavfnPlanner를 기본 플래너로 사용합니다 (use_astar: false)
.
4. Custom UDP 제어 통신 (nav2_to_udp_mapper.py)
자율주행 시 Nav2의 cmd_vel (Twist 형태의 제어 명령)을 수신(geometry_msgs.msg.Twist)하여, 외부 모터 제어기 등으로 전달할 수 있도록 UDP 소켓 통신으로 매핑해 주는 커스텀 노드를 포함하고 있습니다
.
🚀 Execution Flow (Launch Scripts)
단일 스크립트 실행만으로 꼬여있는 프로세스를 정리하고 센서와 주요 알고리즘을 한 번에 실행할 수 있도록 Bash 스크립트가 구현되어 있습니다.
실행 스크립트 주요 로직
초기화: 기존에 실행 중이던 ROS 2 데몬, 라이다(ld08), IMU(mpu6050), Nav2 프로세스를 pkill -9로 깔끔하게 강제 종료합니다
.
센서 구동: i2cset 명령어로 MPU6050의 절전 모드를 해제한 후 센서 노드들을 백그라운드(&)로 실행합니다
.
오도메트리 & TF: RF2O 레이저 오도메트리와 가상 TF(slam_back.launch.py)를 실행합니다
.
프로세스 종료 관리: 스크립트 실행 중 터미널 종료 시 trap "kill 0" EXIT 명령이 작동하여 백그라운드에 띄워둔 모든 프로세스 자식들을 함께 종료합니다
.
사용 방법 (Usage)
1. SLAM (지도 작성) 모드 실행 라이다와 IMU를 이용하여 주변 환경 지도를 생성합니다.
./run_slam.sh  # (제공된 SLAM 런치 쉘 스크립트 파일명으로 대체)
# 내부적으로 carto_slam.launch.py를 최종 실행합니다 [7].
2. Navigation2 (자율 주행) 모드 실행 사전에 저장된 지도(map_last.yaml)를 불러와 자율주행을 시작합니다
.
./run_nav2.sh  # (제공된 Nav2 런치 쉘 스크립트 파일명으로 대체)
# 내부적으로 nav2_bringup의 bringup_launch.py를 실행하며 
# map_last.yaml 및 nav2_slam_params.yaml을 로드합니다 [14].
