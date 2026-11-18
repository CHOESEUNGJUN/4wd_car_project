# ESP32-CAM 펌웨어 올리기 (윈도우 · 리눅스 · 맥)

> English : [ESP32_UPLOAD.en.md](ESP32_UPLOAD.en.md)

차(Arduino UNO)는 Arduino IDE 로 올리지만, **ESP32-CAM 은 Arduino IDE 로 올리면 안 된다.** 이 문서는 ESP32-CAM 만 다룬다.

| 방법 | 언제 | 필요한 것 |
|---|---|---|
| **A. 완성 펌웨어 올리기** (가장 쉬움) | 코드를 안 고칠 때 | 릴리스의 `4wd-car-esp32cam-v1.0-factory.bin` + 크롬 (또는 esptool) |
| **B. 소스에서 빌드해서 올리기** | 코드를 고칠 때 | PlatformIO (VS Code 확장 또는 명령줄) |

---

## 0. 왜 Arduino IDE 로 올리면 안 되나

- Arduino IDE 의 ESP32 보드 패키지는 **미리 만들어 둔 ESP32 라이브러리**를 쓴다. 그 라이브러리는 블루투스·와이파이 메모리를 **내부 메모리(약 320KB)** 에 잡는다.
- 우리 코드는 **카메라 + 와이파이 + 블루투스(HC-06 연결)** 를 동시에 켠다. 미리 만든 라이브러리로는 내부 메모리가 모자라서 **업로드도 되고 처음엔 돌지만, 블루투스를 켜는 순간 멈추거나 재시작**한다 (2026-11-11 시험).
- 그래서 `esp32/esp32cam_line/platformio.ini` 의 `custom_sdkconfig` 로 **ESP32 라이브러리를 직접 다시 빌드**한다 :
  - 블루투스·와이파이 메모리를 외부 메모리(PSRAM 4MB)에 먼저 잡기
  - BLE, 블루투스 오디오(A2DP·HFP), GATT 끄기 (우리는 HC-06 시리얼만 씀)
  - 블루투스 작업 스택 줄이기, 와이파이 버퍼 줄이기
  - 결과 : HC-06 연결 중에도 내부 메모리 33~36KB 남음 (그 전에는 9~20KB, 멈춤)
- Arduino IDE 는 이 설정(라이브러리 다시 빌드)을 쓸 수 없다. **PlatformIO 로 빌드하거나, 그렇게 빌드해 둔 완성 펌웨어(`.bin`)를 올린다.**

---

## 1. 공통 준비 : 보드 연결

1. ESP32-CAM 을 **ESP32-CAM-MB** (마이크로 USB 받침 보드)에 꽂는다.
2. **데이터 되는** USB 케이블로 컴퓨터에 연결 (충전 전용 케이블이면 포트가 안 보임).
3. 포트 확인 (MB 보드의 USB 칩은 보통 **CH340**) :

| | 포트 이름 | 드라이버 |
|---|---|---|
| 윈도우 | `COM3`, `COM5` … (장치 관리자 → 포트(COM & LPT) → "USB-SERIAL CH340") | 안 보이면 WCH 의 **CH340 드라이버** 설치 |
| 맥 | `/dev/cu.usbserial-XXXX` 또는 `/dev/cu.wchusbserialXXXX` (`ls /dev/cu.*`) | 요즘 macOS 는 기본 포함 |
| 리눅스 | `/dev/ttyUSB0` (`ls /dev/ttyUSB*`) | 기본 포함 (ch341). 권한 : `sudo usermod -aG dialout $USER` 후 **다시 로그인** |

4. 올리기 모드 : MB 보드가 **자동**으로 들어간다. "Failed to connect" 가 나오면 **IO0 버튼을 누른 채 RST 를 한 번 눌렀다 떼고**, 다시 올리기.

---

## 2. 방법 A : 완성 펌웨어 올리기 (빌드 없이)

