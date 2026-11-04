/* 2팀 블루투스 수동/자동 + 검은 선 라인트레이서 (버전 B)
 *  - 자동 모드 주행 : GitHub 4wd-rc-car-line-tracer 의 RC_CAR_BLACK_LINE (v1.1) 그대로
 *    밝은 바탕 + 검은 선. 핀도 그 저장소 기준 (라인센서 왼쪽 A3, 가운데 A4, 오른쪽 A5)
 *  - 블루투스/모드 전환/안전장치/상태 줄 : team2_bt_mode 와 같음 (앱 3종 그대로 사용)
 *  - 바뀐 점 : delay 대신 pause 를 써서 기다리는 동안에도 블루투스를 확인함.
 *    자동 중에 수동 명령이 오면 회피, 흔들기 같은 긴 동작도 바로 그만둠.
 *  - 회전 시간 자동 측정은 켤 때가 아니라 처음 자동 모드로 바꿀 때 한 번 함 (선 위에 올려놓고 자동 누르기)
 *  고칠 때는 아래 [설정] 숫자만 바꾸면 됨. 시간은 전부 ms (1000 = 1초).
 */

#include <SoftwareSerial.h>
#include <Servo.h>

// ============================================================
// [설정]
// ============================================================

// --- 속도 (0~255) ---
int SPEED       = 150;     // 앞으로 갈 때 : 직진, 살짝 보정, 회피 중 직진
int TURN_SPEED  = 170;     // 제자리에서 돌 때 : 코너 회전, 좌우 흔들기, 회피 회전, 출발 때 자동 측정
int CORNER_MAX_MS    = 800;   // 코너에서 최대 이만큼만 돎 (못 찾으면 좌우로 흔들며 찾기로 넘어감)
int LEFT_CORNER_ONLY = 1;     // 1 = 트랙 코너가 전부 왼쪽 : 오른쪽 센서만 보면 회전 대신 살짝 보정, 선 놓치면 항상 왼쪽부터 흔들기. 0 = 좌우 코너 다 있음

// --- 회피 회전 시간 (ms) ※ 차마다 다름. TURN_TEST_BLACK_LINE.ino 로 재서 맞추기 ---
int TURN_RIGHT_90_MS = 550;   // TURN_SPEED 170 기준으로 잰 값. 자동 측정되면 그 값으로 바뀜
int TURN_LEFT_90_MS  = 900;
int TURN_RIGHT_45_MS = 500;
int TURN_LEFT_45_MS  = 500;

// --- 자동 회전 측정 (처음 자동 모드로 바꿀 때, 차를 선과 나란히 올려놓고) ---
int AUTO_CAL       = 1;      // 1 = 자동 측정, 0 = 위 값 그대로
int CAL_TIMEOUT_MS = 5000;   // 이 시간 안에 선을 못 만나면 측정 포기

// --- 회피 중 장애물과 거리 유지 (옆으로 나가기, 지나가기 구간) ---
int KEEP_TOL_MM  = 30;       // 처음 거리에서 이만큼 넘게 달라지면 고침
int KEEP_TURN_MS = 60;       // 한 번 고칠 때 도는 시간 (TURN_SPEED 170 기준)

// --- 회피 직진 시간 (ms) ---
int PASS_STEP_MS  = 240;      // 지나갈 때 한 번에 전진하는 시간 (전진 -> 정지 -> 옆 확인 반복)
int PASS_EXTRA_MS = 600;      // 장애물이 안 보이게 된 뒤 더 가는 시간 (차 뒷부분까지 빠지게)
int PASS_MAX_MS   = 3600;     // 한 구간 최대 시간 (끝없이 가는 것 방지)
int RETURN_MAX_MS = 2800;     // 45도 꺾고 선 찾으며 직진하는 최대 시간
int LINE_OVER_MS  = 480;      // 선 발견 후 가운데 센서가 선을 넘어갈 때까지 더 가는 최대 시간
int LINE_TURN_MAX_MS = 700;   // 선 위에서 원래 방향으로 돌 때 최대 시간 (가운데 센서가 선을 보면 바로 멈춤)
int TURN_PAUSE_MS = 100;      // 돌고 나서 멈춰 있는 시간
int SETTLE_MS     = 150;      // 전진 후 멈춰서 차가 완전히 설 때까지 기다리는 시간

// --- 장애물 크기에 따라 달라지는 거리 (mm) -> MM_PER_SEC 로 시간으로 바꿈 ---
int MM_PER_SEC     = 268;     // SPEED 로 1초에 가는 거리 (자로 재서 맞추기)
int SIDE_MARGIN_MM = 100;     // 옆으로 나갈 때 : 잰 장애물 폭 + 이 값만큼은 무조건 직진
int LEG2_MIN_MM    = 100;     // 지나가기 : 최소 (장애물까지 거리 + 이 값)은 가고 나서 판단

