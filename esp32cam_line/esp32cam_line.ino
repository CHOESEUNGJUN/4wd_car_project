/* 굴노 카메라 라인 인식 (ESP32-CAM, AI-Thinker 보드, OV2640)
 *  - 바닥을 흑백 160x120 으로 찍어서 검은 선 위치를 찾는다
 *      가까운 띠(화면 아래쪽) : 지금 차가 선에서 얼마나 벗어났는지
 *      먼 띠(화면 위쪽)       : 앞으로 올 커브 방향
 *  - 결과를 시리얼(U0T, 9600)로 아두이노 D0 에 보낸다. 한 줄 형식 (docs/CAMERA.md)
 *      C,<가까운 위치>,<먼 위치>,<플래그>\n
 *        위치 : -100(화면 왼쪽 끝) ~ 0(가운데) ~ +100(오른쪽 끝)
 *        플래그 : 1 = 가까운 띠에서 선 찾음, 2 = 먼 띠에서 찾음, 4 = 교차선(띠 대부분이 검정)
 *    9600 인 이유 : 아두이노가 블루투스(SoftwareSerial)를 받는 동안 빠른 시리얼 글자를 놓치기 때문
 *  - 와이파이는 프린터처럼 : 처음(또는 저장된 핫스팟을 못 찾으면) 설정용 와이파이 "GULNO-CAM-SETUP-xxxx" 를 켠다.
 *    폰으로 접속하면 설정 화면이 뜨고, 내 핫스팟 이름·암호를 넣으면 ESP32 안에 저장하고 그 핫스팟에 붙는다.
 *    핫스팟 암호는 코드에 없고, 설정용 와이파이 암호는 처음 켤 때 무작위로 만들어 시리얼에 찍는다 (docs/CAMERA.md 6장)
 *  - 핫스팟에 붙은 뒤 같은 핫스팟의 폰/PC 에서 http://gulno-cam.local (안 되면 시리얼의 I,ip 주소) 을 열면
 *    카메라 화면 + 인식 결과를 보고 어두움 기준을 바로 조절할 수 있다 (저장은 안 됨, 맞는 값은 아래 숫자로 고정)
 *  보드 : "AI Thinker ESP32-CAM". SD카드 필요 없음
 */

#include "esp_camera.h"
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <ESPmDNS.h>
#include "img_converters.h"

// ============================================================
// [설정]
// ============================================================
const char* SETUP_SSID = "GULNO-CAM-SETUP";   // 설정용 와이파이 이름 (뒤에 칩 번호 4자리가 붙음)
const char* HOSTNAME = "gulno-cam";            // 핫스팟에서 http://gulno-cam.local
const unsigned long CONNECT_WAIT_MS = 15000;   // 켤 때 저장된 핫스팟을 이만큼 기다리고, 못 붙으면 설정 모드
// 핫스팟 이름·암호는 여기 적지 않는다 (공개 저장소). 설정 화면에서 넣으면 ESP32 안(Preferences)에 저장된다

const int W = 160, H = 120;               // 흑백 QQVGA
const int NEAR_Y0 = 85, NEAR_Y1 = 110;    // 가까운 띠 (화면 아래쪽 줄 범위)
const int FAR_Y0 = 35, FAR_Y1 = 55;       // 먼 띠 (화면 위쪽 줄 범위)
int darkOffset = 40;                      // 띠 평균 밝기보다 이만큼 어두우면 "선". 화면에서 바꿀 수 있음
const int MIN_DARK_COLS = 3;              // 선으로 칠 최소 폭 (칸). 이보다 좁으면 잡음
const float CROSS_RATIO = 0.6;            // 띠 폭의 이 비율 넘게 검정이면 교차선
const bool FLIP_V = false, MIRROR_H = false;   // 카메라를 거꾸로 달았으면 true
const unsigned long SEND_MS = 50;         // 아두이노로 보내는 간격 (초당 20번)

// AI-Thinker ESP32-CAM 핀
#define PWDN_GPIO_NUM 32
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM 0
#define SIOD_GPIO_NUM 26
#define SIOC_GPIO_NUM 27
#define Y9_GPIO_NUM 35
#define Y8_GPIO_NUM 34
#define Y7_GPIO_NUM 39
#define Y6_GPIO_NUM 36
#define Y5_GPIO_NUM 21
#define Y4_GPIO_NUM 19
#define Y3_GPIO_NUM 18
#define Y2_GPIO_NUM 5
#define VSYNC_GPIO_NUM 25
#define HREF_GPIO_NUM 23
#define PCLK_GPIO_NUM 22
#define FLASH_LED 4

