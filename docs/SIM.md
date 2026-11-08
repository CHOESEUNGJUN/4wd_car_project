# 시뮬레이션 (우분투 서버에서 돌리기)

차의 물리(모터·감속기·L298N·배터리·바닥 마찰·요철)를 계산해서 코너와 출발을 미리 시험한다.
**모르는 값(L298N 손실, 감속기 마찰, 바닥 마찰계수)에 결과가 크게 좌우되므로, 차에서 잰 값으로 `--calibrate` 해서 맞춘 뒤 쓴다.**

| 파일 | 내용 |
|---|---|
| `sim/model.py` | 물리 모델. 숫자는 전부 `Params` |
| `sim/run.py` | 실험 실행 (CPU 코어 나눠서), 결과 CSV + 그래프 PNG |
| `sim/experiments.yaml` | 실험 조건, 기본값, 실측값, 맞출 값 후보 |
| `sim/results/` | 결과 (git 에 안 올라감) |

## 1. 우분투에 처음 설치

```bash
sudo apt install -y git python3-venv gh
gh auth login            # 비공개 저장소라 GitHub 로그인
gh repo clone CHOESEUNGJUN/4wd_car_project
cd 4wd_car_project
python3 -m venv .venv
. .venv/bin/activate
pip install -r sim/requirements.txt
```

## 2. 돌리기

```bash
cd 4wd_car_project && git pull && . .venv/bin/activate
python sim/run.py --list          # 실험 목록
python sim/run.py corner          # 코너 : CORNER_IN x 마찰계수 x 배터리 x 요철
python sim/run.py start           # 출발 : PWM x 배터리 x 킥 있음/없음
```

오래 걸리는 실험은 `tmux` 안에서 (`tmux new -s sim` → 실행 → `Ctrl+b d` 로 빠져나옴, `tmux a -t sim` 으로 다시 보기).

## 3. 결과 보기

- 터미널에 조건별 결과가 찍히고 `sim/results/<날짜_시각>_<실험>.csv` 와 `.png` 로 저장된다.
- 다른 PC·폰에서 보려면 우분투에서 `python3 -m http.server 8000 -d sim/results` → 브라우저로 `http://<우분투 IP>:8000` (같은 와이파이 안에서만).

| CSV 칸 | 뜻 |
|---|---|
| `yaw_deg` / `yaw_min` | T 초 동안 오른쪽으로 돈 각도 평균 / 가장 나쁜 경우 |
| `fwd_cm` / `fwd_min` | 앞으로 간 거리 평균 / 가장 나쁜 경우 |
| `inner_stall` | 안쪽(오른쪽) 바퀴가 멈춰 있던 비율 (감속기를 못 돌리고 끌림) |
| `lift` | 요철 때문에 바퀴가 뜬 비율 |

## 4. 실측값으로 모델 맞추기

1. 차에서 잰다 : 바닥에서 **겨우 출발하는 PWM** 과 그때 **배터리 전압**(멀티미터), 코너 명령 0.8초 동안 **돈 각도**(영상으로).
2. `experiments.yaml` 의 `measured:` 에 적는다.
3. `python sim/run.py --calibrate` → 오차가 가장 작은 값을 `base:` 에 적는다. 이후 실험은 그 값으로 돈다.

## 5. 지금까지 결과 (실측으로 맞추기 전, 참고용)

- **출발** : 니켈수소 5개가 약해지면(5.8V) PWM 150 이상이어야 출발. 킥으로도 못 버팀 → 배터리 6개 또는 2S 리튬이온 권장.
- **코너** (바깥 220, 요철 있는 합판) : 안쪽 −90 이 가장 잘 돎. −170(제자리 회전)은 바퀴만 비비고 가장 덜 돎. 그래서 버전 C 의 `CORNER_IN` 이 −90.
- 마찰계수 0.4~0.5 에서 코너가 0.7 보다 3~5배 잘 돈다 (옆으로 미끄러져야 방향이 바뀌므로).

## 6. 다음 : 트랙 맵

직접 그린 트랙(선 모양)을 넣고, 버전 C 의 라인센서·PID·코너 로직을 그대로 흉내 내서 한 바퀴 돌리는 기능을 추가할 예정.
