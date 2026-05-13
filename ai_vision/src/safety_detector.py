import os
import cv2                 # 영상 처리 (OpenCV)
import numpy as np         # 행렬 연산 (데이터 처리)
import onnxruntime as ort  # AI 모델 실행 엔진
import socket              # UDP 통신 (경보 신호 전송)
import time                # 시간 측정 1.5초 지연 계산
import subprocess          # 외부 프로그램(FFmpeg) 실행

# =========================================================
# [설정]
# =========================================================
TARGET_CLASSES = {
    0: "Helmet",
    1: "No-Helmet",
    2: "No-Vest",
    3: "Person",
    4: "Vest"
}
DANGER_CLASSES      = {1, 2}   
CONFIDENCE_THRES    = 0.50   # 50% 이상의 확신이 있을 때만 인정
IOU_THRES           = 0.30   # 박스가 30% 이상 겹치면 중복으로 판단 (NMS)
PROCESS_EVERY_N     = 3      # 매 프레임 연산하면 라파가 힘들어하니까 3프레임당 1번만 AI를 돌려 효율을 높인다.     
TRIGGER_DELAY       = 1.5    # 1.5초 동안 위험이 지속되어야 신호 전송

UDP_IP   = "127.0.0.1" # 로컬 서버로 신호 전송
UDP_PORT = 9999
os.environ["QT_QPA_PLATFORM"] = "xcb" # GUI 관련 환경 설정

# =========================================================
# [RTSP 송출 설정]
# =========================================================
# [RTSP 송출 설정] FFmpeg를 이용해 네트워크 스트리밍 생성
SERVER_IP = "192.168.0.48"
RTSP_URL = f"rtsp://{SERVER_IP}:8554/mystream"

# FFmpeg 명령어: 영상을 H.264로 압축하여 RTSP 서버로 던짐
ffmpeg_cmd = [
    'ffmpeg', '-y', '-f', 'rawvideo', '-vcodec', 'rawvideo',
    '-pix_fmt', 'bgr24', '-s', '640x480', '-r', '20',
    '-i', '-', '-c:v', 'libx264', '-preset', 'ultrafast', '-tune', 'zerolatency',
    '-f', 'rtsp', RTSP_URL
]
# FFmpeg 프로세스 시작 (파이썬에서 영상 데이터를 파이프로 밀어 넣을 준비)
ffmpeg_p = subprocess.Popen(ffmpeg_cmd, stdin=subprocess.PIPE)
print(f"[*] RTSP 송출 파이프 준비 완료 → {RTSP_URL}")

# =========================================================
# [UDP 소켓 초기화]
# =========================================================
# UDP 소켓: 아주 가볍고 빠른 패킷 전송 (경보 신호용)
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.setblocking(False) # 네트워크 지연으로 프로그램이 멈추지 않게 비동기 설정

def send_udp(flag: int):
    try:
        sock.sendto(str(flag).encode(), (UDP_IP, UDP_PORT))
    except Exception as e:
        print(f"[!] UDP 전송 에러: {e}")

# =========================================================
# [모델 로드 및 전처리/후처리]
# =========================================================
# ONNX 모델 로드
ONNX_PATH = "/home/lee/project/test4/safety_final.onnx"
session = ort.InferenceSession(ONNX_PATH, providers=['CPUExecutionProvider'])

model_inputs = session.get_inputs()
input_name   = model_inputs[0].name
input_shape  = model_inputs[0].shape
output_names = [o.name for o in session.get_outputs()]
input_height = input_shape[2] if isinstance(input_shape[2], int) else 640
input_width  = input_shape[3] if isinstance(input_shape[3], int) else 640

def letterbox(im, new_shape=(640, 640), color=(114, 114, 114)):
# 이미지 비율을 유지하면서 크기를 조정하고 남는 공간을 채움
    h, w  = im.shape[:2]
    r     = min(new_shape[0] / h, new_shape[1] / w)
    new_unpad = (int(round(w * r)), int(round(h * r)))
    dw, dh = (new_shape[1] - new_unpad[0]) / 2, (new_shape[0] - new_unpad[1]) / 2
    if (w, h) != new_unpad:
        im = cv2.resize(im, new_unpad, interpolation=cv2.INTER_LINEAR)
    # 테두리 여백 추가 (Padding)
    top, bottom = int(round(dh - 0.1)), int(round(dh + 0.1))
    left, right = int(round(dw - 0.1)), int(round(dw + 0.1))
    im = cv2.copyMakeBorder(im, top, bottom, left, right, cv2.BORDER_CONSTANT, value=color)
    return im, r, (dw, dh)