// --- 장애물 감지 / 초음파 ---
int AVOID_MM      = 100;      // 이 거리 안에 장애물 -> 멈춤 (아래 OBSTACLE_MODE 에 따라 기다리거나 피함). 수동 모드에서는 전진만 막음

// --- 장애물을 만났을 때 (자동 모드) ---
int OBSTACLE_MODE = 0;        // 0 = 멈춰서 기다리다가 치우면 다시 출발,  1 = 돌아서 피하기 (회피 1~8단계)
int CLEAR_COUNT   = 5;        // 연속으로 이만큼 장애물이 안 보여야 치운 걸로 봄 (잡음 방지, 60ms 간격)
int CLEAR_WAIT_MS = 500;      // 치운 걸 확인하고 출발하기 전 기다리는 시간 (손 뺄 시간)
int PING_MS       = 60;       // 초음파 쏘는 간격 (너무 자주 쏘면 메아리가 섞여 가짜 값)
int MIN_VALID_MM  = 20;       // 이보다 작으면 잡음으로 버림
long ECHO_WAIT_US = 12000;    // 메아리 기다리는 최대 시간 (약 2m)
int SIDE_SEEN_EXTRA_MM = 200; // 옆을 볼 때 (예상 거리 + 이 값) 안쪽이면 "보임"

// --- 장애물 폭 재기 (서보) ---
int SWEEP_MAX_DEG  = 70;      // 정면 기준 좌우 몇 도까지
int SWEEP_STEP_DEG = 10;      // 몇 도씩
int SERVO_STEP_MS  = 150;     // 한 칸 움직이고 기다리는 시간
int SERVO_BIG_MS   = 500;     // 크게 돌릴 때 기다리는 시간
int OBST_EXTRA_MM  = 150;     // (정면 거리 + 이 값) 안쪽이면 같은 장애물
int SIDE_MAX_MM    = 400;     // 장애물 끝이 안 보이면 이 값으로 침
int SIDE_LOOK_DEG  = 90;      // 지나갈 때 옆을 보는 각도
int SERVO_CENTER   = 90;      // 서보 정면
int SERVO_FLIP     = 1;       // 서보 좌우가 반대로 달렸으면 1

// --- 기타 ---
int AVOID_SIDE = 0;                   // 0 자동(짧은 쪽), 1 무조건 오른쪽, 2 무조건 왼쪽 (테스트용)
// --- 선을 놓쳤을 때 : 제자리에서 좌우로 머리를 흔들며 찾기 ---
int SWAY_START_MS = 180;            // 처음 흔드는 폭 (TURN_SPEED 170 기준)
int SWAY_GROW_MS  = 100;            // 한 번 흔들 때마다 늘어나는 폭
unsigned long SWAY_MAX_MS = 3000;   // 이 시간 동안 못 찾으면 정지 (선이 다시 보일 때까지 서 있음)
unsigned long LOST_GRACE_MS = 300;  // 선이 안 보여도 이 시간 동안은 하던 동작 유지. 이보다 길게 안 보일 때만 흔들기 시작
unsigned long LINE_GRACE_MS = 500;  // 양쪽 센서가 동시에 선을 봐도(1,?,1) 이 시간 동안은 하던 동작 유지. 그 뒤에도 계속이면 정지

// --- 블루투스 / 모드 ---
const unsigned long LINK_MS = 600;         // 수동: 이 시간 무명령이면 정지 (앱 반복 주기 200ms보다 충분히 크게)
const unsigned long SWITCH_HOLD_MS = 500;  // 자동 모드로 전환할 때 무조건 정지하는 안전 지연 (수동 전환은 즉시)
const unsigned long LINK_SHOW_MS = 1500;   // 앱은 0.6초마다 'h'를 보낸다

// ============================================================
// [핀]
// ============================================================
int RightMotor_E_pin = 5, LeftMotor_E_pin = 6;     // 모터 속도(PWM)
int RightMotor_1_pin = 8, RightMotor_2_pin = 9;    // 오른쪽 모터 방향
int LeftMotor_3_pin = 10, LeftMotor_4_pin = 11;    // 왼쪽 모터 방향
int L_Line = A3, C_Line = A4, R_Line = A5;         // 라인센서 실제 위치 : 왼쪽 A3, 가운데 A4, 오른쪽 A5 (검은 선 = 1, 흰 바탕 = 0)
int trigPin = 13, echoPin = 12, servoPin = 2;      // 초음파, 서보
#define BT_RXD 3                                   // 블루투스 모듈 TXD -> D3
#define BT_TXD 4                                   // 블루투스 모듈 RXD -> D4

