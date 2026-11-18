# Gallery: 4wd-car (Team "Gulno", 굴러라 노란바퀴)

> 한국어 : [GALLERY.md](GALLERY.md) · Videos and original files: [release 4wd-car ESP32 Car Control V1.0](https://github.com/CHOESEUNGJUN/4wd_car_project/releases/tag/4wd-car-esp32-car-control-v1.0)

## 1. The finished car (2026-11-18)

<p>
<img src="img/gallery/car_front.jpg" width="32%" alt="Front">
<img src="img/gallery/car_front_left.jpg" width="32%" alt="Front left">
<img src="img/gallery/car_left.jpg" width="32%" alt="Left">
</p>
<p>
<img src="img/gallery/car_right_side.jpg" width="32%" alt="Right side">
<img src="img/gallery/car_rear.jpg" width="32%" alt="Rear">
<img src="img/gallery/car_bluetooth_hc06_2.jpg" width="32%" alt="HC-06 Bluetooth and servo">
</p>

| Where | What |
|---|---|
| Front, top | ESP32-CAM (the board with the antenna), HC-SR04 ultrasonic sensor |
| Front, bottom | 3 line sensors (left A3 / centre A4 / right A5) |
| Middle | Arduino UNO + sensor shield, L298N motor driver (low-frequency wiring: ENA 9, IN2 5, ENB 10, IN3 6) |
| Small breadboard at the front | HC-06 Bluetooth (TXD → D3, RXD → D4) |
| Rear | AA battery pack (motors); the ESP32-CAM runs from a separate 5 V power bank |

## 2. What the ESP32-CAM saw (SD recording rec_0012, HSV view)

![Red light → green light → stop sign](img/gallery/esp_strip.png)

| 🔴 Red light → stop | 🟢 Green light → go | 🛑 Stop sign → stop 3 s |
|---|---|---|
| 848.1 s: 1211 red pixels, fill **99 %**, white 0 % → car stops (`RED`) | 856.3 s: 1144 green pixels → car drives on (`LINE`) | 893.6 s: fill **72 %**, centre white **15 %** → stop sign |

- The 160x120 picture the ESP32 actually saw, painted with its colour classes: red, green, yellow, white; dark grey = not a signal.
- The border colour is the ESP32's decision (red / green / magenta = stop sign). The sign's text is mirrored because horizontal mirroring was off (it doesn't affect the decision).
- A red light and a stop sign are both red, so they are told apart by **shape**: the phone's red screen almost fills its box (99 %) with no white, while the octagon leaves the corners empty (72 %) and has white text in the middle (15 %).

## 3. What the car recorded itself (ESP32-CAM SD recordings)

Video the ESP32-CAM recorded to its SD card while driving (5 frames per second, HSV view). The bottom line shows the time, the ESP's decision and the car's state (`LINE` line following, `CORNER`, `RED` stopped, …).

| 🔴 Waiting at a red light, 🟢 going on green | 🛑 Stop sign → stop 3 s → go |
|---|---|
| ![Red to green](img/gallery/drive_0012_red_to_green.gif) | ![Stop sign](img/gallery/drive_0012_stop_sign.gif) |
| log_0012, 850–859 s: stays stopped (`RED`) while red, back to `LINE` 0.3 s after green | log_0012, 724–731 s: stop sign (magenta border) → `RED` for ~3 s → drives on |

**One whole run (log_0011, 55 s at 2× speed)**: manual → auto start → stop on red for 18 s → go on green → obstacle stop → go → stop on red

![The whole run](img/gallery/drive_0011_full.gif)

**The 4 min 26 s run, one frame every 6 s (log_0012)**: red, green and magenta borders are the ESP's decision at that moment

![Frames from the run](img/gallery/drive_0012_sheet.png)

Original videos (1,544 frames in all) and logs: `drive_rec_0011/0012.mjpeg` and `drive_log_0011/0012.csv` in the release (play with VLC or `esp32/rec_viewer.html`).

## 4. The web page while driving (on a phone or laptop)

| Red light | Green light | Stop sign |
|---|---|---|
| <img src="img/gallery/web_red.jpg" alt="Web page, red light"> | <img src="img/gallery/web_green.jpg" alt="Web page, green light"> | <img src="img/gallery/web_stop.jpg" alt="Web page, stop sign"> |

Left: the HSV video the ESP32 sees (border = current signal). Right: drive buttons, traffic-light recognition and SD recording, car status (mode, action, distance ahead, line sensors, stage). The page UI is in Korean. Full videos: `screen_web_drive_1~3.mp4` in the release.

## 5. Figures (README)

| | |
|---|---|
| ![Low-frequency PWM](img/pwm_high_vs_low.en.png) | ![HSV colour ranges](img/hsv_ranges.en.png) |
| Coil current at high vs low frequency PWM (README 5-1) | Colour ranges the ESP32 uses (README 5-3) |
