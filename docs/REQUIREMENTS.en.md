# Requirements: explanation · code · photos

> 한국어 : [REQUIREMENTS.md](REQUIREMENTS.md) · See also : [final report](REPORT_20261116.en.md) · [code walkthrough](CODE.en.md) · [PWM](PWM.en.md)

Line numbers point to the default car code, **version E** ([`motor_control_low_frequency.ino`](../arduino/motor_control_low_frequency/motor_control_low_frequency.ino)). Versions C and D share the structure ([CODE.en.md](CODE.en.md)).

| # | Requirement | Implemented in | Verified |
|---|---|---|---|
| 1 | Drive forward/back and steer left/right from a mobile app | 3 apps + `driveManual()` | ✅ track |
| 2 | Switch between auto and manual | `a` / `m` + `setMode()` | ✅ |
| 3 | Auto mode follows the line (PID for stability) | `pidLine()` | ✅ passed (C·E 11-16, D 11-18) |
| 4 | Parse Bluetooth commands and act | `serviceBT()` | ✅ |
| 5 | Keep the mode state and branch the control logic | `enum Mode` + `loop()` | ✅ |
| 6 | React within 300 ms of a Bluetooth command | non-blocking `loop()` | ✅ ~15 ms by design; log figures below |
| Extra 1 | Detect obstacles with the ultrasonic sensor | `updateObstacle()` | ✅ |
| Extra 2 | ESP32-CAM controls the car and reads traffic lights and signs | ESP32 firmware + `x v n` on the car | ✅ track (11-18) |
| Extra 3 | Optimise the DC motor PWM frequency | Timer1 122.5 Hz + low-frequency values | ✅ track (11-16, 11-23) |

---

## 1. Drive and steer from a mobile app

**How**: a button in the phone app (Android HC-06, iPhone HM-10, web) sends a one-letter command over Bluetooth, and the car sets the left and right wheel speeds for that letter. While held, the app resends it every 0.2 s; on release it sends `s` (stop) twice.

| Button | ▲ | ▼ | ◀ | ▶ | ↺ | ↻ | ■ |
|---|---|---|---|---|---|---|---|
| Letter | `g` forward | `b` back | `l` left | `r` right | `q` spin left | `w` spin right | `s` stop |

<p align="center"><img src="img/app_web_control.png" width="260" alt="Web control app"><br><sub>Web control app (apps/web/index.html): connect, manual/auto switch, direction buttons</sub></p>