// enum은 첫 함수보다 위에 둔다(아두이노 IDE가 함수 선언을 첫 함수 앞에 끼워 넣음).
enum Mode { MANUAL, AUTO };

// ============================================================
SoftwareSerial BT(BT_RXD, BT_TXD);
Servo EduServo;
int last_state = -1;               // 직전 라인 상태 (바뀔 때만 모터 제어)
bool lostStopped = false;          // 흔들어서 찾다가 못 찾고 멈춘 상태
unsigned long lost_since = 0;      // 선이 안 보이기 시작한 시각 (0 = 선 보는 중)
unsigned long both_since = 0;      // 양쪽 센서가 동시에 선을 보기 시작한 시각 (0 = 아님)
int lastSide = 0;                  // 마지막으로 선을 본 쪽 (-1 왼쪽, +1 오른쪽, 0 가운데)
unsigned long lastPing = 0;
int frontDist = 9999;              // 마지막으로 잰 거리(mm). 상태 줄과 수동 모드 장애물 판단에 씀
int obstLeftMM = 0, obstRightMM = 0;   // 잰 장애물 폭 (정면 기준 왼쪽 / 오른쪽)

Mode mode = MANUAL;
char cmd = 's';
char act = 's';                    // 앱에 보내는 지금 동작: g 직진, b 후진, l/r 좌/우, q/w 제자리, s 정지, o 장애물 정지
// 앱에 보내는 지금 단계 (자세한 디버그용)
// MAN 수동, HOLD 전환 대기, CAL 회전 측정, LINE 라인 추종, CORNER 코너 회전, NOLINE 선 놓침(잠깐 유지),
// SWAY 좌우로 흔들며 찾기, LOST 못 찾고 정지, OBST 장애물 대기, AV1~AV8 회피 단계
const char* stage = "MAN";
const char* const AV_STAGE[] = {"AV1", "AV2", "AV3", "AV4", "AV5", "AV6", "AV7", "AV8"};
bool calibrated = false;           // 회전 시간 자동 측정을 했는지
unsigned long holdUntil = 0, lastRx = 0, lastLink = 0, lastTx = 0, lastLog = 0;

void setup() {
  EduServo.attach(servoPin);
  EduServo.write(SERVO_CENTER);
  pinMode(echoPin, INPUT);
  pinMode(trigPin, OUTPUT);
  pinMode(RightMotor_E_pin, OUTPUT);
  pinMode(RightMotor_1_pin, OUTPUT);
  pinMode(RightMotor_2_pin, OUTPUT);
  pinMode(LeftMotor_3_pin, OUTPUT);
  pinMode(LeftMotor_4_pin, OUTPUT);
  pinMode(LeftMotor_E_pin, OUTPUT);
  Serial.begin(115200);  // 시리얼 모니터도 115200 으로 맞추기 (USB라서 빨라도 됨)
  BT.begin(9600);        // 블루투스 모듈은 9600 그대로 (HC-06 기본값, SoftwareSerial 은 빠르면 깨짐)
  stopCar();
}

void loop() {
  serviceBT();

  // 모드 전환 직후 안전 지연
  if (millis() < holdUntil) { stopCar(); stage = "HOLD"; return; }

  if (mode == AUTO) {
    if (!calibrated) { calibrated = true; calibrateTurns(); return; }   // 처음 자동일 때 한 번
    if (millis() - lastPing >= (unsigned long)PING_MS && checkObstacle(Ultrasonic())) return;
    followLine();                    // 라인센서는 쉬지 않고 계속 봄
    return;
  }

  // 수동: 끊기면 정지, 장애물 앞에서는 앞으로 가는 명령만 막음
  stage = "MAN";
  if (millis() - lastPing >= (unsigned long)PING_MS) Ultrasonic();
  if (millis() - lastRx > LINK_MS) cmd = 's';
  bool blocked = frontDist < AVOID_MM && (cmd == 'g' || cmd == 'l' || cmd == 'r');
  driveManual(blocked ? 's' : cmd);
  if (blocked) act = 'o';
}