// ============================================================
struct Band { bool found; bool cross; int pos; int cx; };

WebServer server(80);
DNSServer dns;                            // 설정 모드 : 어떤 주소를 쳐도 설정 화면으로 (공항 와이파이처럼)
Preferences prefs;
bool setupMode = false;
String setupName, setupPass, ssidOptions, myIp, savedSsid, failWhy;
unsigned long lastInfo = 0;
uint8_t lastFrame[W * H];                 // 화면 보여 주기용 마지막 프레임
Band lastNear, lastFar;
unsigned long frames = 0, lastFpsAt = 0, lastSend = 0;
float fps = 0;

// 띠(y0~y1 줄) 안에서 검은 선의 가운데를 찾는다
Band findLine(const uint8_t* img, int y0, int y1) {
  Band b = {false, false, 0, -1};
  long sum = 0;
  for (int y = y0; y < y1; y++)
    for (int x = 0; x < W; x++) sum += img[y * W + x];
  int th = sum / ((long)(y1 - y0) * W) - darkOffset;   // 조명이 바뀌어도 따라가는 임계값

  // 열마다 어두운 칸이 띠 높이의 절반 넘으면 그 열은 "검정"
  int need = (y1 - y0) / 2, darkCols = 0;
  long wsum = 0;
  for (int x = 0; x < W; x++) {
    int dark = 0;
    for (int y = y0; y < y1; y++) if (img[y * W + x] < th) dark++;
    if (dark > need) { darkCols++; wsum += x; }
  }
  if (darkCols < MIN_DARK_COLS) return b;
  b.found = true;
  b.cross = darkCols > W * CROSS_RATIO;
  b.cx = wsum / darkCols;
  b.pos = (b.cx - W / 2) * 100 / (W / 2);
  return b;
}