def preprocess(frame):
    img, ratio, (pad_w, pad_h) = letterbox(frame, (input_height, input_width))
    img = cv2.cvtColor(img, cv2.COLOR_BGR2RGB).astype(np.float32) / 255.0
    img = np.ascontiguousarray(np.transpose(img, (2, 0, 1)))[np.newaxis]
    return img, ratio, pad_w, pad_h

def postprocess(outputs, ratio, pad_w, pad_h):
# 모델 결과값(배열)을 실제 좌표와 클래스 정보로 변환
    preds = np.squeeze(outputs[0])
    if preds.ndim == 1: preds = preds[np.newaxis]
    if preds.shape[0] < preds.shape[1]: preds = preds.T
    
    num_feat = preds.shape[1]
    if num_feat == 6: # [x, y, w, h, score, class] 형식
        scores_all, class_ids_all = preds[:, 4], preds[:, 5].astype(int)
        cx, cy, w_, h_ = preds[:, 0] + preds[:, 2] / 2, preds[:, 1] + preds[:, 3] / 2, preds[:, 2], preds[:, 3]
    elif num_feat == 10: # 점수가 여러 클래스로 나뉘어 있는 형식
        obj_scores, cls_scores = preds[:, 4], preds[:, 5:] # 존재 확률 * 클래스 확률
        class_ids_all = np.argmax(cls_scores, axis=1) # 가장 확률 높은 클래스 번호
        scores_all = obj_scores * cls_scores[np.arange(len(preds)), class_ids_all] # 최종 점수 계산
        cx, cy, w_, h_ = preds[:, 0], preds[:, 1], preds[:, 2], preds[:, 3]
    else: return []

    # 설정한 기준치(예: 50%)보다 낮은 점수나 관심 없는 클래스는 버림.
    valid = (scores_all >= CONFIDENCE_THRES) & np.isin(class_ids_all, list(TARGET_CLASSES.keys()))
    if not np.any(valid): return [] # 유효한 결과가 없으면 즉시 종료

    # valid(유효한) 데이터만 남기기
    cx, cy, w_, h_, scores_all, class_ids_all = cx[valid], cy[valid], w_[valid], h_[valid], scores_all[valid], class_ids_all[valid]

    # (중심점x, 중심점y) -> (좌측 상단x, 좌측 상단y)로 변환하고
    # letterbox에서 추가했던 여백(pad)을 빼고, 줄였던 비율(ratio)만큼 다시 키웁니다.
    x, y = (cx - w_ / 2 - pad_w) / ratio, (cy - h_ / 2 - pad_h) / ratio
    boxes_arr = np.stack([x, y, w_ / ratio, h_ / ratio], axis=1).astype(int)
    
    # ★ 다중 클래스 NMS 박스 겹침 현상 방지
    # 헬멧과 사람이 겹쳐 있을 때 NMS가 '같은 물체'라고 착각해서 지우지 않도록, 
    # 클래스마다 박스 좌표를 아주 멀리 떨어뜨려(offset) 별개로 인식하게 만듭니다.
    MAX_WH = 4096
    offset_boxes = (boxes_arr + (class_ids_all[:, np.newaxis] * MAX_WH * np.array([1, 1, 0, 0]))).tolist()
    
    # 겹침 정도(IOU)를 따져서 가장 점수 높은 박스만 골라냅니다.
    # 앞에서 미리 걸러냈더라도, 함수 입장에서는 "내가 최종적으로 신뢰할 기준점"이 무엇인지 알아야 내부 로직을 완성할 수 있기 때문에 한 번 더 명시해 주는 것이 안전.
    # NMSBoxes 함수는 고작 몇십 개 수준의 "알짜배기" 박스만 처리하면 되므로 전체적인 실행 속도가 라즈베리 파이 같은 환경에서 훨씬 유리.
    indices = cv2.dnn.NMSBoxes(offset_boxes, scores_all.tolist(), CONFIDENCE_THRES, IOU_THRES) 
    
    # 최종적으로 살아남은 박스들만 모아서 리스트로 반환합니다.
    if len(indices) == 0: return []
    return [{'box': boxes_arr[i].tolist(), 'score': float(scores_all[i]), 'class_id': int(class_ids_all[i])} for i in indices.flatten()]