// ============================================================
// 블루투스
// ============================================================
// 들어온 글자 처리 + 0.2초마다 상태 줄 송신 + 0.5초마다 시리얼 로그. 기다리는 동안에도 계속 불러 준다.
void serviceBT() {
  while (BT.available()) {
    char c = BT.read();
    // 명령표에 없는 글자(터미널 앱의 \r \n 등)는 무시
    if (c == 0 || !strchr("gblrsqwamh", c)) continue;
    lastLink = millis();
    // 'h'는 앱이 살아 있다는 신호일 뿐. lastRx(주행 명령 시각)는 건드리지 않는다.
    if (c == 'h') continue;
    Serial.print(F("수신: ")); Serial.println(c);
    lastRx = millis();
    if (c == 'a') setMode(AUTO);
    else if (c == 'm') setMode(MANUAL);
    else {
      if (mode == AUTO) setMode(MANUAL);  // 자동 중 조종 명령 = 사람이 개입
      cmd = c;
    }
  }
  unsigned long now = millis();
  // 폰에 상태 한 줄: 모드(a/m)+동작, 거리(cm), 라인센서 L C R, 단계. 예) "ag,23,010,LINE"
  if (now - lastTx >= 200) {
    lastTx = now;
    BT.print(mode == AUTO ? 'a' : 'm'); BT.print(act);
    BT.print(','); BT.print(frontDist >= 9999 ? 999 : frontDist / 10);
    BT.print(','); BT.print(digitalRead(L_Line)); BT.print(digitalRead(C_Line)); BT.print(digitalRead(R_Line));
    BT.print(','); BT.println(stage);
  }
  if (now - lastLog >= 200) {      // 115200 이라 0.2초마다 찍어도 주행에 지장 없음
    lastLog = now;
    bool linked = lastLink != 0 && now - lastLink < LINK_SHOW_MS;
    Serial.print(linked ? F("링크=O ") : F("링크=X "));
    Serial.print(mode == AUTO ? F("AUTO ") : F("MAN  "));
    Serial.print(F("단계=")); Serial.print(stage);
    Serial.print(F(" act=")); Serial.print(act);
    Serial.print(F(" 거리=")); Serial.print(frontDist);
    Serial.print(F("mm 라인=")); Serial.print(digitalRead(L_Line));
    Serial.print(digitalRead(C_Line)); Serial.println(digitalRead(R_Line));
  }
}

void setMode(Mode m) {
  mode = m;
  cmd = 's';
  stopCar();
  EduServo.write(SERVO_CENTER);      // 회피 중이었으면 서보를 정면으로
  // 자동으로 갈 때만 안전 지연 동안 무조건 정지. 수동으로 갈 때는 즉시 (멈추고 바로 조종 가능)
  holdUntil = (m == AUTO) ? millis() + SWITCH_HOLD_MS : 0;
  last_state = -1;                   // 자동으로 들어갈 때 라인 상태를 처음부터 다시 판단
  lostStopped = false;
  lost_since = 0;
  both_since = 0;
}

// 기다리는 동안 블루투스를 계속 확인. 수동으로 바뀌면 바로 돌아옴 (delay 대신 씀)
void pause(unsigned long ms) {
  unsigned long start = millis();
  while (millis() - start < ms) if (autoAborted()) return;
}

// 오래 걸리는 자동 동작 안에서 부름: 수동으로 바뀌었으면 true (그 동작을 바로 그만둔다)
bool autoAborted() {
  serviceBT();
  return mode != AUTO;
}

void driveManual(char c) {
  switch (c) {
    case 'g': driveLR( 150,  150); break;  // 전진
    case 'b': driveLR(-150, -150); break;  // 후진
    case 'l': driveLR(  60,  150); break;  // 좌회전(곡선)
    case 'r': driveLR( 150,   60); break;  // 우회전(곡선)
    case 'q': driveLR(-120,  120); break;  // 제자리 좌회전
    case 'w': driveLR( 120, -120); break;  // 제자리 우회전
    default:  stopCar(); return;           // 's' 포함
  }
  act = c;
}

// 왼쪽, 오른쪽 속도(-255~255, 음수는 후진)로 drive 호출
void driveLR(int left, int right) {
  drive(right >= 0 ? HIGH : LOW, left >= 0 ? HIGH : LOW,
        constrain(abs(right), 0, 255), constrain(abs(left), 0, 255));
}

// ============================================================
// 장애물 : 달리면서 정면만 확인 -> 보이면 멈추고 다시 재서 진짜면 회피
// ============================================================
// 장애물을 치울 때까지 멈춰서 기다림. CLEAR_COUNT 번 연속으로 장애물이 안 보이면 다시 출발
void waitUntilClear() {
  stopCar();
  act = 'o';
  stage = "OBST";
  EduServo.write(SERVO_CENTER);
  int clear = 0;
  while (clear < CLEAR_COUNT) {
    pause(PING_MS);
    if (mode != AUTO) return;
    if (Ultrasonic() >= AVOID_MM) clear++;
    else clear = 0;
  }
  pause(CLEAR_WAIT_MS);
}

