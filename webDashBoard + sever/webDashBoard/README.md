# Robot Patrol Dashboard

로봇 순찰 시스템을 제어하고 상태를 확인하기 위한 웹 대시보드입니다.

이 대시보드는 브라우저에서 실행되며, Socket.IO를 통해 서버와 통신하고 JSMpeg 스트림을 통해 카메라 영상과 맵 영상을 표시합니다.

---

## 주요 기능

- 실시간 카메라 영상 표시
- 로봇 연결 상태 표시
- 서버 통신 상태 및 지연 시간 표시
- 좌/우 모터 상태 표시
- 초음파 거리 표시
- 조도 값 표시
- 서보 카메라 각도 표시
- 수동 주행 제어
- 자율 주행 모드 전환
- 조명 ON/OFF 제어
- 경고음 ON/OFF 제어
- 맵 화면 표시

---

## 폴더 구조

```text
webDashBoard
├── index.html
├── style.css
└── script.js
```

---

## 파일별 역할

| 파일 | 역할 |
|---|---|
| `index.html` | 대시보드 화면 구조 |
| `style.css` | 대시보드 UI 디자인 |
| `script.js` | 서버 통신, 영상 스트림, 버튼 제어 로직 |

---

### 대시보드 제어 화면
<img width="1100" height="650" alt="웹 대시보드" src="https://github.com/user-attachments/assets/21b62217-4d76-4f35-88b9-d4b07d45e682" />

### 대시보드 맵 화면
<img width="1100" height="650" alt="웹 대시보드2" src="https://github.com/user-attachments/assets/a46c909b-c6d7-46f5-b504-439406b363a3" />

### 모바일 반응형 화면
<img width="1100" height="650" alt="웹 대시보드 모바일" src="https://github.com/user-attachments/assets/53f2000e-e1f0-416c-a0c6-1802f851015a" />  

---

## 사용 기술

| 구분 | 내용 |
|---|---|
| Frontend | HTML, CSS, JavaScript |
| Realtime Communication | Socket.IO |
| Video Streaming | JSMpeg |
| Icon | Font Awesome |
| Font | Inter |
| Camera Stream | WebSocket |
| Map Stream | WebSocket |

---

## 서버 연결 구조

브라우저는 현재 접속한 호스트 주소를 기준으로 서버에 연결합니다.

