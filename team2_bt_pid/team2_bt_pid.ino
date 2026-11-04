/* 2팀 블루투스 수동/자동 + PID 라인트레이서 (버전 C)
 *  - 자동 모드 주행 : 라인센서 3개(0/1)로 오차를 만들고 PID 로 좌우 속도를 정함. 밝은 바탕 + 검은 선
 *  - Kp, Ki, Kd, 기본 속도는 폰 앱/웹 슬라이더로 바로 바꿈 (업로드 필요 없음, EEPROM 에 저장)
 *  - 블루투스/모드 전환/안전장치/상태 줄 : 버전 A, B 와 같음 (앱 그대로 사용)
 *
 *  오차 (검은 선 = 1)        L C R
 *     0 : 가운데             0 1 0
 *    +1 / +2 : 선이 오른쪽   0 1 1 / 0 0 1
 *    -1 / -2 : 선이 왼쪽     1 1 0 / 1 0 0
 *    ±3 : 선 놓침 (마지막으로 본 쪽)  0 0 0
 *  보정 = Kp*오차 + Ki*누적오차 + Kd*(오차 - 직전 오차)
 *  왼쪽 = 기본 속도 + 보정, 오른쪽 = 기본 속도 - 보정  (보정이 + 면 오른쪽으로 돎)
 */

#include <SoftwareSerial.h>
#include <Servo.h>
#include <EEPROM.h>

// ============================================================
// [설정]
// ============================================================

// --- PID (처음 값. 폰에서 바꾸면 EEPROM 값이 우선) ---
float KP = 40;        // 오차 1 당 보정. 좌우로 흔들리면 내림
float KI = 0;         // 누적 오차(오차 x 초) 1 당 보정. 긴 커브에서 점점 더 꺾게 함. 0 부터 조금씩
float KD = 30;        // 오차가 한 칸 바뀔 때 반대로 잡아 주는 양. 흔들림이 남으면 올림
int BASE = 120;       // 기본 속도 (0~255)
const unsigned long PID_MS = 20;      // PID 계산 주기
const float I_LIMIT = 50;             // 누적 오차 한계 (너무 쌓이지 않게)
// 디지털 센서는 오차가 0 -> 1 처럼 계단으로 바뀌어서 D 가 한 번(20ms)만 생기면 모터가 반응하기 전에 사라진다.
// 그래서 D 는 바뀐 순간 생긴 값을 20ms 마다 이 비율로 줄이며 약 0.1초 동안 이어지게 한다.
const float D_DECAY = 0.8;

// --- 선 놓침 / 교차선 ---
const unsigned long LOST_STOP_MS = 1500;   // 선을 이만큼 못 보면 정지 (그동안은 마지막 쪽으로 크게 꺾으며 찾음)
const unsigned long CROSS_STOP_MS = 500;   // 센서 3개 모두 검정이 이만큼 계속되면 정지 (선 끝)

// --- 장애물 ---
const int AVOID_MM = 100;             // 이 거리 안이면 정지 (자동), 전진 금지 (수동)
const int CLEAR_COUNT = 5;            // 연속 이만큼 안 보여야 다시 출발
const unsigned long PING_MS = 60;     // 초음파 쏘는 간격
const int MIN_VALID_MM = 20;          // 이보다 작으면 잡음으로 버림

// --- 블루투스 / 모드 ---
const unsigned long LINK_MS = 600;         // 수동: 이 시간 무명령이면 정지
const unsigned long SWITCH_HOLD_MS = 500;  // 자동 모드로 전환할 때 무조건 정지하는 안전 지연 (수동 전환은 즉시)
const unsigned long AUTO_LINK_MS = 1500;   // 자동 중 폰 신호가 이만큼 없으면 정지하고 수동으로 (앱은 0.6초마다 'h')
const unsigned long LINK_SHOW_MS = 1500;   // 앱은 0.6초마다 'h'를 보낸다

// ============================================================
// [핀]
// ============================================================
int RightMotor_E_pin = 5, LeftMotor_E_pin = 6;     // 모터 속도(PWM)
int RightMotor_1_pin = 8, RightMotor_2_pin = 9;    // 오른쪽 모터 방향
int LeftMotor_3_pin = 10, LeftMotor_4_pin = 11;    // 왼쪽 모터 방향
int L_Line = A3, C_Line = A4, R_Line = A5;         // 라인센서 왼쪽 A3, 가운데 A4, 오른쪽 A5 (검은 선 = 1)
int trigPin = 13, echoPin = 12, servoPin = 2;      // 초음파, 서보
#define BT_RXD 3                                   // 블루투스 모듈 TXD -> D3
#define BT_TXD 4                                   // 블루투스 모듈 RXD -> D4

