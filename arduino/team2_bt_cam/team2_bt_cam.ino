/* 2팀 블루투스 수동/자동 + PID 라인트레이서 + 카메라 주행 (버전 D = 버전 C + ESP32-CAM 카메라 주행)
 *  - 버전 C (team2_bt_pid) 와 버전 D 둘 다 제출용. 버전 D 는 버전 C 에 ESP32-CAM 카메라 주행을 더한 것 (브랜치 extra/esp32cam)
 *  - 버전 C 와 다른 곳 : 카메라 주행 글자 G L R Q W 를 받음 (serviceBT, driveManual, 장애물 막기). 나머지는 버전 C 와 같음
 *  - 자동 모드 주행 : 라인센서 3개(0/1)로 오차를 만들고 PID 로 좌우 속도를 정함. 밝은 바탕 + 검은 선
 *  - Kp, Ki, Kd, 기본 속도는 아래 [설정] 숫자를 고쳐서 업로드 (앱 PID 슬라이더는 쓰지 않음)
 *  - 블루투스/모드 전환/안전장치/상태 줄 : PROTOCOL.md (앱 굴노)
 *
 *  오차 (검은 선 = 1)        L C R
 *     0 : 가운데             0 1 0
 *    +1 / +2 : 선이 오른쪽   0 1 1 / 0 0 1
 *    -1 / -2 : 선이 왼쪽     1 1 0 / 1 0 0
 *    ±3 : 선 놓침 (마지막으로 본 쪽)  0 0 0
 *  오차 0, ±1 (직선, 완만한 커브) : PID
 *    보정 = Kp*오차 + Ki*누적오차 + D항
 *    왼쪽 = 기본 속도 + 보정, 오른쪽 = 기본 속도 - 보정  (보정이 + 면 오른쪽으로 돎)
 *  오차 ±2, ±3 (코너, 선 놓침) : 바깥 바퀴 CORNER_OUT, 안쪽 바퀴 CORNER_IN 으로 회전 (기본: 원을 그리듯)
 *    가운데 센서가 선을 다시 보면 PID 로 돌아감
 *  자세한 설명 : docs/PID.md
 *
 *  주행은 두 가지 (폰 앱 또는 ESP32-CAM 웹 화면의 버튼으로 고름)
 *    라인센서 주행 : 'a' -> 자동 모드. 위의 라인센서 3개 + PID 로 이 아두이노가 스스로 달림 (0.02초마다 판단)
 *    카메라 주행   : ESP32-CAM 이 카메라로 라인을 보고 대문자 G L R Q W 를 0.15초마다 보냄 (브랜치 extra/esp32cam).
 *                    이 아두이노는 수동 모드에서 조종 글자처럼 받지만 속도는 자동 주행과 같음 :
 *                    G = BASE/BASE, L·R = BASE∓KP (PID ±1 정도), Q·W = CORNER_OUT/CORNER_IN (코너와 같음).
 *                    수동과 같은 안전장치 : LINK_MS 동안 명령이 없으면 정지, 앞 장애물이면 G L R 막음
 *  메모리 : String·malloc 을 쓰지 않음 (UNO 는 2KB 라 조각나면 멈춤). 글자는 F() 로 플래시에서 바로
 */

#include <SoftwareSerial.h>
#include <Servo.h>

// ============================================================
// [설정]
// ============================================================

// --- PID ---
const float KP = 51;        // 오차 1 당 보정. 좌우로 흔들리면 내림
const float KI = 0.2;         // 누적 오차(오차 x 초) 1 당 보정. 긴 커브에서 점점 더 꺾게 함. 0 부터 조금씩
const float KD = 30;        // 오차가 한 칸 바뀔 때 반대로 잡아 주는 양. 흔들림이 남으면 올림
const int BASE = 120;       // 기본 속도 (0~255)
// --- 코너(오차 ±2)·선 놓침(±3) : 바깥 바퀴와 안쪽 바퀴 힘을 따로 ---
//   CORNER_IN  -170 : 제자리 회전 (확실히 돌지만 접지력이 많이 필요. 요철에서 미끄러짐)
//   CORNER_IN   -90 : 안쪽은 천천히 뒤로, 바깥은 앞으로 (4바퀴 모두 돌면서 돎. 시뮬레이션에서 가장 잘 돎)
//   CORNER_IN     0 : 안쪽 바퀴를 세우고 바깥만 돌림 (예전 방식)
const int CORNER_OUT = 180;     // 바깥 바퀴 (앞으로). 새 배터리(전압 높음)에서 220 은 너무 크게 돎. 배터리 약해져 덜 돌면 200~220
const int CORNER_IN = -90;      // 안쪽 바퀴 (- 는 뒤로). 감속기가 작아 힘이 약하므로 0 이 아니면 최소 MIN_PWM(90),
                                //   방향이 바뀌는 순간 0.08초 최대 힘(KICK). 안 돌면 -130 쪽으로, 요철에서 미끄러지면 -90 쪽으로
