/* 굴노 카메라 : 신호등 보기 (ESP32-CAM, AI-Thinker 보드, OV2640)
 *  - 원칙 : 카메라로 라인 주행은 하지 않는다. 라인은 차(아두이노)가 라인센서 3개 + PID 로 따라가고,
 *    카메라는 신호등(빨강·초록·노랑)만 보고 차에 블루투스로 신호 글자를 보낸다 (x 정지 / v 서행 / n 보통, 0.2초마다)
 *  - 와이파이는 프린터처럼 : 처음(또는 저장된 핫스팟을 못 찾으면) 설정용 와이파이 "GULNO-CAM-SETUP-xxxx" 를 켠다.
 *    폰으로 접속하면 설정 화면이 뜨고, 내 핫스팟 이름·암호를 넣으면 ESP32 안에 저장하고 그 핫스팟에 붙는다.
 *    핫스팟 암호는 코드에 없고, 설정용 와이파이 암호는 처음 켤 때 무작위로 만들어 시리얼에 찍는다 (docs/CAMERA.md)
 *  - 핫스팟에 붙은 뒤 같은 핫스팟의 폰/PC 에서 http://gulno-cam.local : 컬러 영상, 신호, 차 상태, 조종, 블루투스, SD 기록
 *  보드 : "AI Thinker ESP32-CAM". 올리기는 PlatformIO (docs/CAMERA.md)
 */

#include "esp_camera.h"
#include <WiFi.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <ESPmDNS.h>
#include "img_converters.h"
#include "esp_http_server.h"
#include "BluetoothSerial.h"
#include <atomic>
#include "SD_MMC.h"

// ============================================================
// [설정]
// ============================================================
const char* SETUP_SSID = "GULNO-CAM-SETUP";   // 설정용 와이파이 이름 (뒤에 칩 번호 4자리가 붙음)
const char* HOSTNAME = "gulno-cam";            // 핫스팟에서 http://gulno-cam.local
const char* BT_PIN = "1234";                   // HC-06 기본 PIN
const unsigned long CONNECT_WAIT_MS = 15000;   // 켤 때 저장된 핫스팟을 이만큼 기다리고, 못 붙으면 설정 모드
// 핫스팟 이름·암호는 여기 적지 않는다 (공개 저장소). 설정 화면에서 넣으면 ESP32 안(Preferences)에 저장된다

const int W = 160, H = 120;               // 컬러 QQVGA
const int FLASH_DUTY = 30;                // 앞 플래시 LED 밝기 (0~255). 최대로 오래 켜면 뜨겁고 전원이 약하면 재부팅
const int JPEG_Q = 40, JPEG_Q_BT = 25;     // 화면 영상 압축 (낮을수록 많이 압축 = 작고 흐림). _BT = 차와 블루투스로 연결됐을 때
const int STREAM_MS = 100, STREAM_MS_BT = 200;   // 영상 한 장 간격 (초당 10장 / 블루투스 연결 중 5장)
// 안테나 하나를 와이파이·블루투스가 나눠 써서, 영상이 무거우면 조종 글자(블루투스)가 늦어진다 -> 차와 연결되면 영상을 가볍게
// 신호등 인식 : 사진에서 빨강·초록·노랑 덩어리를 찾음 (교안 3·4팀처럼 색 범위로). 아이폰 앱 굴노의 "신호등 화면"을 신호등으로 씀
volatile bool sigMode = false;            // 웹 화면 "신호등 인식" 버튼으로 켬 (저장)
volatile uint8_t sigState = 0;            // 0 = 없음, 1 = 빨강(정지), 2 = 초록(주행), 3 = 노랑(서행), 4 = 정지 표지판 (3장 연속 같아야 바뀜, 빨강은 2장)
const int SIG_Y1 = H;                     // 이 줄 위쪽만 봄. H = 화면 전체 (카메라가 라인을 안 보니 바닥 테이프와 섞일 일이 없음). 바닥 색이 걸리면 줄임
const int SIG_MIN = 40;                   // 2칸마다 본 픽셀 중 이만큼 넘게 그 색이면 신호 (화면 전체면 4800개 중)
volatile int sigR = 0, sigG = 0, sigY = 0; // 마지막 사진에서 센 빨강·초록·노랑 픽셀 수 (웹 화면·로그로 기준 맞추기)
// 정지 표지판 = 빨간 팔각형 + 흰 테두리·글자. 신호등(폰 화면 빨강)과 같은 빨강이라 모양으로 가름 :
//   빨강 덩어리를 감싼 네모 칸에서 빨강이 차지하는 비율(채움)이 SIGN_FILL_MAX% 보다 작고 (팔각형은 모서리가 비고 글자가 있음)
//   그 칸 안에 흰색이 SIGN_WHITE_MIN% 넘게 있으면 표지판. 폰 빨간 화면은 채움 90% 넘고 흰색이 거의 없음
const int SIGN_FILL_MAX = 80;             // % (가짜 사진 시험 : 표지판 61~68%, 폰 빨간 화면 86~100% (아래 버튼 칸 포함) -> 사이)
const int SIGN_WHITE_MIN = 4;             // %
const unsigned long STOP_WAIT_MS = 3000;  // 정지 표지판 : 이만큼 서 있다가 스스로 출발 (빨간불은 초록이 될 때까지)
const unsigned long STOP_IGNORE_MS = 5000; // 출발한 뒤 이만큼은 같은 표지판을 무시 (지나가는 동안 또 서지 않게)
volatile int sigFill = 0, sigWhite = 0;   // 마지막 사진의 빨강 덩어리 채움 % / 흰색 % (웹 화면·로그)
volatile unsigned long stopUntil = 0;     // 정지 표지판으로 서 있는 끝 시각 (0 = 아님)
unsigned long stopLeft() { long d = (long)(stopUntil - millis()); return stopUntil && d > 0 ? d : 0; }
bool camFlip = false, camMirror = true;   // 화면 위아래 / 좌우. 이 보드는 그대로면 좌우가 바뀌어 보여서 좌우 반전이 기본. 웹 화면에서 바꾸면 저장됨

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
#define STATUS_LED 33                     // 뒷면 작은 빨간 LED (LOW = 켜짐)

