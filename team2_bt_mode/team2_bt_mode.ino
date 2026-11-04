// 2팀 통합 스케치: 블루투스 수동/자동 모드 전환 + 끊김 안전장치
// 교안 코드 공통1(drive), 1-5(lineTrace), 1-6(장애물), 2-2~2-5를 한 파일로 합침
// 핀 번호는 교안 공통 기초 핀 약속 기준. 내 차량과 다르면 바꾸고 팀 노트에 적는다.
#include <SoftwareSerial.h>
#include <Servo.h>

// ---- 핀 ----
const int ENA = 5, IN1 = 8, IN2 = 9;    // 오른쪽 모터 2개
const int ENB = 6, IN3 = 10, IN4 = 11;  // 왼쪽 모터 2개
const int PIN_L = A5, PIN_C = A4, PIN_R = A3;
const int TRIG = 13, ECHO = 12;
#define BT_RXD 3
#define BT_TXD 4

// ---- 직접 재서 바꿀 값 ----
const int TH = 450;         // 라인 임계값 (1팀 STEP 2)
const int BASE = 120;       // 기본 속도
const int SLOW = 30;        // 커브 안쪽 바퀴 속도
const int STOP_CM = 15, RESUME_CM = 20;
const unsigned long HOLD_MS = 1000;  // 장애물 정지 후 최소 유지
const unsigned long LINK_MS = 600;   // 이 시간 무명령이면 정지 (앱 반복 주기 200ms보다 충분히 크게)
const unsigned long SWITCH_HOLD_MS = 500;  // 자동 모드로 전환할 때 무조건 정지하는 안전 지연 (수동 전환은 즉시)
const unsigned long AUTO_LINK_MS = 5000;   // 자동 중 폰 신호가 5초 없으면 정지하고 수동으로 (앱은 0.6초마다 'h')

SoftwareSerial BT(BT_RXD, BT_TXD);
Servo myservo;

// enum은 첫 함수보다 위에 둔다. 아두이노 IDE가 함수 선언을 첫 함수 앞에 자동으로 끼워 넣어서,
// 아래에 두면 'Mode was not declared' 오류가 난다.
enum Mode { MANUAL, AUTO };
enum State { DRIVING, STOPPED };

// 지금 실제로 하는 동작. 앱에 보내서 화면에 표시한다.
// g 직진, b 후진, l 좌회전, r 우회전, q/w 제자리 회전, s 정지, o 장애물 정지
char act = 's';

// ---- 모터 (공통 1) ----
void setMotor(int en, int a, int b, int speed) {
  digitalWrite(a, speed >= 0 ? HIGH : LOW);
  digitalWrite(b, speed >= 0 ? LOW : HIGH);
  analogWrite(en, constrain(abs(speed), 0, 255));
}

void drive(int left, int right) {
  setMotor(ENB, IN3, IN4, left);
  setMotor(ENA, IN1, IN2, right);
}

// ---- 라인트레이싱 (1팀 코드 1-5) ----
bool lastL = false, lastC = true, lastR = false;

void lineTrace() {
  bool l = analogRead(PIN_L) > TH;
  bool c = analogRead(PIN_C) > TH;
  bool r = analogRead(PIN_R) > TH;
  if (!l && !c && !r) { l = lastL; c = lastC; r = lastR; }  // 선을 놓침: 직전 값
  else { lastL = l; lastC = c; lastR = r; }

  if (l && r) { drive(0, 0); act = 's'; }           // 교차선 / 끝
  else if (l) { drive(SLOW, BASE); act = 'l'; }     // 왼쪽으로
  else if (r) { drive(BASE, SLOW); act = 'r'; }     // 오른쪽으로
  else { drive(BASE, BASE); act = 'g'; }            // 직진
}

// ---- 초음파 (1팀 STEP 3) ----
long readCm() {
  digitalWrite(TRIG, LOW); delayMicroseconds(2);
  digitalWrite(TRIG, HIGH); delayMicroseconds(10);
  digitalWrite(TRIG, LOW);
  long us = pulseIn(ECHO, HIGH, 25000);  // 25ms 안에 안 오면 포기
  if (us == 0) return 999;               // 측정 실패는 '멀다'
  return us * 0.0343 / 2;
}

// ---- 수동 주행 (코드 2-2) ----
void driveManual(char c) {
  act = c;
  switch (c) {
    case 'g': drive( 150,  150); break;  // 전진
    case 'b': drive(-150, -150); break;  // 후진
    case 'l': drive(  60,  150); break;  // 좌회전(곡선)
    case 'r': drive( 150,   60); break;  // 우회전(곡선)
    case 'q': drive(-120,  120); break;  // 제자리 좌회전
    case 'w': drive( 120, -120); break;  // 제자리 우회전
    default:  drive(0, 0); act = 's';    // 's' 포함
  }
}

// ---- 모드 (코드 2-3) ----
Mode mode = MANUAL;
char cmd = 's';
unsigned long holdUntil = 0, lastRx = 0;
unsigned long lastLink = 0;  // 아무 글자나 마지막으로 받은 시각 (연결 표시용, 주행 판단에는 안 씀)
const unsigned long LINK_SHOW_MS = 1500;  // 앱은 0.6초마다 'h'를 보낸다