// enum, struct 는 첫 함수보다 위에 둔다(아두이노 IDE가 함수 선언을 첫 함수 앞에 끼워 넣음).
enum Mode { MANUAL, AUTO };
struct Saved { unsigned int magic; float kp, ki, kd; int base; };
const unsigned int SAVE_MAGIC = 0xC0DF;   // 저장 형식이나 기본값을 새로 쓰고 싶으면 숫자를 바꿔서 옛 값을 무시 (C0DF: 흔들림 수정 후 기본값)

// ============================================================
SoftwareSerial BT(BT_RXD, BT_TXD);
Servo EduServo;

Mode mode = MANUAL;
char cmd = 's';
char act = 's';           // 앱에 보내는 지금 동작: g 직진, b 후진, l/r 좌/우, q/w 제자리, s 정지, o 장애물 정지
// 앱에 보내는 지금 단계: MAN 수동, HOLD 전환 대기, LINE 라인 추종, NOLINE 선 놓침(찾는 중),
// LOST 선 못 찾음(정지), CROSS 교차선, END 선 끝(정지), OBST 장애물 대기
const char* stage = "MAN";
unsigned long holdUntil = 0, lastRx = 0, lastLink = 0, lastTx = 0, lastLog = 0;

int frontDist = 9999;     // 마지막으로 잰 거리(mm)
unsigned long lastPing = 0;
bool obstacleStopped = false;
int clearCount = 0;

int pidErr = 0;           // 지금 오차 (-3~3)
float pidOut = 0;         // 지금 보정값
int lastErr = 0, lastSide = 0;
float integ = 0, dTerm = 0;
unsigned long lastPid = 0, lostSince = 0, crossSince = 0;

void setup() {
  // 초음파 센서를 정면으로 돌린 뒤 서보 신호를 끈다. 켜 두면 블루투스(SoftwareSerial)가
  // 글자를 주고받을 때마다 서보 펄스가 흐트러져 센서가 덜덜 떤다. 신호를 꺼도 그 자리에 그대로 있다.
  EduServo.attach(servoPin);
  EduServo.write(90);
  delay(400);
  EduServo.detach();
  pinMode(echoPin, INPUT);
  pinMode(trigPin, OUTPUT);
  pinMode(RightMotor_E_pin, OUTPUT);
  pinMode(RightMotor_1_pin, OUTPUT);
  pinMode(RightMotor_2_pin, OUTPUT);
  pinMode(LeftMotor_3_pin, OUTPUT);
  pinMode(LeftMotor_4_pin, OUTPUT);
  pinMode(LeftMotor_E_pin, OUTPUT);
  Serial.begin(115200);  // 시리얼 모니터도 115200
  BT.begin(9600);        // 블루투스 모듈은 9600
  loadPid();
  stopCar();
  Serial.print(F("=== 버전 C (PID) 시작 === "));
  printPid(Serial);
}

void loop() {
  serviceBT();
  if (millis() - lastPing >= PING_MS) updateObstacle();

  // 모드 전환 직후 안전 지연
  if (millis() < holdUntil) { stopCar(); stage = "HOLD"; return; }

  if (mode == AUTO) {
    if (obstacleStopped) { stopCar(); act = 'o'; stage = "OBST"; resetPid(); return; }
    pidLine();
    return;
  }

  // 수동: 끊기면 정지, 장애물 앞에서는 앞으로 가는 명령만 막음
  stage = "MAN";
  pidErr = 0; pidOut = 0;
  if (millis() - lastRx > LINK_MS) cmd = 's';
  bool blocked = obstacleStopped && (cmd == 'g' || cmd == 'l' || cmd == 'r');
  driveManual(blocked ? 's' : cmd);
  if (blocked) act = 'o';
}

// ============================================================
// PID 라인 추종
// ============================================================
// 라인센서 → 오차. 선 놓침(0,0,0)은 따로 처리하므로 여기서는 0 을 돌려줌
int lineError(int L, int C, int R) {
  if (!L && C && !R) return 0;
  if (!L && C && R)  return 1;
  if (!L && !C && R) return 2;
  if (L && C && !R)  return -1;
  if (L && !C && !R) return -2;
  return 0;   // 1,0,1 / 1,1,1 은 가운데로 봄
}

