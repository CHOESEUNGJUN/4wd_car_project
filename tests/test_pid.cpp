// 버전 C (team2_bt_pid) 를 PC 에서 돌려 보는 테스트. 실행: tests/run.sh
#include "team2_bt_pid.ino"
#include <cassert>

int R() { return pins[8] == HIGH ? pwm[5] : -pwm[5]; }    // 오른쪽 바퀴 (음수 = 뒤로)
int L() { return pins[10] == HIGH ? pwm[6] : -pwm[6]; }   // 왼쪽 바퀴
bool alive = true;                                        // 앱이 켜져 있어 0.5초마다 h 를 보냄
unsigned long nextH = 0;
void hook() { if (alive && T >= nextH) { BT.feed("h"); nextH = T + 500; } }
void run(int ms) { unsigned long end = T + ms; while (T < end) loop(); }
void line(int l, int c, int r) { dig[A3] = l; dig[A4] = c; dig[A5] = r; }
// 차가 폰에 보낸 마지막 상태 줄
std::string last() {
  auto& o = BT.out; auto e = o.rfind('\n'); if (e == std::string::npos) return "";
  auto b = o.rfind('\n', e - 1); b = (b == std::string::npos) ? 0 : b + 1; return o.substr(b, e - b);
}
// 시리얼 모니터의 마지막 로그 줄
std::string logLine() { auto& o = Serial.out; auto e = o.rfind("링크="); return o.substr(e, o.find('\n', e) - e); }
bool sent(const char* s) { return BT.out.find(s) != std::string::npos; }
bool logHas(const char* s) { return logLine().find(s) != std::string::npos; }

int main() {
  tickHook = hook;
  setup();
  T = 1000;
  assert(!EduServo.attached());   // 서보는 정면 본 뒤 신호 끔 (떨림 방지)

  // 수동 : 받은 대로 움직이고, 시리얼 로그에 동작·명령·실제 모터 출력이 나옴
  BT.feed("g\r\n"); run(250);
  assert(L() == 150 && R() == 150);
  puts(logLine().c_str());
  assert(logHas("act=g cmd=g") && logHas("모터L=150 R=150") && logHas("장애물=X") && logHas("Kp40"));
  run(700); assert(L() == 0);                             // 0.6초 무명령이면 정지

  // 자동 : 0.5초 안전 지연 뒤 기본 속도 120 으로 직진
  line(0, 1, 0); BT.feed("a"); run(300); assert(mode == AUTO && L() == 0);
  run(500); assert(L() == 120 && R() == 120 && last() == "ag,999,010,LINE,0,0");

  // 선이 오른쪽 (0,1,1) -> 오른쪽으로. 첫 칸 P40 + D30 = 70
  line(0, 1, 1); run(21); assert(pidErr == 1 && L() == 190 && R() == 50);
  run(40); assert(L() < 190 && L() > 165);                // D 는 줄어들며 이어짐
  run(300); assert(L() >= 160 && L() <= 165);             // 0.3초 뒤엔 거의 P 만
  assert(logHas("오차=1 P=40"));

  // 선 위(오차 2)에서는 안쪽 바퀴를 뒤로 돌리지 않음
  line(0, 1, 0); run(200); line(0, 0, 1); run(21);
  assert(pidErr == 2 && R() == 0 && L() == 255);

  // 선이 왼쪽 (1,0,0) -> 왼쪽으로
  line(1, 0, 0); run(60); assert(pidErr == -2 && L() < R() && L() >= 0);

  // 선 놓침 -> 마지막 본 쪽으로 크게 (뒤로 허용), 1.5초 못 찾으면 정지
  line(0, 0, 0); run(60); assert(pidErr == -3 && L() < 0 && R() == 255);
  run(1600); assert(L() == 0 && !strcmp(stage, "LOST"));
  line(0, 1, 0); run(100); assert(L() == 120);

  // 설정 명령 : 자동 유지, 값 바뀌고, 폰에 돌려주고, EEPROM 에 저장
  BT.feed("#P30\n#D12.5\r\n#I0.5\n#B110\n"); run(100);
  assert(mode == AUTO && KP == 30 && KD == 12.5f && KI == 0.5f && BASE == 110);
  assert(sent("#P30.0,I0.50,D12.5,B110"));
  KP = 99; loadPid(); assert(KP == 30 && BASE == 110);

  // 장애물 -> 정지, 치우면 다시 출발
  echoUs = 300; run(200); assert(L() == 0 && !strcmp(stage, "OBST"));
  echoUs = 0; run(500); assert(L() == 110);

  // 자동 중 조종 명령 -> 지연 없이 수동
  BT.feed("b"); run(10); assert(mode == MANUAL && L() == -150);

  // 수동 중 앞에 장애물 -> 전진만 막고 후진은 됨, 로그에 장애물=O
  echoUs = 300; run(100);
  for (int k = 0; k < 3; k++) { BT.feed("g"); run(100); }
  assert(L() == 0 && logHas("act=o") && logHas("장애물=O"));
  BT.feed("b"); run(50); assert(L() == -150);
  echoUs = 0; run(800);

  // 자동 중 폰 신호가 끊기면 1.5초 안에 정지하고 수동으로
  line(0, 1, 0); alive = false; BT.feed("a"); run(700); assert(mode == AUTO && L() == 110);
  run(1000); assert(mode == MANUAL && L() == 0 && R() == 0);

  puts("버전 C 테스트 통과");
}
