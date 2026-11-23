# 차 코드 해설 : 4가지 버전 비교 (고주파·저주파 · 카메라 있음·없음)

> English : [CODE.en.md](CODE.en.md) · 함께 보기 : [README](../README.md) · [PID 설명](PID.md) · [PWM 계산](PWM.md) · [최종 보고서](REPORT_20261116.md)

차(Arduino UNO) 코드는 **한 뼈대에서 갈라진 4가지**다. 고주파 / 저주파, 카메라 없음 / 있음.

| 버전 | 폴더 | 모터 PWM | 카메라 신호 | 배선 | 어디에 |
|---|---|---|---|---|---|
| **C** | [`team2_bt_pid`](../arduino/team2_bt_pid/team2_bt_pid.ino) | 고주파 976.6Hz (Timer0, 5·6번 핀) | 없음 | 교안 원래 | `main`, 브랜치 |
| **E** (기본) | [`motor_control_low_frequency`](../arduino/motor_control_low_frequency/motor_control_low_frequency.ino) | **저주파 122.5Hz** (Timer1, 9·10번 핀) | 없음 | 4개 맞바꿈 | `main`, 브랜치 |
| **D** | [`team2_bt_cam`](https://github.com/CHOESEUNGJUN/4wd_car_project/blob/extra/esp32cam/arduino/team2_bt_cam/team2_bt_cam.ino) | 고주파 976.6Hz | **있음** (`x v n`) | 교안 원래 | 브랜치 `extra/esp32cam` |
| **D 저주파** | [`motor_control_low_frequency_cam`](https://github.com/CHOESEUNGJUN/4wd_car_project/blob/extra/esp32cam/arduino/motor_control_low_frequency_cam/motor_control_low_frequency_cam.ino) | **저주파 122.5Hz** | **있음** | 4개 맞바꿈 | 브랜치 (2026-11-18 주행 시험에 쓴 것) |

```
        버전 C (PID + 블루투스, 고주파) ── 원래 과제
          │                    │
   + 저주파 PWM (3곳)     + 카메라 신호 (6곳)
          ▼                    ▼
       버전 E               버전 D ── 추가 과제
                               │
                        + 저주파 PWM (같은 3곳)
                               ▼
                         버전 D 저주파
```

---

## 1. 코드 구조 : 어느 부분이 무슨 파트인가 (4버전 공통)

모든 버전이 같은 순서로 되어 있다. 줄 번호를 누르면 그 자리로 간다 (C = 버전 C, E, D, D저 = D 저주파).

| 파트 | 함수·부분 | 하는 일 | 줄 |
|---|---|---|---|
| **설정** | `[설정]` 의 `const` 값 | PID 세기, 속도, 코너, 시간, 안전 거리, 킥. **맞추는 값은 여기만 고친다** | [C 24](../arduino/team2_bt_pid/team2_bt_pid.ino#L24) · [E 27](../arduino/motor_control_low_frequency/motor_control_low_frequency.ino#L27) · [D 37](https://github.com/CHOESEUNGJUN/4wd_car_project/blob/extra/esp32cam/arduino/team2_bt_cam/team2_bt_cam.ino#L37) · [D저 40](https://github.com/CHOESEUNGJUN/4wd_car_project/blob/extra/esp32cam/arduino/motor_control_low_frequency_cam/motor_control_low_frequency_cam.ino#L40) |
| **핀** | `[핀]` | 모터 속도·방향, 라인센서, 초음파, 서보, 블루투스 핀. **고주파 / 저주파가 갈리는 곳 ①** | [C 74](../arduino/team2_bt_pid/team2_bt_pid.ino#L74) · [E 77](../arduino/motor_control_low_frequency/motor_control_low_frequency.ino#L77) · [D 91](https://github.com/CHOESEUNGJUN/4wd_car_project/blob/extra/esp32cam/arduino/team2_bt_cam/team2_bt_cam.ino#L91) · [D저 94](https://github.com/CHOESEUNGJUN/4wd_car_project/blob/extra/esp32cam/arduino/motor_control_low_frequency_cam/motor_control_low_frequency_cam.ino#L94) |
| **시작** | `setup()` | 서보를 정면으로, 핀 모드, 블루투스 9600, 시리얼 115200. **저주파는 여기서 Timer1 설정 ②** | [C 112](../arduino/team2_bt_pid/team2_bt_pid.ino#L112) · [E 116](../arduino/motor_control_low_frequency/motor_control_low_frequency.ino#L116) · [D 129](https://github.com/CHOESEUNGJUN/4wd_car_project/blob/extra/esp32cam/arduino/team2_bt_cam/team2_bt_cam.ino#L129) · [D저 133](https://github.com/CHOESEUNGJUN/4wd_car_project/blob/extra/esp32cam/arduino/motor_control_low_frequency_cam/motor_control_low_frequency_cam.ino#L133) |
| **한 바퀴** | `loop()` | 블루투스 받기 → 장애물 재기 → 전환 대기 → 자동이면 `pidLine()`, 수동이면 `driveManual()` | [C 136](../arduino/team2_bt_pid/team2_bt_pid.ino#L136) · [E 146](../arduino/motor_control_low_frequency/motor_control_low_frequency.ino#L146) · [D 153](https://github.com/CHOESEUNGJUN/4wd_car_project/blob/extra/esp32cam/arduino/team2_bt_cam/team2_bt_cam.ino#L153) · [D저 163](https://github.com/CHOESEUNGJUN/4wd_car_project/blob/extra/esp32cam/arduino/motor_control_low_frequency_cam/motor_control_low_frequency_cam.ino#L163) |
| **라인 → 오차** | `lineError()` | 센서 3개 (왼·가운데·오른) → 오차 −2 \~ +2 | [C 164](../arduino/team2_bt_pid/team2_bt_pid.ino#L164) · [E 174](../arduino/motor_control_low_frequency/motor_control_low_frequency.ino#L174) · [D 183](https://github.com/CHOESEUNGJUN/4wd_car_project/blob/extra/esp32cam/arduino/team2_bt_cam/team2_bt_cam.ino#L183) · [D저 193](https://github.com/CHOESEUNGJUN/4wd_car_project/blob/extra/esp32cam/arduino/motor_control_low_frequency_cam/motor_control_low_frequency_cam.ino#L193) |
| **PID 라인 추종** | `pidLine()` | 20ms 마다 P + I + D → 왼쪽 = 기본 + 보정, 오른쪽 = 기본 − 보정. 코너(±2)·선 놓침(±3)은 바깥·안쪽 바퀴 값으로. **카메라판은 빨간불 정지·노랑 서행이 여기에** | [C 175](../arduino/team2_bt_pid/team2_bt_pid.ino#L175) · [E 185](../arduino/motor_control_low_frequency/motor_control_low_frequency.ino#L185) · [D 199](https://github.com/CHOESEUNGJUN/4wd_car_project/blob/extra/esp32cam/arduino/team2_bt_cam/team2_bt_cam.ino#L199) · [D저 209](https://github.com/CHOESEUNGJUN/4wd_car_project/blob/extra/esp32cam/arduino/motor_control_low_frequency_cam/motor_control_low_frequency_cam.ino#L209) |
| **장애물** | `updateObstacle()` | 초음파로 거리, 10cm 안이면 자동은 정지·수동은 전진 막기 | [C 239](../arduino/team2_bt_pid/team2_bt_pid.ino#L239) · [E 249](../arduino/motor_control_low_frequency/motor_control_low_frequency.ino#L249) · [D 263](https://github.com/CHOESEUNGJUN/4wd_car_project/blob/extra/esp32cam/arduino/team2_bt_cam/team2_bt_cam.ino#L263) · [D저 273](https://github.com/CHOESEUNGJUN/4wd_car_project/blob/extra/esp32cam/arduino/motor_control_low_frequency_cam/motor_control_low_frequency_cam.ino#L273) |
| **블루투스** | `serviceBT()`, `setMode()` | 한 글자 명령 받기 (`g b l r q w s a m h`), 상태 줄 0.2초마다 보내기, 수동/자동 전환. **카메라판은 `x v n` 도 받음** | [C 256](../arduino/team2_bt_pid/team2_bt_pid.ino#L256) · [E 266](../arduino/motor_control_low_frequency/motor_control_low_frequency.ino#L266) · [D 280](https://github.com/CHOESEUNGJUN/4wd_car_project/blob/extra/esp32cam/arduino/team2_bt_cam/team2_bt_cam.ino#L280) · [D저 290](https://github.com/CHOESEUNGJUN/4wd_car_project/blob/extra/esp32cam/arduino/motor_control_low_frequency_cam/motor_control_low_frequency_cam.ino#L290) |
| **수동 조종** | `driveManual()` | 글자 → 왼쪽·오른쪽 속도 (전진, 곡선, 제자리 회전) | [C 320](../arduino/team2_bt_pid/team2_bt_pid.ino#L320) · [E 330](../arduino/motor_control_low_frequency/motor_control_low_frequency.ino#L330) · [D 346](https://github.com/CHOESEUNGJUN/4wd_car_project/blob/extra/esp32cam/arduino/team2_bt_cam/team2_bt_cam.ino#L346) · [D저 356](https://github.com/CHOESEUNGJUN/4wd_car_project/blob/extra/esp32cam/arduino/motor_control_low_frequency_cam/motor_control_low_frequency_cam.ino#L356) |
| **모터 값 다듬기** | `shapePwm()`, `driveManualLR()` | 좌우 보정(`TRIM`), 최소 힘(`MIN_PWM`) 아래면 올리기, 출발·방향 바뀔 때 킥. **킥 값 ③** | [C 340](../arduino/team2_bt_pid/team2_bt_pid.ino#L340) · [E 350](../arduino/motor_control_low_frequency/motor_control_low_frequency.ino#L350) · [D 366](https://github.com/CHOESEUNGJUN/4wd_car_project/blob/extra/esp32cam/arduino/team2_bt_cam/team2_bt_cam.ino#L366) · [D저 376](https://github.com/CHOESEUNGJUN/4wd_car_project/blob/extra/esp32cam/arduino/motor_control_low_frequency_cam/motor_control_low_frequency_cam.ino#L376) |
| **모터 출력** | `driveLR()`, `drive()`, `brakeCar()` | 방향 핀 HIGH/LOW + `analogWrite` (= PWM 가로 선 높이) | [C 367](../arduino/team2_bt_pid/team2_bt_pid.ino#L367) · [E 377](../arduino/motor_control_low_frequency/motor_control_low_frequency.ino#L377) · [D 393](https://github.com/CHOESEUNGJUN/4wd_car_project/blob/extra/esp32cam/arduino/team2_bt_cam/team2_bt_cam.ino#L393) · [D저 403](https://github.com/CHOESEUNGJUN/4wd_car_project/blob/extra/esp32cam/arduino/motor_control_low_frequency_cam/motor_control_low_frequency_cam.ino#L403) |

---

## 2. 고주파 → 저주파 : 바뀐 곳 3군데 (C → E, D → D 저주파)

로직(PID, 블루투스, 안전장치)은 **한 줄도 안 바뀐다.** 모터 신호의 주파수를 바꾸려고 다음 세 곳만 다르다. 원리는 [`PWM.md`](PWM.md).

**① 핀 (파트 : 핀)** — 속도 핀을 Timer0(5·6) 에서 Timer1(9·10) 으로, 방향 핀은 비게 된 5·6 으로. 배선 4개 맞바꿈 (5↔9, 6↔10).

```cpp
// 고주파 (C, D)
const int RightMotor_E_pin = 5, LeftMotor_E_pin = 6;     // 모터 속도(PWM)
const int RightMotor_1_pin = 8, RightMotor_2_pin = 9;    // 오른쪽 모터 방향
const int LeftMotor_3_pin = 10, LeftMotor_4_pin = 11;    // 왼쪽 모터 방향
// 저주파 (E, D 저주파)
const int RightMotor_E_pin = 9, LeftMotor_E_pin = 10;    // 모터 속도(PWM, 122.5Hz)
const int RightMotor_1_pin = 8, RightMotor_2_pin = 5;
const int LeftMotor_3_pin = 6, LeftMotor_4_pin = 11;
```

**② Timer1 설정 (파트 : 시작 `setup()`)** — 저주파판에만 있는 세 줄.

```cpp
TIMSK1 = 0;              // 서보 라이브러리가 켜 둔 Timer1 인터럽트 끄기
TCCR1A = _BV(WGM10);     // 8비트 위상 보정 PWM (삼각파, TOP 255)
TCCR1B = _BV(CS12);      // 분주 256 → 16,000,000 ÷ 256 ÷ 510 = 122.5Hz
```

**③ 출발 킥 (파트 : 설정 → `shapePwm()` 에서 씀)**

```cpp
// 고주파 : 전류가 끝까지 못 차서 세게, 길게 밀어야 출발
const int KICK_PWM = 255;  const unsigned long KICK_MS = 120;
// 저주파 : 작은 값에서도 힘이 차서 약하게, 짧게
const int KICK_PWM = 130;  const unsigned long KICK_MS = 60;   // 버전 E · D 저주파 (2026-11-23)
```

---

## 3. 카메라 신호 추가 : 바뀐 곳 6군데 (C → D)

라인은 그대로 라인센서 + PID 가 따라가고, ESP32-CAM 이 블루투스로 보낸 글자에 따라 **멈추거나 속도만** 바꾼다.

| # | 파트 | 바뀐 것 |
|---|---|---|
| 1 | 설정 | `SLOW_BASE = 100` (노란불 서행 속도), `SIG_HOLD_MS = 1000` (신호가 1초 안 오면 보통으로) |
| 2 | 전역 변수 | `sigCmd` (마지막 신호 글자), `sigAt` (받은 시각) |
| 3 | 블루투스 `serviceBT()` | `x v n` 도 받고, 모드는 그대로 두고 신호만 기억 |
| 4 | 새 함수 | `signalNow()` (지금 신호, 빨강은 시간으로 안 풀림), `baseSpeed()` (노랑이면 `SLOW_BASE`) |
| 5 | PID `pidLine()` | 빨간불이면 정지(`RED`), 기본 속도를 `BASE` 대신 `baseSpeed()` 로 |
| 6 | 모드 `setMode()` | 사람이 모드를 고르면 신호 초기화 |

```cpp
// 3. serviceBT() : 신호 글자는 기억만 (주행 명령이 아님)
if (strchr("xvn", c)) { sigCmd = c; sigAt = millis(); continue; }

// 4. 지금 신호와 기본 속도
char signalNow() { return sigCmd == 'x' || millis() - sigAt < SIG_HOLD_MS ? sigCmd : 'n'; }
int baseSpeed() { return signalNow() == 'v' ? SLOW_BASE : BASE; }

// 5. pidLine() : 빨간불이면 그 자리에 정지, 아니면 신호에 맞는 속도로 PID
if (signalNow() == 'x') { stopCar(); stage = "RED"; resetPid(); lostSince = 0; crossSince = 0; return; }
int left = baseSpeed() + (int)u, right = baseSpeed() - (int)u;
```

정지 표지판은 ESP32 가 `x` 를 3초 보낸 뒤 `n` 으로 바꿔서 처리한다 (차 코드는 빨간불과 같음).

---

## 4. 설정값 비교 (파트 : 설정)

| 이름 | 뜻 | C · D (고주파) | **E · D 저주파 (저주파, 2026-11-23 맞춤)** |
|---|---|---|---|
| `KP` / `KI` / `KD` | PID 세기 | 51 / 0.2 / 30 | **30 / 0 / 25** |
| `BASE` | 직선 기본 속도 | 120 | **90** |
| `SLOW_BASE` | 노란불 서행 (카메라판만) | (D) 100 | **(D 저주파) 70** |
| `CORNER_OUT` / `CORNER_IN` | 코너 바깥 / 안쪽 (− = 뒤로) | 180 / −90 | **150 / −100** |
| `MIN_PWM` | 0 이 아니면 최소 힘 | 90 | **60** |
| `KICK_PWM` / `KICK_MS` | 출발 킥 | 255 / 0.12초 | **130 / 0.06초** |
| `MAN_FWD` | 수동 전진·후진 | 100 | **80** |
| `MAN_CURVE_OUT` / `IN` | 수동 곡선 | 100 / 80 | **110 / 60** |
| `MAN_SPIN` | 수동 제자리 회전 | 130 | **140** |

(2026-11-18 카메라 주행 시험은 D 저주파에 고주파 값(BASE 120, KP 51 …)을 쓴 채로 통과했다. 그 뒤 트랙에서 저주파 값을 맞춰 E 와 D 저주파에 같이 넣음.)

**왜 저주파판 값이 다른가** : 저주파는 같은 `analogWrite` 값에서 코일에 흐르는 전류가 1.5\~2.6배라 ([`PWM.md`](PWM.md)), 고주파에서 맞춘 값을 그대로 쓰면 차가 더 빠르고 보정이 세서 직선에서 흔들렸다. 그래서
- 직선 속도(`BASE`)와 PID 세기(`KP`, `KD`)를 낮추고, `KI` 는 0 으로
- 저주파는 60 에서도 힘이 차므로 바닥(`MIN_PWM`)을 90 → 60, 킥도 130 으로
- 4륜이 미끄러지며 돌아 코너는 힘이 많이 들어서 코너·회전 값은 올림

---

## 5. 어느 것을 올리나

| 차 상태 | 올릴 폴더 |
|---|---|
| 카메라 없음, 배선 4개 맞바꿈 (기본) | `motor_control_low_frequency` (E) |
| 카메라 없음, 교안 원래 배선 | `team2_bt_pid` (C) |
| 카메라 있음, 배선 맞바꿈 | `motor_control_low_frequency_cam` (D 저주파) |
| 카메라 있음, 교안 원래 배선 | `team2_bt_cam` (D) |

**배선과 코드는 짝이 맞아야 한다** : 저주파 코드를 원래 배선에 올리면 속도 핀과 방향 핀이 뒤바뀌어 바퀴가 이상하게 돈다. 배선은 [README 2장](../README.md#2-배선).