const unsigned long PID_MS = 20;      // PID 계산 주기
const float I_LIMIT = 50;             // 누적 오차 한계 (너무 쌓이지 않게)
// 디지털 센서는 오차가 0 -> 1 처럼 계단으로 바뀌어서 D 가 한 번(20ms)만 생기면 모터가 반응하기 전에 사라진다.
// 그래서 D 는 바뀐 순간 생긴 값을 20ms 마다 이 비율로 줄이며 약 0.1초 동안 이어지게 한다.
const float D_DECAY = 0.8;

// --- 선 놓침 / 교차선 ---
const unsigned long LOST_PID_MS = 500;     // 선을 놓치면 먼저 이만큼 PID 로 마지막 쪽으로 보정 (앞으로 가며 완만히, 안쪽 바퀴 0 까지만)
const unsigned long LOST_STOP_MS = 500;    // 그래도 없으면 이만큼 마지막으로 본 쪽으로 크게 꺾으며 찾음 (길면 너무 많이 돎)
const unsigned long BACK_SEARCH_MS = 1000; // 그래도 없으면 반대쪽으로 이만큼 (되돌아오는 0.5초 + 반대쪽 0.5초) 찾고 정지.
                                           // 센서 간격(4.9cm)이 테이프(1.9cm)보다 넓어 코너 뒤 넘치면 선이 반대쪽 틈에 있음
const unsigned long CROSS_STOP_MS = 500;   // 센서 3개 모두 검정이 이만큼 계속되면 정지 (선 끝)

// --- 장애물 ---
const int AVOID_MM = 100;             // 이 거리(10cm) 안이면 정지 (자동), 전진 금지 (수동). 5cm 는 멈추기 전에 부딪힐 수 있어 10cm
const int CLEAR_COUNT = 5;            // 연속 이만큼 안 보여야 다시 출발
const unsigned long PING_MS = 60;     // 초음파 쏘는 간격
const int MIN_VALID_MM = 20;          // 이보다 작으면 잡음으로 버림

// --- 블루투스 / 모드 ---
const unsigned long LINK_MS = 450;         // 수동·카메라 주행: 이 시간 무명령이면 정지 (앱은 누르는 동안 0.2초마다, 카메라 주행은 0.15초마다 보냄. 한 번 놓쳐도 안 멈추게 450)
// --- 수동 조종 힘 (바닥에서 4륜은 바퀴가 미끄러지며 돌아서 힘이 많이 듦. 차에 맞게 조정) ---
const int MAN_FWD = 100;            // 전진·후진 (새 배터리에서 130 은 빠름. MIN_PWM 90 보다 작으면 출발 못 함)
const int MAN_CURVE_OUT = 100;      // 곡선 회전 바깥 바퀴 (255 -> 100, 차에서 맞춤)
const int MAN_CURVE_IN = 80;       // 곡선 회전 안쪽 바퀴
const int MAN_SPIN = 130;           // 제자리 회전 (230 -> 130, 차에서 맞춤)
const int MIN_PWM = 90;             // 0 이 아니면 최소 이만큼 (작으면 바닥에서 모터가 안 돎)
const int KICK_PWM = 255;           // 출발하거나 방향이 바뀌는 순간 주는 힘
const unsigned long KICK_MS = 120;  // 그 시간 (배터리가 약해도 출발하게 0.08 -> 0.12초)
const float TRIM_L = 1.0, TRIM_R = 1.0;   // 한쪽이 빠르면 빠른 쪽을 0.9 처럼 낮춰 맞춤
const unsigned long SWITCH_HOLD_MS = 500;  // 자동 모드로 전환할 때 무조건 정지하는 안전 지연 (수동 전환은 즉시)
const unsigned long AUTO_LINK_MS = 5000;   // 자동 중 폰 신호가 5초 없으면 정지하고 수동으로 (앱은 0.6초마다 'h')
const unsigned long LINK_SHOW_MS = 1500;   // 앱은 0.6초마다 'h'를 보낸다

