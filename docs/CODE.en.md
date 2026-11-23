# Car code walkthrough: the four versions compared (high / low frequency · with / without camera)

> 한국어 : [CODE.md](CODE.md) · See also: [README](../README.en.md) · [PWM calculation](PWM.en.md) · [final report](REPORT_20261116.en.md)

The car (Arduino UNO) code is **four versions grown from one skeleton**: high / low frequency, without / with the camera.

| Version | Folder | Motor PWM | Camera signals | Wiring | Where |
|---|---|---|---|---|---|
| **C** | [`team2_bt_pid`](../arduino/team2_bt_pid/team2_bt_pid.ino) | high 976.6 Hz (Timer0, pins 5·6) | no | lecture deck original | `main`, branch |
| **E** (default) | [`motor_control_low_frequency`](../arduino/motor_control_low_frequency/motor_control_low_frequency.ino) | **low 122.5 Hz** (Timer1, pins 9·10) | no | 4 wires swapped | `main`, branch |
| **D** | [`team2_bt_cam`](https://github.com/CHOESEUNGJUN/4wd_car_project/blob/extra/esp32cam/arduino/team2_bt_cam/team2_bt_cam.ino) | high 976.6 Hz | **yes** (`x v n`) | lecture deck original | branch `extra/esp32cam` |
| **D low** | [`motor_control_low_frequency_cam`](https://github.com/CHOESEUNGJUN/4wd_car_project/blob/extra/esp32cam/arduino/motor_control_low_frequency_cam/motor_control_low_frequency_cam.ino) | **low 122.5 Hz** | **yes** | 4 wires swapped | branch (used in the 2026-11-18 driving test) |

```
        Version C (PID + Bluetooth, high frequency) ── original assignment
          │                         │
   + low-frequency PWM (3 places)   + camera signals (6 places)
          ▼                         ▼
       Version E                 Version D ── extra assignment
                                    │
                             + low-frequency PWM (same 3 places)
                                    ▼
                              Version D low
```

---

## 1. Code structure: which part is which (same in all four)

All versions follow the same order. Click a line number to jump there (C, E, D, D-low).

| Part | Function / block | What it does | Line |
|---|---|---|---|
| **Settings** | `const` values under `[설정]` | PID gains, speeds, corners, times, safety distance, kick. **Tuning only touches this part** | [C 24](../arduino/team2_bt_pid/team2_bt_pid.ino#L24) · [E 27](../arduino/motor_control_low_frequency/motor_control_low_frequency.ino#L27) · [D 37](https://github.com/CHOESEUNGJUN/4wd_car_project/blob/extra/esp32cam/arduino/team2_bt_cam/team2_bt_cam.ino#L37) · [D-low 40](https://github.com/CHOESEUNGJUN/4wd_car_project/blob/extra/esp32cam/arduino/motor_control_low_frequency_cam/motor_control_low_frequency_cam.ino#L40) |
| **Pins** | `[핀]` | motor speed and direction, line sensors, ultrasonic, servo, Bluetooth pins. **High vs low frequency difference ①** | [C 74](../arduino/team2_bt_pid/team2_bt_pid.ino#L74) · [E 77](../arduino/motor_control_low_frequency/motor_control_low_frequency.ino#L77) · [D 91](https://github.com/CHOESEUNGJUN/4wd_car_project/blob/extra/esp32cam/arduino/team2_bt_cam/team2_bt_cam.ino#L91) · [D-low 94](https://github.com/CHOESEUNGJUN/4wd_car_project/blob/extra/esp32cam/arduino/motor_control_low_frequency_cam/motor_control_low_frequency_cam.ino#L94) |
| **Start** | `setup()` | servo facing forward, pin modes, Bluetooth 9600, Serial 115200. **Low frequency sets Timer1 here ②** | [C 112](../arduino/team2_bt_pid/team2_bt_pid.ino#L112) · [E 116](../arduino/motor_control_low_frequency/motor_control_low_frequency.ino#L116) · [D 129](https://github.com/CHOESEUNGJUN/4wd_car_project/blob/extra/esp32cam/arduino/team2_bt_cam/team2_bt_cam.ino#L129) · [D-low 133](https://github.com/CHOESEUNGJUN/4wd_car_project/blob/extra/esp32cam/arduino/motor_control_low_frequency_cam/motor_control_low_frequency_cam.ino#L133) |
| **Main loop** | `loop()` | read Bluetooth → measure obstacle → switch hold → `pidLine()` in auto, `driveManual()` in manual | [C 136](../arduino/team2_bt_pid/team2_bt_pid.ino#L136) · [E 146](../arduino/motor_control_low_frequency/motor_control_low_frequency.ino#L146) · [D 153](https://github.com/CHOESEUNGJUN/4wd_car_project/blob/extra/esp32cam/arduino/team2_bt_cam/team2_bt_cam.ino#L153) · [D-low 163](https://github.com/CHOESEUNGJUN/4wd_car_project/blob/extra/esp32cam/arduino/motor_control_low_frequency_cam/motor_control_low_frequency_cam.ino#L163) |
| **Line → error** | `lineError()` | 3 sensors (left, centre, right) → error −2 to +2 | [C 164](../arduino/team2_bt_pid/team2_bt_pid.ino#L164) · [E 174](../arduino/motor_control_low_frequency/motor_control_low_frequency.ino#L174) · [D 183](https://github.com/CHOESEUNGJUN/4wd_car_project/blob/extra/esp32cam/arduino/team2_bt_cam/team2_bt_cam.ino#L183) · [D-low 193](https://github.com/CHOESEUNGJUN/4wd_car_project/blob/extra/esp32cam/arduino/motor_control_low_frequency_cam/motor_control_low_frequency_cam.ino#L193) |
| **PID line following** | `pidLine()` | every 20 ms P + I + D → left = base + u, right = base − u. Corners (±2) and lost line (±3) use outer / inner wheel values. **The camera versions stop at red and slow on yellow here** | [C 175](../arduino/team2_bt_pid/team2_bt_pid.ino#L175) · [E 185](../arduino/motor_control_low_frequency/motor_control_low_frequency.ino#L185) · [D 199](https://github.com/CHOESEUNGJUN/4wd_car_project/blob/extra/esp32cam/arduino/team2_bt_cam/team2_bt_cam.ino#L199) · [D-low 209](https://github.com/CHOESEUNGJUN/4wd_car_project/blob/extra/esp32cam/arduino/motor_control_low_frequency_cam/motor_control_low_frequency_cam.ino#L209) |
| **Obstacle** | `updateObstacle()` | ultrasonic distance; within 10 cm auto stops and manual blocks forward | [C 239](../arduino/team2_bt_pid/team2_bt_pid.ino#L239) · [E 249](../arduino/motor_control_low_frequency/motor_control_low_frequency.ino#L249) · [D 263](https://github.com/CHOESEUNGJUN/4wd_car_project/blob/extra/esp32cam/arduino/team2_bt_cam/team2_bt_cam.ino#L263) · [D-low 273](https://github.com/CHOESEUNGJUN/4wd_car_project/blob/extra/esp32cam/arduino/motor_control_low_frequency_cam/motor_control_low_frequency_cam.ino#L273) |
| **Bluetooth** | `serviceBT()`, `setMode()` | one-letter commands (`g b l r q w s a m h`), status line every 0.2 s, manual / auto switch. **Camera versions also accept `x v n`** | [C 256](../arduino/team2_bt_pid/team2_bt_pid.ino#L256) · [E 266](../arduino/motor_control_low_frequency/motor_control_low_frequency.ino#L266) · [D 280](https://github.com/CHOESEUNGJUN/4wd_car_project/blob/extra/esp32cam/arduino/team2_bt_cam/team2_bt_cam.ino#L280) · [D-low 290](https://github.com/CHOESEUNGJUN/4wd_car_project/blob/extra/esp32cam/arduino/motor_control_low_frequency_cam/motor_control_low_frequency_cam.ino#L290) |
| **Manual drive** | `driveManual()` | letter → left and right speed (forward, curve, spin) | [C 320](../arduino/team2_bt_pid/team2_bt_pid.ino#L320) · [E 330](../arduino/motor_control_low_frequency/motor_control_low_frequency.ino#L330) · [D 346](https://github.com/CHOESEUNGJUN/4wd_car_project/blob/extra/esp32cam/arduino/team2_bt_cam/team2_bt_cam.ino#L346) · [D-low 356](https://github.com/CHOESEUNGJUN/4wd_car_project/blob/extra/esp32cam/arduino/motor_control_low_frequency_cam/motor_control_low_frequency_cam.ino#L356) |
| **Shaping motor values** | `shapePwm()`, `driveManualLR()` | left/right trim (`TRIM`), raise to the minimum (`MIN_PWM`), kick on start / direction change. **Kick values ③** | [C 340](../arduino/team2_bt_pid/team2_bt_pid.ino#L340) · [E 350](../arduino/motor_control_low_frequency/motor_control_low_frequency.ino#L350) · [D 366](https://github.com/CHOESEUNGJUN/4wd_car_project/blob/extra/esp32cam/arduino/team2_bt_cam/team2_bt_cam.ino#L366) · [D-low 376](https://github.com/CHOESEUNGJUN/4wd_car_project/blob/extra/esp32cam/arduino/motor_control_low_frequency_cam/motor_control_low_frequency_cam.ino#L376) |
| **Motor output** | `driveLR()`, `drive()`, `brakeCar()` | direction pins HIGH/LOW + `analogWrite` (= height of the PWM line) | [C 367](../arduino/team2_bt_pid/team2_bt_pid.ino#L367) · [E 377](../arduino/motor_control_low_frequency/motor_control_low_frequency.ino#L377) · [D 393](https://github.com/CHOESEUNGJUN/4wd_car_project/blob/extra/esp32cam/arduino/team2_bt_cam/team2_bt_cam.ino#L393) · [D-low 403](https://github.com/CHOESEUNGJUN/4wd_car_project/blob/extra/esp32cam/arduino/motor_control_low_frequency_cam/motor_control_low_frequency_cam.ino#L403) |

---

## 2. High → low frequency: the 3 places that change (C → E, D → D low)

The logic (PID, Bluetooth, safety) **does not change at all**. Only these three places differ, to change the motor signal frequency. Theory: [`PWM.en.md`](PWM.en.md).

**① Pins (part: pins)** — speed pins move from Timer0 (5·6) to Timer1 (9·10); direction pins move to the freed 5·6. Swap 4 wires (5↔9, 6↔10).

```cpp
// High frequency (C, D)
const int RightMotor_E_pin = 5, LeftMotor_E_pin = 6;     // motor speed (PWM)
const int RightMotor_1_pin = 8, RightMotor_2_pin = 9;    // right motor direction
const int LeftMotor_3_pin = 10, LeftMotor_4_pin = 11;    // left motor direction
// Low frequency (E, D low)
const int RightMotor_E_pin = 9, LeftMotor_E_pin = 10;    // motor speed (PWM, 122.5 Hz)
const int RightMotor_1_pin = 8, RightMotor_2_pin = 5;
const int LeftMotor_3_pin = 6, LeftMotor_4_pin = 11;
```

**② Timer1 setup (part: start `setup()`)** — three lines only in the low-frequency versions.

```cpp
TIMSK1 = 0;              // turn off the Timer1 interrupt the Servo library left on
TCCR1A = _BV(WGM10);     // 8-bit phase-correct PWM (triangle, TOP 255)
TCCR1B = _BV(CS12);      // prescaler 256 → 16,000,000 ÷ 256 ÷ 510 = 122.5 Hz
```

**③ Start kick (part: settings → used in `shapePwm()`)**

```cpp
// High frequency: the current never fills up, so push hard and long to start
const int KICK_PWM = 255;  const unsigned long KICK_MS = 120;
// Low frequency: force fills even at small values, so a softer, shorter kick
const int KICK_PWM = 130;  const unsigned long KICK_MS = 60;   // versions E and D low (2026-11-23)
```

---

## 3. Adding camera signals: the 6 places that change (C → D)

Line following stays line sensors + PID; the letters the ESP32-CAM sends over Bluetooth only **stop the car or change its speed**.

| # | Part | What changed |
|---|---|---|
| 1 | Settings | `SLOW_BASE = 100` (yellow-light slow speed), `SIG_HOLD_MS = 1000` (back to normal if no signal for 1 s) |
| 2 | Globals | `sigCmd` (last signal letter), `sigAt` (when it came) |
| 3 | Bluetooth `serviceBT()` | also accepts `x v n`; remembers the signal, mode unchanged |
| 4 | New functions | `signalNow()` (current signal; red never times out), `baseSpeed()` (`SLOW_BASE` on yellow) |
| 5 | PID `pidLine()` | stop on red (`RED`); base speed is `baseSpeed()` instead of `BASE` |
| 6 | Mode `setMode()` | clears the signal when a person picks a mode |

```cpp
// 3. serviceBT(): signal letters are only remembered (not drive commands)
if (strchr("xvn", c)) { sigCmd = c; sigAt = millis(); continue; }

// 4. current signal and base speed
char signalNow() { return sigCmd == 'x' || millis() - sigAt < SIG_HOLD_MS ? sigCmd : 'n'; }
int baseSpeed() { return signalNow() == 'v' ? SLOW_BASE : BASE; }

// 5. pidLine(): stop in place on red, otherwise PID at the speed the signal allows
if (signalNow() == 'x') { stopCar(); stage = "RED"; resetPid(); lostSince = 0; crossSince = 0; return; }
int left = baseSpeed() + (int)u, right = baseSpeed() - (int)u;
```

A stop sign is handled by the ESP32 sending `x` for 3 s and then `n` (the car treats it like a red light).

---

## 4. Settings compared (part: settings)

| Name | Meaning | C · D (high freq.) | **E · D low (low freq., tuned 2026-11-23)** |
|---|---|---|---|
| `KP` / `KI` / `KD` | PID gains | 51 / 0.2 / 30 | **30 / 0 / 25** |
| `BASE` | straight base speed | 120 | **90** |
| `SLOW_BASE` | yellow-light speed (camera versions) | (D) 100 | **(D low) 70** |
| `CORNER_OUT` / `CORNER_IN` | corner outer / inner (− = backwards) | 180 / −90 | **150 / −100** |
| `MIN_PWM` | minimum power when non-zero | 90 | **60** |
| `KICK_PWM` / `KICK_MS` | start kick | 255 / 0.12 s | **130 / 0.06 s** |
| `MAN_FWD` | manual forward / back | 100 | **80** |
| `MAN_CURVE_OUT` / `IN` | manual curve | 100 / 80 | **110 / 60** |
| `MAN_SPIN` | manual spin | 130 | **140** |

(The 2026-11-18 camera driving test passed with D low still on the high-frequency values (BASE 120, KP 51 …). The low-frequency values were then tuned on the track and put into both E and D low.)

**Why the low-frequency values differ**: at low frequency the same `analogWrite` value drives 1.5–2.6× the coil current ([`PWM.en.md`](PWM.en.md)), so with the values tuned for high frequency the car was faster and corrected harder, and weaved on straights. So:
- lower straight speed (`BASE`) and PID gains (`KP`, `KD`), `KI` to 0
- low frequency has full force even at 60, so the floor (`MIN_PWM`) goes 90 → 60 and the kick to 130
- a 4WD car skids when it turns and needs a lot of force there, so the corner and turning values go up

---

## 5. Which one to upload

| Car | Folder |
|---|---|
| No camera, 4 wires swapped (default) | `motor_control_low_frequency` (E) |
| No camera, original lecture-deck wiring | `team2_bt_pid` (C) |
| Camera, wires swapped | `motor_control_low_frequency_cam` (D low) |
| Camera, original wiring | `team2_bt_cam` (D) |

**The wiring and the code must match**: uploading low-frequency code to the original wiring swaps the speed and direction pins and the wheels behave strangely. Wiring: [README chapter 2](../README.en.md#2-wiring).