```javascript
const host = window.location.hostname;
const socket = io(`http://${host}:3000`);
```

| 포트 | 용도 |
|---|---|
| `3000` | Socket.IO 서버 통신 |
| `9999` | 실시간 카메라 영상 스트림 |
| `10000` | 맵 영상 스트림 |

---

## 영상 스트림

### 카메라 영상

```javascript
new JSMpeg.Player(`ws://${host}:9999/`, {
  canvas: videoCanvas,
  autoplay: true,
  audio: false
});
```

### 맵 영상

```javascript
new JSMpeg.Player(`ws://${host}:10000/`, {
  canvas: document.getElementById('map-canvas'),
  autoplay: true,
  audio: false
});
```

---

## 서버에서 수신하는 로봇 상태 데이터

서버는 `robot_to_user` 이벤트로 로봇 상태를 전달합니다.

```javascript
socket.on('robot_to_user', (data) => {
  // 로봇 상태 UI 업데이트
});
```

수신 데이터 예시는 다음과 같습니다.

```json
{
  "status": "정상",
  "lux": 120,
  "light": "ON",
  "motor_l": 500,
  "motor_r": 500,
  "camera_angle": 90,
  "ultrasonic": 35
}
```

| 데이터 | 설명 |
|---|---|
| `status` | 경고 상태 |
| `lux` | 조도 값 |
| `light` | 조명 상태 |
| `motor_l` | 왼쪽 모터 속도 |
| `motor_r` | 오른쪽 모터 속도 |
| `camera_angle` | 카메라 서보 각도 |
| `ultrasonic` | 초음파 거리 |

---

## 사용자 제어 명령

사용자가 버튼을 누르면 `user_to_server` 이벤트로 명령을 전송합니다.

```javascript
function sendCommand(cmd) {
  socket.emit('user_to_server', cmd);
}
```

---

## 주행 제어 명령

| 버튼 | 전송 명령 | 설명 |
|---|---|---|
| 전진 | `ROVER_FORWARD` | 로봇 전진 |
| 후진 | `ROVER_BACKWARD` | 로봇 후진 |
| 좌회전 | `ROVER_LEFT` | 제자리 좌회전 |
| 우회전 | `ROVER_RIGHT` | 제자리 우회전 |
| 정지 | `ROVER_STOP` | 로봇 정지 |

---

## 카메라 제어 명령

| 버튼 | 전송 명령 | 설명 |
|---|---|---|
| 좌회전 | `CAM_LEFT` | 카메라 왼쪽 회전 |
| 우회전 | `CAM_RIGHT` | 카메라 오른쪽 회전 |
| 정지 | `CAM_STOP` | 카메라 회전 정지 |

---

## 모드 및 장치 제어 명령

| 기능 | 전송 명령 |
|---|---|
| 자율 주행 ON | `AUTO_MODE_ON` |
| 자율 주행 OFF | `AUTO_MODE_OFF` |
| 조명 ON | `LIGHT_ON` |
| 조명 OFF | `LIGHT_OFF` |
| 경고 ON | `SIREN_ON` |
| 경고 OFF | `SIREN_OFF` |

---

## 자율 주행 모드

자율 주행 모드가 켜지면 수동 주행 버튼과 카메라 제어 버튼이 비활성화됩니다.

```javascript
if (isAuto) {
  sendCommand('AUTO_MODE_ON');
  sendCommand('ROVER_STOP');
} else {
  sendCommand('AUTO_MODE_OFF');
}
```

---

## 연결 상태 표시

대시보드는 서버 연결 상태와 로봇 데이터 수신 상태를 각각 표시합니다.

| 상태 | 설명 |
|---|---|
| Server Online | Socket.IO 서버 연결 성공 |
| Server Offline | Socket.IO 서버 연결 끊김 |
| Robot Connected | 최근 로봇 데이터 수신 |
| Robot Disconnected | 일정 시간 동안 로봇 데이터 미수신 |

지연 시간은 2초마다 `ping` 이벤트로 측정합니다.

```javascript
setInterval(() => {
  const start = Date.now();
  socket.volatile.emit('ping', () => {
    const latency = Date.now() - start;
  });
}, 2000);
```

---

## 실행 방법

브라우저에서 대시보드 페이지에 접속합니다.

```text
http://서버_IP/dashboard
```

또는 개발 환경에 따라 다음과 같이 접속합니다.

```text
http://localhost/dashboard
```

서버에서는 다음 포트들이 실행되어 있어야 합니다.

```text
3000  : Socket.IO 서버
9999  : 카메라 스트림
10000 : 맵 스트림
```

---

## 주의사항

- `script.js`는 현재 접속한 호스트 주소를 기준으로 서버에 연결합니다.
- 카메라 스트림은 `ws://host:9999/` 주소를 사용합니다.
- 맵 스트림은 `ws://host:10000/` 주소를 사용합니다.
- Socket.IO 서버는 `http://host:3000`에서 동작해야 합니다.
- 자율 주행 모드가 켜지면 수동 제어 버튼이 비활성화됩니다.
- 모바일 화면에서도 사용할 수 있도록 반응형 CSS가 적용되어 있습니다.

---

## 프로젝트 요약

이 웹 대시보드는 로봇의 실시간 상태 확인과 원격 제어를 위한 프론트엔드입니다.

사용자는 브라우저에서 카메라 영상을 보면서 로봇을 수동 조작하거나, 자율 주행 모드를 켤 수 있습니다.  
또한 모터 속도, 초음파 거리, 조도, 경고 상태, 서버 연결 상태를 실시간으로 확인할 수 있습니다.