**Code** — car [`driveManual()`](../arduino/motor_control_low_frequency/motor_control_low_frequency.ino#L330-L341) :
```cpp
case 'g': driveManualLR( MAN_FWD,  MAN_FWD); break;              // forward
case 'b': driveManualLR(-MAN_FWD, -MAN_FWD); break;              // back
case 'l': driveManualLR( MAN_CURVE_IN,  MAN_CURVE_OUT); break;   // left (curve)
case 'r': driveManualLR( MAN_CURVE_OUT, MAN_CURVE_IN); break;    // right (curve)
case 'q': driveManualLR(-MAN_SPIN,  MAN_SPIN); break;            // spin left
case 'w': driveManualLR( MAN_SPIN, -MAN_SPIN); break;            // spin right
```
App [`apps/web/index.html`](../apps/web/index.html#L170-L176) :
```js
setInterval(() => { if (held && held !== 's') write?.(held); }, 200);   // resend every 0.2 s while held
function press(c) { held = c; write?.(c); render(); }
function release() { if (held === null) return; held = null; write?.('s'); write?.('s'); render(); }
```

---

## 2. Auto / manual switch

**How**: "Auto" in the app → confirm dialog → `a`; "Manual" → `m`. Going to auto holds the car stopped for 0.5 s first (safety delay); going to manual is immediate. Pressing a direction button during auto counts as a person taking over and switches to manual at once.

**Code** [`setMode()`](../arduino/motor_control_low_frequency/motor_control_low_frequency.ino#L319-L328) :
```cpp
void setMode(Mode m) {
  mode = m;
  cmd = 's';
  stopCar();
  holdUntil = (m == AUTO) ? millis() + SWITCH_HOLD_MS : 0;   // 0.5 s hold only when going to auto
  resetPid();
  ...
}
```

---

## 3. Auto line following (PID)

**How**: three line sensors (black = 1) give the error: `010` = 0, `011`/`110` = ±1, `001`/`100` = ±2 (corner), `000` = lost. Within ±1, **PID** computes a correction u every 20 ms: left = BASE + u, right = BASE − u. Since the sensors are only 0/1, D is stretched over ~0.1 s (`D_DECAY`) and I is cleared when the error crosses over. Corners (±2) skip PID and turn with outer 150 / inner −100.

<p align="center"><img src="img/car_front.jpg" width="32%" alt="Front: 3 line sensors, ultrasonic"> <img src="https://raw.githubusercontent.com/CHOESEUNGJUN/4wd_car_project/extra/esp32cam/docs/img/gallery/drive_0011_full.gif" width="58%" alt="Line following"><br><sub>Left: front of the car (3 line sensors underneath) · right: auto line following (recorded by the car)</sub></p>

**Code** [`pidLine()`](../arduino/motor_control_low_frequency/motor_control_low_frequency.ino#L185-L245) :
```cpp
if ((e > 0 && integ < 0) || (e < 0 && integ > 0)) integ = 0;   // clear I when the error crosses
integ = constrain(integ + e * dt, -I_LIMIT, I_LIMIT);           // I
dTerm = dTerm * D_DECAY + KD * (e - lastErr);                   // D: lasts ~0.1 s
float u = KP * e + KI * integ + dTerm;                          // P + I + D
int left = BASE + (int)u, right = BASE - (int)u;
driveManualLR(constrain(left, 0, 255), constrain(right, 0, 255));
```
**Stability**: KP 51 → 30, KD 25, BASE 90 (tuned for low frequency, 2026-11-23) reduced weaving on straights. More in README chapter 4 and [`PID.md`](PID.md) (Korean).

---

## 4. Parsing Bluetooth commands

**How**: characters from the HC-06 are read one at a time over D3/D4 SoftwareSerial (9600). **Anything not in the command table (newlines etc.) is dropped**; `h` (heartbeat) only refreshes the link time; `a`/`m` switch mode; the rest are stored as the drive command.

**Code** [`serviceBT()`](../arduino/motor_control_low_frequency/motor_control_low_frequency.ino#L266-L282) :
```cpp
while (BT.available()) {
  char c = BT.read();
  if (c == 0 || !strchr("gblrsqwamh", c)) continue;   // drop anything not in the table
  lastLink = millis();
  if (c == 'h') continue;                              // heartbeat
  lastRx = millis();
  if (c == 'a') setMode(AUTO);
  else if (c == 'm') setMode(MANUAL);
  else {
    if (mode == AUTO) setMode(MANUAL);                 // manual input during auto = takeover
    cmd = c;
  }
}
```
Car → app: one status line every 0.2 s (`ag,23,010,LINE` = mode, action, distance, sensors, stage). Full letter protocol: [`PROTOCOL.md`](../PROTOCOL.md).

---

## 5. Mode state and branching

**How**: the mode lives in one variable, `enum Mode {{ MANUAL, AUTO }}`, changed only by `setMode()`. Each pass `loop()` checks it and **branches to `pidLine()` in auto or `driveManual()` in manual**. The switch hold and obstacles are handled before either.

**Code** [`loop()`](../arduino/motor_control_low_frequency/motor_control_low_frequency.ino#L146-L168) :
```cpp
void loop() {
  serviceBT();                                           // read commands
  if (millis() - lastPing >= PING_MS) updateObstacle();  // obstacle
  if (millis() < holdUntil) { stopCar(); stage = "HOLD"; return; }   // switch hold
  if (mode == AUTO) {                                    // auto
    if (obstacleStopped) { stopCar(); ... return; }
    pidLine();
    return;
  }
  // manual
  if (millis() - lastRx > LINK_MS) cmd = 's';            // stop if commands stop
  bool blocked = obstacleStopped && (cmd == 'g' || cmd == 'l' || cmd == 'r');
  driveManual(blocked ? 's' : cmd);
}
```

---

## 6. Response within 300 ms

**How**: `loop()` has no `delay()`; every pass reads Bluetooth first and updates the wheels right away. The slowest step in a pass is the ultrasonic reading (max 12 ms, once every 60 ms), and one character takes ~1 ms at 9600 bps. So **the wheels change within ~15 ms of a command arriving** (1/20 of the 300 ms limit).

| Timing | Value | Where |
|---|---|---|
| one character (9600 bps) | ~1 ms | HC-06 |
| ultrasonic max wait | 12ms (every 60 ms) | [`pulseIn(echoPin, HIGH, 12000)`](../arduino/motor_control_low_frequency/motor_control_low_frequency.ino#L253) |
| PID period | 20ms | `PID_MS` |
| command → wheels | **~15 ms or less** | sum of the above |
| signal → car reaction (drive log, incl. ESP confirmation, Bluetooth, 0.2 s status interval) | green 0.11–0.33 s, sign 0.10–0.22 s, red 0.21–0.54 s | report ch. 7 |

The drive-log figures are measured from the status line the car sends every 0.2 s, so they lag by up to that interval, and they include the ESP32 confirmation time (2–3 frames) and Wi-Fi/Bluetooth antenna sharing. The car-side handling itself takes a few ms, as above.

---

## Extra 1. Obstacle detection (ultrasonic)

**How**: the ultrasonic sensor (HC-SR04, TRIG 13 · ECHO 12) fires every 60 ms. **Within 10 cm**, auto stops and manual blocks only forward commands (`g l r`; back and spin still work). After 5 clear readings in a row it drives again.

**Code** [`updateObstacle()`](../arduino/motor_control_low_frequency/motor_control_low_frequency.ino#L249-L260) :
```cpp
long duration = pulseIn(echoPin, HIGH, 12000);
int mm = duration * 340L / 2000;                          // round-trip time → distance (mm)
frontDist = (duration == 0 || mm < MIN_VALID_MM) ? 9999 : mm;
if (frontDist < AVOID_MM) { obstacleStopped = true; clearCount = 0; }   // 10cm
else if (obstacleStopped && ++clearCount >= CLEAR_COUNT) obstacleStopped = false;
```
**Verified**: in log_0011 the car stopped at an obstacle 8 cm away and drove on once it was removed. The sensor sits on the servo at the front (photo in section 3, left).

---

## Extra 2. ESP32-CAM: traffic lights, signs and car control

**How**: the ESP32-CAM grabs a 160×120 colour frame every 45 ms, **looks up each pixel once in an HSV class table (4096 colours)** and counts red, green, yellow and white. The shape of the red blob (fill < 80 %, centre white ≥ 4 %) separates a stop sign from a red light. After 2–3 matching frames it confirms, then, **connected to the HC-06 over Bluetooth**, sends `x` (stop), `v` (slow) or `n` (normal) to the car every 0.2 s. The car keeps following the line and only stops or changes speed. The web page (Wi-Fi) also drives the car and shows its status.

<p align="center"><img src="https://raw.githubusercontent.com/CHOESEUNGJUN/4wd_car_project/extra/esp32cam/docs/img/gallery/esp_strip.png" width="90%" alt="What the ESP32 saw"></p>

![HSV decision flow](img/hsv_pipeline.en.png)

![Red light vs stop sign](img/sign_vs_red.en.png)

**Code** — ESP32 firmware [`esp32cam_line.ino`](https://github.com/CHOESEUNGJUN/4wd_car_project/blob/extra/esp32cam/esp32/esp32cam_line/esp32cam_line.ino#L854-L858) :
```cpp
// stop sign (4) vs red light (1) by the red blob shape
if (red > best) { now = fill < SIGN_FILL_MAX && white >= SIGN_WHITE_MIN ? 4 : 1; best = red; }
if (grn > best) { now = 2; best = grn; }
if (yel > best) now = 3;
if (same >= (cand == 1 ? 2 : 3)) sigState = cand;   // confirm: red after 2 frames, others after 3
```
[Sending](https://github.com/CHOESEUNGJUN/4wd_car_project/blob/extra/esp32cam/esp32/esp32cam_line/esp32cam_line.ino#L907-L912) :
```cpp
char want = sigState == 1 || stopLeft() ? 'x' : sigState == 3 ? 'v' : 'n';
if (sigMode && btOn() && (want != sentSig || millis() - lastSig >= 200)) { ... btSend(want); }
```
Car (camera low-frequency sketch [`motor_control_low_frequency_cam.ino`](https://github.com/CHOESEUNGJUN/4wd_car_project/blob/extra/esp32cam/arduino/motor_control_low_frequency_cam/motor_control_low_frequency_cam.ino)) :
```cpp
if (strchr("xvn", c)) { sigCmd = c; sigAt = millis(); continue; }            // remember the signal only
if (signalNow() == 'x') { stopCar(); stage = "RED"; ... return; }           // stop on red
int left = baseSpeed() + (int)u, right = baseSpeed() - (int)u;              // slow on yellow
```
**Verified** (track, 2026-11-18, SD logs): stopped at red 7 times, started on green 4 times, 3 s stop at a stop sign 5 times. More in report chapters 2–7; car-side changes in [CODE.en.md ch. 3](CODE.en.md).

---

## Extra 3. Optimising the DC motor PWM frequency

**How**: the UNO default PWM (pins 5·6, Timer0, 976.6 Hz) switched off before the coil current filled up, so low-speed force was weak (70 % peak at 90; about 130 needed to move). **Timer1 was set to triangle, prescaler 256 → 122.5 Hz** and the speed pins moved to 9·10. The same value now drives 1.5–2.6× the current, the car starts at small values and the kick dropped from 255 to 130. The PID and speed values were then retuned for low frequency.

![Coil current at the same analogWrite, 976.6 Hz vs 122.5 Hz (MATLAB · Simulink)](img/pwm_coil_current.en.png)

**Code** [`setup()`](../arduino/motor_control_low_frequency/motor_control_low_frequency.ino#L126-L128) :
```cpp
TIMSK1 = 0;              // turn off the Servo library Timer1 interrupt
TCCR1A = _BV(WGM10);     // 8-bit phase-correct PWM (triangle)
TCCR1B = _BV(CS12);      // prescaler 256 → 16,000,000 ÷ 256 ÷ 510 = 122.5 Hz
```
**Verified**: passed the track and smoother (11-16), restart after red / sign in 0.1–0.3 s (11-18), less weaving and good starts with the low-frequency values (11-23). Full calculation and graphs: [PWM.en.md](PWM.en.md).
