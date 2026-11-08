# 시뮬레이션 (우분투 서버에서 돌리기)

차의 물리(모터·감속기·L298N·배터리·바닥 마찰·요철)를 계산해서 코너와 출발을 미리 시험한다.
**모르는 값(L298N 손실, 감속기 마찰, 바닥 마찰계수)에 결과가 크게 좌우되므로, 차에서 잰 값으로 `--calibrate` 해서 맞춘 뒤 쓴다.**

| 파일 | 내용 |
|---|---|
| `sim/model.py` | 물리 모델. 숫자는 전부 `Params` |
| `sim/run.py` | 실험 실행 (CPU 코어 나눠서), 결과 CSV + 그래프 PNG |
| `sim/experiments.yaml` | 실험 조건, 기본값, 실측값, 맞출 값 후보 |
| `sim/track.py`, `sim/tracks/` | 그린 트랙 위에서 버전 C 로직으로 달리기 (6장) |
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

## 6. 트랙 맵 (`sim/track.py`)

트랙 위에서 버전 C 의 라인센서 3개·PID·코너(바깥 CORNER_OUT / 안쪽 CORNER_IN)·선 놓침(NOLINE → 1.5초면 LOST 정지)·교차(CROSS, 0.5초면 END) 로직을 그대로 돌린다. 물리는 위와 같은 모델(무게 x 중력, 모터·감속기, 배터리, 바닥 마찰).

```bash
python sim/track.py team2                 # 2팀 트랙 : 출발 -> 바깥 고리 합류 -> 2바퀴
python sim/track.py team2 --cell 30 --corner-in -110 --battery 6.2 --seed 3
python sim/track.py oval                  # 예제 타원
python sim/track.py sim/tracks/my.png --start 20,50,90 --lap 20,50 --laps 2   # 직접 그린 PNG
```

**2팀 트랙 (`team2`)** : 그림 그대로 (바깥 둥근 사각형 + 가운데 출발선에서 U 턴·불룩한 곡선을 지나 오른쪽 변에 합류). 선은 절연테이프 폭 1.9cm.
- 회색 칸 한 변 `--cell` (기본 **30cm, 짐작한 값 → 실제 트랙에서 재서 넣기**). 바깥 고리가 13 x 9.75칸 (30cm 면 3.9m x 2.9m).
- 빨간 점선 = 판 이음새, 2칸마다. 판마다 높이 **±0.2mm** 무작위 (`--panel-mm`, 판 배치는 `--seed` 로 바꿈).
- 합류점(오른쪽 변, 보라색 원)을 처음 지나면 합류, 그다음부터 지날 때마다 한 바퀴. `--laps 2` 채우면 DONE.
- 시작할 때 **킥 없이 출발하는 최소 PWM** 을 먼저 계산해 보여 준다 (무게 `--mass` 기본 0.65kg, 배터리 `--battery`). BASE 가 이보다 작아도 출발 킥(255, 0.12초)으로 출발하지만, 멈췄다 다시 갈 때 위험.
- 그림은 `sim/tracks/team2.png` 로도 저장.

**직접 그린 PNG** : 흰 바탕에 검은 선(테이프 폭), 픽셀 하나 = `--scale` cm (기본 0.5). `--start x,y,방향` (cm, 왼쪽 아래 0,0, 방향 0 = 오른쪽 90 = 위), `--lap x,y` 바퀴 세는 점, `--panel` 판 한 변 cm.

바꿀 수 있는 값 : `--kp --ki --kd --base --corner-out --corner-in`, `--sensor-x`(차 가운데→센서 cm, 기본 7), `--sensor-gap`(센서 간격 cm, 기본 1.5), `--mass --mu --battery --seed --time`. **센서 위치는 실제 차에서 재서 넣을 것.**

결과 : `sim/results/<날짜>_track_<이름>.png` (주행 경로 : 초록 LINE / 주황 CORNER / 파랑 CROSS / 빨강 NOLINE) + `.csv` (20ms 마다 시간·위치·방향·단계·오차). 터미널 : `result`(DONE / LOST / END / NOSTART), `start_s`(5cm 움직인 시각), `merge_s`(합류 시각), `laps_s`(바퀴별 시간), `lost_events`(선 놓친 횟수).

**지금 결과 (기본값, 실측 전)** : 7.0V 에서 킥 없이 출발 최소 PWM 100 → BASE 120 으로 0.56초에 출발. 82.5초에 합류, 2바퀴 142.9초 / 141.4초, 선 놓침 1번, DONE.
직선에서도 시간의 절반 가까이 CORNER(±2) 상태 : 테이프 1.9cm 에 센서 간격 1.5cm 라 ±1(가운데+옆 센서 동시) 구간이 좁아서, 조금만 비뚤어져도 바로 코너 회전으로 넘어가 지그재그로 간다 → 느린 이유. 실제 차의 한 바퀴 시간과 비교해 보기.

## 7. 실제 주행 기록과 비교 (`--replay`)

1. 차에 `team2_bt_pid_debug` 를 올리고, 폰에 `dist/gulno-debug.apk`("굴노 디버그") 설치.
2. 메모에 `KP50 KD30` 처럼 적고 **자동으로** 전환 → 기록이 저절로 시작(0초 = 자동 전환). **수동으로** 돌아오면 저장 창 → CSV 저장.
3. CSV 를 우분투/맥으로 옮겨서, 차에 올린 값과 같은 값으로 :

```bash
python sim/track.py team2 --mass 0.25 --kp 50 --kd 30 --replay gulno_KP50_KD30_20261109_143000.csv
```

결과 `sim/results/<날짜>_replay_<이름>.png` :
- 위 : 맵 위 시뮬 경로, 5초마다 시각 표시.
- 가운데 : **실제 오차(빨강)와 시뮬 오차(초록)** 를 같은 시간축에. 0초 = 자동 전환 후 HOLD(0.5초)가 끝나고 출발한 순간.
- 아래 : 단계 띠 (초록 LINE / 주황 CORNER / 빨강 NOLINE), 위 줄 실제, 아래 줄 시뮬.

보는 법 : 실제 그래프에서 이상한 시각(선 놓침, 흔들림)을 찾고 → 맵에서 시뮬이 그 시각에 어디였는지 보고 → 같은 곳에서 실제 차가 어떻게 했는지 비교. 같은 구간을 실제가 더 오래 걸리면 시뮬의 무게·마찰·배터리를 실제에 맞게 바꾼다.

실제 차의 위치는 기록에 없다. 0.1초 간격 모터 값을 물리에 넣어 경로를 되살려 보면 6초 만에 어긋난다(차는 0.02초마다 조향을 바꿈). 그래서 맵에는 시뮬 경로만 그린다.