bool checkObstacle(int d) {
  if (d >= AVOID_MM) return false;
  stopCar();
  pause(PING_MS);
  int d2 = Ultrasonic();
  if (d2 < AVOID_MM && mode == AUTO) {  // 다시 재도 가까우면 진짜 장애물
    if (OBSTACLE_MODE == 1) avoidMode(d2);   // 돌아서 피하기
    else                    waitUntilClear();  // 멈춰서 치울 때까지 기다리기
  }
  last_state = -1;                    // 라인 상태 처음부터 다시 판단
  lostStopped = false;
  return true;
}

// ============================================================
// 라인 따라가기 (검은 선 버전 : 센서 값 그대로 씀. 검은 선 = 1, 흰 바탕 = 0)
// ============================================================
void followLine() {
  int L = digitalRead(L_Line), C = digitalRead(C_Line), R = digitalRead(R_Line);

  if (L == 0 && C == 0 && R == 0) {   // 선 놓침 -> 잠깐은 하던 동작 유지, 계속 안 보이면 좌우로 흔들며 찾기
    if (lostStopped) { stage = "LOST"; return; }   // 이미 흔들어 봤는데 못 찾고 멈춘 상태
    stage = "NOLINE";
    if (lost_since == 0) lost_since = millis();
    if (millis() - lost_since < LOST_GRACE_MS) return;   // 센서 사이로 잠깐 빠진 것일 수 있음 -> 하던 동작 유지
    lost_since = 0;
    if (swaySearch()) { last_state = -1; return; }
    if (mode != AUTO) return;                  // 흔드는 중에 수동으로 바뀜
    stage = "LOST";
    lostStopped = true;
    last_state = -1;
    return;
  }
  lostStopped = false;
  lost_since = 0;
  stage = "LINE";

  // 1,?,1 : 양쪽 센서가 동시에 선을 봄 -> 바로 멈추지 않고 LINE_GRACE_MS 동안 하던 동작 유지
  if (L == 1 && R == 1) {
    if (both_since == 0) both_since = millis();
    if (millis() - both_since < LINE_GRACE_MS) return;
  }
  else both_since = 0;

  if (R == 1 && L == 0) lastSide = +1;       // 선이 오른쪽에 있었음
  if (L == 1 && R == 0) lastSide = -1;       // 선이 왼쪽에 있었음
  if (L == 0 && C == 1 && R == 0) lastSide = 0;

  int state = L * 4 + C * 2 + R;
  if (state == last_state) return;
  last_state = state;

  //   L  C  R      (끝 센서 하나만 보면 많이 벗어난 것 -> 크게, 가운데와 같이 보면 -> 살짝)
  if      (L == 0 && C == 1 && R == 0) goForward();             // 직진
  else if (L == 0 && C == 1 && R == 1) curveRight();            // 오른쪽 살짝 보정
  else if (L == 0 && C == 0 && R == 1) {                         // 오른쪽 센서만
    if (LEFT_CORNER_ONLY) curveRight();                           //   오른쪽 코너는 없음 -> 밀린 것, 살짝 보정만
    else                  cornerTurn(true);                       //   오른쪽 코너 : 가운데 센서가 선 볼 때까지 확실히 돌기
  }
  else if (L == 1 && C == 1 && R == 0) curveLeft();             // 왼쪽 살짝 보정
  else if (L == 1 && C == 0 && R == 0) cornerTurn(false);        // 왼쪽 코너 : 가운데 센서가 선 볼 때까지 확실히 돌기
  else if (L == 1 && R == 1)           stopCar();               // 정지
}

