# Flashing the ESP32-CAM firmware (Windows · Linux · macOS)

> 한국어 : [ESP32_UPLOAD.md](ESP32_UPLOAD.md)

The car (Arduino UNO) is uploaded with the Arduino IDE, but **the ESP32-CAM must not be uploaded from the Arduino IDE.** This guide covers the ESP32-CAM only.

| Method | When | You need |
|---|---|---|
| **A. Flash the finished firmware** (easiest) | You don't change the code | `4wd-car-esp32cam-v1.0-factory.bin` from the release + Chrome (or esptool) |
| **B. Build from source and upload** | You change the code | PlatformIO (VS Code extension or command line) |

---

## 0. Why not the Arduino IDE

- The Arduino IDE's ESP32 board package uses **prebuilt ESP32 libraries**, which put Bluetooth and Wi-Fi memory in **internal RAM (~320 KB)**.
- Our firmware runs **camera + Wi-Fi + Bluetooth (HC-06 link)** at the same time. With the prebuilt libraries there isn't enough internal memory: it **uploads and starts, but freezes or restarts the moment Bluetooth turns on** (tested 2026-11-11).
- So `esp32/esp32cam_line/platformio.ini` **rebuilds the ESP32 libraries** through `custom_sdkconfig`:
  - Bluetooth and Wi-Fi memory goes to external PSRAM (4 MB) first
  - BLE, Bluetooth audio (A2DP, HFP) and GATT are off (we only use the HC-06 serial link)
  - smaller Bluetooth task stacks and Wi-Fi buffers
  - result: 33–36 KB internal memory free with HC-06 connected (9–20 KB and freezes before)
- The Arduino IDE can't use these settings (it can't rebuild the libraries). **Build with PlatformIO, or flash the firmware (`.bin`) built that way.**

---

## 1. Connecting the board (all methods)