// ============================================================

// ---- 웹 서버 하나 (포트 80) : 화면, 조종, 영상 모두. 영상만 화면마다 따로 작업으로 보냄 ----
httpd_handle_t web = nullptr;
httpd_req_t* cur = nullptr;               // 지금 처리 중인 요청 (서버는 요청을 하나씩 처리)
String curArgs;                           // 그 요청의 a=1&b=2 (주소 뒤 ? 또는 POST 본문)

String urlDecode(const char* t) {
  String o; char h[3] = {0};
  for (; *t; t++) {
    if (*t == '+') o += ' ';
    else if (*t == '%' && t[1] && t[2]) { h[0] = t[1]; h[1] = t[2]; o += (char)strtol(h, nullptr, 16); t += 2; }
    else o += *t;
  }
  return o;
}
bool hasArg(const char* k) { char v[2]; return httpd_query_key_value(curArgs.c_str(), k, v, sizeof v) != ESP_ERR_NOT_FOUND; }
String arg(const char* k) {
  char v[100] = {0};
  if (httpd_query_key_value(curArgs.c_str(), k, v, sizeof v) != ESP_OK) return "";
  return urlDecode(v);
}
void send(int code, const char* type = "text/plain", const String& body = "") {
  const char* st = code == 200 ? "200 OK" : code == 204 ? "204 No Content" : code == 302 ? "302 Found" : code == 400 ? "400 Bad Request"
                 : code == 403 ? "403 Forbidden" : code == 503 ? "503 Service Unavailable" : "404 Not Found";
  httpd_resp_set_status(cur, st);
  httpd_resp_set_type(cur, type);
  httpd_resp_set_hdr(cur, "Cache-Control", "no-store");
  httpd_resp_send(cur, body.c_str(), body.length());
}
BluetoothSerial BT;
volatile bool btReady = false;            // BT.begin 끝남 (그 전에 BT 를 건드리면 멈춤)
bool btOn() { return btReady && BT.connected(); }
char carLine[64] = "";                    // 아두이노 상태 줄 (loop 에서만 씀. 큰따옴표·역슬래시는 빼고 저장)
volatile bool carClear = false;           // 블루투스 끊김 -> 상태 줄 비우기 (btTask 가 표시, loop 가 비움)
volatile bool btWant = false;             // 연결 유지 (끄면 폰 앱이 HC-06 을 쓸 수 있음)
volatile bool btScanReq = false, btScanning = false, btFail = false;
const uint32_t HEAP_MIN = 12000;          // 내부 메모리가 이보다 적게 5초 이어지면 재시작 (아래 loop)
const uint32_t BT_NEED = 40000;           // 블루투스를 켤 때 남아 있어야 할 내부 메모리 (PlatformIO 빌드는 블루투스가 외부 메모리를 먼저 써서 적게 필요)
// 블루투스 상태는 여러 작업(웹 서버, 블루투스 작업, loop)이 같이 씀 -> 반드시 btLock 을 잡고 읽고 씀
// (안 잡으면 한쪽이 String 을 바꾸는 사이 다른 쪽이 읽다가 메모리가 깨져서 멈출 수 있음)
SemaphoreHandle_t btLock = xSemaphoreCreateMutex();   // btAddr, btName, btLabel, btList
SemaphoreHandle_t txLock = xSemaphoreCreateMutex();   // BT.write (조종 글자는 웹 서버, 연결 신호 h 는 loop 가 보냄)
String btAddr, btName;                    // 연결할 기기 (목록에서 고르면 ESP32 에 저장)
void btSend(char c) { xSemaphoreTake(txLock, portMAX_DELAY); BT.write(c); xSemaphoreGive(txLock); }
char btLabel[40] = "";                    // 화면에 보일 기기 이름 (바꿀 때 한 번만 만들어 둠)
void setBtLabel() {
  const String& s = btName.length() ? btName : btAddr;
  size_t n = 0;
  for (size_t i = 0; i < s.length() && n < sizeof btLabel - 1; i++) if (s[i] != '"' && s[i] != '\\' && s[i] != '<') btLabel[n++] = s[i];
  btLabel[n] = 0;
}
String btList = "[]";                     // 찾은 기기 목록 JSON [{"n":이름,"a":주소,"r":신호}]
DNSServer dns;                            // 설정 모드 : 어떤 주소를 쳐도 설정 화면으로 (공항 와이파이처럼)
Preferences prefs;
bool setupMode = false;
String setupName, setupPass, ssidOptions, myIp, savedSsid, failWhy;
unsigned long lastInfo = 0;
String esc(String t);                     // 아래 와이파이 설정 부분에 있음
// 사진은 판단하고 버림. 화면용 버퍼는 전부 외부 메모리(PSRAM) : 내부 메모리는 와이파이·블루투스가 써야 해서
uint8_t* color;                           // 화면용 컬러 RGB565 (W x H x 2)
uint8_t* view;                            // 신호 표시를 그린 화면 복사본 (RGB565)
unsigned long frames = 0, lastFpsAt = 0, lastViewer = 0, lastHb = 0;
float fps = 0;
volatile uint32_t tSig = 0, tCopy = 0, tJpeg = 0, nJ = 0;   // 단계별 시간 (µs 합, 시리얼 I,step_us)
volatile uint32_t colorNo = 0;            // 컬러 버퍼가 새 사진으로 바뀔 때마다 +1 (영상은 새 사진일 때만 JPEG 로)

