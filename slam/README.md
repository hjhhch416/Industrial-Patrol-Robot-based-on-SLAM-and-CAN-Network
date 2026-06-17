# 🚀 ROS 2 Cartographer SLAM & Nav2 Project for RPi 5

이 프로젝트는 Raspberry Pi 5 환경에서 ROS 2 (Jazzy)를 기반으로 LD08 라이다와 MPU6050 IMU 센서를 융합하여 2D Cartographer SLAM을 수행하고, 생성된 지도를 바탕으로 Navigation2(Nav2) 자율주행을 구현한 종합 패키지입니다 [1, 2].

## 🛠 System Overview & Requirements

*   **OS/Middleware**: Ubuntu & ROS 2 Jazzy [1, 2]
*   **Sensors**: 
    *   **LiDAR**: LD08 (`ld08_driver` 패키지 사용) [1, 2]
    *   **IMU**: MPU6050 (I2C 통신, `ros2_mpu6050` 노드 사용) [1, 3]
*   **Key Algorithms**: Cartographer (2D SLAM), RF2O Laser Odometry, Robot Localization (EKF), Navigation2 [4-7]

---

## 📂 Key Features & Node Configuration

### 1. Cartographer 2D SLAM (`my_carto.lua`)
Raspberry Pi 5의 하드웨어 리소스 한계를 고려하여 최적화된 파라미터가 적용되었습니다.
*   **IMU 융합**: `TRAJECTORY_BUILDER_2D.use_imu_data = true`로 설정하여 IMU 데이터를 궤적 생성에 적극 반영합니다 [8].
*   **프레임 설정**: `tracking_frame`은 `base_link`를 사용하며, EKF 노드가 Odom을 자체적으로 발행하므로 충돌 방지를 위해 `provide_odom_frame`은 `false`로 설정했습니다 [4].
*   **연산량 최적화**: 라이다 스캔의 최대 거리(`max_range`)를 8.0m로 제한하고, 루프 클로징 최적화 주기(`optimize_every_n_nodes`)를 45로 설정하여 RPi5의 CPU 부하를 대폭 감소시키고 시스템 안정성을 확보했습니다 [9].

### 2. Sensor Fusion & Odometry (`my_ekf.yaml`)
*   **RF2O Laser Odometry**: 라이다 스캔 데이터를 바탕으로 실시간 2D 오도메트리를 추정합니다 [3, 5].
*   **EKF Filter (`robot_localization`)**: `two_d_mode: true` 및 `publish_tf: true` 로 설정되어 2D 평면 상의 최종 `odom` 좌표계를 시스템에 발행합니다 [7].

### 3. Navigation2 (`nav2_slam_params.yaml`)
안정적인 자율주행을 위한 Nav2 파라미터 세부 설정입니다.
*   **Controller Server**: 로봇이 주행 중 경로를 이탈하거나 제어가 지연될 때 발생하는 Aborted 오류를 방지하기 위해 `failure_tolerance`를 10000.0초로 넉넉하게 설정했습니다 [10].
*   **Costmap 튜닝**: 로봇의 크기(`robot_radius`)는 0.22m로 반영되었으며 [11, 12], 장애물 인식의 최대 범위(`obstacle_max_range`)는 2.5m, 레이트레이싱(`raytrace_max_range`)은 3.0m로 튜닝되었습니다 [11, 12].
*   **Planner Server**: A* 알고리즘 대신 `NavfnPlanner`를 기본 플래너로 사용합니다 (`use_astar: false`) [13].

### 4. Custom UDP 제어 통신 (`nav2_to_udp_mapper.py`)
*   자율주행 시 Nav2의 `cmd_vel` (`geometry_msgs.msg.Twist`) 형태 제어 명령을 수신하여, 외부 시스템으로 전달할 수 있도록 UDP 소켓 통신을 지원하는 커스텀 매퍼 노드가 구현되어 있습니다 [14].

---

## 🚀 Execution Flow (Launch Scripts)

단일 스크립트 실행만으로 꼬여있는 프로세스를 정리하고 센서와 주요 알고리즘을 한 번에 실행할 수 있도록 Bash 스크립트가 지원됩니다 [1, 2].

**실행 흐름 로직:**
1.  **초기화**: 기존에 실행 중이던 ROS 2 데몬, 라이다(`ld08`), IMU(`mpu6050`), Nav2 관련 프로세스들을 `pkill -9`로 깔끔하게 강제 종료합니다 [2].
2.  **센서 구동**: `i2cset` 명령어로 MPU6050를 초기화한 뒤 센서 노드들을 백그라운드(`&`)로 실행합니다 [1, 3].
3.  **오도메트리 & TF**: RF2O 레이저 오도메트리와 가상 TF(`slam_back.launch.py`)를 순차적으로 실행합니다 [3, 5].
4.  **프로세스 종료 관리**: 스크립트 종료 시 `trap "kill 0" EXIT` 명령이 작동하여 띄워둔 모든 백그라운드 프로세스를 함께 안전하게 종료합니다 [5, 6].

### 사용 방법 (Usage)

**1. SLAM (지도 작성) 모드 실행**
```bash
# carto_slam.launch.py를 실행하여 맵 생성
./run_slam.sh 
2. Navigation2 (자율 주행) 모드 실행
# map_last.yaml 지도를 불러와 자율주행 실행
./run_nav2.sh