const express = require('express');
const http = require('http');
const path = require('path');
const WebSocket = require('ws');
const { Server } = require('socket.io');
const Stream = require('node-rtsp-stream');

// --- 설정 변수 ---
const WEB_SERVER_PORT = 3000;
const ROBOT_DATA_PORT = 5000;
const VIDEO_WS_PORT = 9999;   // 카메라용
const MAP_WS_PORT = 10000;    // 맵(RViz2)용

// RTSP 주소
const RTSP_CAMERA_URL = 'rtsp://localhost:8554/mystream';
const RTSP_MAP_URL = 'rtsp://localhost:8554/mapstream'; 

let robotSocket = null;
let videoStream = null;
let mapStream = null;

const app = express();
const httpServer = http.createServer(app);
const io = new Server(httpServer, { cors: { origin: "*" } });

// 정적 파일 서빙
app.use(express.static(path.join(__dirname)));

// --- 스트림 생성 함수 ---
function startStreams() {
    // 1. 카메라 스트림 (9999) - 현재 404 에러가 나더라도 설정은 유지
    if (!videoStream) {
        videoStream = new Stream({
            name: 'robot_camera',
            streamUrl: RTSP_CAMERA_URL,
            wsPort: VIDEO_WS_PORT,
            ffmpegOptions: {
                '-rtsp_transport': 'tcp',  // ffplay와 동일한 안정성 확보
                '-fflags': 'nobuffer',     // 지연 방지
                '-codec:v': 'mpeg1video',
                '-s': '640x480',
                '-b:v': '1000k',
                '-r': '25',                // 프레임 강제 고정
                '-bf': '0'
            }
        });
        console.log(`🎥 카메라 스트림 시작 (Port: ${VIDEO_WS_PORT})`);
    }

    // 2. 맵 스트림 (10000) - 인코더 에러 수정 버전
    if (!mapStream) {
        mapStream = new Stream({
            name: 'robot_map',
            streamUrl: RTSP_MAP_URL,
            wsPort: MAP_WS_PORT,
            ffmpegOptions: {
                '-rtsp_transport': 'tcp',  // 네트워크 패킷 손실 방지
                '-fflags': 'nobuffer',
                '-codec:v': 'mpeg1video',
                '-s': '640x480',
                '-b:v': '1000k',           // 맵 선명도를 위해 비트레이트 소폭 상향
                '-r': '30',                // 중요: 10fps 에러를 해결하기 위해 30fps로 강제 고정
                '-bf': '0',                // 지연 시간 단축
                '-threads': '4'            // 멀티 코어 사용
            }
        });
        console.log(`🗺️ 맵 스트림 시작 (Port: ${MAP_WS_PORT})`);
    }
}

// --- 로봇 데이터 통신 (WebSocket) ---
const wssRobot = new WebSocket.Server({ port: ROBOT_DATA_PORT });
wssRobot.on('connection', (ws) => {
    console.log('🤖 [로봇] 연결됨');
    robotSocket = ws;
    startStreams();

    ws.on('message', (msg) => {
        try {
            const data = JSON.parse(msg);
            io.emit('robot_to_user', data);
        } catch (e) {
            io.emit('robot_to_user', { raw: msg.toString() });
        }
    });

    ws.on('close', () => {
        console.log('❌ [로봇] 연결 종료');
        robotSocket = null;
    });
});

// --- 사용자 웹 통신 (Socket.io) ---
io.on('connection', (socket) => {
    console.log('👤 [웹] 사용자 접속');
    startStreams();

    socket.on('user_to_server', (cmd) => {
        if (robotSocket && robotSocket.readyState === WebSocket.OPEN) {
            robotSocket.send(cmd);
            console.log(`🚀 명령 전달 -> 로봇: ${cmd}`);
        } else {
            socket.emit('server_msg', '로봇과 연결되어 있지 않습니다.');
        }
    });

    socket.on('ping', (cb) => {
        if (typeof cb === 'function') cb();
    });
});

httpServer.listen(WEB_SERVER_PORT, () => {
    console.log(`
    ================================================
    🚀 통합 로봇 관제 서버 가동 중
    🔗 웹 주소: http://localhost:${WEB_SERVER_PORT}
    🤖 로봇 포트: ${ROBOT_DATA_PORT}
    🎥 카메라 포트: ${VIDEO_WS_PORT}
    🗺️ 맵 스트림 포트: ${MAP_WS_PORT}
    ================================================
    `);
});