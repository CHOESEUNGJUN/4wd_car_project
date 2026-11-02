# 미니카 2팀 조종 앱

차량 스케치는 `team2_bt_mode/team2_bt_mode.ino` 하나로 HC-06과 HM-10 둘 다 됩니다(같은 3·4번 핀, 9600).

| 폰/PC | 모듈 | 앱 | 실행 |
|---|---|---|---|
| 안드로이드 | HC-06 | `android/` | Android Studio로 폴더 열기 → 폰 USB 연결 → Run. 폰 설정에서 HC-06 먼저 페어링(1234) |
| 아이폰 | HM-10 | `ios_mac/` | Xcode로 `MiniCar.xcodeproj` 열기 → Signing에서 내 Apple ID 팀 선택 → 아이폰 연결 → Run |
| 맥 | HM-10 | `ios_mac/` | 같은 프로젝트에서 실행 대상을 My Mac으로 → Run |
| 맥 | HC-06 | `web/index.html` | 맥에서 HC-06 페어링 → 크롬으로 파일 열기 → 시리얼 연결 |
| 안드로이드·맥 | HM-10 | `web/index.html` | 크롬에서 BLE 연결 |
| 아이폰 | HM-10 | `web/index.html` | 사파리는 안 됨. 무료 앱 Bluefy로 열기(https 주소 필요) |

공통 동작(교안 2팀 STEP 4~6과 같음)
- 버튼을 누르는 동안 200ms마다 같은 명령, 떼면 `s`
- 모드 버튼은 `a`/`m`, 모드 표시는 차가 돌려주는 `a`/`m`으로 바뀜
- 연결 직후 `m`을 보내 수동으로 맞춤
- 앱이 뒤로 가면 정지(`s`)

HM-10 클론 중에는 UUID가 FFE0/FFE1이 아닌 것이 있습니다. 연결 후 "서비스가 없습니다"가 뜨면 LightBlue 앱으로 UUID를 확인해서 `Car.swift`, `index.html`의 값을 바꿉니다.
