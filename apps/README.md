# 미니카 2팀 조종 앱 : 굴노

앱 이름은 **굴노** (안드로이드, 아이폰·맥, 웹 제목 모두). 아이콘은 차 바퀴 사진을 오려서 만들었다 — 원본은 `icon/gulno-1024.png`, 오린 바퀴는 `icon/wheel-cutout.png`.

전체 동작, 명령표, 상태 줄 형식은 [저장소 README](../README.md) 참고.
차량 스케치는 버전 C(`team2_bt_pid`) 또는 버전 D(`team2_bt_cam`)를 쓴다 (HC-06, HM-10 모두 3·4번 핀, 9600).

| 폰/PC | 모듈 | 앱 | 실행 |
|---|---|---|---|
| 안드로이드 | HC-06 | `android/` | `dist/gulno.apk` 설치, 또는 Android Studio 로 `android/` 열고 ▶ |
| 아이폰 | HM-10 | `ios_mac/` | Xcode 로 `MiniCar.xcodeproj` → Signing 에서 내 Apple ID 팀 → 아이폰 연결 → ▶ |
| 맥 | HM-10 | `ios_mac/` | 같은 프로젝트, 실행 대상 My Mac → ▶ |
| 맥 | HC-06 | `web/index.html` | 맥에서 HC-06 페어링 → 크롬으로 열기 → 시리얼 연결 |
| 안드로이드·맥 | HM-10 | `web/index.html` | 크롬에서 BLE 연결 |
| 아이폰 | HM-10 | `web/index.html` | 사파리는 안 됨. 무료 앱 Bluefy 로 열기 (https 주소 필요) |

## 연결

- **안드로이드**: 기기 찾기 → 주변 기기 목록(페어링 안 된 것도). `HC-` 기기는 ★ 로 맨 위, 신호 세기 표시. 누르면 연결, 처음이면 PIN 1234.
- **아이폰·맥**: 기기 찾기 → HM-10 이름(`HMSoft`, `BT05` 등) 누르기.
- **웹**: BLE 연결 또는 시리얼 연결 버튼 → 브라우저 목록에서 고르기.

## 세 앱 공통 동작 (교안 2팀 STEP 4~6)

- 버튼을 누르는 동안 0.2초마다 같은 명령, 떼면 `s` 두 번
- 모드 버튼은 `a` / `m`. 모드·동작 표시는 앱이 짐작하지 않고 차가 보내는 상태 줄로만 바꿈
- 0.6초마다 연결 신호 `h`
- 연결 직후 `m` 을 보내 수동으로 맞춤
- 앱이 뒤로 가면 누르던 버튼을 놓은 것으로 처리 (정지)
- 화면: 모드 · 동작, 단계(라인 추종 / 코너 회전 / 흔들며 찾기 / 회피 n단계 …), 앞 거리, 라인센서 ○●○

## 신호등 화면 (아이폰·맥 앱, 이 브랜치)

화면 아래 **"신호등 화면"** 버튼 → 화면 전체가 초록 → 노랑 → 빨강 (순수 #00ff00 / #ffff00 / #ff0000). 누르면 다음 색, `자동 n초` 로 저절로 (0 = 끔), 밝기 최대·화면 안 꺼짐. 차 연결 없이 된다.
ESP32-CAM 신호등 인식 시험용 : 카메라 앞 위쪽에 들고 있으면 빨강 정지, 초록 주행, 노랑 서행 (`docs/CAMERA.md` 2장). 코드 `ios_mac/MiniCar/LightView.swift`.

## 참고

- HM-10 클론 중에는 UUID 가 FFE0/FFE1 이 아닌 것이 있다. 연결 후 "HM-10(FFE0) 서비스가 없습니다" 가 뜨면 LightBlue 앱으로 UUID 를 확인해서 `ios_mac/MiniCar/Car.swift`, `web/index.html` 의 값을 바꾼다.
- 시뮬레이터·에뮬레이터에는 블루투스가 없어서 화면만 확인된다.
- 안드로이드 APK 다시 만들기: `cd android && ./gradlew assembleDebug` → `app/build/outputs/apk/debug/app-debug.apk`