void resetPid() { integ = 0; dTerm = 0; lastErr = 0; lastPid = 0; }

void pidLine() {
  unsigned long now = millis();
  if (lastPid != 0 && now - lastPid < PID_MS) return;
  float dt = (lastPid == 0 || now - lastPid > 200) ? PID_MS / 1000.0 : (now - lastPid) / 1000.0;
  lastPid = now;

  int L = digitalRead(L_Line), C = digitalRead(C_Line), R = digitalRead(R_Line);
  int e;

  if (L && C && R) {                         // 모두 검정 : 교차선이면 지나가고, 오래 계속되면 선 끝
    if (crossSince == 0) crossSince = now;
    if (now - crossSince >= CROSS_STOP_MS) { stopCar(); stage = "END"; resetPid(); return; }
    stage = "CROSS";
    e = 0;
  } else crossSince = 0;

  if (!L && !C && !R) {                      // 선 놓침 : 마지막으로 본 쪽으로 크게 꺾으며 찾기
    if (lostSince == 0) lostSince = now;
    if (now - lostSince >= LOST_STOP_MS) { stopCar(); stage = "LOST"; resetPid(); return; }
    stage = "NOLINE";
    e = lastSide * 3;
  } else if (!(L && C && R)) {
    lostSince = 0;
    stage = "LINE";
    e = lineError(L, C, R);
    if (e != 0) lastSide = e > 0 ? 1 : -1;
  }

  if ((e > 0 && integ < 0) || (e < 0 && integ > 0)) integ = 0;   // 반대쪽으로 넘어가면 누적 초기화
  integ = constrain(integ + e * dt, -I_LIMIT, I_LIMIT);
  dTerm = dTerm * D_DECAY + KD * (e - lastErr);   // 바뀐 순간 생기고 약 0.1초에 걸쳐 줄어듦
  float u = KP * e + KI * integ + dTerm;
  lastErr = e;
  pidErr = e;
  pidOut = u;

  int left = BASE + (int)u, right = BASE - (int)u;
  // 선 위(오차 ±2 이내)에서는 안쪽 바퀴를 0 까지만 줄이고 뒤로 돌리지 않는다.
  // 뒤로 돌리면 차가 확 꺾여 선을 지나쳐 반대쪽으로 넘어가며 좌우로 흔들린다. 선을 놓쳤을 때(±3)만 허용.
  int lo = abs(e) <= 2 ? 0 : -255;
  driveLR(constrain(left, lo, 255), constrain(right, lo, 255));
  act = u > 20 ? 'r' : u < -20 ? 'l' : 'g';
}

// ============================================================
// 장애물 (멈추지 않고 계속 재면서 판단)
// ============================================================
void updateObstacle() {
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  long duration = pulseIn(echoPin, HIGH, 12000);
  lastPing = millis();
  int mm = duration * 340L / 2000;
  frontDist = (duration == 0 || mm < MIN_VALID_MM) ? 9999 : mm;

  if (frontDist < AVOID_MM) { obstacleStopped = true; clearCount = 0; }
  else if (obstacleStopped && ++clearCount >= CLEAR_COUNT) obstacleStopped = false;
}