// ============================================================
// [핀]
// ============================================================
// 핀 (센서 확장 쉴드의 S 번호). 바뀌지 않는 값이라 모두 const int 로 (RAM 을 안 쓰고, 잘못 바꾸면 컴파일 오류)
const int RightMotor_E_pin = 5, LeftMotor_E_pin = 6;     // 모터 속도(PWM)
const int RightMotor_1_pin = 8, RightMotor_2_pin = 9;    // 오른쪽 모터 방향
const int LeftMotor_3_pin = 10, LeftMotor_4_pin = 11;    // 왼쪽 모터 방향
const int L_Line = A3, C_Line = A4, R_Line = A5;         // 라인센서 왼쪽 A3, 가운데 A4, 오른쪽 A5 (검은 선 = 1)
const int trigPin = 13, echoPin = 12, servoPin = 2;      // 초음파, 서보
const int BT_RXD = 3;                                    // 블루투스 모듈 TXD -> D3 (아두이노가 받는 쪽)
const int BT_TXD = 4;                                    // 블루투스 모듈 RXD -> D4 (아두이노가 보내는 쪽)

// enum 은 첫 함수보다 위에 둔다(아두이노 IDE가 함수 선언을 첫 함수 앞에 끼워 넣음).
enum Mode { MANUAL, AUTO };

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
int motorL = 0, motorR = 0;   // 실제로 모터에 준 속도 (-255~255, 음수는 뒤로). 시리얼 로그용
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
  stopCar();
  Serial.print(F("=== 버전 D (PID + 카메라 주행) 시작 === Kp ")); Serial.print(KP);
  Serial.print(F(" Ki ")); Serial.print(KI);
  Serial.print(F(" Kd ")); Serial.print(KD);
  Serial.print(F(" 기본 속도 ")); Serial.println(BASE);
}