// ============================================================
// 회피 : 1 폭 재기 -> 2 돌기 -> 3 옆으로 나가기 -> 4 다시 돌기 -> 5 지나가기
//        -> 6 45도 꺾기 -> 7 직진하며 라인 찾기 -> 8 원래 방향으로 정렬 (README 그림 번호)
//   toRight = true : 오른쪽으로 피함 (장애물은 왼쪽)
//   수동으로 바뀌면 그 자리에서 그만둠
// ============================================================
void avoidMode(int d0) {
  bool toRight = true;
  int sideOff = 0;                 // 서보가 볼 방향 (장애물 쪽)
  int widthMM = 0;                 // 3번에서 옆으로 간 거리

  int step = 1;
  while (step != 0 && mode == AUTO) {
    stage = AV_STAGE[step - 1];
    switch (step) {
      case 1:  // 폭 재기 -> 짧은 쪽으로 방향 정하기. 아무것도 없으면 가짜 감지라 취소
        measureObstacle(d0);
        if (obstLeftMM == 0 && obstRightMM == 0) { step = 0; break; }
        toRight = (obstRightMM <= obstLeftMM);
        if (AVOID_SIDE == 1) toRight = true;
        if (AVOID_SIDE == 2) toRight = false;
        sideOff = toRight ? -SIDE_LOOK_DEG : SIDE_LOOK_DEG;
        step = 2;
        break;

      case 2:  // 빈 쪽으로 90도
        turn90(toRight);
        step = 3;
        break;

      case 3:  // 옆으로 나가기 : 잰 폭 + 여유만큼은 무조건, 옆에 보이면 더
        widthMM = msToMm(passObstacle(sideOff, (toRight ? obstRightMM : obstLeftMM) + SIDE_MARGIN_MM,
                                      d0 + SIDE_SEEN_EXTRA_MM));
        step = 4;
        break;

      case 4:  // 장애물 쪽으로 90도 (원래 진행 방향)
        turn90(!toRight);
        step = 5;
        break;

      case 5:  // 지나가기 : 옆을 보며 조금 전진 -> 정지 -> 확인, 안 보일 때까지
        passObstacle(sideOff, d0 + LEG2_MIN_MM, widthMM + SIDE_SEEN_EXTRA_MM);
        lookAt(0, SERVO_BIG_MS);
        step = 6;
        break;

      case 6:  // 선 쪽으로 45도
        turn45(!toRight);
        step = 7;
        break;

      case 7:  // 직진하며 라인 찾기. 못 찾으면 회피 끝 (라인 주행이 전진하며 찾음)
        step = findLine() ? 8 : 0;
        break;

      case 8:  // 원래 방향으로 돌아 가운데 센서가 선을 볼 때까지 -> 라인 주행
        spin(toRight, TURN_SPEED);
        runUntil(LINE_TURN_MAX_MS, C_Line);
        stopCar();
        step = 0;
        break;
    }
  }
  if (mode != AUTO) { stopCar(); EduServo.write(SERVO_CENTER); }
}

// 선 찾으며 직진. 찾으면 가운데 센서가 선을 넘어갈 때까지 더 가고 true
bool findLine() {
  if (!moveMs(RETURN_MAX_MS, true)) return false;
  unsigned long start = millis();
  bool centerSeen = false;
  goForward();
  while (millis() - start < (unsigned long)LINE_OVER_MS) {
    if (autoAborted()) break;
    if (readLine(C_Line) == 0) centerSeen = true;
    else if (centerSeen) break;    // 선 위였다가 벗어남 = 넘어감
  }
  stopCar();
  pause(SETTLE_MS);
  return mode == AUTO;
}

// 장애물을 옆에 두고 지나가기. 돌려주는 값 = 간 시간(ms)
//   minMM 만큼은 무조건 가고, 그 뒤 옆 거리가 seenMM 이상이면(안 보이면) 끝
int passObstacle(int sideOff, int minMM, int seenMM) {
  int minMs = mmToMs(minMM);
  int goneMs = 0;
  int ds = lookAt(sideOff, SERVO_BIG_MS);
  int keepMM = 0;                  // 처음 보인 옆 거리 (이 거리를 유지)
  while (mode == AUTO && goneMs < PASS_MAX_MS && !(ds >= seenMM && goneMs >= minMs)) {
    if (ds < seenMM) {             // 옆에 장애물 보임 -> 거리 유지
      if (keepMM == 0) keepMM = ds;
      else keepDistance(sideOff, ds, keepMM);
    }
    moveMs(PASS_STEP_MS, false);
    goneMs += PASS_STEP_MS;
    pause(SETTLE_MS);
    ds = lookAt(sideOff, SERVO_STEP_MS);
  }
  if (mode != AUTO) return goneMs;
  moveMs(PASS_EXTRA_MS, false);
  return goneMs + PASS_EXTRA_MS;
}

// 서보를 천천히 훑어서 장애물이 좌우로 얼마나 뻗어 있는지 저장
void measureObstacle(int d0) {
  obstLeftMM = obstRightMM = 0;
  int limit = d0 + OBST_EXTRA_MM;
  lookAt(-SWEEP_MAX_DEG, SERVO_BIG_MS);
  for (int off = -SWEEP_MAX_DEG; off <= SWEEP_MAX_DEG && mode == AUTO; off += SWEEP_STEP_DEG) {
    int d = lookAt(off, SERVO_STEP_MS);
    if (d >= limit) continue;
    int side = d * sin(radians(abs(off)));             // 옆으로 뻗은 길이
    if (abs(off) >= SWEEP_MAX_DEG || side > SIDE_MAX_MM) side = SIDE_MAX_MM;
    if (off < 0) obstLeftMM  = max(obstLeftMM, side);
    else         obstRightMM = max(obstRightMM, side);
  }
  lookAt(0, SERVO_BIG_MS);
}