// ---- 웹 화면 ----
const char PAGE[] PROGMEM = R"HTML(<!doctype html><html lang="ko"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1"><title>굴노 카메라</title>
<style>body{font-family:system-ui;margin:12px;background:#111;color:#eee}img{width:100%;max-width:480px;image-rendering:pixelated;border:1px solid #444}
input{width:100%;max-width:480px}.k{color:#9cf}</style></head><body>
<h3>굴노 카메라 라인 인식</h3>
<img id="f" src="/frame"><div id="s">...</div>
<p>어두움 기준 (띠 평균보다 몇 어두우면 선) <b id="v"></b><br><input id="o" type="range" min="5" max="120"></p>
<p class="k">흰 가로줄 = 띠 범위, 흑백 세로줄 = 찾은 선 가운데, 회색 점선 = 화면 가운데</p>
<h3>조종 (와이파이)</h3>
<div class="pad"><button data-c="q">↺</button><button data-c="g">▲</button><button data-c="w">↻</button>
<button data-c="l">◀</button><button data-c="s">■</button><button data-c="r">▶</button>
<span></span><button data-c="b">▼</button><span></span></div>
<p><button id="auto">자동으로</button> <button id="man">수동으로</button> <span id="cs"></span></p>
<style>.pad{display:grid;grid-template-columns:repeat(3,80px);gap:8px}.pad button,#auto,#man{font-size:26px;padding:14px;touch-action:none;user-select:none}</style>
<script>
const $=i=>document.getElementById(i);
async function tick(){
  try{const s=await (await fetch('/status')).json();
    $('s').textContent=`가까운 ${s.nf?s.np:'없음'} · 먼 ${s.ff?s.fp:'없음'}${s.cross?' · 교차선':''} · ${s.fps} fps`;
    $('v').textContent=s.off; if(document.activeElement!==$('o'))$('o').value=s.off;}catch(e){}
  $('f').src='/frame?'+Date.now();
}
$('o').onchange=()=>fetch('/set?off='+$('o').value);
setInterval(tick,250);
// 앱과 같은 규칙 : 누르는 동안 0.2초마다 같은 글자, 떼면 s 두 번, 0.6초마다 h (연결 신호)
const send=c=>fetch('/cmd?c='+c).then(r=>{$('cs').textContent=r.ok?'':'조종 안 됨 (설정 모드)';}).catch(()=>{$('cs').textContent='연결 끊김';});
let held=null,rep=null;
function press(c){release(false);held=c;send(c);rep=setInterval(()=>send(c),200);}
function release(stop=true){if(rep){clearInterval(rep);rep=null;} if(held&&stop){send('s');setTimeout(()=>send('s'),60);} held=null;}
document.querySelectorAll('.pad button').forEach(b=>{
  b.onpointerdown=e=>{e.preventDefault();press(b.dataset.c);};
  b.onpointerup=b.onpointerleave=b.onpointercancel=()=>release();
});
$('auto').onclick=()=>{if(confirm('자동 모드로 전환하시겠습니까?'))send('a');};
$('man').onclick=()=>send('m');
setInterval(()=>send('h'),600);
document.addEventListener('visibilitychange',()=>{if(document.hidden)release();});
</script></body></html>)HTML";

void handleFrame() {
  static uint8_t view[W * H];
  memcpy(view, lastFrame, sizeof view);
  for (int x = 0; x < W; x++) {           // 띠 경계
    view[NEAR_Y0 * W + x] = view[(NEAR_Y1 - 1) * W + x] = 255;
    view[FAR_Y0 * W + x] = view[(FAR_Y1 - 1) * W + x] = 255;
  }
  for (int y = 0; y < H; y += 3) view[y * W + W / 2] = 128;   // 화면 가운데
  const Band* bs[2] = {&lastNear, &lastFar};
  const int ys[2][2] = {{NEAR_Y0, NEAR_Y1}, {FAR_Y0, FAR_Y1}};
  for (int i = 0; i < 2; i++) if (bs[i]->found)                // 찾은 선 가운데
    for (int y = ys[i][0]; y < ys[i][1]; y++)
      for (int dx = -1; dx <= 1; dx++) { int x = bs[i]->cx + dx; if (x >= 0 && x < W) view[y * W + x] = (y & 1) ? 0 : 255; }
  uint8_t* jpg = nullptr; size_t len = 0;
  if (!fmt2jpg(view, sizeof view, W, H, PIXFORMAT_GRAYSCALE, 70, &jpg, &len)) { server.send(500); return; }
  server.sendHeader("Cache-Control", "no-store");
  server.send_P(200, "image/jpeg", (const char*)jpg, len);
  free(jpg);
}

void handleStatus() {
  char buf[160];
  snprintf(buf, sizeof buf, "{\"nf\":%d,\"np\":%d,\"ff\":%d,\"fp\":%d,\"cross\":%d,\"off\":%d,\"fps\":%.1f}",
           lastNear.found, lastNear.pos, lastFar.found, lastFar.pos, lastNear.cross, darkOffset, fps);
  server.send(200, "application/json", buf);
}

// 웹 조종 글자를 선(U0T)으로 아두이노에 "K,g" 처럼 전달. 버전 D 가 블루투스 글자와 똑같이 처리
void handleCmd() {
  String c = server.arg("c");
  if (setupMode || c.length() != 1 || !strchr("gblrsqwamh", c[0])) { server.send(403); return; }
  Serial.printf("K,%c\n", c[0]);
  server.send(204);
}

void handleSet() {
  if (server.hasArg("off")) darkOffset = constrain(server.arg("off").toInt(), 5, 120);
  server.send(200, "text/plain", "ok");
}

// ---- 와이파이 설정 (프린터처럼) ----
String esc(String t) { t.replace("&", "&amp;"); t.replace("<", "&lt;"); t.replace(">", "&gt;"); t.replace("\"", "&quot;"); return t; }

void handleSetupPage() {
  String h = F("<!doctype html><html lang=\"ko\"><head><meta charset=\"utf-8\"><meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
               "<title>굴노 카메라 설정</title><style>body{font-family:system-ui;margin:16px;background:#111;color:#eee}"
               "input,button{font-size:18px;width:100%;max-width:420px;margin:6px 0;padding:8px}</style></head><body>"
               "<h3>굴노 카메라 와이파이 설정</h3><p>연결할 핫스팟(2.4GHz)을 넣으세요. 아이폰은 핫스팟의 '호환성 최대화'를 켭니다.</p>"
               "<form method=\"post\" action=\"/save\"><input name=\"s\" list=\"l\" placeholder=\"핫스팟 이름\" required>"
               "<datalist id=\"l\">");
  h += ssidOptions;
  h += F("</datalist>");
  if (savedSsid.length()) h += "<p style=\"color:#fc6\">저장된 핫스팟 <b>" + esc(savedSsid) + "</b> : " + failWhy +
                               "<br>핫스팟이 보이면 알아서 다시 붙습니다 (아이폰은 개인용 핫스팟 화면을 열어 두기).</p>";
  h += F("<input name=\"p\" type=\"password\" placeholder=\"핫스팟 암호\"><button>저장하고 연결</button></form>"
         "<p><a href=\"/cam\" style=\"color:#9cf\">카메라 화면 보기</a></p></body></html>");
  server.send(200, "text/html", h);
}

void handleSave() {
  String s = server.arg("s"), p = server.arg("p");
  if (s.length() == 0 || s.length() > 32 || p.length() > 63) { server.send(400, "text/plain", "이름 1~32자, 암호 63자 이하"); return; }
  prefs.putString("ssid", s);
  prefs.putString("pass", p);
  server.send(200, "text/html", F("<meta charset=\"utf-8\"><body style=\"font-family:system-ui;background:#111;color:#eee\">"
                                  "<h3>저장했습니다</h3><p>카메라가 다시 켜지며 핫스팟에 연결합니다. 폰도 그 핫스팟으로 돌아가서 "
                                  "http://gulno-cam.local 을 여세요.</p></body>"));
  delay(800);
  ESP.restart();
}

// 설정용 와이파이 암호 : 처음 한 번 무작위로 만들어 저장 (프린터 스티커 같은 것. 코드·칩 번호로는 알 수 없음)
String getSetupPass() {
  String pw = prefs.getString("setup_pw", "");
  if (pw.length() == 8) return pw;
  const char* cs = "abcdefghjkmnpqrstuvwxyz23456789";   // 헷갈리는 글자(l, 1, o, 0, i) 뺌
  for (int i = 0; i < 8; i++) pw += cs[esp_random() % 31];
  prefs.putString("setup_pw", pw);
  return pw;
}

void startWifi() {
  prefs.begin("gulno", false);
  WiFi.mode(WIFI_STA);
  WiFi.enableIPv6();                       // 아이폰 핫스팟은 맥에 IPv6 만 주기도 해서, IPv6 로도 접속되게
  WiFi.setHostname(HOSTNAME);
  String ssid = prefs.getString("ssid", "");
  savedSsid = ssid;
  if (ssid.length()) {                     // 저장된 핫스팟에 붙어 보기
    WiFi.begin(ssid.c_str(), prefs.getString("pass", "").c_str());
    unsigned long t0 = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t0 < CONNECT_WAIT_MS) delay(200);
  }
  if (WiFi.status() == WL_CONNECTED) {
    WiFi.setAutoReconnect(true);           // 잠깐 끊겨도 알아서 다시 붙음
    myIp = WiFi.localIP().toString();
    MDNS.begin(HOSTNAME);
    MDNS.addService("http", "tcp", 80);
    return;
  }
  // 설정 모드 : 주변 핫스팟 목록을 받아 두고 설정용 와이파이를 켬
  int st = WiFi.status();
  failWhy = ssid.length() == 0 ? "저장된 핫스팟 없음"
          : st == WL_NO_SSID_AVAIL ? "핫스팟을 못 찾음 (꺼짐 / 5GHz / 숨김)"
          : st == WL_CONNECT_FAILED ? "암호가 틀림"
          : "연결 안 됨 (" + String(st) + ")";
  setupMode = true;
  WiFi.disconnect();
  int n = WiFi.scanNetworks();
  for (int i = 0; i < n; i++) ssidOptions += "<option value=\"" + esc(WiFi.SSID(i)) + "\">";
  char suffix[8];
  snprintf(suffix, sizeof suffix, "-%04X", (unsigned)(ESP.getEfuseMac() >> 32) & 0xFFFF);
  setupName = String(SETUP_SSID) + suffix;
  setupPass = getSetupPass();
  WiFi.mode(ssid.length() ? WIFI_AP_STA : WIFI_AP);   // 저장된 핫스팟이 있으면 설정 모드에서도 계속 다시 찾음
  WiFi.softAP(setupName.c_str(), setupPass.c_str());
  if (ssid.length()) WiFi.begin(ssid.c_str(), prefs.getString("pass", "").c_str());
  myIp = WiFi.softAPIP().toString();
  dns.start(53, "*", WiFi.softAPIP());
}