// ---- 웹 화면 ----
// ---- SD 카드 기록 (주행을 다시 보면서 값 맞추기) ----
// 영상 : 신호 표시(테두리 색)를 그린 컬러 JPEG 을 0.2초마다 이어 붙인 rec_NNNN.mjpeg (VLC 로 바로, 또는 ffmpeg -i rec_0001.mjpeg rec.mp4)
// 로그 : 0.1초마다 한 줄 log_NNNN.csv. frame 열 = 그때까지 저장한 영상 장 수 (영상과 맞춰 보기)
// SD 는 1비트 모드 (4비트는 앞 LED GPIO4 와 겹침). FAT32. 작업은 코어 0 에서 (신호 처리를 안 막음)
const int REC_Q = 85;                     // 기록 JPEG 품질 (웹 영상 40 보다 선명)
const unsigned long REC_FRAME_MS = 200, REC_LOG_MS = 100, REC_FLUSH_MS = 2000;
volatile bool sdOk = false, recOn = false, recRunning = false;   // recRunning = 기록 작업이 떠 있음 (기록할 때만 띄우고 끝나면 지움 : 메모리)
volatile int recNo = 0;
volatile uint32_t recFrames = 0;
bool makeJpeg(uint8_t** jpg, size_t* len, int q);
extern SemaphoreHandle_t jpgLock;