// 서보를 정면 기준 offset 도 (- 왼쪽, + 오른쪽)로 돌리고 거리(mm) 재기. 항상 멈춘 상태에서 잼
int lookAt(int offset, int waitMs) {
  stopCar();
  EduServo.write(SERVO_CENTER + (SERVO_FLIP ? -offset : offset));
  pause(waitMs);
  return Ultrasonic();
}

int Ultrasonic() {                 // 정면 거리(mm). 없으면 9999
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  long duration = pulseIn(echoPin, HIGH, ECHO_WAIT_US);
  lastPing = millis();
  int mm = duration * 340L / 2000;
  frontDist = (duration == 0 || mm < MIN_VALID_MM) ? 9999 : mm;
  return frontDist;
}

// ============================================================
// 동작
// ============================================================
// 회피 중 선 찾기용 : 센서는 검은색에서 1 -> 뒤집어서 "선 = 0" 으로 맞춤 (runUntil, findLine 에서 씀)
int readLine(int pin) { return !digitalRead(pin); }

int mmToMs(int mm) { return (long)mm * 1000 / MM_PER_SEC; }
int msToMm(int ms) { return (long)ms * MM_PER_SEC / 1000; }

// ms 동안 전진하고 멈춤. watchLine 이면 가는 중 선 보이면 멈추고 true
bool moveMs(int ms, bool watchLine) {
  goForward();
  bool found = false;
  if (watchLine) found = runUntilAny(ms);
  else           pause(ms);
  stopCar();
  return found;
}

// ms 동안 기다리다가 센서 pin 이 선(0)을 보면 바로 true. 수동으로 바뀌면 false
bool runUntil(int ms, int pin) {
  unsigned long start = millis();
  while (millis() - start < (unsigned long)ms) {
    if (autoAborted()) return false;
    if (readLine(pin) == 0) return true;
  }
  return false;
}

// ms 동안 기다리다가 라인센서 중 하나라도 선을 보면 바로 true. 수동으로 바뀌면 false
bool runUntilAny(int ms) {
  unsigned long start = millis();
  while (millis() - start < (unsigned long)ms) {
    if (autoAborted()) return false;
    if (readLine(L_Line) == 0 || readLine(C_Line) == 0 || readLine(R_Line) == 0) return true;
  }
  return false;
}

// 자동 회전 측정 : 선 위에서 제자리로 돌며 가운데 센서가 선을 다시 만나는 시간을 잼
//   처음 다시 만남 = 약 180도, 그다음 = 약 360도 -> 1도당 시간과 출발할 때 늦는 시간으로 90도 / 45도 계산
void calibrateTurns() {
  if (!AUTO_CAL || !centerOnLine()) return;      // 선 위에 없으면 측정 안 함 (설정값 그대로)
  stage = "CAL";
  calibrateTurn(true);
  pause(300);
  if (mode != AUTO) return;
  calibrateTurn(false);
  pause(300);
}

bool calibrateTurn(bool toRight) {
  unsigned long start = millis();
  spin(toRight, TURN_SPEED);
  long t180 = waitLineEntry(start);
  long t360 = (t180 > 0) ? waitLineEntry(start) : 0;
  stopCar();
  if (t180 <= 0 || t360 <= t180) return false;    // 실패 -> 설정값 그대로
  float msPerDeg = (t360 - t180) / 180.0;
  float lag = t180 - 180 * msPerDeg;               // 멈춘 상태에서 출발할 때 늦는 시간
  if (lag < 0) lag = 0;
  int t90 = lag + 90 * msPerDeg;
  int t45 = lag + 45 * msPerDeg;
  if (t90 < 200 || t90 > 2000) return false;       // 말이 안 되는 값이면 버림
  if (toRight) { TURN_RIGHT_90_MS = t90; TURN_RIGHT_45_MS = t45; }
  else         { TURN_LEFT_90_MS  = t90; TURN_LEFT_45_MS  = t45; }
  return true;
}