bool startCamera() {
  camera_config_t c = {};
  c.ledc_channel = LEDC_CHANNEL_0; c.ledc_timer = LEDC_TIMER_0;
  c.pin_d0 = Y2_GPIO_NUM; c.pin_d1 = Y3_GPIO_NUM; c.pin_d2 = Y4_GPIO_NUM; c.pin_d3 = Y5_GPIO_NUM;
  c.pin_d4 = Y6_GPIO_NUM; c.pin_d5 = Y7_GPIO_NUM; c.pin_d6 = Y8_GPIO_NUM; c.pin_d7 = Y9_GPIO_NUM;
  c.pin_xclk = XCLK_GPIO_NUM; c.pin_pclk = PCLK_GPIO_NUM; c.pin_vsync = VSYNC_GPIO_NUM; c.pin_href = HREF_GPIO_NUM;
  c.pin_sccb_sda = SIOD_GPIO_NUM; c.pin_sccb_scl = SIOC_GPIO_NUM;
  c.pin_pwdn = PWDN_GPIO_NUM; c.pin_reset = RESET_GPIO_NUM;
  c.xclk_freq_hz = 20000000;
  c.pixel_format = PIXFORMAT_GRAYSCALE;    // 흑백 : 선 찾기에 충분하고 빠름
  c.frame_size = FRAMESIZE_QQVGA;          // 160 x 120
  c.fb_count = 2;
  c.fb_location = CAMERA_FB_IN_PSRAM;
  c.grab_mode = CAMERA_GRAB_LATEST;        // 항상 가장 최근 프레임
  if (esp_camera_init(&c) != ESP_OK) return false;
  sensor_t* s = esp_camera_sensor_get();
  s->set_vflip(s, FLIP_V);
  s->set_hmirror(s, MIRROR_H);
  return true;
}

