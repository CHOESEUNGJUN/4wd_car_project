#pragma once
// PC 에서 스케치를 돌려 보기 위한 가짜 아두이노.
// 모터 핀 출력, 라인센서(dig), 초음파(echoUs), 블루투스 글자(BT.feed), 시간(T)을 흉내 낸다.
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <cstdio>
#include <queue>
#include <string>
#define HIGH 1
#define LOW 0
#define OUTPUT 1
#define INPUT 0
#define A3 17
#define A4 18
#define A5 19
#define F(s) (s)
#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))
#define constrain(x, a, b) ((x) < (a) ? (a) : ((x) > (b) ? (b) : (x)))
inline double radians(double d) { return d * M_PI / 180; }
typedef unsigned char byte;

// 시간 : millis() 를 부를 때마다 1ms 흐름 (기다리는 루프가 끝나도록). tickHook 은 그때마다 불림
inline unsigned long T = 0;
inline void (*tickHook)() = nullptr;
inline unsigned long millis() { T++; if (tickHook) tickHook(); return T; }
inline void delay(unsigned long ms) { T += ms; }

inline int pins[32], pwm[32], dig[32];
inline long echoUs = 0;   // 초음파 메아리 시간 (0 = 측정 안 됨)
inline void pinMode(int, int) {}
inline void digitalWrite(int p, int v) { pins[p] = v; }
inline void analogWrite(int p, int v) { pwm[p] = v; }
inline int analogRead(int p) { return dig[p] ? 800 : 100; }
inline int digitalRead(int p) { return dig[p]; }
inline void delayMicroseconds(int) {}
inline long pulseIn(int, int, long) { return echoUs; }

struct Ser {
  std::queue<char> in;   // 들어올 글자 (feed 로 넣음)
  std::string out;       // 보낸 글자
  void begin(long) {}
  int available() { return in.size(); }
  char read() { char c = in.front(); in.pop(); return c; }
  void write(char c) { out += c; }
  void print(char c) { out += c; }
  void print(long v) { out += std::to_string(v); }
  void print(int v) { print((long)v); }
  void print(unsigned long v) { print((long)v); }
  void print(const char* s) { out += s; }
  void print(double v, int d) { char b[32]; snprintf(b, sizeof b, "%.*f", d, v); out += b; }
  void println() { out += '\n'; }
  template <class X> void println(X x) { print(x); out += '\n'; }
  void feed(const char* s) { while (*s) in.push(*s++); }
};
typedef Ser Print;
inline Ser Serial;
struct SoftwareSerial : Ser { SoftwareSerial(int, int) {} };

struct Servo {
  int pos = 90;
  bool on = false;
  void attach(int) { on = true; }
  void detach() { on = false; }
  bool attached() { return on; }
  void write(int p) { pos = p; }
};

struct EEPROMClass {
  unsigned char mem[1024] = {0};
  template <class X> void get(int a, X& x) { memcpy(&x, mem + a, sizeof x); }
  template <class X> void put(int a, const X& x) { memcpy(mem + a, &x, sizeof x); }
};
inline EEPROMClass EEPROM;