// ============================================================
// 블루투스
// ============================================================
// 들어온 글자 처리 + 0.2초마다 상태 줄 송신 + 0.2초마다 시리얼 로그
// '#' 로 시작해서 줄바꿈으로 끝나는 건 설정 명령: #P60  #I0.5  #D40  #B140  #? (지금 값 보내기)
void serviceBT() {
  static char pbuf[12];
  static int plen = -1;          // -1 = 설정 명령 받는 중 아님
  while (BT.available()) {
    char c = BT.read();
    if (plen >= 0) {
      if (c == '\n' || c == '\r') { pbuf[plen] = 0; applyParam(pbuf); plen = -1; }
      else if (plen < (int)sizeof(pbuf) - 1) pbuf[plen++] = c;
      continue;
    }
    if (c == '#') { plen = 0; lastLink = millis(); continue; }
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
  // 자동 중 폰 신호('h' 등)가 AUTO_LINK_MS 넘게 없으면 끊긴 것 → 정지하고 수동으로
  if (mode == AUTO && millis() - lastLink > AUTO_LINK_MS) {
    setMode(MANUAL);
    Serial.println(F("[연결 끊김] 자동 주행 중지"));
  }
  // 폰에 상태 한 줄: 모드+동작, 거리(cm), 라인센서 L C R, 단계, 오차, 보정. 예) "ar,23,011,LINE,1,60"
  if (now - lastTx >= 200) {
    lastTx = now;
    BT.print(mode == AUTO ? 'a' : 'm'); BT.print(act);
    BT.print(','); BT.print(frontDist >= 9999 ? 999 : frontDist / 10);
    BT.print(','); BT.print(digitalRead(L_Line)); BT.print(digitalRead(C_Line)); BT.print(digitalRead(R_Line));
    BT.print(','); BT.print(stage);
    BT.print(','); BT.print(pidErr);
    BT.print(','); BT.println((int)pidOut);
  }
  if (now - lastLog >= 200) {
    lastLog = now;
    bool linked = lastLink != 0 && now - lastLink < LINK_SHOW_MS;
    Serial.print(linked ? F("링크=O ") : F("링크=X "));
    Serial.print(mode == AUTO ? F("AUTO ") : F("MAN  "));
    Serial.print(F("단계=")); Serial.print(stage);
    Serial.print(F(" 라인=")); Serial.print(digitalRead(L_Line));
    Serial.print(digitalRead(C_Line)); Serial.print(digitalRead(R_Line));
    Serial.print(F(" 오차=")); Serial.print(pidErr);
    Serial.print(F(" 보정=")); Serial.print((int)pidOut);
    Serial.print(F(" 거리=")); Serial.print(frontDist);
    Serial.println(F("mm"));
  }
}

// "P60" / "I0.5" / "D40" / "B140" / "?" 적용 → 저장 → 지금 값을 폰에 돌려줌
void applyParam(const char* s) {
  float v = atof(s + 1);
  switch (s[0]) {
    case 'P': KP = constrain(v, 0, 500); break;
    case 'I': KI = constrain(v, 0, 50); break;
    case 'D': KD = constrain(v, 0, 1000); break;
    case 'B': BASE = constrain((int)v, 0, 255); break;
    case '?': sendPid(); return;
    default: return;
  }
  integ = 0;
  savePid();
  sendPid();
  Serial.print(F("PID 변경: "));
  printPid(Serial);
}

// "#P60.0,I0.0,D40.0,B140"
void printPid(Print& out) {
  out.print(F("#P")); out.print(KP, 1);
  out.print(F(",I")); out.print(KI, 2);
  out.print(F(",D")); out.print(KD, 1);
  out.print(F(",B")); out.println(BASE);
}
void sendPid() { printPid(BT); }

void loadPid() {
  Saved s;
  EEPROM.get(0, s);
  if (s.magic != SAVE_MAGIC) return;   // 저장한 적 없음 → 위 [설정] 값
  KP = s.kp; KI = s.ki; KD = s.kd; BASE = s.base;
}

void savePid() {
  Saved s = { SAVE_MAGIC, KP, KI, KD, BASE };
  EEPROM.put(0, s);   // 바뀐 바이트만 씀
}

void setMode(Mode m) {
  mode = m;
  cmd = 's';
  stopCar();
  // 자동으로 갈 때만 안전 지연 동안 무조건 정지. 수동으로 갈 때는 즉시 (멈추고 바로 조종 가능)
  holdUntil = (m == AUTO) ? millis() + SWITCH_HOLD_MS : 0;
  resetPid();
  lostSince = 0;
  crossSince = 0;
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

// ============================================================
// 모터
// ============================================================
// 왼쪽, 오른쪽 속도(-255~255, 음수는 후진)
void driveLR(int left, int right) {
  drive(right >= 0 ? HIGH : LOW, left >= 0 ? HIGH : LOW, abs(right), abs(left));
}

void stopCar() { act = 's'; drive(HIGH, HIGH, 0, 0); }

void drive(int rightDir, int leftDir, int rightSpeed, int leftSpeed) {   // 방향 HIGH 앞 / LOW 뒤
  digitalWrite(RightMotor_1_pin, rightDir);
  digitalWrite(RightMotor_2_pin, !rightDir);
  digitalWrite(LeftMotor_3_pin, leftDir);
  digitalWrite(LeftMotor_4_pin, !leftDir);
  analogWrite(RightMotor_E_pin, rightSpeed);
  analogWrite(LeftMotor_E_pin, leftSpeed);
}
