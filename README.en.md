# Gulno Camera (ESP32-CAM): extra project branch `extra/esp32cam`

> 한국어 : [README.md](README.md)
>
> This branch is **version D** = version C (PID, `main`) plus an **ESP32-CAM camera** (both C and D are submitted). It goes beyond the lecture's team 2 **advanced B "first-person (camera view) control"** (p. 47): the camera recognises **traffic lights** and stop signs, and **the ESP32 decides by itself** and sends signals to the car over Bluetooth (lecture team 4 decides in Python on a PC; we do it inside the ESP32 with no PC).
> **Principle: the camera does not follow the line.** The car follows the line with 3 line sensors + PID; the camera only watches traffic lights and signs.
> **What it detects, and why we moved to low-frequency driving and HSV image processing: [chapters 9 and 5](#9-extra-project-esp32-cam-camera-branch-extraesp32cam) below.**
> For version C alone, see the [`main` branch](https://github.com/CHOESEUNGJUN/4wd_car_project/tree/main).

```
 5V external power ──▶ ESP32-CAM ── Wi-Fi (your hotspot) ──▶ any phone/laptop: http://gulno-cam.local (or 192.168.4.1)
                         │          colour video (border = current signal), pixel counts, car status, drive buttons, control, SD recording
                         └──── Bluetooth ──▶ HC-06 ──▶ Arduino (version D, line following with line sensors + PID)
 Phone app Gulno (iPhone / Android) "traffic light screen" (green → yellow → red) / "stop sign" ──▶ hold it in front of the camera
```

<p align="center"><img src="docs/img/web_phone.png" width="320" alt="ESP32-CAM web page on a phone: red light detected, controls, signal recognition, SD recording, address, car status"><br><sub>The ESP32-CAM web page on a phone (sample state: red light detected → red border, car stopped in RED). UI is in Korean.</sub></p>

| Drive (web page button) | What it does |
|---|---|
| Line-sensor drive (라인센서 주행) | `a` → the car drives itself with 3 line sensors + PID (same as version C) |
| Traffic light recognition on/off (신호등 인식) | The car keeps following the line; the camera keeps sending only signals every 0.2 s: 🔴 `x` stop, 🟡 `v` slow (base speed 120 → 100), 🟢 `n` normal, 🛑 stop sign = stop 3 s then go. Mode is unchanged |

**Handling the ESP32-CAM (every time)** — details in [`docs/CAMERA.md` chapter 0](docs/CAMERA.md) (Korean)

1. Turn on the iPhone hotspot (Maximize Compatibility) → connect the laptop / phone too
2. Turn on the car (version D) → power the ESP32-CAM with 5V → ready when the front white LED stops blinking and **stays on**
3. Browser **http://gulno-cam.local** (if it doesn't open, use the numeric address from the page's address card, e.g. `http://172.20.10.2`; on Android use "Camera" in the app. Since 2026-11-18 the ESP's own Wi-Fi at `192.168.4.1` only comes up when the hotspot can't be found, to save memory)
4. **Traffic light recognition: on** + phone traffic-light screen → **Line-sensor drive**; to stop, **Stop · Manual (정지 · 수동)**
5. Finish: Stop · Manual → disconnect Bluetooth → power off. One-time setup: upload (PlatformIO) and enter Wi-Fi (`CAMERA.md` ch. 4–5); troubleshooting in ch. 9

| | |
|---|---|
| Folders | **`arduino/`** = uploaded to the UNO (car) with the Arduino IDE (version C `team2_bt_pid`, version D `team2_bt_cam` (original wiring), **default: version D + low frequency `motor_control_low_frequency_cam`** (used in the 2026-11-18 driving test), version E `motor_control_low_frequency` and camera variant `motor_control_low_frequency_cam` for low-frequency PWM, diagnostics `diag`) · **`esp32/`** = uploaded to the ESP32-CAM with PlatformIO · **`apps/`** = Gulno apps (iPhone and Android both have traffic-light / stop-sign screens; Android also has a camera screen) |
| Camera code | [`esp32/esp32cam_line/`](esp32/esp32cam_line) (`esp32cam_line.ino`, web page `page.h`, build settings `platformio.ini`) |
| Docs | [`docs/CAMERA.md`](docs/CAMERA.md) (Korean): handling, structure, signals, wiring (5V external power), upload, Wi-Fi (printer-style setup), web page, speed and memory, mounting, troubleshooting, SD recording |
| Command table | [`PROTOCOL.md`](PROTOCOL.md) (`x v n` are version D only) |
| Reports | [`docs/REPORT_20261116.en.md`](docs/REPORT_20261116.en.md) (11-12 → 11-16, English) · [`docs/REPORT_20261111.md`](docs/REPORT_20261111.md) (Korean) |
| Next steps | [`docs/NEXT.md`](https://github.com/CHOESEUNGJUN/4wd_car_project/blob/main/docs/NEXT.md) on `main` (checklist, Korean) |
| Upload | **Not from the Arduino IDE** (its prebuilt libraries run out of memory once Bluetooth turns on). Flash the finished firmware (`.bin`) with Chrome or esptool, or build with PlatformIO → **[ESP32 flashing guide (Windows · Linux · macOS)](docs/ESP32_UPLOAD.en.md)**  The car is uploaded from the Arduino IDE (default `motor_control_low_frequency_cam`) |
| Status (2026-11-16) | Confirmed: Bluetooth + Wi-Fi + camera together, web control, signal response ~0.1–0.15 s (~20 fps), SD recording and viewer, low-frequency PWM (passed the track, smoother). Camera line detection and camera driving removed by the principle change. Coded and tested only (not yet measured on the board): HSV colour detection and HSV view, IRAM optimisation, direct access 192.168.4.1, Android app. Next: mount the camera on the car and test line-sensor driving with traffic lights and signs |

Below is the car documentation (versions C and E), same as `main`. The default is the low-frequency wiring (chapter 2, 2-1).

---

# 4wd-car: Team 2 "Gulno" (굴러라 노란바퀴, Roll, Yellow Wheels) Autonomous Mini Car

PID line follower + low-frequency PWM driving + Bluetooth manual / auto switch + ESP32-CAM traffic lights and stop signs

> 한국어 : [README.md](README.md)

A 4WD RC car (Eduino kit, Arduino UNO) that **follows a black line with PID control (auto)**. You drive it from the phone app "Gulno" (manual) and switch to auto with one button.
If the link drops, the car stops by itself, and while it drives on its own the phone shows what it is doing (straight, corner, searching for the line, waiting for an obstacle …).

<p align="center">
<img src="docs/img/car_front.jpg" width="23%" alt="Front">
<img src="docs/img/car_side.jpg" width="23%" alt="Side">
<img src="docs/img/car_rear.jpg" width="23%" alt="Rear">
<img src="docs/img/car_bt.jpg" width="23%" alt="HC-06 Bluetooth">
<br><sub>The finished car (2026-11-18): front · side · rear · HC-06 Bluetooth</sub>
</p>

- Folders: **`arduino/`** = code uploaded to the UNO (the car) with the Arduino IDE. The camera (ESP32, PlatformIO) lives in **`esp32/`** on the branch `extra/esp32cam`.
- Car code: **default is `arduino/motor_control_low_frequency/` (version E = PID + low-frequency PWM)**. Version C (`team2_bt_pid`) is kept for the lecture deck's original wiring (why the pins differ: [2-1](#2-1-why-low-frequency-moves-the-pins)). First finished build: [release 1.0, PID version](https://github.com/CHOESEUNGJUN/4wd_car_project/releases/tag/v1.0) (includes the app APK).
- Lecture deck: team 2 in `minicar_lecture_deck_all_teams_v5.pdf` (pages 34–48)
- Protocol (team agreement): [`PROTOCOL.md`](PROTOCOL.md) — change this document first, and agree on it, before changing the communication code in the apps or sketches.
- Extra project **ESP32-CAM camera** lives on the branch [`extra/esp32cam`](https://github.com/CHOESEUNGJUN/4wd_car_project/tree/extra/esp32cam) (chapter 9).
- **What we changed in driving and image processing, and why: [chapter 5](#5-what-we-changed-and-why-high--low-frequency-pwm-hsv-image-processing)** (high → low frequency PWM, camera line following → traffic lights and signs only, RGB → HSV)
- **Finished release: [4wd-car ESP32 Car Control V1.0](https://github.com/CHOESEUNGJUN/4wd_car_project/releases/tag/4wd-car-esp32-car-control-v1.0)** (Arduino sketches, ESP32-CAM firmware, Android app, drive logs and videos, how-to) · **Final report: [`docs/REPORT_20261116.en.md`](docs/REPORT_20261116.en.md)**
- Next steps: [`docs/NEXT.md`](docs/NEXT.md) (retune signal thresholds, test the Android app on a phone, low-frequency numbers; Korean)

| Version | Folder | Motor PWM | Camera | Status |
|---|---|---|---|---|
| **C** | `arduino/team2_bt_pid/` | High frequency (default ~980 Hz) | None | **Passed** (2026-11-16, team car code) |
| **E (default)** | `arduino/motor_control_low_frequency/` | **Low frequency ~122 Hz** (swap 4 wires) | None | **Passed** (2026-11-16, smoother) — [release 1.0.1](https://github.com/CHOESEUNGJUN/4wd_car_project/releases/tag/4wd-car-low-frequency-pwm-pid-control-car-v1.0.1) |
| D | `arduino/team2_bt_cam/` on branch `extra/esp32cam` (+ low-frequency `motor_control_low_frequency_cam`) | C or E | ESP32-CAM: traffic lights and stop signs (HSV) | Code done, not yet tested on the car |

---

## 0. Getting started (for newcomers)

How someone who downloads this repository can build the same car and make it drive. Details are in each chapter.

**Parts**

| Part | Note |
|---|---|
| 4WD RC car kit (Arduino UNO + sensor shield, L298N motor driver, 4 TT gear motors) | Based on the Eduino 4WD kit |
| 3 digital line sensors (black line = 1) | Left, centre, right at the front |
| Ultrasonic sensor + servo | Obstacle stop |
| Bluetooth module **HC-06** (Android) or **HM-10** (iPhone, web) | 9600 bps |
| Battery (motors) | A fresh battery spins harder → see 4-3, 4-4 |
| Track: bright floor + **black electrical tape (~1.9 cm wide)** | Adjust the corner values on slippery boards |

**1) Get the code** — on GitHub, **Code → Download ZIP** and unzip, or

```bash
git clone https://github.com/CHOESEUNGJUN/4wd_car_project.git
```

**2) Wiring** — see [chapter 2](#2-wiring) (**motor driver uses the low-frequency wiring**). Line sensors **left A3 / centre A4 / right A5**, Bluetooth **TXD → D3, RXD → D4** (crossed).

**3) Upload**
1. Install the [Arduino IDE](https://www.arduino.cc/en/software) (2.x).
2. Open `arduino/motor_control_low_frequency/motor_control_low_frequency.ino` (with the original wiring: `team2_bt_pid`) → board **Arduino Uno**, select the port → **Upload**.
3. If `Servo.h` is missing, install `Servo` from the **Library Manager**.

**4) Check the sensors** — lift the car so the wheels are free, Serial Monitor at **115200**.
Put each line sensor over the tape and check that `라인=100 / 010 / 001` (line = …) changes at the right position.
If not, turn the sensor's sensitivity screw; if left and right are swapped, swap `L_Line` and `R_Line` at the top of the sketch.

**5) Connect the app** — [chapter 6](#6-apps)
- Android + HC-06: download `gulno.apk` from [release 1.0](https://github.com/CHOESEUNGJUN/4wd_car_project/releases/tag/v1.0) and install it (or open `apps/android` in Android Studio and press ▶).
- iPhone + HM-10: open `apps/ios_mac/MiniCar.xcodeproj` in Xcode and press ▶.
- No install: open `apps/web/index.html` in Chrome (HM-10 over BLE; on a Mac also HC-06 over serial).

**6) Drive** — check that ▲ moves the car in manual → put the car on the line and press **Auto (자동으로)** → it starts after 0.5 s.
Checklist in the [chapter 3](#3-first-run) table.

**7) Tune for your track** — if it wobbles or misses corners, change `KP`, `KD`, `CORNER_OUT` in the order of [4-4](#4-4-settings-and-tuning) and upload again. Fixes by symptom are in [`docs/PID.md`](docs/PID.md) (Korean).

---

## 1. Overview

```
 [phone app / web] ──Bluetooth 9600──▶ [HC-06 or HM-10] ──D3/D4──▶ [Arduino UNO] ──▶ motors, servo
   buttons: g b l r q w s                (wireless serial)              3 line sensors
   mode buttons: a / m                                                   ultrasonic sensor
   every 0.6 s: h (heartbeat)
       ◀── every 0.2 s one status line "ag,23,010,LINE" ──
```

| Folder | What | When |
|---|---|---|
| `arduino/team2_bt_pid/` (Arduino IDE → UNO) | **Version C** car sketch (lecture deck's original wiring) | PID line following + separate outer/inner wheel power in corners. See `docs/PID.md` |
| `arduino/motor_control_low_frequency/` (Arduino IDE → UNO) | **Default: version E = version C + low-frequency PWM (passed 2026-11-16)** | Motor PWM ~122 Hz: more low-speed torque and smoother (track tested), start kick 170 / 0.06 s. **Swap 4 wires** (ENA 9, IN2 5, ENB 10, IN3 6). With the original wiring, use version C. End of `docs/PID.md` |
| Camera (extra project) | ESP32-CAM traffic light / stop sign detection (HSV) + Wi-Fi page + Bluetooth | **Only on branch [`extra/esp32cam`](https://github.com/CHOESEUNGJUN/4wd_car_project/tree/extra/esp32cam)** (chapter 9) |
| `arduino/diag/` (Arduino IDE → UNO) | Diagnostic sketch | Only to check that Bluetooth characters arrive and sensors read |
| `apps/ios_mac/` | iPhone / Mac app (Xcode) | iPhone + **HM-10** |
| `apps/android/` | Android app | Android + **HC-06** |
| `apps/web/index.html` | Web control page | Directly in Chrome (HM-10 BLE / HC-06 on a Mac) |

---

## 2. Wiring

Same as the common pin plan in the lecture deck (pp. 131–140). Numbers are the S (signal) pins on the sensor shield.

| Part | Pins |
|---|---|
| L298N motor driver | **Default (low frequency, versions E and D)**: ENA **9**, IN1 8, IN2 **5** (right) / ENB **10**, IN3 **6**, IN4 11 (left)<br>Original wiring (version C, lecture deck): ENA 5, IN2 9 / ENB 6, IN3 10 — only the 4 bold pins differ ([2-1](#2-1-why-low-frequency-moves-the-pins)) |
| 3 line sensors | left A3 / centre A4 / right A5 (black line = 1) |
| Ultrasonic | TRIG 13, ECHO 12 |
| Servo (ultrasonic mount) | 2 — **keep it connected**. Faces forward (90°) at power-on |
| **Bluetooth module** | VCC → V(3) 5V, GND → G(3), **TXD → S(3) = D3**, **RXD → S(4) = D4** |

- Bluetooth TX/RX are **crossed** (the module's TXD goes to D3, which the Arduino reads). If it connects but no characters pass, swap the two wires.
- Don't use pins 0 and 1 (shared with USB; uploads fail).
- RXD is 3.3 V logic, so a divider D4 ─ 1 kΩ ─ RXD ─ 2 kΩ ─ GND is safer (optional).
- **The HC-06 doesn't work with iPhones** (classic Bluetooth). iPhones need the HM-10 (BLE). Same pins and code; just swap the module.

### 2-1. Why low frequency moves the pins

The UNO's PWM pins (the ones that can set a speed) belong to **three timers**, and each timer has one PWM frequency. Going to low frequency (~122 Hz) means changing one timer's settings.

| Timer | PWM pins | Already used by | If its frequency changes |
|---|---|---|---|
| Timer0 | **5, 6** | `millis()`, `delay()` (the Arduino clock) | the clock runs wrong, breaking the 20 ms PID period and link-loss detection → **can't change** |
| Timer1 | **9, 10** | Servo library (only faces forward once at startup) | **free to change** → ~122 Hz |
| Timer2 | 3, 11 | pin 3 = Bluetooth receive (D3), pin 11 = motor direction | not free |

- In the lecture deck's wiring the speed pins (ENA, ENB) are on **5 and 6 = Timer0**, whose frequency can't change → move the speed pins to **9 and 10, where Timer1 comes out**.
- The **direction pins (IN2, IN3)** that were on 9 and 10 only switch on and off (digital), so any pin works; they move to the freed pins 5 and 6.
- So it is just **swapping 4 wires**: 5↔9, 6↔10. In the code only the 4 pin numbers and a few Timer1 lines differ.
- Cost: the servo can't move while driving (the motors use Timer1). The ultrasonic mount servo only needs to face forward once at startup, so that's fine.
- Why low frequency is the default (low-speed torque, smoothness): [5-1](#5-1-motors-high-frequency-pwm-version-c--low-frequency-pwm-version-e).

---

## 3. First run

1. In the Arduino IDE, open `arduino/motor_control_low_frequency/motor_control_low_frequency.ino` (original wiring: `team2_bt_pid`) via **File → Open** and upload.
   (If an old file is still open, the IDE compiles the old content. After downloading a new copy, close the tab and reopen.)
2. Lift the car so the wheels are free and open the Serial Monitor — **115200 for version C**, 9600 for the diagnostic sketch.
3. Connect with the phone app (chapter 5).
4. Checklist

| Try | Correct result |
|---|---|
| Right after connecting | Serial shows `링크 O` (link O), app shows "manual · stop" |
| Hold ▲ | Wheels turn forward, app shows "manual · straight" |
| Release | Stops immediately |
| **Auto (자동으로)** | "Switch to auto mode?" → switch → orange banner, stops 0.5 s, then follows the line |
| Any direction button during auto | Back to manual instantly, moves that way (even during a long manoeuvre) |
| Close the app / turn off Bluetooth | In manual it stops within 0.6 s; the app shows a red banner "no remote control" |
| Hand in front of the sensor (10 cm) | Forward is blocked, "obstacle stop" |

---

## 4. Core technique: PID line following

### 4-1. What is PID

**PID control** is a **feedback control** method: it keeps measuring how far you are from the target (the error) and corrects the output to shrink that error.
It is used almost everywhere a target has to be held: factory temperature, drone attitude, car cruise control.

On our car the target is **the line in the middle of the car**, and the output is **the speed difference between the left and right wheels**.

```
            ┌───────────┐  error e  ┌─────────┐ correction u ┌───────────────────┐
 target ───▶│ target -   │─────────▶│   PID   │─────────────▶│ left  = BASE + u  │──▶ car moves
 (line in   │ current    │          └─────────┘              │ right = BASE - u  │      │
  middle)   └───────────┘                                    └───────────────────┘      │
                 ▲                                                                       │
                 └──────────── measured again by 3 line sensors (every 20 ms) ◀──────────┘
```

The output (correction u) is the sum of three terms.

```
u = Kp × e  +  Ki × ∫e dt  +  Kd × de/dt
    ── P ──    ──── I ────    ──── D ────
```

| Term | Name | Reacts to | Bicycle analogy | If too large |
|---|---|---|---|---|
| **P** | Proportional | how far off you are **now** | steer more the further off you are | overshoots and wobbles left and right |
| **I** | Integral | how **long** you have been off (error summed over time) | keep being pushed to one side → steer more and more | keeps steering after the curve and swings the other way |
| **D** | Derivative | how **fast** the error changes | coming back to the line fast → steer less in advance (brake) | jerks every time the error changes |

P alone overshoots and comes back over and over (oscillation); D damps that, and I removes the one-sided drift P leaves behind (motor imbalance, long curves).

### 4-2. How it works on our car

**① Making the error — 3 line sensors (black line = 1, white floor = 0)**

| Left centre right | Error e | Meaning | What the car does |
|---|---|---|---|
| 0 1 0 | 0 | line in the middle | PID → straight |
| 0 1 1 / 1 1 0 | +1 / −1 | line slightly right / left | PID → steer a little that way |
| 0 0 1 / 1 0 0 | +2 / −2 | only an edge sensor = corner | corner turn: outer 180 / inner −90 |
| 0 0 0 | ±3 | line lost (last side seen) | PID correction 0.5 s → last side 0.5 s → other side 1 s → stop |
| 1 1 1 | 0 | cross line | straight; longer than 0.5 s = end of line, stop |

**② PID calculation — every 20 ms (`pidLine()`)**

```cpp
integ = constrain(integ + e * dt, -I_LIMIT, I_LIMIT);   // I: sum the error over time (up to ±50)
dTerm = dTerm * D_DECAY + KD * (e - lastErr);           // D: appears when the error changes, fades over 0.1 s
float u = KP * e + KI * integ + dTerm;                  // P + I + D

int left = BASE + u, right = BASE - u;                  // u > 0 → left faster → turns right
driveManualLR(constrain(left, 0, 255), constrain(right, 0, 255));
```

**③ Steering with the wheels — a car without a steering wheel (differential drive)**

The car has no steering; it turns only by the **speed difference between left and right wheels**. The outer wheel speeds up by u and the inner wheel slows down. (The inner wheel is raised to a minimum of 90 even if the math says less.)

```
   straight (e = 0)     slightly right (e = +1)     right corner (e = +2)
   ▲       ▲           ▲         ▲               ▲          ▼
  [L]     [R]         [L]       [R]             [L]        [R]
  120     120         170~200     90               180        -90 (reverse)
```

### 4-3. What we changed for this car

Why textbook PID doesn't work as-is, and the fix:

| Problem | Fix |
|---|---|
| Sensors are only 0/1, so the error jumps in **steps** (0 → 1). Normal D lasts only one 20 ms tick and is gone before the motor reacts | D decays by ×0.8 every 20 ms so it lasts ~0.1 s (`D_DECAY` 0.8) |
| Accumulated I makes the car swing the other way after a curve | Reset I to 0 when the error crosses to the other side, and clamp to ±50 (`I_LIMIT`) |
| PID alone can't make sharp corners (only an edge sensor sees the line) | Corners use fixed outer 180 / inner −90 instead of PID. Clear I and D when returning to PID so it doesn't jerk |
| Small gearbox: small PWM values don't turn the wheels | Minimum 90 when non-zero, plus 0.12 s at full power (255) when the direction changes. Root-cause fix: **version E** (low-frequency PWM ~122 Hz, swap 4 wires) — chapter 5-1 |
| Sensor spacing (4.9 cm) is wider than the tape (1.9 cm), so the line can sit between sensors → `000`. After an overshooting corner the line is on the other side | Don't turn immediately when lost: PID correction for 0.5 s first, then search last side → other side |
| The same PWM turns more or less depending on battery voltage | Corner outer set to 180 instead of 220 for a fresh battery (200–220 when weaker) |

### 4-4. Settings and tuning

Edit the numbers in the `[설정]` (settings) block at the top of the sketch (`motor_control_low_frequency` or `team2_bt_pid`; both use the same PID and corner values) and upload. The error and correction are shown on Serial (115200) and in the app.

| Name | Now | Meaning |
|---|---|---|
| `KP` / `KI` / `KD` | 51 / 0.2 / 30 | P, I, D gains |
| `BASE` | 120 | base speed on straights |
| `CORNER_OUT` / `CORNER_IN` | 180 / −90 | outer / inner wheel in corners |
| `PID_MS` | 20 | calculation period (ms) |
| `LOST_PID_MS` / `LOST_STOP_MS` / `BACK_SEARCH_MS` | 500 / 500 / 1000 | line lost: PID correction / search last side / search other side (ms) |

Tuning order (one at a time): with **Ki = Kd = 0, raise Kp** until just before it starts to wobble → if it wobbles, **raise Kd** → if it under-turns in corners, raise `CORNER_OUT` → if it drifts to one side, a little **Ki** → when all good, raise `BASE` and repeat.

- **More detail and fixes by symptom: [`docs/PID.md`](docs/PID.md)** (Korean).
- 2026-11-09 work report: [`docs/REPORT_20261109.md`](docs/REPORT_20261109.md) (Korean).

> **Pin note**: line sensors are left A3 / centre A4 / right A5, which differs from the lecture deck (A5/A4/A3). After the first upload, cover each sensor by hand and check that the serial line value changes at the right position. If not, only change `L_Line, C_Line, R_Line` at the top of the sketch.

---

## 5. What we changed and why: high / low frequency PWM, HSV image processing

### 5-1. Motors: high-frequency PWM (version C) → low-frequency PWM (version E)

PWM switches the motor power on and off very fast and sets the power by the fraction of time it is on (duty). The **frequency** is how many times per second it switches.

![Coil current at 30 % duty, high vs low frequency](docs/img/pwm_high_vs_low.en.png)

At the same average voltage (30 % duty), the high-frequency on-time (0.3 ms) is about as long as the coil current takes to rise, so peak force stops at 66 %; with low frequency the pulse (2.5 ms) is long enough to reach 100 % → it beats static friction and the wheel turns (simple RL model; the friction line is an example).

| | High frequency (version C) | Low frequency (version E) |
|---|---|---|
| Frequency | UNO default `analogWrite`: pins 5 and 6 at ~**980 Hz** (~1 ms period) | Timer1 set directly: pins 9 and 10 at ~**122 Hz** (~8 ms period) |
| Low-speed force | Weak. Short pulses switch off before the coil current builds up → small PWM doesn't turn the wheel | Strong. Long pulses let the current build up each time, so even a small duty turns the wheel |
| Consequence | Minimum `MIN_PWM` 90 + start kick 255 (0.12 s) to force a start → **jerky** in manual, and at low speed only a whine with no torque | Kick cut to **170 for 0.06 s** → smooth starts and slow driving, easier precise control |
| Downside | Quiet | Low hum; the servo can't move while driving (Timer1 drives the motors; the servo only needs to face forward once at startup) |
| Wiring | ENA 5, IN2 9 / ENB 6, IN3 10 | **Swap 5↔9 and 6↔10** (Timer1 outputs only on pins 9 and 10). Swap back for version C |

- **Why**: version C was a half fix that forced starts with a kick and a minimum power. The real cause (low-speed torque) was the PWM frequency; lowering it lets the car start with a much smaller kick. The goal is following the line precisely, not top speed, so we fixed the low frequency.
- Result (2026-11-16): passed the track, smoother. Version C is kept as-is for rollback. `millis()`, Bluetooth and ultrasonic use Timer0 / SoftwareSerial and are unaffected. Details: "버전 E" at the end of [`docs/PID.md`](docs/PID.md).

### 5-2. Camera: line following → traffic lights and signs only

| First (2026-11-11) | Now (2026-11-16) |
|---|---|
| The camera found the floor line, cleaned it with morphology, drew the path and target, and drove from the camera | **No camera line following.** The line is followed by the line sensors + PID (same as version C); the camera only gives signals |

- **Why**: on the track the car was faster than expected, so steering from the camera (~20 fps plus Wi-Fi/Bluetooth delay) was too late. The line sensors + PID already follow the line every 20 ms, so the camera does **one job (watching signals)** — faster and safer. If the camera stops, line following continues, and a red light never times out, so the car stays stopped.
- Lecture team 4 sends the video to a PC and decides in Python → we decide **inside the ESP32** with no PC (no Wi-Fi video round trip, so it is faster).

### 5-3. Colour detection: RGB rules → HSV + class table

| | RGB (first) | **HSV + table (now)** |
|---|---|---|
| Method | Compare R, G, B directly, e.g. "R high and G, B low = red" | Convert to hue H (0–359°), saturation S, value V and decide by **hue** (lecture team 3's method). Red H < 20 or ≥ 330, yellow 35–75, green 85–170, white = low saturation and bright |
| Weakness / strength | Misses dark or faded colours; when exposure changes while driving, all three values shift together | Hue stays the same when brightness changes → **same colour in a dark room or at speed** |
| Test (17 synthetic scenes) | 12 correct (missed dark red, green, yellow, faded red, dark sign) | **All 17 correct**; floor (beige, wood) and white walls are not signals |
| Speed | Comparisons for every pixel | A 4096-colour (4 bits per R, G, B) class table (4 KB) built once at boot; per pixel just **one table lookup** → no math |

![Colour ranges the ESP32 uses, and its decision when brightness or saturation changes](docs/img/hsv_ranges.en.png)

![HSV view: left = camera video, right = colour class the ESP32 sees (dark red and faded colours become the same red)](docs/hsv_view.png)


- **Why**: when the car drove fast, recognition often failed because colours got darker or washed out → HSV separates brightness from hue. HSV is normally slower to compute, but the precomputed table makes it lighter than the RGB comparisons.
- **HSV view is the fastest video**: no JPEG encoding; only colour classes are packed and sent, and the browser paints them (~1.4 KB per frame; on the same hotspot link JPEG 3–4 fps → HSV 5–11 fps, measured).
- Turn on **HSV view** in the web camera settings to paint the video with the colour class the ESP32 sees (red, green, yellow, white; grey = not a signal) — useful for tuning under real lighting.

### 5-4. Speed: recognition speed over resolution

| Tried | Result | Chosen |
|---|---|---|
| 320x240 colour | Sensor gives only 9–10 fps → ~105 ms per loop | ✗ |
| Sensor JPEG (hardware compression) + decode | Decoding takes 95 ms → 110 ms per loop | ✗ |
| Camera clock 24 MHz | Not faster, only more incomplete frames | ✗ (20 MHz) |
| **160x120 colour, every pixel** | ~48 ms per loop (~20 fps), catches stop signs down to an 11 px radius | ✓ |

- Signal response: ~2 s at first → **~0.1–0.15 s** (confirmed after 2 frames for red, 3 for other colours; a change is sent immediately and repeated every 0.2 s).
- Per-frame functions run from IRAM with -O2 (flash code and PSRAM frames share the cache, so code would be evicted); each frame is read once and signal counting and video copy share the same row pass. The field of view is already maximal (the OV3660 scales the full sensor even at small sizes; wider needs a wide-angle lens module).
- **The camera chip is an OV3660 (confirmed on the board 2026-11-18, not an OV2640).** The sensor sets the frame rate: at 160x120 colour the driver runs the sensor clock at 40 MHz, giving **one frame every 45 ms (22.2 fps)** (measured on the VSYNC pin). Of the 48 ms loop only ~7 ms is processing; the rest is waiting for the next frame, so trimming code no longer helps. Raising the sensor clock (PLL multiplier 8 → 10) makes the sensor output 27.8 fps, but the ESP32 can't capture it: incomplete frames rise from 1 to 28 per 10 s, and multiplier 12 breaks almost every frame → left unchanged. Going faster needs an ESP32-S3 board (faster camera input)

---

## 6. Apps

### Android (HC-06)
- Install: download `gulno.apk` from the [release](https://github.com/CHOESEUNGJUN/4wd_car_project/releases/tag/v1.0) on the phone and install (allow "unknown sources"). To build yourself, open `apps/android` in Android Studio and press ▶.
- Connect: **Find devices (기기 찾기)** → nearby devices. Devices starting with `HC-` are marked **★** at the top with signal strength (closer to 0 = nearer). Tap to connect; first time PIN **1234**.
- Allow "Nearby devices" when asked.

### iPhone · Mac (HM-10)
- Open `apps/ios_mac/MiniCar.xcodeproj` in Xcode → Signing & Capabilities, choose your Team (Apple ID) → connect the iPhone → ▶. Choose My Mac as the run target for the Mac app.
- After the first install, trust the developer app in **Settings → General → VPN & Device Management**, and turn on **Settings → Privacy & Security → Developer Mode**.
- With a free Apple ID you have to reinstall (▶) every 7 days.

### Web (`apps/web/index.html`)
- Open in Chrome. **BLE connect** = HM-10 (Android/Mac Chrome; on iPhone use the Bluefy app), **Serial connect** = an HC-06 paired with the Mac.
- Keyboard: arrow keys, Q/E spin in place, Space stop.

### What the screen shows

```
 Connected: HC-06                 [Disconnect]
 Auto · spin left               [To manual]     ← mode · current action
        ↺   ▲   ↻
        ◀   ■   ▶
            ▼
 Searching for the line, left and right         ← current stage
 Front: 23cm · sensors ○ ● ○                    ← distance ahead · line sensors (● = black line)
```

(The app UI is in Korean.)

---

## 7. How it works

### Commands (phone → car, one character)

| Char | Meaning |
|---|---|
| `g` `b` `l` `r` | forward, backward, left (curve), right (curve) |
| `q` `w` | spin left, spin right |
| `s` | stop |
| `a` `m` | to auto mode, to manual mode |
| `h` | heartbeat (every 0.6 s, does not affect driving) |
| `x` `v` `n` | **version D only**: ESP32-CAM traffic light (red stop, yellow slow, normal). `docs/CAMERA.md` |

`g l r b s q w` match the course app (Eduino_RC_Car_Controller); `a m h` are new. Characters not in the table (line breaks added by terminal apps, etc.) are ignored.

### Mode switch

```
            'a' received
  [manual] ───────────────▶ [auto]
     ▲                         │
     └─────────────────────────┘
      'm' received, or any direction button during auto (= human takes over)

  To manual: stop immediately, controllable right away (no delay)
  To auto:   app asks "Switch to auto mode?" → 0.5 s forced stop (SWITCH_HOLD_MS) → go
             during those 0.5 s the app shows an orange banner "switching to auto · 0.5 s safety delay"
```

### Safety

| Situation | Behaviour | Setting |
|---|---|---|
| Commands stop arriving in manual (phone off, out of range) | Stops after 0.45 s. The app resends the same command every 0.2 s while a button is held | `LINK_MS` 450 |
| **Bluetooth lost during auto** | If no heartbeat (`h`, every 0.6 s) for 1.5 s → stop and go to manual. After reconnecting you must press auto again | `AUTO_LINK_MS` 1500 |
| Button released | Sends `s` twice (one character can be lost while the car is sending status) | — |
| Switch to auto | Confirmation + 0.5 s forced stop (manual is instant) | `SWITCH_HOLD_MS` 500 |
| Bluetooth lost / car not responding | App: no status line for over 1 s → red banner "no remote control", controls, mode and PID buttons disabled | app 1000 ms |
| Obstacle ahead in manual | Only forward commands (g l r) are blocked; backward and spin work | `AVOID_MM` 10 cm |

- `h` is used for the auto-mode link check and the "link O/X" display, but not for the manual stop timer (`LINK_MS`). Otherwise, if `s` is lost on release, `h` would keep the link alive and the car would keep driving.
- **If Bluetooth drops, both manual and auto stop.** If you start auto from a serial terminal app that doesn't send `h`, it stops after 1.5 s (our three apps send it).

### Status line (car → phone, every 0.2 s)

```
ag,23,010,LINE
││ │  │   └ stage
││ │  └ line sensors left·centre·right (1 = black line)
││ └ distance ahead in cm (999 = no reading)
│└ current action: g straight b back l/r left/right q/w spin s stop o obstacle stop
└ mode: a auto, m manual
```

| Stage | App shows |
|---|---|
| `MAN` | manual control |
| `HOLD` | switching (0.5 s) |
| `LINE` | following the line |
| `CORNER` | turning a corner |
| `NOLINE` | line lost (searching) |
| `LOST` | line not found (stopped) |
| `OBST` | waiting for obstacle |
| `CROSS` / `END` | crossing a cross line / end of line (stop) |

Version C appends error and correction: `ar,23,011,LINE,1,60`. PID values are sent on a separate line `#P60.0,I0.00,D40.0,B140`.

---

## 8. Debugging

- The Serial Monitor (115200) prints one line every 0.2–0.3 s:
  - Version C: `링크=O MAN  단계=MAN act=g cmd=g 라인=010 오차=0 보정=0 모터L=150 R=150 거리=230mm 장애물=X`
    (link, stage, action, last manual command, line, error, correction, actual motor speeds L/R (negative = backward), distance, obstacle)
    - `장애물=O` (obstacle = O) means forward (g l r) is blocked
- Each received command prints `수신: g` (received: g) immediately.
- `링크 O` = the app is connected and sending the heartbeat every 0.6 s. Serial terminal apps don't send `h`, so it shows O only while driving.
- If you suspect Bluetooth, upload `arduino/diag/`: it shows received characters in hex and echoes them back (9600). The motors don't move.

### Common problems

| Symptom | Cause → fix |
|---|---|
| Compile error `'Mode' was not declared` (odd line numbers) | IDE has an old file open → close the tab and reopen |
| `BT누적=0` (BT total = 0) in the diagnostic sketch | TXD/RXD swapped → D3 ← module TXD, D4 → module RXD, crossed |
| Module not listed in the iPhone app | It's an HC-06 → use an HM-10, or Android / Mac |
| iPhone settings say "connected" but no characters pass | iOS doesn't expose the HC-06 data channel (SPP) to apps → as above |
| HC-06 port "busy" on the Mac | The Chrome web control tab holds it → close the tab |
| Garbled characters | Baud mismatch → module 9600, Serial Monitor 115200 for version C |
| Car turns away from the line | Line sensor pins left/right swapped → swap `L_Line` and `R_Line` |
| Ultrasonic servo jitters | Bluetooth (SoftwareSerial) disturbs the servo pulse on every character → fixed by turning the servo signal off after facing forward. If it still jitters, check the battery |
| All three line sensor values similarly low | Sensor too close to the floor or covered → adjust height / sensitivity |
| Device not visible in the Android app | Permission denied / Bluetooth off → allow "Nearby devices" in app settings |

---

## 9. Extra project: ESP32-CAM camera (branch `extra/esp32cam`)

**Version D (also submitted)** = version C plus an ESP32-CAM. **The car follows the line with its 3 line sensors + PID; the camera only watches traffic lights and stop signs** (no camera line following — see 5-2). The ESP32 decides the colour itself and sends a signal character to the car over Bluetooth (no PC). Not on `main`; developed only on the branch.

```
 5V external power ──▶ ESP32-CAM ── Wi-Fi ──▶ any phone/laptop: http://gulno-cam.local or http://192.168.4.1
                         │          colour video (border = current signal), HSV view, pixel counts, car status, drive buttons, SD recording
                         └──── Bluetooth ──▶ HC-06 ──▶ Arduino (version D, line following with line sensors + PID)
 Phone app Gulno (iPhone / Android) "traffic light screen" / "stop sign" ──▶ hold it in front of the camera
```

**What it detects**

| Seen | Character sent by the ESP32 | Car (stays in auto) |
|---|---|---|
| 🔴 red light | `x` (every 0.2 s) | Stop, until another signal arrives or a person sends `a`/`m` (doesn't drive even if the ESP32 dies) |
| 🟡 yellow light | `v` | Slow: base speed 120 → 100 (back to normal after 1 s without `v`) |
| 🟢 green light / no signal | `n` | Normal |
| 🛑 stop sign (red octagon + white text) | `x` for 3 s → `n` | Stops 3 s then drives on, ignores the same sign for 5 s |

Red light and stop sign are both red, so they are told apart by **shape**: how much of the red blob's bounding box is red (phone red screen 86–100 %, octagon 61–68 %) and how much white (text, border) is inside.

| | |
|---|---|
| Get it | `git clone -b extra/esp32cam https://github.com/CHOESEUNGJUN/4wd_car_project.git` (or `git switch extra/esp32cam`) |
| Docs | `docs/CAMERA.md` on the branch (handling, signals and signs, wiring, upload, Wi-Fi, web page, speed and memory, SD recording; Korean), report `docs/REPORT_20261111.md` |
| Upload | **Not from the Arduino IDE** (its prebuilt libraries run out of memory once Bluetooth turns on). Flash the finished firmware (`.bin`) with Chrome or esptool, or build with PlatformIO → **[ESP32 flashing guide (Windows · Linux · macOS)](docs/ESP32_UPLOAD.en.md)** |
| Status (2026-11-16) | Bluetooth + Wi-Fi + camera together, signal response ~0.1–0.15 s (~20 fps), SD recording and viewer confirmed. HSV, HSV view and direct access (192.168.4.1) are coded and tested offline, not yet measured on the board |