[릴리스 4wd-car ESP32 Car Control V1.0](https://github.com/CHOESEUNGJUN/4wd_car_project/releases/tag/4wd-car-esp32-car-control-v1.0) 에서 **`4wd-car-esp32cam-v1.0-factory.bin`** 을 받는다. 주소는 **`0x0`**.

### A-1. 웹 브라우저로 (설치 없음, 윈도우·맥·리눅스 모두)

1. **크롬 또는 엣지**로 [Espressif ESP Tool (esptool-js)](https://espressif.github.io/esptool-js/) 를 연다.
2. Baudrate 460800 → **Connect** → 목록에서 ESP32-CAM 포트 선택.
3. Flash Address **`0x0`**, 파일 `4wd-car-esp32cam-v1.0-factory.bin` → **Program**.
4. 끝나면 MB 보드의 **RST** 를 누른다.

### A-2. esptool 명령으로

| | 설치 | 올리기 |
|---|---|---|
| 윈도우 (PowerShell) | `py -m pip install esptool` | `py -m esptool --chip esp32 --port COM5 --baud 460800 write_flash 0x0 4wd-car-esp32cam-v1.0-factory.bin` |
| 맥 | `python3 -m pip install --user esptool` | `python3 -m esptool --chip esp32 --port /dev/cu.usbserial-XXXX --baud 460800 write_flash 0x0 4wd-car-esp32cam-v1.0-factory.bin` |
| 리눅스 | `python3 -m pip install --user esptool` | `python3 -m esptool --chip esp32 --port /dev/ttyUSB0 --baud 460800 write_flash 0x0 4wd-car-esp32cam-v1.0-factory.bin` |

- 포트 이름은 1장에서 확인한 것으로 바꾼다.
- `factory.bin` = 부트로더 + 파티션 + 프로그램을 합친 것 (`0x0` 에 한 번에). `app.bin` 은 이미 이 펌웨어가 들어 있는 보드를 고칠 때만 (`0x10000`).
- 저장된 핫스팟·블루투스 기기를 지우고 처음부터 하려면 올리기 전에 `… --port <포트> erase_flash`.

---

## 3. 방법 B : 소스에서 빌드해서 올리기 (PlatformIO)

### B-1. 설치

**VS Code 를 쓰면 (세 OS 같음)** : VS Code → 확장 → **PlatformIO IDE** 설치 → 저장소의 **`esp32/esp32cam_line` 폴더**를 연다 (저장소 맨 위 폴더가 아님). 아래 상태 막대의 ✓ (Build), → (Upload), 플러그 (Serial Monitor).

**명령줄로 쓰면** :

| | 설치 | 명령 앞에 붙는 것 |
|---|---|---|
| 윈도우 (PowerShell) | [python.org](https://www.python.org/) 에서 Python 3 설치 (Add to PATH 체크) → `py -m pip install platformio` | `py -m platformio` |
| 맥 | `python3 -m venv ~/.pio-venv && ~/.pio-venv/bin/pip install platformio` | `~/.pio-venv/bin/pio` |
| 리눅스 | `python3 -m venv ~/.pio-venv && ~/.pio-venv/bin/pip install platformio` + [PlatformIO udev 규칙](https://docs.platformio.org/en/latest/core/installation/udev-rules.html) + dialout 그룹 (1장) | `~/.pio-venv/bin/pio` |

### B-2. 빌드하고 올리기

```bash
cd esp32/esp32cam_line
pio run -t upload                        # 빌드 + 올리기 (포트 자동)
pio run -t upload --upload-port COM5     # 포트 지정 (맥 /dev/cu.usbserial-XXXX, 리눅스 /dev/ttyUSB0)
pio device monitor                       # 시리얼 보기 (9600, 나가기 Ctrl+C)
```

(`pio` 대신 위 표의 "명령 앞에 붙는 것" 을 쓴다. 윈도우는 `cd esp32\esp32cam_line`)

- **처음 빌드는 오래 걸린다** : ESP32 라이브러리를 `platformio.ini` 설정대로 다시 만드느라 약 2GB 를 받고 20~40분. 그다음부터는 1분 안팎.
- 처음 빌드가 중간에 멈추면 (`IncompatiblePlatform … v6.2.0-post.1`, `.sconsign…tmp` 없음 등) **같은 명령을 한 번 더** 실행하면 이어서 끝난다.
- `platformio.ini` 의 `custom_sdkconfig` 를 고치면 라이브러리를 다시 만든다 (몇 분).
- 윈도우는 저장소를 **한글·공백이 없는 짧은 경로** (예 `C:\work\4wd_car_project`) 에 두는 게 안전하다 (빌드 도구가 긴 경로·한글 경로에서 실패하는 일이 있음).

---

## 4. 올린 뒤 확인

시리얼 모니터(9600)에 10초마다 안내 줄이 나온다 :

| 줄 | 뜻 |
|---|---|
| `I,setup,GULNO-CAM-SETUP-xxxx,<암호>` | 처음(설정 모드) : 폰으로 이 와이파이에 붙어 핫스팟 이름·암호를 넣는다 ([`CAMERA.md`](CAMERA.md) 5장) |
| `I,ip,172.20.10.x,gulno-cam.local` | 핫스팟에 붙음 → 브라우저로 `http://gulno-cam.local` |
| `I,time,…` / `I,heap,…` | 처리 시간, 남은 메모리 |
| `E,camera` | 카메라를 못 찾음 → 카메라 리본 케이블 다시 꽂고 전원을 껐다 켬 |

- 맥에서 시리얼 모니터를 열면 ESP32 가 한 번 재시작한다 (정상).
- 앞 흰 LED : 연결 중 깜빡, 핫스팟·블루투스까지 다 붙으면 계속 켜짐.

## 5. 자주 막히는 곳

| 증상 | 해결 |
|---|---|
| 포트가 안 보임 | 데이터 되는 케이블로 바꾸기, 윈도우는 CH340 드라이버, 허브 말고 컴퓨터에 바로 |
| `Failed to connect` / `Wrong boot mode` | IO0 누른 채 RST → 떼고 다시. 다른 프로그램(시리얼 모니터, 다른 탭)이 포트를 잡고 있으면 닫기 |
| 리눅스 `Permission denied: /dev/ttyUSB0` | `sudo usermod -aG dialout $USER` 후 다시 로그인 |
| 올렸는데 블루투스를 켜면 멈춤·재시작 | Arduino IDE 로 올린 것 → 이 문서의 방법 A 또는 B 로 다시 |
| `Brownout detector` 로 재시작 | 전원 부족 → 5V 2A 이상, 짧은 케이블. 차 배터리와 따로 |
| 처음 빌드가 멈춤 | 같은 명령 한 번 더 (3장) |
