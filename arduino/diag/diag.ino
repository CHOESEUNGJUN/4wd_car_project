// 진단 스케치: 블루투스로 글자가 들어오는지, 센서 값이 읽히는지 한 번에 본다.
// 업로드 후 시리얼 모니터를 9600으로 연다. 모터는 움직이지 않는다.
#include <SoftwareSerial.h>
#include <Servo.h>

const int PIN_L = A5, PIN_C = A4, PIN_R = A3;
const int TRIG = 13, ECHO = 12;
SoftwareSerial BT(3, 4);  // RX=D3 (모듈 TXD), TX=D4 (모듈 RXD)
Servo myservo;

unsigned long rxCount = 0;

long readCm() {
  digitalWrite(TRIG, LOW); delayMicroseconds(2);
  digitalWrite(TRIG, HIGH); delayMicroseconds(10);
  digitalWrite(TRIG, LOW);
  long us = pulseIn(ECHO, HIGH, 25000);
  if (us == 0) return 999;
  return us * 0.0343 / 2;
}

void setup() {
  pinMode(TRIG, OUTPUT); pinMode(ECHO, INPUT);
  // 정면으로 돌린 뒤 서보 신호를 끔 (켜 두면 블루투스 통신 때문에 덜덜 떤다)
  myservo.attach(2); myservo.write(90); delay(400); myservo.detach();
  Serial.begin(9600);
  BT.begin(9600);
  Serial.println("진단 시작. 폰에서 글자를 보내 보세요.");
}

void loop() {
  // 블루투스로 들어온 글자를 그대로 보여 주고(16진수 포함), 폰으로 되돌려 보낸다
  while (BT.available()) {
    char c = BT.read();
    rxCount++;
    Serial.print("BT 수신: '");
    if (c >= 32 && c < 127) Serial.print(c); else Serial.print('?');
    Serial.print("' 0x"); Serial.println((byte)c, HEX);
    BT.write(c);
  }
  // 시리얼 모니터에서 친 글자는 폰으로 보낸다
  while (Serial.available()) BT.write(Serial.read());

  static unsigned long last = 0;
  if (millis() - last >= 1000) {
    last = millis();
    Serial.print("L="); Serial.print(analogRead(PIN_L));
    Serial.print(" C="); Serial.print(analogRead(PIN_C));
    Serial.print(" R="); Serial.print(analogRead(PIN_R));
    Serial.print("  거리="); Serial.print(readCm());
    Serial.print("cm  BT누적="); Serial.println(rxCount);
  }
}