void loop() {
  serviceBT();
  if (millis() - lastPing >= PING_MS) updateObstacle();

  // 모드 전환 직후 안전 지연
  if (millis() < holdUntil) { stopCar(); stage = "HOLD"; return; }

  if (mode == AUTO) {
    // 장애물 : 정지하고 기다림. 치워지면(CLEAR_COUNT 번 연속 안 보이면) 그 자리에서 다시 라인 추종.
    // 기다린 시간이 선 놓침·교차 시간에 들어가지 않게 타이머를 비움 (안 비우면 출발하자마자 LOST)
    if (obstacleStopped) { stopCar(); act = 'o'; stage = "OBST"; resetPid(); lostSince = 0; crossSince = 0; return; }
    pidLine();
    return;
  }

  // 수동: 끊기면 정지, 장애물 앞에서는 앞으로 가는 명령만 막음
  stage = "MAN";
  pidErr = 0; pidOut = 0;
  if (millis() - lastRx > LINK_MS) cmd = 's';
  bool blocked = obstacleStopped && strchr("glrGLR", cmd);   // 앞에 장애물 : 앞으로 가는 명령만 막음 (카메라 주행 글자 포함)
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
  bool lostPid = false;                      // 선 놓친 직후 : 회전 대신 PID 보정

  if (L && C && R) {                         // 모두 검정 : 교차선이면 지나가고, 오래 계속되면 선 끝
    if (crossSince == 0) crossSince = now;
    if (now - crossSince >= CROSS_STOP_MS) { stopCar(); stage = "END"; resetPid(); return; }
    stage = "CROSS";
    e = 0;
  } else crossSince = 0;

  if (!L && !C && !R) {                      // 선 놓침 : PID 보정 -> 마지막 쪽으로 크게 꺾으며 찾기 -> 반대쪽 -> 정지
    if (lostSince == 0) lostSince = now;
    unsigned long lost = now - lostSince;
    if (lost >= LOST_PID_MS + LOST_STOP_MS + (lastSide != 0 ? BACK_SEARCH_MS : 0)) { stopCar(); stage = "LOST"; resetPid(); return; }
    stage = "NOLINE";
    lostPid = lost < LOST_PID_MS;
    e = (lost < LOST_PID_MS + LOST_STOP_MS ? lastSide : -lastSide) * 3;   // 마지막 쪽 먼저, 못 찾으면 반대쪽
  } else if (!(L && C && R)) {
    lostSince = 0;
    stage = "LINE";
    e = lineError(L, C, R);
    if (e != 0) lastSide = e > 0 ? 1 : -1;
  }

  // 코너(끝 센서만 선을 봄, ±2) 또는 선 놓침(±3) : 바깥 바퀴 CORNER_OUT, 안쪽 바퀴 CORNER_IN 으로 돈다.
  // (조향이 없으니 좌우 바퀴 힘 차이로 돈다. CORNER_IN 이 음수면 안쪽 바퀴가 뒤로 돌아 더 날카롭게 돎)
  // 가운데 센서가 선을 다시 보면(오차 ±1 이하) 아래 PID 로 돌아간다.
  if (abs(e) >= 2 && !lostPid) {
    int dir = e > 0 ? 1 : -1;                       // + 오른쪽으로 돎
    // 수동 조종과 같은 힘 다듬기(최소 MIN_PWM, 방향 바뀔 때 KICK)를 거쳐서 4바퀴가 실제로 돌게 한다
    if (dir > 0) driveManualLR(CORNER_OUT, CORNER_IN);   // 오른쪽 코너 : 왼쪽이 바깥
    else         driveManualLR(CORNER_IN, CORNER_OUT);   // 왼쪽 코너 : 오른쪽이 바깥
    if (abs(e) == 2) stage = "CORNER";
    integ = 0; dTerm = 0; lastErr = e;              // PID 로 돌아갈 때 쌓인 값 때문에 튀지 않게
    pidErr = e; pidOut = 0;
    act = dir > 0 ? 'w' : 'q';
    return;
  }

  if ((e > 0 && integ < 0) || (e < 0 && integ > 0)) integ = 0;   // 반대쪽으로 넘어가면 누적 초기화
  integ = constrain(integ + e * dt, -I_LIMIT, I_LIMIT);
  dTerm = dTerm * D_DECAY + KD * (e - lastErr);   // 바뀐 순간 생기고 약 0.1초에 걸쳐 줄어듦
  float u = KP * e + KI * integ + dTerm;
  lastErr = e;
  pidErr = e;
  pidOut = u;

  int left = BASE + (int)u, right = BASE - (int)u;
  // PID 구간(오차 ±1 이내)에서는 안쪽 바퀴를 0 까지만 줄이고 뒤로 돌리지 않는다 (확 꺾여 좌우로 흔들리는 것 방지)
  // 출발 킥과 최소 힘(MIN_PWM)을 거쳐서 보냄. 안 거치면 배터리가 약할 때 기본 속도만으로는 감속기를 못 돌려 출발을 못 한다
  driveManualLR(constrain(left, 0, 255), constrain(right, 0, 255));
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
void serviceBT() {
  while (BT.available()) {
    char c = BT.read();
    // 명령표에 없는 글자(터미널 앱의 \r \n, 앱이 보내는 #P60 같은 설정 글자 등)는 무시
    if (c == 0 || !strchr("gblrsqwamhGLRQW", c)) continue;   // 대문자 G L R Q W = 카메라 주행 (ESP32-CAM 이 보냄)
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
    // 예) 링크=O MAN  단계=MAN act=g cmd=g 라인=010 오차=0 보정=0 모터L=150 R=150 거리=230mm 장애물=X
    Serial.print(F("단계=")); Serial.print(stage);
    Serial.print(F(" act=")); Serial.print(act);
    Serial.print(F(" cmd=")); Serial.print(cmd);
    Serial.print(F(" 라인=")); Serial.print(digitalRead(L_Line));
    Serial.print(digitalRead(C_Line)); Serial.print(digitalRead(R_Line));
    Serial.print(F(" 오차=")); Serial.print(pidErr);
    Serial.print(F(" 보정=")); Serial.print((int)pidOut);
    Serial.print(F(" 모터L=")); Serial.print(motorL);
    Serial.print(F(" R=")); Serial.print(motorR);
    Serial.print(F(" 거리=")); Serial.print(frontDist);
    Serial.print(F("mm 장애물=")); Serial.println(obstacleStopped ? 'O' : 'X');
  }
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
    case 'g': driveManualLR( MAN_FWD,  MAN_FWD); break;              // 전진
    case 'b': driveManualLR(-MAN_FWD, -MAN_FWD); break;              // 후진
    case 'l': driveManualLR( MAN_CURVE_IN,  MAN_CURVE_OUT); break;   // 좌회전(곡선)
    case 'r': driveManualLR( MAN_CURVE_OUT, MAN_CURVE_IN); break;    // 우회전(곡선)
    case 'q': driveManualLR(-MAN_SPIN,  MAN_SPIN); break;            // 제자리 좌회전
    case 'w': driveManualLR( MAN_SPIN, -MAN_SPIN); break;            // 제자리 우회전
    // 카메라 주행 (ESP32-CAM 이 카메라로 보고 0.15초마다 보냄) : 수동 조종처럼 받지만 속도는 자동 주행과 같게
    case 'G': driveManualLR(BASE, BASE); break;                                        // 직진
    case 'L': driveManualLR(BASE - (int)KP, BASE + (int)KP); break;                    // 살짝 왼쪽 (PID ±1 보정 정도)
    case 'R': driveManualLR(BASE + (int)KP, BASE - (int)KP); break;                    // 살짝 오른쪽
    case 'Q': driveManualLR(CORNER_IN, CORNER_OUT); break;                             // 크게 왼쪽 (코너와 같음)
    case 'W': driveManualLR(CORNER_OUT, CORNER_IN); break;                             // 크게 오른쪽
    default:  brakeCar(); act = 's'; return;              // 's' 포함 : 브레이크로 바로 섬
  }
  act = tolower(c);                                       // 앱에는 소문자로 (g l r q w 와 같은 동작 이름)
}