void setup() {
  Serial.begin(9600);                      // U0T(GPIO1) -> 아두이노 D0(RX)
  pinMode(FLASH_LED, OUTPUT); digitalWrite(FLASH_LED, LOW);
  if (!startCamera()) {
    while (true) { Serial.println("E,camera"); delay(1000); }   // 카메라 리본 케이블 확인
  }
  startWifi();
  server.on("/", [] { if (setupMode) handleSetupPage(); else server.send_P(200, "text/html", PAGE); });
  server.on("/cmd", handleCmd);
  server.on("/cam", [] { server.send_P(200, "text/html", PAGE); });
  server.on("/save", HTTP_POST, [] { if (setupMode) handleSave(); else server.send(403); });
  server.onNotFound([] {                   // 설정 모드 : 폰이 여는 아무 주소나 설정 화면으로
    if (setupMode) { server.sendHeader("Location", "http://" + myIp + "/"); server.send(302); }
    else server.send(404);
  });
  server.on("/frame", handleFrame);
  server.on("/status", handleStatus);
  server.on("/set", handleSet);
  server.begin();
}

void loop() {
  camera_fb_t* fb = esp_camera_fb_get();
  if (fb) {
    if (fb->len >= (size_t)(W * H)) {
      lastNear = findLine(fb->buf, NEAR_Y0, NEAR_Y1);
      lastFar = findLine(fb->buf, FAR_Y0, FAR_Y1);
      memcpy(lastFrame, fb->buf, W * H);
      frames++;
    }
    esp_camera_fb_return(fb);
  }
  if (millis() - lastSend >= SEND_MS) {    // 아두이노로 (9600 이라 초당 20번만)
    lastSend = millis();
    int flags = (lastNear.found ? 1 : 0) | (lastFar.found ? 2 : 0) | (lastNear.cross ? 4 : 0);
    Serial.printf("C,%d,%d,%d\n", lastNear.pos, lastFar.pos, flags);
  }
  if (millis() - lastFpsAt >= 1000) { fps = frames * 1000.0 / (millis() - lastFpsAt); frames = 0; lastFpsAt = millis(); }
  if (lastInfo == 0 || millis() - lastInfo >= 10000) {     // 안내 줄 (I 로 시작 : 아두이노는 C 줄만 읽어서 상관없음)
    lastInfo = millis();
    if (setupMode) {
      Serial.printf("I,setup,%s,%s\n", setupName.c_str(), setupPass.c_str());
      if (savedSsid.length()) {
        Serial.printf("I,fail,%s,%s\n", savedSsid.c_str(), failWhy.c_str());
        WiFi.begin(savedSsid.c_str(), prefs.getString("pass", "").c_str());   // 10초마다 다시 시도
      }
    }
    else Serial.printf("I,ip,%s,%s.local\n", myIp.c_str(), HOSTNAME);
  }
  if (setupMode) {
    dns.processNextRequest();
    if (savedSsid.length() && WiFi.status() == WL_CONNECTED) {   // 핫스팟이 나타나서 붙음 -> 설정 모드 끝
      Serial.printf("I,joined,%s\n", savedSsid.c_str());
      delay(300);
      ESP.restart();                       // 다시 켜면서 핫스팟 모드로 (켤 때 바로 붙음)
    }
  }
  server.handleClient();
}