1. Put the ESP32-CAM on an **ESP32-CAM-MB** (micro-USB base board).
2. Use a USB cable that **carries data** (a charge-only cable shows no port).
3. Find the port (the MB board's USB chip is usually a **CH340**):

| | Port name | Driver |
|---|---|---|
| Windows | `COM3`, `COM5` … (Device Manager → Ports (COM & LPT) → "USB-SERIAL CH340") | if missing, install WCH's **CH340 driver** |
| macOS | `/dev/cu.usbserial-XXXX` or `/dev/cu.wchusbserialXXXX` (`ls /dev/cu.*`) | built in on recent macOS |
| Linux | `/dev/ttyUSB0` (`ls /dev/ttyUSB*`) | built in (ch341). Permission: `sudo usermod -aG dialout $USER`, then **log in again** |

4. Upload mode: the MB board enters it **automatically**. On "Failed to connect", **hold IO0, press and release RST**, then upload again.

---

## 2. Method A: flash the finished firmware (no build)

Download **`4wd-car-esp32cam-v1.0-factory.bin`** from the [release 4wd-car ESP32 Car Control V1.0](https://github.com/CHOESEUNGJUN/4wd_car_project/releases/tag/4wd-car-esp32-car-control-v1.0). Address **`0x0`**.

### A-1. In the browser (nothing to install; Windows, macOS, Linux)

1. Open [Espressif ESP Tool (esptool-js)](https://espressif.github.io/esptool-js/) in **Chrome or Edge**.
2. Baudrate 460800 → **Connect** → pick the ESP32-CAM port.
3. Flash Address **`0x0`**, file `4wd-car-esp32cam-v1.0-factory.bin` → **Program**.
4. When done, press **RST** on the MB board.

### A-2. With esptool

| | Install | Flash |
|---|---|---|
| Windows (PowerShell) | `py -m pip install esptool` | `py -m esptool --chip esp32 --port COM5 --baud 460800 write_flash 0x0 4wd-car-esp32cam-v1.0-factory.bin` |
| macOS | `python3 -m pip install --user esptool` | `python3 -m esptool --chip esp32 --port /dev/cu.usbserial-XXXX --baud 460800 write_flash 0x0 4wd-car-esp32cam-v1.0-factory.bin` |
| Linux | `python3 -m pip install --user esptool` | `python3 -m esptool --chip esp32 --port /dev/ttyUSB0 --baud 460800 write_flash 0x0 4wd-car-esp32cam-v1.0-factory.bin` |

- Replace the port with the one from chapter 1.
- `factory.bin` = bootloader + partition table + program in one file (at `0x0`). `app.bin` is only for a board that already runs this firmware (`0x10000`).
- To forget the saved hotspot and Bluetooth device and start fresh, run `… --port <port> erase_flash` before flashing.

---

## 3. Method B: build from source (PlatformIO)

### B-1. Install

**With VS Code (same on all three)**: VS Code → Extensions → **PlatformIO IDE** → open the repository's **`esp32/esp32cam_line` folder** (not the repository root). Status bar: ✓ (Build), → (Upload), plug (Serial Monitor).

**From the command line**:

| | Install | Command prefix |
|---|---|---|
| Windows (PowerShell) | install Python 3 from [python.org](https://www.python.org/) (tick Add to PATH) → `py -m pip install platformio` | `py -m platformio` |
| macOS | `python3 -m venv ~/.pio-venv && ~/.pio-venv/bin/pip install platformio` | `~/.pio-venv/bin/pio` |
| Linux | `python3 -m venv ~/.pio-venv && ~/.pio-venv/bin/pip install platformio` + [PlatformIO udev rules](https://docs.platformio.org/en/latest/core/installation/udev-rules.html) + dialout group (chapter 1) | `~/.pio-venv/bin/pio` |

### B-2. Build and upload

```bash
cd esp32/esp32cam_line
pio run -t upload                        # build + upload (port auto-detected)
pio run -t upload --upload-port COM5     # choose the port (macOS /dev/cu.usbserial-XXXX, Linux /dev/ttyUSB0)
pio device monitor                       # serial monitor (9600, quit with Ctrl+C)
```

(Use the command prefix from the table instead of `pio`. On Windows: `cd esp32\esp32cam_line`.)

- **The first build is slow**: it rebuilds the ESP32 libraries with the `platformio.ini` settings, downloading ~2 GB and taking 20–40 min. After that about a minute.
- If the first build stops part way (`IncompatiblePlatform … v6.2.0-post.1`, missing `.sconsign…tmp`, etc.), **run the same command again**; it continues and finishes.
- Changing `custom_sdkconfig` in `platformio.ini` rebuilds the libraries (a few minutes).
- On Windows, keep the repository in a **short path without spaces or non-ASCII characters** (e.g. `C:\work\4wd_car_project`); the build tools can fail on long or non-ASCII paths.

---

## 4. After flashing

The serial monitor (9600) prints status lines every 10 s:

| Line | Meaning |
|---|---|
| `I,setup,GULNO-CAM-SETUP-xxxx,<password>` | First time (setup mode): join this Wi-Fi with a phone and enter your hotspot name and password ([`CAMERA.md`](CAMERA.md) ch. 5, Korean) |
| `I,ip,172.20.10.x,gulno-cam.local` | Joined the hotspot → open `http://gulno-cam.local` |
| `I,time,…` / `I,heap,…` | processing time, free memory |
| `E,camera` | camera not found → reseat the camera ribbon cable and power-cycle |

- On macOS, opening the serial monitor restarts the ESP32 once (normal).
- Front white LED: blinks while connecting, stays on once hotspot and Bluetooth are connected.

## 5. Common problems

| Symptom | Fix |
|---|---|
| No port | use a data cable, install the CH340 driver on Windows, plug directly into the computer (not a hub) |
| `Failed to connect` / `Wrong boot mode` | hold IO0, press RST, release, retry. Close anything else holding the port (serial monitor, browser tab) |
| Linux `Permission denied: /dev/ttyUSB0` | `sudo usermod -aG dialout $USER`, log in again |
| Uploaded, but freezes or restarts when Bluetooth turns on | it was uploaded from the Arduino IDE → reflash with method A or B |
| Restarts with `Brownout detector` | not enough power → 5 V 2 A or more, short cable, separate from the car battery |
| First build stops | run the same command again (chapter 3) |