void setMode(Mode m) {
  mode = m; cmd = 's';
  drive(0, 0);
  // 자동으로 갈 때만 안전 지연 동안 무조건 정지. 수동으로 갈 때는 즉시 (멈추고 바로 조종 가능)
  holdUntil = (m == AUTO) ? millis() + SWITCH_HOLD_MS : 0;
  act = 's';
}

// ---- 장애물 상태 (코드 1-6) ----
State state = DRIVING;
unsigned long lastPing = 0, stoppedAt = 0;
long dist = 999;

void setup() {
  pinMode(IN1, OUTPUT); pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT); pinMode(IN4, OUTPUT);
  pinMode(ENA, OUTPUT); pinMode(ENB, OUTPUT);
  pinMode(TRIG, OUTPUT); pinMode(ECHO, INPUT);
  // 초음파 센서를 정면으로 돌린 뒤 서보 신호를 끈다. 켜 두면 블루투스(SoftwareSerial)가
  // 글자를 주고받을 때마다 서보 펄스가 흐트러져 센서가 덜덜 떤다. 신호를 꺼도 그 자리에 그대로 있다.
  myservo.attach(2); myservo.write(90); delay(400); myservo.detach();
  Serial.begin(9600);
  BT.begin(9600);
}

void loop() {
  unsigned long now = millis();

  // 1) 60ms마다 거리 측정, 히스테리시스로 정지/출발 판단
  if (now - lastPing >= 60) { lastPing = now; dist = readCm(); }
  if (state == DRIVING && dist < STOP_CM) {
    state = STOPPED; stoppedAt = now;
  } else if (state == STOPPED && dist > RESUME_CM && now - stoppedAt > HOLD_MS) {
    state = DRIVING;
  }

  // 2) 블루투스 명령
  while (BT.available()) {
    char c = BT.read();
    // 명령표에 없는 글자는 무시. 터미널 앱이 붙이는 \r \n 때문에
    // 'g'가 바로 정지로 덮이거나 'a' 직후 수동으로 돌아가는 문제를 막는다.
    if (c == 0 || !strchr("gblrsqwamh", c)) continue;
    lastLink = millis();
    // 'h'는 앱이 살아 있다는 신호일 뿐. lastRx(주행 명령 시각)는 건드리지 않는다.
    // 건드리면 떼는 순간의 's'를 놓쳤을 때 'h'가 연결을 유지시켜 차가 계속 달린다.
    if (c == 'h') continue;
    Serial.print("수신: "); Serial.println(c);
    lastRx = millis();
    if (c == 'a') setMode(AUTO);
    else if (c == 'm') setMode(MANUAL);
    else {
      if (mode == AUTO) setMode(MANUAL);  // 자동 중 조종 명령 = 사람이 개입
      cmd = c;
    }
  }

  // 자동 중 폰 신호('h' 등)가 AUTO_LINK_MS 넘게 없으면 끊긴 것 → 정지하고 수동으로
  if (mode == AUTO && millis() - lastLink > AUTO_LINK_MS) {
    setMode(MANUAL);
    Serial.println(F("[연결 끊김] 자동 주행 중지"));
  }

  // 폰에 상태 한 줄: 모드(a/m) + 동작 + ',' + 거리. 예) "ag,23" = 자동, 직진, 23cm
  static unsigned long lastTx = 0;
  if (now - lastTx >= 200) {
    lastTx = now;
    BT.print(mode == AUTO ? 'a' : 'm'); BT.print(act);
    BT.print(','); BT.println(dist);
  }

  // 3) 전환 직후 0.5초 정지
  if (millis() < holdUntil) { drive(0, 0); act = 's'; return; }

  // 4) 주행. 장애물 정지가 가장 우선.
  // ponytail: 자동 모드는 폰이 아무것도 안 보내므로 끊김 감지가 없다(교안 설계 그대로).
  //           자동에서도 끊기면 멈추게 하려면 앱이 자동 중에 하트비트를 보내도록 바꿔야 한다.
  if (mode == AUTO) {
    if (state == STOPPED) { drive(0, 0); act = 'o'; }
    else lineTrace();
  } else {
    if (millis() - lastRx > LINK_MS) cmd = 's';  // 코드 2-4: 끊기면 정지
    char c = cmd;
    // 코드 2-5 확장: 장애물 앞에서는 앞으로 가는 명령(g, l, r)만 막는다. 후진과 제자리 회전은 허용.
    bool blocked = state == STOPPED && (c == 'g' || c == 'l' || c == 'r');
    driveManual(blocked ? 's' : c);
    if (blocked) act = 'o';
  }

  // 시리얼 모니터 확인용 (필요 없으면 지워도 된다)
  static unsigned long lastLog = 0;
  if (now - lastLog >= 500) {
    lastLog = now;
    // 링크=O: 앱이 연결되어 신호를 보내는 중. 터미널 앱은 'h'를 안 보내서 조종 중에만 O가 된다.
    bool linked = lastLink != 0 && millis() - lastLink < LINK_SHOW_MS;
    Serial.print(linked ? "링크=O " : "링크=X ");
    Serial.print(mode == AUTO ? "AUTO " : "MAN  ");
    Serial.print("act="); Serial.print(act);
    Serial.print(" cmd="); Serial.print(cmd);
    Serial.print(" dist="); Serial.print(dist);
    Serial.println(state == STOPPED ? " STOP" : "");
  }
}
