// 2팀 블루투스 수동/자동 + 직접 짠 주행 코드 (버전 A)
// - 자동 모드 주행 : 붙여 준 주행 코드(SPEED 200, 데드존 찾기, 장애물 mm) 그대로
// - 핀 번호 : 원래 파일(team2_bt_mode, 교안 공통 핀 약속) 기준
// - 블루투스/모드 전환/안전장치/상태 줄 : team2_bt_mode 와 같음 (앱 3종 그대로 사용)
// 주행 함수(goForward, spinLeft, spinRight, stopCar, drive)는 원본 풀버전과 같고,
// 상태 표시용 act 한 줄씩만 더함. 핀은 원본(A4/A3/A2)이 아니라 원래 파일 기준(A5/A4/A3).
#include <SoftwareSerial.h>
#include <Servo.h>

// ===== 주행 설정 =====
int SPEED = 200;        // 주행속도 200
int TURN_SPEED = 150;   // 커브 속도 150

// ===== 장애물 설정 =====
const int OBSTACLE_MM = 50;              // 5cm 이내면 장애물
const unsigned long PING_INTERVAL = 10;  // 초음파 측정 주기(ms)
const int HIT_COUNT_NEED = 1;            // 연속 N회 감지되면 정지
const int CLEAR_COUNT_NEED = 3;          // 연속 N회 안 보이면 재출발

// ===== 데드존(선 이탈) 설정 =====
const unsigned long LOST_HOLD_MS = 3000;
const int LOST_SPEED = 120;
const int LOST_MODE = 1;  // 0 = 원래 방향의 반대로, 1 = 원래 방향 그대로

// ===== 블루투스 / 모드 설정 =====
const unsigned long LINK_MS = 600;         // 수동: 이 시간 무명령이면 정지 (앱 반복 주기 200ms보다 충분히 크게)
const unsigned long SWITCH_HOLD_MS = 500;  // 모드 전환 직후 무조건 정지하는 안전 지연
const unsigned long LINK_SHOW_MS = 1500;   // 앱은 0.6초마다 'h'를 보낸다

// ===== 핀 (원래 파일 기준) =====
int RightMotor_E_pin = 5;
int LeftMotor_E_pin = 6;
int RightMotor_1_pin = 8;
int RightMotor_2_pin = 9;
int LeftMotor_3_pin = 10;
int LeftMotor_4_pin = 11;

int L_Line = A5;
int C_Line = A4;
int R_Line = A3;

int trigPin = 13;
int echoPin = 12;
int servoPin = 2;

#define BT_RXD 3
#define BT_TXD 4

SoftwareSerial BT(BT_RXD, BT_TXD);
Servo EduServo;

// enum은 첫 함수보다 위에 둔다(아두이노 IDE가 함수 선언을 첫 함수 앞에 끼워 넣음).
enum Mode { MANUAL, AUTO };

// ===== 상태 변수 (주행) =====
int last_state = -1;
unsigned long lost_since = 0;
const char* action = "정지";
unsigned long lastDebug = 0;
unsigned long lastPing = 0;
int frontDist = 9999;

bool obstacleStopped = false;
int hitCount = 0;
int clearCount = 0;

int lastRightDir = HIGH;
int lastLeftDir = HIGH;
int lastRightSpeed = 0;
int lastLeftSpeed = 0;

// ===== 상태 변수 (블루투스 / 모드) =====
Mode mode = MANUAL;
char cmd = 's';
char act = 's';  // 앱에 보내는 지금 동작: g 직진, b 후진, l/r 좌/우, q/w 제자리, s 정지, o 장애물 정지
// 앱에 보내는 지금 단계 (자세한 디버그용)
// MAN 수동, HOLD 전환 대기, LINE 라인 추종, NOLINE 선 놓침, DEAD 데드존 찾기, OBST 장애물 정지
const char* stage = "MAN";
unsigned long holdUntil = 0, lastRx = 0, lastLink = 0, lastTx = 0;

void setup() {
  EduServo.attach(servoPin);
  EduServo.write(90);

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
  Serial.println(F("=== 시작 (수동 모드) ==="));
  Serial.print(F("SPEED: "));
  Serial.println(SPEED);
  Serial.print(F("TURN_SPEED: "));
  Serial.println(TURN_SPEED);
}