// 가운데 센서가 선에서 벗어났다가 다시 선을 만날 때까지 기다림. 돌려주는 값 = start 부터 걸린 ms (실패하거나 수동으로 바뀌면 0)
long waitLineEntry(unsigned long start) {
  while (centerOnLine())  if (autoAborted() || millis() - start > (unsigned long)CAL_TIMEOUT_MS) return 0;
  pause(20);                                        // 선 가장자리에서 값이 튀는 것 무시
  while (!centerOnLine()) if (autoAborted() || millis() - start > (unsigned long)CAL_TIMEOUT_MS) return 0;
  return millis() - start;
}

// 장애물과 거리 유지 : 처음 거리(keepMM)보다 가까우면 바깥쪽으로, 멀면 장애물 쪽으로 살짝 돌기
void keepDistance(int sideOff, int ds, int keepMM) {
  bool obstacleOnLeft = (sideOff < 0);
  if      (ds < keepMM - KEEP_TOL_MM) turn(obstacleOnLeft, KEEP_TURN_MS);    // 가까워짐 -> 장애물 반대쪽으로
  else if (ds > keepMM + KEEP_TOL_MM) turn(!obstacleOnLeft, KEEP_TURN_MS);   // 멀어짐 -> 장애물 쪽으로
}

bool centerOnLine() { return readLine(C_Line) == 0; }      // 가운데 센서가 검은 선 위 (readLine 이 뒤집어서 선 = 0)

// 코너 : 끝 센서만 선을 봄 -> 가운데 센서가 선을 다시 볼 때까지 TURN_SPEED 로 확실히 돌기 (최대 CORNER_MAX_MS)
void cornerTurn(bool toRight) {
  stage = "CORNER";
  spin(toRight, TURN_SPEED);
  if (runUntil(CORNER_MAX_MS, C_Line)) goForward();   // 가운데 센서가 선을 보면 바로 전진
  last_state = -1;
}

// 선 놓침 : 제자리에서 좌우로 머리를 흔들며 찾기. 찾으면 true
//   마지막으로 선을 본 쪽부터 (모르면 왼쪽부터), 가운데를 지나 반대쪽으로 갈 때마다 폭을 SWAY_GROW_MS 씩 넓힘
bool swaySearch() {
  bool toRight = (lastSide > 0) && !LEFT_CORNER_ONLY;   // 왼쪽 코너만 있는 트랙이면 항상 왼쪽부터
  int prevAmp = 0, amp = SWAY_START_MS;
  unsigned long start = millis();
  stage = "SWAY";
  while (mode == AUTO && millis() - start < SWAY_MAX_MS) {
    spin(toRight, TURN_SPEED);
    if (runUntilAny(prevAmp + amp)) { goForward(); return true; }   // 찾으면 멈추지 않고 바로 전진
    toRight = !toRight;
    prevAmp = amp;
    amp += SWAY_GROW_MS;
  }
  stopCar();
  return false;
}

void turn90(bool toRight) { turn(toRight, toRight ? TURN_RIGHT_90_MS : TURN_LEFT_90_MS); }
void turn45(bool toRight) { turn(toRight, toRight ? TURN_RIGHT_45_MS : TURN_LEFT_45_MS); }

void turn(bool toRight, int ms) {  // 멈춘 상태에서 ms 만큼 제자리 회전 후 멈춤
  stopCar();
  if (mode != AUTO) return;
  spin(toRight, TURN_SPEED);
  pause(ms);
  stopCar();
  pause(TURN_PAUSE_MS);
}

void spin(bool toRight, int speed) {     // 제자리 회전 (오른쪽 : 오른쪽 바퀴 뒤, 왼쪽 바퀴 앞)
  act = toRight ? 'w' : 'q';
  if (toRight) drive(LOW, HIGH, speed, speed);
  else         drive(HIGH, LOW, speed, speed);
}

void goForward()  { act = 'g'; drive(HIGH, HIGH, SPEED, SPEED); }
void stopCar()    { act = 's'; drive(HIGH, HIGH, 0, 0); }
void curveLeft()  { act = 'l'; drive(HIGH, HIGH, min(SPEED * 1.4, 255), max(SPEED * 0.4, 50)); }
void curveRight() { act = 'r'; drive(HIGH, HIGH, max(SPEED * 0.4, 50), min(SPEED * 1.4, 255)); }

void drive(int rightDir, int leftDir, int rightSpeed, int leftSpeed) {   // 방향 HIGH 앞 / LOW 뒤
  digitalWrite(RightMotor_1_pin, rightDir);
  digitalWrite(RightMotor_2_pin, !rightDir);
  digitalWrite(LeftMotor_3_pin, leftDir);
  digitalWrite(LeftMotor_4_pin, !leftDir);
  analogWrite(RightMotor_E_pin, rightSpeed);
  analogWrite(LeftMotor_E_pin, leftSpeed);
}