void recTask(void*) {                     // 기록 하나 : 붙이고 -> 파일 열고 -> recOn 이 꺼질 때까지 쓰고 -> 닫고 떼고 -> 작업 끝 (메모리 돌려줌)
  char p1[24], p2[24], line[200];
  snprintf(p1, sizeof p1, "/rec_%04d.mjpeg", recNo);
  snprintf(p2, sizeof p2, "/log_%04d.csv", recNo);
  File vid, lg;
  if (SD_MMC.begin("/sdcard", true, false, SDMMC_FREQ_DEFAULT, 2)) {   // 1비트 모드, 포맷 안 함, 파일 2개까지
    vid = SD_MMC.open(p1, FILE_WRITE);
    lg = SD_MMC.open(p2, FILE_WRITE);
  }
  if (vid && lg) {
    lg.print("ms,frame,fps,heap,sig_mode,sig,red,green,yellow,fill,white,stop_ms,bt,car\n");
    recFrames = 0;
    unsigned long lastFrame = 0, lastLog = 0, lastFlush = millis();
    while (recOn) {
      unsigned long now = millis();
      if (now - lastFrame >= REC_FRAME_MS) {
        lastFrame = now;
        uint8_t* jpg = nullptr; size_t len = 0;
        xSemaphoreTake(jpgLock, portMAX_DELAY);
        bool ok = makeJpeg(&jpg, &len, REC_Q);
        xSemaphoreGive(jpgLock);
        if (ok) { if (vid.write(jpg, len) != len) recOn = false; else recFrames++; free(jpg); }
      }
      if (now - lastLog >= REC_LOG_MS) {
        lastLog = now;
        int n = snprintf(line, sizeof line, "%lu,%lu,%.1f,%u,%d,%d,%d,%d,%d,%d,%d,%lu,%d,\"%s\"\n",
                         now, (unsigned long)recFrames, fps, (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
                         sigMode ? 1 : 0, sigState, sigR, sigG, sigY, sigFill, sigWhite, stopLeft(), btOn() ? 1 : 0, carLine);
        if (n > 0 && lg.write((const uint8_t*)line, min(n, (int)sizeof line - 1)) == 0) recOn = false;
      }
      if (now - lastFlush >= REC_FLUSH_MS) { lastFlush = now; vid.flush(); lg.flush(); }   // 2초마다 실제로 씀 (전원이 꺼져도 거기까지는 남음)
      vTaskDelay(pdMS_TO_TICKS(20));
    }
  }
  recOn = false;
  if (vid) vid.close();
  if (lg) lg.close();
  SD_MMC.end();
  recRunning = false;
  vTaskDelete(nullptr);
}

#include "page.h"                         // 웹 화면 (HTML·자바스크립트는 따로 둬야 PlatformIO 가 함수로 잘못 읽지 않음)

// 지금 프레임 + 신호 표시를 JPEG 로 (웹 영상, SD 기록 공용). 신호등 인식이 켜져 있으면 테두리를 지금 신호 색으로
// (빨강 / 초록 / 노랑, 없음 = 회색). SIG_Y1 이 화면보다 작으면 보는 범위 아래 끝에 흰 점선
const uint16_t GREEN = 0x07E0, YELLOW = 0xFFE0, RED = 0xF800, WHITE = 0xFFFF, GRAY = 0x8410, MAGENTA = 0xF81F;
inline void dot(int x, int y, uint16_t c) {
  if (x < 0 || x >= W || y < 0 || y >= H) return;
  view[(y * W + x) * 2] = c >> 8; view[(y * W + x) * 2 + 1] = c & 0xFF;
}
bool makeJpeg(uint8_t** jpg, size_t* len, int q) {   // q = 압축 품질
  memcpy(view, color, W * H * 2);
  if (sigMode) {
    uint16_t c = sigState == 1 ? RED : sigState == 2 ? GREEN : sigState == 3 ? YELLOW : sigState == 4 ? MAGENTA : GRAY;   // 정지 표지판 = 자홍
    for (int k = 0; k < 3; k++) {
      for (int x = 0; x < W; x++) { dot(x, k, c); dot(x, H - 1 - k, c); }
      for (int y = 0; y < H; y++) { dot(k, y, c); dot(W - 1 - k, y, c); }
    }
    if (SIG_Y1 < H) for (int x = 0; x < W; x += 3) dot(x, SIG_Y1, WHITE);
  }
  return fmt2jpg(view, W * H * 2, W, H, PIXFORMAT_RGB565, q, jpg, len);
}

// 연속 영상 (MJPEG). 화면마다 따로 작업으로 보내서 신호 인식·조종을 막지 않는다
// 화면마다 따로 작업을 띄워서 보냄 -> 노트북·폰 2개까지 동시에 영상 (하나가 붙잡고 있어도 다른 화면이 기다리지 않음)
const int MAX_VIEWERS = 2;                // 화면 하나당 내부 메모리 약 4KB
std::atomic<int> viewers{0};
SemaphoreHandle_t jpgLock;                // makeJpeg 의 view 버퍼를 여러 화면이 같이 쓰지 않게

void streamLoop(httpd_req_t* req) {
  httpd_resp_set_type(req, "multipart/x-mixed-replace;boundary=f");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  char head[64];
  uint32_t sentNo = 0;
  unsigned long sentAt = 0;
  while (true) {
    // 새 사진이 없으면 같은 그림을 다시 압축하지 않음. 단 1초에 한 번은 보냄 (모폴로지 화면처럼 사진이 안 바뀌어도 브라우저가 닫혔는지 알아야 작업이 끝남)
    if (colorNo == sentNo && millis() - sentAt < 1000) { vTaskDelay(pdMS_TO_TICKS(10)); continue; }
    sentNo = colorNo; sentAt = millis();
    uint8_t* jpg = nullptr; size_t len = 0;
    xSemaphoreTake(jpgLock, portMAX_DELAY);
    uint32_t tj = micros();
    bool ok = makeJpeg(&jpg, &len, btOn() ? JPEG_Q_BT : JPEG_Q);
    tJpeg += micros() - tj; nJ++;
    xSemaphoreGive(jpgLock);
    if (!ok) return;
    int n = snprintf(head, sizeof head, "\r\n--f\r\nContent-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n", (unsigned)len);
    esp_err_t r = httpd_resp_send_chunk(req, head, n);
    if (r == ESP_OK) r = httpd_resp_send_chunk(req, (const char*)jpg, len);
    free(jpg);
    if (r != ESP_OK) return;               // 브라우저가 닫음
    vTaskDelay(pdMS_TO_TICKS(btOn() ? STREAM_MS_BT : STREAM_MS));
  }
}

void streamTask(void* arg) {
  httpd_req_t* req = (httpd_req_t*)arg;
  streamLoop(req);
  httpd_req_async_handler_complete(req);
  viewers--;
  vTaskDelete(nullptr);
}

esp_err_t streamHandler(httpd_req_t* req) {
  if (viewers >= MAX_VIEWERS) { httpd_resp_set_status(req, "503 Busy"); httpd_resp_sendstr(req, "busy"); return ESP_OK; }
  httpd_req_t* copy = nullptr;
  if (httpd_req_async_handler_begin(req, &copy) != ESP_OK) return ESP_FAIL;
  viewers++;
  // 코어 0 에서 (JPEG 압축이 무거워서, 카메라·라인 처리를 하는 코어 1 을 뺏지 않게)
  if (xTaskCreatePinnedToCore(streamTask, "view", 4096, copy, 1, nullptr, 0) != pdPASS) {
    viewers--; httpd_req_async_handler_complete(copy); return ESP_FAIL;
  }
  return ESP_OK;
}

// 요청 하나를 handler 함수로 넘김 (주소 뒤 ? 값과 POST 본문을 curArgs 로)
esp_err_t route(httpd_req_t* r) {
  cur = r; curArgs = "";
  size_t ql = httpd_req_get_url_query_len(r);
  if (ql) { char* q = (char*)malloc(ql + 1); httpd_req_get_url_query_str(r, q, ql + 1); curArgs = q; free(q); }
  if (r->method == HTTP_POST && r->content_len && r->content_len < 512) {
    char b[512]; int n = httpd_req_recv(r, b, r->content_len); if (n > 0) { b[n] = 0; curArgs += (curArgs.length() ? "&" : ""); curArgs += b; }
  }
  ((void (*)())r->user_ctx)();
  return ESP_OK;
}

void pageMain() { if (setupMode) handleSetupPage(); else send(200, "text/html", PAGE); }
void pageCam() { send(200, "text/html", PAGE); }
void pageSave() { if (setupMode) handleSave(); else send(403); }
void pageList() {
  xSemaphoreTake(btLock, portMAX_DELAY); String l = btList; xSemaphoreGive(btLock);
  send(200, "application/json", l);
}

// 설정 모드 : 폰이 여는 아무 주소나 설정 화면으로 (공항 와이파이처럼)
esp_err_t notFound(httpd_req_t* r, httpd_err_code_t) {
  if (!setupMode) { httpd_resp_send_404(r); return ESP_OK; }
  static String loc; loc = "http://" + myIp + "/";
  httpd_resp_set_status(r, "302 Found");
  httpd_resp_set_hdr(r, "Location", loc.c_str());
  httpd_resp_send(r, nullptr, 0);
  return ESP_OK;
}

void startServer() {
  jpgLock = xSemaphoreCreateMutex();
  httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
  cfg.max_uri_handlers = 12;
  cfg.stack_size = 6144;
  cfg.lru_purge_enable = true;             // 끊긴 옛 연결은 버리고 새 요청 받기
  cfg.send_wait_timeout = 2;
  cfg.close_fn = onClose;
  if (httpd_start(&web, &cfg) != ESP_OK) return;
  struct { const char* u; httpd_method_t m; void (*f)(); } routes[] = {
    {"/", HTTP_GET, pageMain}, {"/cam", HTTP_GET, pageCam}, {"/save", HTTP_POST, pageSave},
    {"/set", HTTP_GET, handleSet},
    {"/bt", HTTP_GET, handleBt}, {"/btlist", HTTP_GET, pageList}};
  for (auto& x : routes) { httpd_uri_t u = {x.u, x.m, route, (void*)x.f}; httpd_register_uri_handler(web, &u); }
  httpd_uri_t st = {"/stream", HTTP_GET, streamHandler, nullptr};
  httpd_register_uri_handler(web, &st);
  httpd_uri_t ws = {};
  ws.uri = "/ws"; ws.method = HTTP_GET; ws.handler = wsHandler; ws.is_websocket = true;
  httpd_register_uri_handler(web, &ws);
  httpd_register_err_handler(web, HTTPD_404_NOT_FOUND, notFound);
}

// 블루투스 : 따로 도는 작업에서 HC-06 에 붙고, 끊기면 다시 붙음 (찾는 데 몇 초 걸려서 loop 를 막지 않게)
void btTask(void*) {
  // 블루투스 클래식 + 와이파이 + 카메라를 한꺼번에 켤 내부 메모리가 ESP32-CAM 에 없다 (2026-11-11 시험) :
  //   나중에 켜면 블루투스가 시작하다 멈추고, 먼저 켜면 와이파이가 메모리 부족으로 멈춘다.
  //   그래서 남은 메모리가 BT_NEED 보다 적으면 켜지 않고 화면에 알린다 (멈추지 않게)
  while (!btWant && !btScanReq) vTaskDelay(pdMS_TO_TICKS(200));
  while (heap_caps_get_free_size(MALLOC_CAP_INTERNAL) < BT_NEED) { btFail = true; btWant = false; btScanReq = false; vTaskDelay(pdMS_TO_TICKS(1000)); }
  BT.begin("GULNO-CAM", true, true);       // 마스터(내가 먼저 붙음), BLE 는 안 써서 그 메모리는 돌려받음
  BT.setPin(BT_PIN, strlen(BT_PIN));
  btReady = true;
  while (true) {
    if (btScanReq) {                       // 주변 기기 찾기 (약 5초)
      btScanReq = false; btScanning = true;
      if (BT.connected()) BT.disconnect();
      BTScanResults* r = BT.discover(5000);
      String j = "[";
      for (int i = 0; r && i < r->getCount(); i++) {
        BTAdvertisedDevice* d = r->getDevice(i);
        if (j.length() > 1) j += ",";
        j += "{\"n\":\"" + esc(d->haveName() ? String(d->getName().c_str()) : String("")) + "\",\"a\":\"" +
             d->getAddress().toString() + "\",\"r\":" + String(d->getRSSI()) + "}";
      }
      j += "]";
      xSemaphoreTake(btLock, portMAX_DELAY); btList = j; xSemaphoreGive(btLock);
      btScanning = false;
    }
    static unsigned long lastTry = 0;      // 연결 재시도는 5초마다 (자주 하면 와이파이와 안테나를 다퉈서 화면이 끊김)
    if (btWant && !BT.connected() && millis() - lastTry > 5000) {
      lastTry = millis();
      xSemaphoreTake(btLock, portMAX_DELAY); String a = btAddr; xSemaphoreGive(btLock);   // 복사해서 잠금 밖에서 연결 (몇 초 걸림)
      if (a.length()) BT.connect(BTAddress(a));
    }
    if (!btWant && BT.connected()) { BT.disconnect(); carClear = true; }
    vTaskDelay(pdMS_TO_TICKS(300));
  }
}

void readCar() {                           // 아두이노 상태 줄 받기 (고정 버퍼 : 0.2초마다 String 을 만들었다 버리지 않게)
  static char buf[64];
  static uint8_t n = 0;
  if (carClear) { carLine[0] = 0; carClear = false; }
  if (!btReady) return;
  while (BT.available()) {
    char c = BT.read();
    if (c == '\n') { buf[n] = 0; if (n) memcpy(carLine, buf, n + 1); n = 0; }
    else if (c != '\r' && c != '"' && c != '\\' && n < sizeof buf - 1) buf[n++] = c;
  }
}

// /bt?scan=1 찾기, /bt?a=<주소>&n=<이름> 그 기기에 연결 (저장), /bt?on=1 저장한 기기에 연결, /bt?on=0 끊기
void handleBt() {
  if (hasArg("scan")) btScanReq = true;
  if (hasArg("a")) {
    String a = arg("a"), n = arg("n");
    xSemaphoreTake(btLock, portMAX_DELAY); btAddr = a; btName = n; setBtLabel(); xSemaphoreGive(btLock);
    prefs.putString("bt_addr", a); prefs.putString("bt_name", n);
    if (btOn()) BT.disconnect();
    btWant = true;
  }
  if (hasArg("on")) { xSemaphoreTake(btLock, portMAX_DELAY); bool has = btAddr.length(); xSemaphoreGive(btLock); btWant = arg("on") == "1" && has; }
  send(204);
}

// 화면으로 보내는 상태 한 덩어리 (웹소켓으로 0.2초마다 밀어 줌)
void buildStatus(char* buf, size_t n) {
  snprintf(buf, n, "{\"sf\":%d,\"sw\":%d,\"stop\":%lu,\"mir\":%d,\"flip\":%d,\"sigm\":%d,\"sig\":%d,\"rc\":%d,\"gc\":%d,\"yc\":%d,\"smin\":%d,\"fps\":%.1f,\"bt\":%d,\"btw\":%d,\"scan\":%d,\"btfail\":%d,\"btn\":\"%s\",\"car\":\"%s\",\"sd\":%d,\"rec\":%d,\"recn\":%d,\"recf\":%lu}",
           sigFill, sigWhite, stopLeft(), camMirror ? 1 : 0, camFlip ? 1 : 0, sigMode ? 1 : 0, sigState, sigR, sigG, sigY, SIG_MIN, fps,
           btOn() ? 1 : 0, btWant ? 1 : 0, btScanning || btScanReq ? 1 : 0, btFail ? 1 : 0, btLabel, carLine, sdOk ? 1 : 0, recOn ? 1 : 0, recNo, (unsigned long)recFrames);
}

// ---- 웹소켓 /ws : 연결 하나를 열어 두고 조종 글자는 받고(화면 -> 차), 상태는 밀어 줌(차·카메라 -> 화면) ----
const int MAX_WS = 2;
int wsFd[MAX_WS] = {-1, -1};

unsigned long wsAt[MAX_WS];

void wsAdd(int fd) {                      // 화면 하나 등록 (이미 있으면 그대로, 없으면 빈 자리 / 웹소켓이 아닌 자리 / 가장 오래된 자리)
  for (int i = 0; i < MAX_WS; i++) if (wsFd[i] == fd) return;
  int k = 0;
  for (int i = 0; i < MAX_WS; i++) {
    if (wsFd[i] < 0 || httpd_ws_get_fd_info(web, wsFd[i]) != HTTPD_WS_CLIENT_WEBSOCKET) { k = i; break; }
    if (wsAt[i] < wsAt[k]) k = i;
  }
  wsFd[k] = fd; wsAt[k] = millis();
}

void onClose(httpd_handle_t, int fd) {    // 연결이 닫히면 그 자리를 비움 (번호가 다른 요청에 다시 쓰여도 헷갈리지 않게)
  for (int i = 0; i < MAX_WS; i++) if (wsFd[i] == fd) wsFd[i] = -1;
  close(fd);
}

esp_err_t wsHandler(httpd_req_t* r) {
  wsAdd(httpd_req_to_sockfd(r));
  lastViewer = millis();
  if (r->method == HTTP_GET) return ESP_OK; // 처음 연결 (이 버전은 여기 안 옴 : 화면이 첫 글자 h 를 보내면 위에서 등록)
  uint8_t b[8];
  httpd_ws_frame_t f = {};
  f.payload = b;
  if (httpd_ws_recv_frame(r, &f, sizeof b) != ESP_OK) return ESP_FAIL;
  if (f.type != HTTPD_WS_TYPE_TEXT || f.len != 1 || setupMode) return ESP_OK;
  char c = b[0];
  // 조종 글자 하나 : 앱과 똑같이 그대로 차에 (a = 라인센서 자동 주행, m = 수동)
  if (strchr("gblrsqwam", c) && btOn()) btSend(c);
  return ESP_OK;
}

// 열려 있는 화면 모두에게 보냄. 보낸 화면이 있으면 true
bool wsBroadcast(httpd_ws_type_t type, const uint8_t* data, size_t len) {
  bool any = false;
  for (int i = 0; i < MAX_WS; i++) {
    if (wsFd[i] < 0) continue;
    httpd_ws_client_info_t info = httpd_ws_get_fd_info(web, wsFd[i]);
    if (info == HTTPD_WS_CLIENT_INVALID) { wsFd[i] = -1; continue; }   // 닫힘
    if (info != HTTPD_WS_CLIENT_WEBSOCKET) { if (millis() - wsAt[i] > 2000) wsFd[i] = -1; continue; }   // 연결 중이거나 다른 요청
    httpd_ws_frame_t f = {};
    f.type = type; f.payload = (uint8_t*)data; f.len = len;
    httpd_ws_send_frame_async(web, wsFd[i], &f);
    any = true;
  }
  return any;
}

void pushStatus() {
  static char buf[400];
  buildStatus(buf, sizeof buf);
  if (wsBroadcast(HTTPD_WS_TYPE_TEXT, (const uint8_t*)buf, strlen(buf))) lastViewer = millis();   // 화면이 열려 있음 -> 차에 연결 신호 h 계속
}


void handleSet() {
  sensor_t* cam = esp_camera_sensor_get();
  if (hasArg("mir")) { camMirror = arg("mir") == "1"; cam->set_hmirror(cam, camMirror); prefs.putBool("mir", camMirror); }
  if (hasArg("flip")) { camFlip = arg("flip") == "1"; cam->set_vflip(cam, camFlip); prefs.putBool("flip", camFlip); }
  if (hasArg("sig")) { sigMode = arg("sig") == "1"; prefs.putBool("sig", sigMode); sigState = 0;
    if (!sigMode && btOn()) for (int i = 0; i < 3; i++) btSend('n'); }   // 인식을 끄면 차의 빨강 정지를 풀어 줌 (빨강은 시간으로 안 풀려서)
  if (hasArg("rec")) {                    // SD 기록 켬/끔. 켤 때마다 번호 +1 (rec_0001.mjpeg, log_0001.csv …)
    bool on = arg("rec") == "1" && sdOk;
    if (on && !recOn && !recRunning) {
      recNo = prefs.getInt("recn", 0) + 1; prefs.putInt("recn", recNo);
      recOn = recRunning = true;
      if (xTaskCreatePinnedToCore(recTask, "rec", 6144, nullptr, 1, nullptr, 0) != pdPASS) recOn = recRunning = false;   // 코어 0 (신호 처리를 안 막음)
    } else if (!on) recOn = false;        // 작업이 파일을 닫고 스스로 끝남
  }
  send(200, "text/plain", "ok");
}

// ---- 와이파이 설정 (프린터처럼) ----
String esc(String t) { t.replace("&", "&amp;"); t.replace("<", "&lt;"); t.replace(">", "&gt;"); t.replace("\"", "&quot;"); t.replace("\\", "/"); return t; }

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
  send(200, "text/html", h);
}