void loop() {
  serviceBT();
  updateObstacle();
  debugStatus(frontDist);

  // 모드 전환 직후 안전 지연
  if (millis() < holdUntil) { stopCar(); stage = "HOLD"; return; }

  if (mode == AUTO) {
    if (obstacleStopped) { act = 'o'; stage = "OBST"; return; }
    followLine();
    return;
  }

  // 수동: 끊기면 정지, 장애물 앞에서는 앞으로 가는 명령만 막음
  stage = "MAN";
  if (millis() - lastRx > LINK_MS) cmd = 's';
  bool blocked = obstacleStopped && (cmd == 'g' || cmd == 'l' || cmd == 'r');
  driveManual(blocked ? 's' : cmd);
  if (blocked) act = 'o';
}

// ============================================================
// 블루투스
// ============================================================
// 들어온 글자 처리 + 0.2초마다 상태 줄 송신. 오래 걸리는 동작 중에도 계속 불러 준다.
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
  // 폰에 상태 한 줄: 모드(a/m)+동작, 거리(cm), 라인센서 L C R, 단계. 예) "ag,23,010,LINE"
  if (millis() - lastTx >= 200) {
    lastTx = millis();
    BT.print(mode == AUTO ? 'a' : 'm'); BT.print(act);
    BT.print(','); BT.print(frontDist >= 9999 ? 999 : frontDist / 10);
    BT.print(','); BT.print(digitalRead(L_Line)); BT.print(digitalRead(C_Line)); BT.print(digitalRead(R_Line));
    BT.print(','); BT.println(stage);
  }
}

