# 📂 Vision AI: 작업자 안전 장비 탐지 엔진

본 모듈은 건설 현장 내 작업자의 안전 장비(헬멧, 조끼) 착용 여부를 실시간으로 감지하고, 판단 결과에 따라 로봇의 주행을 제어 및 관제 서버로 영상을 송출하는 **YOLO 기반 시각 지능 엔진**입니다.

## 1. 데이터셋 및 학습 (Dataset & Training)

다양한 현장 환경에서도 높은 강인함(Robustness)을 유지하기 위해 데이터 증강 및 정제 과정을 거쳤습니다.

* **데이터 구성**: `Helmet`, `No-Helmet`, `No-Vest`, `Person`, `Vest` 총 5개 클래스
* **학습 전략**: 실제 현장에서 발생할 수 있는 미착용 사례 데이터를 집중 확보하여 학습 편향을 최소화했습니다.

| 데이터셋 클래스 분포 | 탐지 객체 가시화 샘플 |
| :---: | :---: |
| <img src="https://github.com/user-attachments/assets/f110d069-4a6f-413a-934b-e3820e2d0d92" width="400"> | <img src="https://github.com/user-attachments/assets/4464deda-5692-4cce-9e6a-fa3b8c2a6e3e" width="400"> |

---

## 2. 모델 성능 검증 (Model Performance)

임베디드 환경(Raspberry Pi 5)에서 실시간 추론이 가능하도록 최적화된 모델을 사용하였으며, 50 Epoch 학습 결과 실환경 적용에 적합한 수치를 달성했습니다.

* **mAP@0.5**: **0.978** 달성
* **성능 요약**: Precision과 Recall의 균형이 뛰어나며, 특히 작업자(Person)와 헬멧(Helmet) 탐지에서 98% 이상의 정확도를 확보했습니다.

### 📈 학습 결과 지표

| Precision-Recall Curve | Loss & Metrics 추이 |
| :---: | :---: |
| <img src="https://github.com/user-attachments/assets/7756b345-42d1-427a-9566-2c7fdd2a69e7" width="400"> | <img src="https://github.com/user-attachments/assets/c5ac616e-ba5b-4aeb-beb2-e9c127e9fb1b" width="400"> |

---

