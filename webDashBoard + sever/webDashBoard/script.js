document.addEventListener('DOMContentLoaded', () => {
    // === 1. 설정 및 변수 초기화 ===
    const host = window.location.hostname;
    const socket = io(`http://${host}:3000`);

    // UI 요소 참조
    const mapPlaceholder = document.getElementById('map-placeholder');
    const mapImage = document.getElementById('map-canvas'); // MJPEG용 img 태그
    const videoCanvas = document.getElementById('video-canvas'); // JSMpeg용 canvas
    
    const serverDot = document.querySelector('.server-dot');
    const robotDot = document.querySelector('.robot-dot');
    const connectionStatusDisplay = document.getElementById('connection-status');
    const latencyStatusDisplay = document.getElementById('latency-status');
    const modeValueDisplay = document.querySelector('.mode-value');

    const motorLeftDisplay = document.getElementById('motor-l-status');
    const motorRightDisplay = document.getElementById('motor-r-status');

    let robotTimeoutId = null;

    // === 2. 영상 스트림 설정 ===

    // [카메라: JSMpeg (9999 포트)]
    try {
        new JSMpeg.Player(`ws://${host}:9999/`, {
            canvas: videoCanvas,
            autoplay: true,
            audio: false,
            videoBufferSize: 512 * 1024,
            preserveDrawingBuffer: false
        });
        console.log(`🎥 카메라 연결 시도: ws://${host}:9999/`);
    } catch (e) {
        console.error("❌ 카메라 초기화 실패:", e);
    }

    try {
    new JSMpeg.Player(`ws://${host}:10000/`, {
        canvas: document.getElementById('map-canvas'),
        autoplay: true,
        audio: false,
        onSourceEstablished: () => {
            console.log("🗺️ 맵 스트림 연결 성공");
            document.getElementById('map-placeholder').style.display = 'none';
            document.getElementById('map-canvas').style.display = 'block';
        }
    });
} catch (e) {
    console.error("❌ 맵 초기화 실패:", e);
}
    
    // === 3. Socket.io 통신 로직 ===

    socket.on('connect', () => {
        serverDot.classList.add('connected');
        connectionStatusDisplay.innerText = 'Online';
        connectionStatusDisplay.style.color = 'var(--accent-green)';
    });

    socket.on('disconnect', () => {
        serverDot.classList.remove('connected');
        connectionStatusDisplay.innerText = 'Offline';
        connectionStatusDisplay.style.color = 'var(--text-secondary)';
        latencyStatusDisplay.innerText = '0 ms';
        robotDot.classList.remove('connected');
        clearTimeout(robotTimeoutId);
    });

    // 지연 시간 측정
    setInterval(() => {
        const start = Date.now();
        socket.volatile.emit('ping', () => {
            const latency = Date.now() - start;
            latencyStatusDisplay.innerText = latency + ' ms';
        });
    }, 2000);

    // 로봇 데이터 수신
    socket.on('robot_to_user', (data) => {
        robotDot.classList.add('connected');
        if (robotTimeoutId) clearTimeout(robotTimeoutId);
        robotTimeoutId = setTimeout(() => { robotDot.classList.remove('connected'); }, 3000);

        // UI 데이터 업데이트
        if (data.status !== undefined) updateWarningUI(data.status);
        if (data.lux !== undefined) document.getElementById('lux-status').innerText = data.lux + ' lx';
        if (data.light !== undefined) document.getElementById('light-status').innerText = (data.light === 'ON' || data.light === true || data.light === 1) ? 'ON' : 'OFF';
        if (data.motor_l !== undefined) motorLeftDisplay.innerText = data.motor_l;
        if (data.motor_r !== undefined) motorRightDisplay.innerText = data.motor_r;
        if (data.camera_angle !== undefined) document.getElementById('camera-angle-status').innerHTML = data.camera_angle + '&deg;';
        if (data.ultrasonic !== undefined) document.getElementById('ultrasonic-status').innerText = data.ultrasonic + ' cm';
    });

    function updateWarningUI(status) {
        const warningStatusDisplay = document.getElementById('warning-status');
        const statusDot = document.querySelector('.status-dot');
        const isWarning = (status === '경고 작동 중' || status === '경고');

        warningStatusDisplay.innerText = isWarning ? '경고 작동 중' : '정상';
        warningStatusDisplay.style.color = isWarning ? 'var(--accent-red)' : 'var(--text-primary)';
        if (statusDot) {
            statusDot.style.boxShadow = isWarning ? '0 0 15px var(--accent-red), 0 0 30px var(--accent-red)' : '0 0 8px var(--accent-red)';
            statusDot.style.animation = isWarning ? 'blink 1s infinite alternate' : 'none';
        }
    }

    function sendCommand(cmd) {
        socket.emit('user_to_server', cmd);
    }

    // === 4. 상호작용 (탭, 모드, 제어) ===

    // 탭 전환
    const tabBtns = document.querySelectorAll('.tab-btn');
    const tabPanes = document.querySelectorAll('.tab-pane');
    tabBtns.forEach(btn => {
        btn.addEventListener('click', () => {
            tabBtns.forEach(b => b.classList.remove('active'));
            tabPanes.forEach(p => p.classList.remove('active'));
            btn.classList.add('active');
            document.getElementById(btn.getAttribute('data-tab')).classList.add('active');
        });
    });

    // 모드 설정
    const autoModeToggle = document.getElementById('auto-mode-toggle');
    const lightToggle = document.getElementById('light-toggle');
    const sirenToggle = document.getElementById('siren-toggle');

    autoModeToggle.addEventListener('change', (e) => {
        const isAuto = e.target.checked;
        const controls = document.querySelectorAll('.d-btn, .cam-btn, #capture-btn');
        
        if (isAuto) {
            modeValueDisplay.innerHTML = '<i class="fas fa-robot"></i> 자율 주행';
            modeValueDisplay.style.color = 'var(--accent-cyan)';
            controls.forEach(c => c.classList.add('disabled'));
            lightToggle.disabled = sirenToggle.disabled = true;
            sendCommand('AUTO_MODE_ON');
            sendCommand('ROVER_STOP');
        } else {
            modeValueDisplay.innerHTML = '<i class="fas fa-gamepad"></i> 수동 조종';
            modeValueDisplay.style.color = 'var(--accent-green)';
            controls.forEach(c => c.classList.remove('disabled'));
            lightToggle.disabled = sirenToggle.disabled = false;
            sendCommand('AUTO_MODE_OFF');
        }
    });

    // 주행 버튼 (공유해주신 500/-300 수치 반영)
    const dBtns = document.querySelectorAll('.d-btn');
    dBtns.forEach(btn => {
        const handleStart = (e) => {
            if (e.cancelable) e.preventDefault();
            if (autoModeToggle.checked) return;

            if (btn.classList.contains('up')) {
                motorLeftDisplay.textContent = '500'; motorRightDisplay.textContent = '500';
                sendCommand('ROVER_FORWARD');
            } else if (btn.classList.contains('down')) {
                motorLeftDisplay.textContent = '-500'; motorRightDisplay.textContent = '-500';
                sendCommand('ROVER_BACKWARD');
            } else if (btn.classList.contains('left')) {
                motorLeftDisplay.textContent = '-300'; motorRightDisplay.textContent = '300';
                sendCommand('ROVER_LEFT');
            } else if (btn.classList.contains('right')) {
                motorLeftDisplay.textContent = '300'; motorRightDisplay.textContent = '-300';
                sendCommand('ROVER_RIGHT');
            } else if (btn.classList.contains('center')) {
                motorLeftDisplay.textContent = '0'; motorRightDisplay.textContent = '0';
                sendCommand('ROVER_STOP');
            }
        };

        const handleStop = (e) => {
            if (e.cancelable) e.preventDefault();
            if (autoModeToggle.checked) return;
            if (!btn.classList.contains('center')) {
                motorLeftDisplay.textContent = '0'; motorRightDisplay.textContent = '0';
                sendCommand('ROVER_STOP');
            }
        };

        btn.addEventListener('mousedown', handleStart);
        btn.addEventListener('touchstart', handleStart, { passive: false });
        btn.addEventListener('mouseup', handleStop);
        btn.addEventListener('mouseleave', handleStop);
        btn.addEventListener('touchend', handleStop, { passive: false });
    });

    // 카메라 제어
    const camLeftBtn = document.querySelector('.cam-left');
    const camRightBtn = document.querySelector('.cam-right');
    if (camLeftBtn && camRightBtn) {
        const handleCam = (cmd) => (e) => { if (e.cancelable) e.preventDefault(); sendCommand(cmd); };
        const stopCam = (e) => { if (e.cancelable) e.preventDefault(); sendCommand('CAM_STOP'); };

        camLeftBtn.addEventListener('mousedown', handleCam('CAM_LEFT'));
        camLeftBtn.addEventListener('touchstart', handleCam('CAM_LEFT'), {passive: false});
        camLeftBtn.addEventListener('mouseup', stopCam);
        camLeftBtn.addEventListener('touchend', stopCam);

        camRightBtn.addEventListener('mousedown', handleCam('CAM_RIGHT'));
        camRightBtn.addEventListener('touchstart', handleCam('CAM_RIGHT'), {passive: false});
        camRightBtn.addEventListener('mouseup', stopCam);
        camRightBtn.addEventListener('touchend', stopCam);
    }

    // 토글 스위치 이벤트
    lightToggle.addEventListener('change', (e) => {
        sendCommand(e.target.checked ? 'LIGHT_ON' : 'LIGHT_OFF');
    });
    sirenToggle.addEventListener('change', (e) => {
        sendCommand(e.target.checked ? 'SIREN_ON' : 'SIREN_OFF');
    });
});

// 스타일 추가
const styleSheet = document.createElement("style");
styleSheet.innerText = `
@keyframes blink { 0% { opacity: 0.5; } 100% { opacity: 1; transform: scale(1.1); } }
.disabled { pointer-events: none; opacity: 0.5; }
`;
document.head.appendChild(styleSheet);