void setMode(Mode m) {
  mode = m;
  cmd = 's';
  stopCar();
  holdUntil = millis() + SWITCH_HOLD_MS;
  last_state = -1;   // 자동으로 들어갈 때 라인 상태를 처음부터 다시 판단
  lost_since = 0;
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
// 장애물 (붙여 준 코드 그대로)
// ============================================================
void updateObstacle() {
  if (millis() - lastPing < PING_INTERVAL) return;
  frontDist = Ultrasonic();
  if (frontDist < OBSTACLE_MM) {
    hitCount++;
    clearCount = 0;
  }
  else {
    clearCount++;
    hitCount = 0;
  }
  if (!obstacleStopped && hitCount >= HIT_COUNT_NEED) {
    obstacleStopped = true;
    if (mode == AUTO) stopCar();
    Serial.print(F("[장애물] 감지, 정지 (거리 "));
    Serial.print(frontDist);
    Serial.println(F(" mm)"));
  }
  else if (obstacleStopped && clearCount >= CLEAR_COUNT_NEED) {
    obstacleStopped = false;
    last_state = -1;
    lost_since = 0;
    Serial.println(F("[장애물] 사라짐, 라인 추종 재개"));
  }
}

void debugStatus(int d) {
  if (millis() - lastDebug < 300) return;

  lastDebug = millis();

  // 링크=O : 앱이 연결되어 신호를 보내는 중 (터미널 앱은 'h'를 안 보내서 조종 중에만 O)
  bool linked = lastLink != 0 && millis() - lastLink < LINK_SHOW_MS;
  Serial.print(linked ? F("[링크 O] ") : F("[링크 X] "));
  Serial.print(mode == AUTO ? F("[자동] ") : F("[수동] "));
  Serial.print(F("거리 "));
  Serial.print(d);
  Serial.print(F(" | 라인 "));
  printLine();
  Serial.print(F(" | 동작 "));
  Serial.print(action);
  Serial.print(F(" | 단계 "));
  Serial.println(stage);
}

void printLine() {
  Serial.print(digitalRead(L_Line));
  Serial.print(",");
  Serial.print(digitalRead(C_Line));
  Serial.print(",");
  Serial.print(digitalRead(R_Line));
}

// ============================================================
// 라인 추종 / 데드존 (붙여 준 코드 그대로, 기다리는 동안 블루투스 확인만 추가)
// ============================================================
bool lineLost() {
  return digitalRead(L_Line) == 1 &&
         digitalRead(C_Line) == 1 &&
         digitalRead(R_Line) == 1;
}

bool waitOrLine(unsigned long ms) {
  unsigned long start = millis();

  while (millis() - start < ms) {
    if (!lineLost()) return true;

    updateObstacle();
    if (obstacleStopped) return true;
    if (autoAborted()) return true;  // 수동으로 바뀜 -> 바로 그만둠
  }

  return false;
}

void lostLineMove() {
  int rd = lastRightDir;
  int ld = lastLeftDir;
  int rs = lastRightSpeed;
  int ls = lastLeftSpeed;

  if (rs == 0 && ls == 0) return;

  stage = "DEAD";
  if (LOST_MODE == 1) {
    Serial.println(F("[데드존] 원래 방향으로 선 찾는 중"));
    drive(rd, ld, LOST_SPEED, LOST_SPEED);
  }
  else {
    if (!(rd == HIGH && ld == HIGH && rs == ls)) {
      Serial.println(F("[데드존] 반대 방향으로 선 찾는 중"));
      drive(!rd, !ld, LOST_SPEED, LOST_SPEED);
    }
  }

  bool interrupted = waitOrLine(LOST_HOLD_MS);

  if (obstacleStopped) return;
  if (mode != AUTO) return;  // 수동으로 바뀌었으면 원래 동작으로 되돌리지 않음

  if (interrupted) {
    Serial.println(F("[데드존] 선 발견, 이동 중단 -> 선에 따라 동작"));
  }

  drive(rd, ld, rs, ls);
  last_state = -1;
}

void followLine() {
  int L = digitalRead(L_Line);
  int C = digitalRead(C_Line);
  int R = digitalRead(R_Line);

  if (L == 1 && C == 1 && R == 1) {
    stage = "NOLINE";
    if (lost_since == 0) {
      lost_since = millis();
      lostLineMove();
      stage = "NOLINE";
    }

    if (millis() - lost_since >= 10000) {
      stopCar();
      Serial.println(F("선 없음 -> 정지"));
      lost_since = millis();
      last_state = -1;
    }

    return;
  }

  lost_since = 0;
  stage = "LINE";

  int state = L * 4 + C * 2 + R;

  if (state == last_state) return;
  last_state = state;

  Serial.print(F("digital: "));
  Serial.print(L);
  Serial.print(", ");
  Serial.print(C);
  Serial.print(", ");
  Serial.print(R);
  Serial.print(" ");

  if (L == 0 && C == 1 && R == 0) {
    goForward();
    Serial.println(F("직진"));
  }
  else if (L == 0 && C == 1 && R == 1) {
    spinLeft();
    Serial.println(F("좌회전"));
  }
  else if (L == 0 && C == 0 && R == 1) {
    spinLeft();
    Serial.println(F("좌회전"));
  }
  else if (L == 1 && C == 1 && R == 0) {
    spinRight();
    Serial.println(F("우회전"));
  }
  else if (L == 1 && C == 0 && R == 0) {
    spinRight();
    Serial.println(F("우회전"));
  }
  else if (L == 1 && R == 1) {
    stopCar();
    Serial.println(F("정지"));
  }
  else {
    Serial.println();
  }
}

int Ultrasonic() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  long duration = pulseIn(echoPin, HIGH, 12000);

  lastPing = millis();

  if (duration == 0) return 9999;

  int mm = ((float)(340 * duration) / 1000) / 2;

  return mm;
}

// ============================================================
// 동작
// ============================================================
void goForward() {
  action = "직진";
  act = 'g';
  drive(HIGH, HIGH, SPEED, SPEED);
}

void spinLeft() {
  action = "좌회전";
  act = 'q';
  drive(HIGH, LOW, TURN_SPEED, TURN_SPEED);
}

void spinRight() {
  action = "우회전";
  act = 'w';
  drive(LOW, HIGH, TURN_SPEED, TURN_SPEED);
}

void stopCar() {
  action = "정지";
  act = 's';
  drive(HIGH, HIGH, 0, 0);
}

// 방향 HIGH 앞 / LOW 뒤. 데드존에서 쓰려고 마지막 값을 기억해 둔다.
void drive(int rightDir, int leftDir, int rightSpeed, int leftSpeed) {
  lastRightDir = rightDir;
  lastLeftDir = leftDir;
  lastRightSpeed = rightSpeed;
  lastLeftSpeed = leftSpeed;
  digitalWrite(RightMotor_1_pin, rightDir);
  digitalWrite(RightMotor_2_pin, !rightDir);
  digitalWrite(LeftMotor_3_pin, leftDir);
  digitalWrite(LeftMotor_4_pin, !leftDir);
  analogWrite(RightMotor_E_pin, rightSpeed);
  analogWrite(LeftMotor_E_pin, leftSpeed);
}