# =========================================================
# [메인 루프]
# =========================================================
cap = cv2.VideoCapture(0, cv2.CAP_V4L2) # 카메라 연결
cap.set(cv2.CAP_PROP_FRAME_WIDTH, 640) # 가로 해상도 640 설정
cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 480) # 세로 해상도 480 설정

frame_count = 0 # 프레임 번호 카운터
last_boxes  = [] # 가장 최근에 AI가 찾은 박스 정보 저장

confirmed_flag = 0   # 최종 확정된 상태 (0: 안전, 1: 위험) -> 전광판/UDP 전송용       
current_raw_danger = 0       # 지금 이 순간 AI가 보고 있는 상태 (0: 없음, 1: 발견)
state_start_time = time.time() # 상태가 변한 '시점'을 기록하는 시계

print("[*] 추론 시작 (1.5초 지연 로직 + 시각화 버그 수정 완료)\n")

while True:
    ret, frame = cap.read()
    if not ret or frame is None: continue

    frame_count += 1
    
    # ── 1. AI 추론 및 1.5초 지연 판단 ──
    if frame_count % PROCESS_EVERY_N == 0:
        img, ratio, pad_w, pad_h = preprocess(frame)
        outputs = session.run(output_names, {input_name: img})
        last_boxes = postprocess(outputs, ratio, pad_w, pad_h)
        
        # 현재 프레임에 위험 요소가 있는지 확인 (0 또는 1)
        new_danger_detected = int(any(item['class_id'] in DANGER_CLASSES for item in last_boxes))
        
        # [핵심] 상태 변화 감지 및 타이머 작동
        if new_danger_detected != current_raw_danger:
            current_raw_danger = new_danger_detected # 위험 상황이 발생했거나 사라짐
            state_start_time = time.time() # 그 순간부터 시간을 잼
        
        # 설정한 TRIGGER_DELAY(1.5초) 이상 상태가 유지될 때만 최종 확정
        if (time.time() - state_start_time) >= TRIGGER_DELAY:
            if current_raw_danger != confirmed_flag:
                confirmed_flag = current_raw_danger # 확정 상태 업데이트
                send_udp(confirmed_flag) # 외부로 신호 전송
                print(f"  [확정] 상태 업데이트 -> {confirmed_flag} ({TRIGGER_DELAY}초 지속)")

    # ── 2. 시각화 (좌표 보정 및 클리핑 적용) ──
    display = frame.copy()
    
    status_text = "DANGER" if confirmed_flag == 1 else "SAFE"
    status_color = (0, 0, 255) if confirmed_flag == 1 else (0, 255, 0)
    cv2.putText(display, f"STATUS: {status_text}", (12, 42), cv2.FONT_HERSHEY_SIMPLEX, 1.2, (0, 0, 0), 4) 
    cv2.putText(display, f"STATUS: {status_text}", (10, 40), cv2.FONT_HERSHEY_SIMPLEX, 1.2, status_color, 3)

    for item in last_boxes:
        x, y, w, h = item['box']
        cid = item['class_id']
        score = item['score']
        color = (0, 0, 255) if cid in DANGER_CLASSES else (0, 255, 0)
        
        # ★ 화면 밖으로 나가지 않도록 좌표 자르기 (Clipping)
        x1 = max(0, x)
        y1 = max(0, y)
        x2 = min(640, x + w)
        y2 = min(480, y + h)
        
        # 박스 그리기
        cv2.rectangle(display, (x1, y1), (x2, y2), color, 2)
        
        # ★ 라벨이 화면 위로 잘리지 않도록 위치 동적 조정
        label_text = f"{TARGET_CLASSES[cid]} {score*100:.0f}%"
        label_y = y1 - 5 if y1 > 20 else y1 + 20
        
        # 글씨 뒤에 검은색 배경을 깔아서 눈에 잘 띄게 만들기
        (tw, th), _ = cv2.getTextSize(label_text, cv2.FONT_HERSHEY_SIMPLEX, 0.6, 2)
        cv2.rectangle(display, (x1, label_y - th - 5), (x1 + tw, label_y + 5), (0,0,0), -1)
        cv2.putText(display, label_text, (x1, label_y), cv2.FONT_HERSHEY_SIMPLEX, 0.6, color, 2)

    # ── 3. 영상 송출 ──
    # FFmpeg 파이프에 이미지 바이트 전송 (RTSP 송출)
    try:
        if ffmpeg_p.poll() is None:
            ffmpeg_p.stdin.write(display.tobytes())
            ffmpeg_p.stdin.flush()
    except:
        break

ffmpeg_p.stdin.close()
ffmpeg_p.wait()
cap.release()