// 수동 조종용 모터 힘 다듬기
//   - 출발하거나 방향이 바뀌는 순간 KICK_MS 동안 최대 힘 (처음에 안 도는 것 방지)
//   - 0 이 아닌데 MIN_PWM 보다 작으면 MIN_PWM
//   - TRIM 으로 좌우 모터 차이 맞춤
int kickSignL = 0, kickSignR = 0;
unsigned long kickUntilL = 0, kickUntilR = 0;

int shapePwm(int v, float trim, int &lastSign, unsigned long &kickUntil) {
  int s = (v > 0) - (v < 0);
  if (s == 0) { lastSign = 0; return 0; }
  if (s != lastSign) { lastSign = s; kickUntil = millis() + KICK_MS; }
  int a = min((int)(abs(v) * trim), 255);
  if (millis() < kickUntil) a = KICK_PWM;
  else if (a < MIN_PWM) a = MIN_PWM;
  return s * a;
}

void driveManualLR(int left, int right) {
  driveLR(shapePwm(left, TRIM_L, kickSignL, kickUntilL), shapePwm(right, TRIM_R, kickSignR, kickUntilR));
}

// 브레이크 : 모터 양쪽 선을 같이 LOW 로 묶어 바퀴를 바로 세움 (그냥 0 이면 굴러가다 섬)
void brakeCar() {
  kickSignL = kickSignR = 0;
  motorL = motorR = 0;
  digitalWrite(RightMotor_1_pin, LOW); digitalWrite(RightMotor_2_pin, LOW);
  digitalWrite(LeftMotor_3_pin, LOW);  digitalWrite(LeftMotor_4_pin, LOW);
  analogWrite(RightMotor_E_pin, 255);  analogWrite(LeftMotor_E_pin, 255);
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
  motorL = leftDir == HIGH ? leftSpeed : -leftSpeed;
  motorR = rightDir == HIGH ? rightSpeed : -rightSpeed;
  digitalWrite(RightMotor_1_pin, rightDir);
  digitalWrite(RightMotor_2_pin, !rightDir);
  digitalWrite(LeftMotor_3_pin, leftDir);
  digitalWrite(LeftMotor_4_pin, !leftDir);
  analogWrite(RightMotor_E_pin, rightSpeed);
  analogWrite(LeftMotor_E_pin, leftSpeed);
}