void handleSave() {
  String s = arg("s"), p = arg("p");
  if (s.length() == 0 || s.length() > 32 || p.length() > 63) { send(400, "text/plain", "이름 1~32자, 암호 63자 이하"); return; }
  prefs.putString("ssid", s);
  prefs.putString("pass", p);
  send(200, "text/html", F("<meta charset=\"utf-8\"><body style=\"font-family:system-ui;background:#111;color:#eee\">"
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
  btAddr = prefs.getString("bt_addr", ""); btName = prefs.getString("bt_name", ""); setBtLabel();
  WiFi.mode(WIFI_STA);
  WiFi.enableIPv6();                       // 아이폰 핫스팟은 맥에 IPv6 만 주기도 해서, IPv6 로도 접속되게
  WiFi.setHostname(HOSTNAME);
  WiFi.setSleep(false);                   // 와이파이 절전 끔 : 켜 두면 영상·웹소켓이 가끔 멈추거나 끊김 (전기는 조금 더 씀)
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
  c.pixel_format = PIXFORMAT_RGB565;       // 늘 컬러 : 라인은 밝기만 뽑아서, 신호등은 색으로
  c.frame_size = FRAMESIZE_QQVGA;          // 160 x 120
  c.fb_count = 2;                          // 2장 : 한 장을 처리하는 동안 다음 장을 미리 찍음 (외부 메모리 38KB 더, 처리와 촬영이 겹쳐서 빨라짐)
  c.fb_location = CAMERA_FB_IN_PSRAM;
  c.grab_mode = CAMERA_GRAB_LATEST;        // 항상 가장 최근 프레임
  if (esp_camera_init(&c) != ESP_OK) return false;
  sensor_t* s = esp_camera_sensor_get();
  s->set_vflip(s, camFlip);
  s->set_hmirror(s, camMirror);
  return true;
}

void setup() {
  Serial.begin(9600);                      // U0T(GPIO1) -> 아두이노 D0(RX)
  pinMode(FLASH_LED, OUTPUT); digitalWrite(FLASH_LED, LOW);
  pinMode(STATUS_LED, OUTPUT); digitalWrite(STATUS_LED, HIGH);
  color = (uint8_t*)ps_calloc(W * H * 2, 1);
  view = (uint8_t*)ps_malloc(W * H * 2);
  ledcAttachChannel(FLASH_LED, 5000, 8, 7); // 채널 7 (카메라 XCLK 가 채널 0 을 씀)
  prefs.begin("gulno", false);             // 저장된 설정 (핫스팟, 블루투스 기기, 화면 방향)
  camMirror = prefs.getBool("mir", true); camFlip = prefs.getBool("flip", false); sigMode = prefs.getBool("sig", false);
  if (!startCamera()) {
    while (true) { Serial.println("E,camera"); delay(1000); }   // 카메라 리본 케이블 확인
  }
  startWifi();
  startServer();
  sdOk = SD_MMC.begin("/sdcard", true, false, SDMMC_FREQ_DEFAULT, 2);   // 1비트 모드, 포맷 안 함, 파일 2개까지. 카드가 있는지만 보고
  if (sdOk) SD_MMC.end();                                                  // 바로 뗌 (기록할 때만 다시 붙임)
  Serial.printf("I,sd,%d\n", sdOk ? 1 : 0);
  xTaskCreatePinnedToCore(btTask, "bt", 4096, nullptr, 1, nullptr, 0);
}

// 신호등 : SIG_Y1 위쪽(지금은 화면 전체)에서 2칸마다 색을 봄. 빨강 = R 높고 G·B 낮음, 초록 = G 높고 R 낮음, 노랑 = R·G 높고 B 낮음
// (폰 화면 순수 빨강·초록·노랑 기준, 실제로 재서 맞추기). 가장 많은 색이 SIG_MIN 넘으면 그 신호
void detectSignal(const uint8_t* b) {
  static uint8_t cand = 0, same = 0;
  int red = 0, grn = 0, yel = 0, x0 = W, x1 = -1, y0 = H, y1 = -1;   // x0~y1 = 빨강 덩어리를 감싼 네모 칸
  for (int y = 0; y < SIG_Y1; y += 2)
    for (int x = 0; x < W; x += 2) {
      int i = y * W + x;
      uint16_t p = (b[2 * i] << 8) | b[2 * i + 1];
      int r = (p >> 11) << 3, g = ((p >> 5) & 63) << 2, bl = (p & 31) << 3;
      if (r >= 160 && g <= 80 && bl <= 80) {
        red++;
        if (x < x0) x0 = x;
        if (x > x1) x1 = x;
        if (y < y0) y0 = y;
        if (y > y1) y1 = y;
      }
      else if (g >= 150 && r <= 110 && bl <= 150) grn++;
      else if (r >= 160 && g >= 150 && bl <= 100) yel++;
    }
  sigR = red; sigG = grn; sigY = yel;
  // 빨강 덩어리 모양 : 네모 칸을 얼마나 채웠나(%), 칸 안에 흰색이 얼마나(%) -> 정지 표지판인지 신호등(폰 화면)인지
  int fill = 0, white = 0;
  if (red > SIG_MIN) {
    int area = ((x1 - x0) / 2 + 1) * ((y1 - y0) / 2 + 1), wc = 0;
    for (int y = y0; y <= y1; y += 2)
      for (int x = x0; x <= x1; x += 2) {
        int i = y * W + x;
        uint16_t p = (b[2 * i] << 8) | b[2 * i + 1];
        if ((p >> 11) << 3 >= 180 && ((p >> 5) & 63) << 2 >= 180 && (p & 31) << 3 >= 180) wc++;
      }
    fill = red * 100 / area; white = wc * 100 / area;
  }
  sigFill = fill; sigWhite = white;
  uint8_t now = 0;
  int best = SIG_MIN;
  if (red > best) { now = fill < SIGN_FILL_MAX && white >= SIGN_WHITE_MIN ? 4 : 1; best = red; }   // 4 = 정지 표지판
  if (grn > best) { now = 2; best = grn; }
  if (yel > best) now = 3;
  if (now == cand) { if (same < 3) same++; } else { cand = now; same = 1; }
  if (same >= (cand == 1 ? 2 : 3)) sigState = cand;   // 빨강(정지)은 2장이면 바로 (서는 쪽은 빨라야 안전), 초록·노랑·없음은 3장 (잘못 출발하지 않게)
}

void loop() {
  static uint32_t tGrab = 0, tProc = 0, tLoop = 0, nT = 0, loopAt = 0;   // 시간 재기 (시리얼 I,time)
  uint32_t t0 = micros();
  if (loopAt) tLoop += t0 - loopAt;
  loopAt = t0;
  camera_fb_t* fb = esp_camera_fb_get();
  uint32_t t1 = micros();
  if (fb) {
    if (fb->len >= (size_t)(W * H * 2)) {
      const uint8_t* b = fb->buf;
      uint32_t ts = micros();
      if (sigMode) detectSignal(b);
      uint32_t tc = micros();
      tSig += tc - ts;
      if (viewers > 0 || recOn) { memcpy(color, b, W * H * 2); colorNo++; }   // 영상을 보거나 SD 기록 중일 때만
      tCopy += micros() - tc;
      frames++;
    }
    esp_camera_fb_return(fb);
    tGrab += t1 - t0; tProc += micros() - t1; nT++;
  }
  static unsigned long lastPush = 0;
  static uint8_t shownSig = 255;          // 신호가 바뀌면 화면에도 0.2초를 기다리지 않고 바로
  if (millis() - lastPush >= 200 || sigState != shownSig) { lastPush = millis(); shownSig = sigState; pushStatus(); }
  // 화면이 열려 있는 동안(웹소켓 연결) 차에 연결 신호 h 를 0.6초마다 대신 보냄.
  // 화면을 닫으면 끊겨서 차의 자동 주행이 1.5초 뒤 멈춘다 (앱과 같은 안전장치)
  if (btOn() && millis() - lastViewer < 2500 && millis() - lastHb >= 600) { lastHb = millis(); btSend('h'); }
  // 신호등 : 지금 신호를 차(버전 D)에 0.2초마다 계속 보냄 (2초면 10번. 교안 4팀처럼 한 번이 아니라 계속 보내서 몇 개 놓쳐도 유지).
  // x 빨강 = 정지, v 노랑 = 서행, n 초록·신호 없음 = 보통. 차는 모드를 안 바꾸고 라인센서로 계속 라인을 따라가면서 신호만 따름.
  // 인식을 끄면 n 을 3번 보내고 그만 보냄 (빨강은 차에서 시간으로 안 풀림, 노랑은 1초 뒤 풀림)
  // 색이 바뀌면 0.2초를 기다리지 않고 바로 보냄
  // 정지 표지판 : 처음 보면 STOP_WAIT_MS 동안 x (정지) -> 그 뒤 n 으로 출발, STOP_IGNORE_MS 동안은 표지판을 무시 (지나가는 동안 또 서지 않게)
  static unsigned long ignoreUntil = 0;
  if (sigMode && sigState == 4 && (long)(millis() - ignoreUntil) >= 0) { stopUntil = millis() + STOP_WAIT_MS; ignoreUntil = stopUntil + STOP_IGNORE_MS; }
  if (stopUntil && !stopLeft()) stopUntil = 0;
  char want = sigState == 1 || stopLeft() ? 'x' : sigState == 3 ? 'v' : 'n';
  static unsigned long lastSig = 0;
  static char sentSig = 0;
  if (sigMode && btOn() && (want != sentSig || millis() - lastSig >= 200)) {
    lastSig = millis(); sentSig = want;
    btSend(want);
  }
  if (!sigMode) { sentSig = 0; stopUntil = 0; }
  if (millis() - lastFpsAt >= 1000) { fps = frames * 1000.0 / (millis() - lastFpsAt); frames = 0; lastFpsAt = millis(); }
  // 메모리 안전장치 : 남은 내부 메모리가 HEAP_MIN 보다 적은 상태가 5초 이어지면, 멈추기 전에 스스로 재시작
  // (핫스팟·블루투스는 저장돼 있어서 다시 붙음). 누수가 있으면 시리얼 I,heap 값이 계속 줄어든다
  static unsigned long lowSince = 0;
  if (heap_caps_get_free_size(MALLOC_CAP_INTERNAL) < HEAP_MIN) {
    if (!lowSince) lowSince = millis();
    else if (millis() - lowSince > 5000) { Serial.println("I,restart,low_heap"); delay(100); ESP.restart(); }
  } else lowSince = 0;
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
    if (nT) Serial.printf("I,time,grab_ms,%lu,proc_ms,%lu,loop_ms,%lu\n", (unsigned long)(tGrab / nT / 1000), (unsigned long)(tProc / nT / 1000), (unsigned long)(tLoop / nT / 1000));
    if (nT) Serial.printf("I,step_us,sig,%lu,copy,%lu,jpeg,%lu\n", (unsigned long)(tSig/nT), (unsigned long)(tCopy/nT), (unsigned long)(nJ ? tJpeg/nJ : 0));
    tGrab = tProc = tLoop = nT = 0; tSig = tCopy = tJpeg = nJ = 0;
    Serial.printf("I,heap,%u,%u,psram,%u,%u\n", (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
                  (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL), (unsigned)ESP.getPsramSize(), (unsigned)ESP.getFreePsram());
  }
  readCar();
  // 상태 LED : 설정 모드 = 깜빡 (0.5초), 핫스팟 연결 = 켜짐, 연결됐다 끊김 = 빠르게 깜빡
  bool on = setupMode ? (millis() / 500) % 2 : WiFi.status() == WL_CONNECTED ? true : (millis() / 120) % 2;
  digitalWrite(STATUS_LED, on ? LOW : HIGH);
  // 앞 플래시 LED : 연결 중(설정 모드, 핫스팟 연결 중, 블루투스 연결 중) = 깜빡, 다 연결되면 = 계속 켜짐 (폰 화면에 비치면 FLASH_DUTY 를 낮춤)
  bool connecting = setupMode || WiFi.status() != WL_CONNECTED || (btWant && !btOn());
  ledcWrite(FLASH_LED, connecting && (millis() / 400) % 2 ? 0 : FLASH_DUTY);
  if (setupMode) {
    dns.processNextRequest();
    if (savedSsid.length() && WiFi.status() == WL_CONNECTED) {   // 핫스팟이 나타나서 붙음 -> 설정 모드 끝
      Serial.printf("I,joined,%s\n", savedSsid.c_str());
      delay(300);
      ESP.restart();                       // 다시 켜면서 핫스팟 모드로 (켤 때 바로 붙음)
    }
  }
}
