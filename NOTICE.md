# NOTICE : 저작권과 라이선스 / Copyright and licenses

> 4wd-car — Team 2 "Gulno" (굴노 · 굴러라 노란바퀴), 2026. **오픈 소스로 공개 / Released as open source.**

## 1. 우리가 만든 것 / Our work

> `LICENSE` 는 MIT 원문만 둡니다 (GitHub 이 라이선스를 알아보게). MIT 는 소스 코드에 적용되고, 사진·영상·그림·로그·문서는 아래처럼 CC BY 4.0 입니다.
> `LICENSE` holds only the MIT text (so GitHub recognises it). MIT applies to the source code; photos, videos, figures, logs and documentation are CC BY 4.0 as below.

| 무엇 / What | 라이선스 / License |
|---|---|
| 소스 코드 : Arduino 스케치 (`arduino/`), ESP32-CAM 펌웨어·웹 화면 (`esp32/`), 앱 (`apps/`), MATLAB·Simulink 스크립트 (`docs/matlab/`), 시험·도구 / Source code: Arduino sketches, ESP32-CAM firmware and web page, apps, MATLAB · Simulink scripts, tests and tools | **MIT** ([`LICENSE`](LICENSE)) |
| 사진·영상·그림(MATLAB·Simulink 로 그린 그래프·스코프 화면 포함)·주행 로그·문서 (`docs/`, README, 릴리스의 사진·영상) / Photos, videos, figures (including graphs and scope screenshots made with MATLAB · Simulink), drive logs, documentation | **CC BY 4.0** — 출처 표시 : "4wd-car Team 2 Gulno" / Attribution: "4wd-car Team 2 Gulno" |

- 누구나 자유롭게 쓰고, 고치고, 다시 배포하고, 상업적으로도 쓸 수 있습니다. 코드는 저작권·허가 문구(`LICENSE`)를, 사진·문서는 출처를 남겨 주세요.
- Anyone may use, modify, redistribute, and use commercially. Keep the copyright and permission notice (`LICENSE`) for code, and give attribution for photos and documents.
- 소프트웨어와 문서는 "있는 그대로" 제공됩니다. 차·배터리·전원을 다룰 때의 안전은 사용자 책임입니다. / Provided "as is"; you are responsible for safety when handling the car, batteries and power.

## 2. 우리 것이 아닌 것 / Third-party components

저장소에 **들어 있지 않고** 빌드할 때 받아 쓰는 것(표에 따로 적은 것 빼고), 또는 릴리스 펌웨어(`.bin`)·앱 안에 함께 들어가는 것. 각자의 라이선스를 따릅니다.
Not stored in this repository (downloaded at build time) unless noted, or linked into the release firmware (`.bin`) and apps. Each keeps its own license.

| 구성 요소 / Component | 쓰는 곳 / Used in | 라이선스 / License |
|---|---|---|
| Arduino AVR core, `Servo`, `SoftwareSerial` | 차 스케치 (UNO) / car sketches | LGPL-2.1 |
| Arduino core for ESP32 (pioarduino 빌드), `WiFi`, `BluetoothSerial`, `SD_MMC`, `Preferences`, `ESPmDNS`, `DNSServer` | ESP32-CAM 펌웨어 / firmware | LGPL-2.1 |
| `huge_app.csv` (파티션 표, arduino-esp32 에서 가져옴 / partition table from arduino-esp32) | 저장소에 포함 / included in this repo | LGPL-2.1 |
| ESP-IDF (Wi-Fi, Bluetooth, FreeRTOS, lwIP, `esp_http_server`) | ESP32-CAM 펌웨어 / firmware | Apache-2.0 (일부 MIT·BSD / parts MIT, BSD) |
| `esp32-camera` (카메라 드라이버, `fmt2jpg` JPEG 압축) | ESP32-CAM 펌웨어 / firmware | Apache-2.0 |
| PlatformIO, platform-espressif32 (pioarduino) | 빌드 도구 / build tools (배포 안 함 / not distributed) | Apache-2.0 |
| esptool / esptool-js | 올리기 도구 / flashing tools (배포 안 함 / not distributed) | GPL-2.0 / Apache-2.0 |
| Gradle wrapper (`apps/android/gradlew`, `gradlew.bat`) | 안드로이드 빌드 / Android build — 저장소에 포함 / included | Apache-2.0 |
| AndroidX, Kotlin standard library | 안드로이드 앱 / Android app | Apache-2.0 |
| SwiftUI, CoreBluetooth (Apple SDK) | 아이폰·맥 앱 / iOS and macOS app | Apple SDK license |
| Web Bluetooth / Web Serial (브라우저 기능 / browser APIs) | `apps/web/index.html` | — |
| MATLAB, Simulink (MathWorks) | `docs/matlab/` 스크립트를 돌리고 문서 그래프를 그리는 데 씀. 저장소·릴리스에 들어 있지 않음, 돌리려면 각자의 MATLAB·Simulink 라이선스 필요 / used to run `docs/matlab/` and draw the document graphs; not included, running them needs your own MATLAB · Simulink license | MathWorks 상용 라이선스 / commercial |

- **LGPL 부분 / LGPL parts** : 릴리스 펌웨어(`4wd-car-esp32cam-v1.0-*.bin`)에는 LGPL-2.1 인 arduino-esp32 가 링크되어 있습니다. 이 저장소에 펌웨어 소스와 빌드 설정(`esp32/esp32cam_line/platformio.ini`)이 모두 있어서, 누구나 그 라이브러리를 바꿔 다시 빌드·링크할 수 있습니다 ([`docs/ESP32_UPLOAD.md`](docs/ESP32_UPLOAD.md) 방법 B). 라이브러리 소스 : https://github.com/espressif/arduino-esp32 , https://github.com/pioarduino/platform-espressif32
  The release firmware links arduino-esp32 (LGPL-2.1). This repository contains the complete firmware source and build configuration, so anyone can modify those libraries and rebuild/relink (see [`docs/ESP32_UPLOAD.en.md`](docs/ESP32_UPLOAD.en.md), method B). Library sources at the links above.
- 라이선스 전문은 각 프로젝트에서 확인하세요 / Full license texts are in each project.

## 3. 교안과 상표 / Lecture deck and trademarks

- **수업 교안** `minicar_lecture_deck_all_teams_v5.pdf` 는 수업에서 받은 자료로 저작권이 작성자에게 있어 **이 저장소에 넣지 않습니다** (2026-11-18 저장소에서 뺌, 예전 커밋 기록에는 남아 있음). 문서에서 "교안 n쪽" 은 그 자료를 가리킵니다.
  The **course lecture deck** belongs to its authors and is **not distributed here** (removed from the repository on 2026-11-18; it remains in older commits). "Lecture deck p. n" in the docs refers to it.
- Arduino, Espressif, ESP32, Eduino, Apple, iPhone, Android, Google Chrome, Bluetooth, MATLAB, Simulink, MathWorks 등은 각 회사의 상표입니다. 이 프로젝트는 그 회사들과 관계가 없습니다.
  Arduino, Espressif, ESP32, Eduino, Apple, iPhone, Android, Google Chrome, Bluetooth, MATLAB, Simulink, MathWorks and other names are trademarks of their owners. This project is not affiliated with them.
