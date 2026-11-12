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
#include <DNSServer.h>
#include <Preferences.h>
#include <ESPmDNS.h>
#include "img_converters.h"
#include "esp_http_server.h"
#include "BluetoothSerial.h"

// ============================================================
// [설정]
// ============================================================
const char* SETUP_SSID = "GULNO-CAM-SETUP";   // 설정용 와이파이 이름 (뒤에 칩 번호 4자리가 붙음)
const char* HOSTNAME = "gulno-cam";            // 핫스팟에서 http://gulno-cam.local
const char* BT_PIN = "1234";                   // HC-06 기본 PIN
const unsigned long CONNECT_WAIT_MS = 15000;   // 켤 때 저장된 핫스팟을 이만큼 기다리고, 못 붙으면 설정 모드
// 핫스팟 이름·암호는 여기 적지 않는다 (공개 저장소). 설정 화면에서 넣으면 ESP32 안(Preferences)에 저장된다

const int W = 160, H = 120;               // 흑백 QQVGA
const int MAX_JUMP = 18;                  // 한 줄 위로 갈 때 라인 가운데가 이보다 많이 튀면 다른 물체로 봄 (칸)
const int LOOK_Y = 42;                    // 향할 곳(목표점)을 잡는 줄. 작을수록 멀리 봄 (0 = 화면 맨 위)
const int FLASH_DUTY = 30;                // 앞 플래시 LED 밝기 (0~255). 최대로 오래 켜면 뜨겁고 전원이 약하면 재부팅
const int JPEG_Q = 40, JPEG_Q_BT = 25;     // 화면 영상 압축 (낮을수록 많이 압축 = 작고 흐림). _BT = 차와 블루투스로 연결됐을 때
const int STREAM_MS = 100, STREAM_MS_BT = 200;   // 영상 한 장 간격 (초당 10장 / 블루투스 연결 중 5장)
// 안테나 하나를 와이파이·블루투스가 나눠 써서, 영상이 무거우면 조종 글자(블루투스)가 늦어진다 -> 차와 연결되면 영상을 가볍게
const int DARK_OFFSET = 40;               // 그 줄 평균 밝기보다 이만큼 어두우면 "라인". 바닥·조명에 맞게 여기서 바꿈
const int MIN_DARK_COLS = 3;              // 선으로 칠 최소 폭 (칸). 이보다 좁으면 잡음
const float CROSS_RATIO = 0.6;            // 한 줄의 이 비율 넘게 검정이면 교차선
bool camGray = false;                     // 흑백으로 찍기 (컬러의 절반 크기라 더 빠름). 라인 확인이 끝나면 켬. 웹 화면에서 바꾸면 저장됨
volatile bool camReinit = false;          // 컬러/흑백을 바꿨으니 카메라를 다시 켜야 함 (loop 에서)
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
struct Band { bool found; bool cross; int pos; int cx; };

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
String carLine;
volatile bool btWant = false;             // 연결 유지 (끄면 폰 앱이 HC-06 을 쓸 수 있음)
volatile bool btScanReq = false, btScanning = false, btFail = false;
const uint32_t BT_NEED = 40000;           // 블루투스를 켤 때 남아 있어야 할 내부 메모리 (PlatformIO 빌드는 블루투스가 외부 메모리를 먼저 써서 적게 필요)
String btAddr, btName;                    // 연결할 기기 (목록에서 고르면 ESP32 에 저장)
String btList = "[]";                     // 찾은 기기 목록 JSON [{"n":이름,"a":주소,"r":신호}]
DNSServer dns;                            // 설정 모드 : 어떤 주소를 쳐도 설정 화면으로 (공항 와이파이처럼)
Preferences prefs;
bool setupMode = false;
String setupName, setupPass, ssidOptions, myIp, savedSsid, failWhy;
unsigned long lastInfo = 0;
String esc(String t);                     // 아래 와이파이 설정 부분에 있음
// 사진은 판단하고 버림. 화면용 버퍼는 전부 외부 메모리(PSRAM) : 내부 메모리는 와이파이·블루투스가 써야 해서
uint8_t* gray;                            // 선 찾기용 흑백 (W x H)
uint8_t* color;                           // 화면용 컬러 RGB565 (W x H x 2)
uint8_t* view;                            // 띠·선을 그린 화면 복사본 (RGB565)
Band lastNear, lastFar;
unsigned long frames = 0, lastFpsAt = 0, lastViewer = 0, lastHb = 0;
float fps = 0;

// 라인 따라가기 : 화면 맨 아래(차 바로 앞)부터 한 줄씩 위로, 바로 아래 줄의 라인과 이어지는 검은 덩어리만 고른다
//   (바닥 얼룩·그림자처럼 떨어져 있는 검정은 무시). 줄마다 라인의 왼쪽 끝·오른쪽 끝·가운데를 저장해서 화면에 칠한다
int16_t trL[H], trR[H], trC[H];           // 줄마다 인식한 라인 (-1 = 없음)
int trTop = H;                            // 라인이 이어진 가장 위 줄
int tgtX = -1, tgtY = -1;                 // 향할 곳

Band summary(int y0, int y1) {            // y0~y1 줄의 라인 가운데 평균 -> 위치 -100~100
  Band b = {false, false, 0, -1};
  long sum = 0; int n = 0;
  for (int y = max(y0, 0); y < min(y1, H); y++) if (trC[y] >= 0) { sum += trC[y]; n++; }
  if (n < 3) return b;
  b.found = true; b.cx = sum / n; b.pos = (b.cx - W / 2) * 100 / (W / 2);
  return b;
}

void trackLine(const uint8_t* img) {
  for (int y = 0; y < H; y++) trC[y] = -1;
  int prev = -1, miss = 0; bool cross = false;
  trTop = H;
  for (int y = H - 1; y >= 0; y--) {
    const uint8_t* r = img + y * W;
    long sum = 0;
    for (int x = 0; x < W; x++) sum += r[x];
    int th = sum / W - DARK_OFFSET;        // 그 줄 평균보다 DARK_OFFSET 어두우면 검정 (조명 따라감)
    int best = -1, bl = 0, br = 0, bestD = 9999, darkAll = 0;
    for (int x = 0; x < W;) {
      if (r[x] >= th) { x++; continue; }
      int st = x;
      while (x < W && r[x] < th) x++;
      int w = x - st; darkAll += w;
      if (w < MIN_DARK_COLS) continue;
      int c = st + w / 2;
      int d = prev < 0 ? abs(c - W / 2) : abs(c - prev);   // 처음엔 가운데에 가까운 것, 다음부턴 아래 줄과 가까운 것
      if (prev >= 0 && d > MAX_JUMP) continue;
      if (d < bestD) { bestD = d; best = c; bl = st; br = x - 1; }
    }
    if (best < 0) {                       // 이 줄엔 없음 : 3줄까지는 건너뛰고, 더 끊기면 라인 끝
      if (prev >= 0 && ++miss > 3) break;
      continue;
    }
    miss = 0; prev = best; trTop = y;
    trL[y] = bl; trR[y] = br; trC[y] = best;
    if (y > H * 2 / 3 && darkAll > W * CROSS_RATIO) cross = true;
  }
  // 가까운 곳 = 아래 1/3, 먼 곳 = 이어진 라인의 위쪽 끝 부분
  lastNear = summary(H * 2 / 3, H);
  lastNear.cross = cross;
  lastFar = trTop < H / 2 ? summary(trTop, trTop + 12) : Band{false, false, 0, -1};
  // 향할 곳 : LOOK_Y 줄 (라인이 거기까지 안 오면 이어진 맨 위)
  tgtY = -1; tgtX = -1;
  for (int y = max(LOOK_Y, trTop); y < H; y++) if (trC[y] >= 0) { tgtY = y; tgtX = trC[y]; break; }
}

// ---- 웹 화면 ----
#include "page.h"                         // 웹 화면 (HTML·자바스크립트는 따로 둬야 PlatformIO 가 함수로 잘못 읽지 않음)

// 지금 프레임 + 띠·선 표시를 JPEG 로 (화면 한 장, 스트림 공용)
// RGB565 색 (앞 바이트가 높은 쪽)
const uint16_t GREEN = 0x07E0, YELLOW = 0xFFE0, RED = 0xF800, WHITE = 0xFFFF;
inline void dot(int x, int y, uint16_t c) {   // 흑백일 때는 색 대신 밝기 (초록 = 흰색, 노랑·빨강 = 흰/검 무늬)
  if (x < 0 || x >= W || y < 0 || y >= H) return;
  if (camGray) { view[y * W + x] = c == GREEN || c == WHITE ? 255 : ((x + y) & 1) ? 0 : 255; return; }
  view[(y * W + x) * 2] = c >> 8; view[(y * W + x) * 2 + 1] = c & 0xFF;
}

// 화면 : 인식한 라인 = 초록 칠, 라인 가운데 경로 = 노란 점, 차 -> 향할 곳 = 빨간 선과 점, 차 가운데 = 흰 점선 (아래쪽)
bool makeJpeg(uint8_t** jpg, size_t* len) {
  bool g = camGray;
  memcpy(view, g ? gray : color, g ? W * H : W * H * 2);
  for (int y = 0; y < H; y++) if (trC[y] >= 0) {
    for (int x = trL[y]; x <= trR[y]; x++) dot(x, y, GREEN);
    if (y % 2 == 0) dot(trC[y], y, YELLOW);
  }
  for (int y = H - 16; y < H; y += 2) dot(W / 2, y, WHITE);
  if (tgtX >= 0) {                        // 차(화면 아래 가운데) -> 향할 곳
    int x0 = W / 2, y0 = H - 1, dx = abs(tgtX - x0), dy = -abs(tgtY - y0), sx = x0 < tgtX ? 1 : -1, sy = y0 < tgtY ? 1 : -1, e = dx + dy;
    while (true) {
      dot(x0, y0, RED); dot(x0 + 1, y0, RED);
      if (x0 == tgtX && y0 == tgtY) break;
      int e2 = 2 * e;
      if (e2 >= dy) { e += dy; x0 += sx; }
      if (e2 <= dx) { e += dx; y0 += sy; }
    }
    for (int j = -2; j <= 2; j++) for (int i = -2; i <= 2; i++) dot(tgtX + i, tgtY + j, RED);
  }
  return fmt2jpg(view, g ? W * H : W * H * 2, W, H, g ? PIXFORMAT_GRAYSCALE : PIXFORMAT_RGB565, btOn() ? JPEG_Q_BT : JPEG_Q, jpg, len);
}

// 연속 영상 (MJPEG). 따로 도는 서버(포트 81)라 라인 인식·조종을 막지 않는다
// 화면마다 따로 작업을 띄워서 보냄 -> 노트북·폰 2개까지 동시에 영상 (하나가 붙잡고 있어도 다른 화면이 기다리지 않음)
const int MAX_VIEWERS = 2;                // 화면 하나당 내부 메모리 약 4KB
volatile int viewers = 0;
SemaphoreHandle_t jpgLock;                // makeJpeg 의 view 버퍼를 여러 화면이 같이 쓰지 않게

void streamLoop(httpd_req_t* req) {
  httpd_resp_set_type(req, "multipart/x-mixed-replace;boundary=f");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  char head[64];
  while (true) {
    uint8_t* jpg = nullptr; size_t len = 0;
    xSemaphoreTake(jpgLock, portMAX_DELAY);
    bool ok = makeJpeg(&jpg, &len);
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
  if (xTaskCreate(streamTask, "view", 4096, copy, 1, nullptr) != pdPASS) {
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
void pageList() { send(200, "application/json", btList); }

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
      btList = j + "]";
      btScanning = false;
    }
    static unsigned long lastTry = 0;      // 연결 재시도는 5초마다 (자주 하면 와이파이와 안테나를 다퉈서 화면이 끊김)
    if (btWant && btAddr.length() && !BT.connected() && millis() - lastTry > 5000) { lastTry = millis(); BT.connect(BTAddress(btAddr)); }
    if (!btWant && BT.connected()) { BT.disconnect(); carLine = ""; }
    vTaskDelay(pdMS_TO_TICKS(300));
  }
}

void readCar() {                           // 아두이노 상태 줄 받기
  static String buf;
  if (!btReady) return;
  while (BT.available()) {
    char c = BT.read();
    if (c == '\n') { buf.trim(); if (buf.length()) carLine = buf; buf = ""; }
    else if (buf.length() < 60) buf += c;
  }
}

// /bt?scan=1 찾기, /bt?a=<주소>&n=<이름> 그 기기에 연결 (저장), /bt?on=1 저장한 기기에 연결, /bt?on=0 끊기
void handleBt() {
  if (hasArg("scan")) btScanReq = true;
  if (hasArg("a")) {
    btAddr = arg("a"); btName = arg("n");
    prefs.putString("bt_addr", btAddr); prefs.putString("bt_name", btName);
    if (btOn()) BT.disconnect();
    btWant = true;
  }
  if (hasArg("on")) btWant = arg("on") == "1" && btAddr.length();
  send(204);
}

// 화면으로 보내는 상태 한 덩어리 (웹소켓으로 0.2초마다 밀어 줌)
void buildStatus(char* buf, size_t n) {
  snprintf(buf, n, "{\"nf\":%d,\"np\":%d,\"ff\":%d,\"fp\":%d,\"cross\":%d,\"mir\":%d,\"flip\":%d,\"gray\":%d,\"fps\":%.1f,\"bt\":%d,\"btw\":%d,\"scan\":%d,\"btfail\":%d,\"btn\":\"%s\",\"car\":\"%s\"}",
           lastNear.found, lastNear.pos, lastFar.found, lastFar.pos, lastNear.cross, camMirror ? 1 : 0, camFlip ? 1 : 0, camGray ? 1 : 0, fps,
           btOn() ? 1 : 0, btWant ? 1 : 0, btScanning || btScanReq ? 1 : 0, btFail ? 1 : 0, esc(btName.length() ? btName : btAddr).c_str(),
           esc(carLine).c_str());
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
  // 조종 글자 하나 : 앱과 똑같이 그대로 차에 (아두이노는 버전 C 그대로)
  if (f.type == HTTPD_WS_TYPE_TEXT && f.len == 1 && !setupMode && strchr("gblrsqwam", b[0]) && btOn()) BT.write(b[0]);
  return ESP_OK;
}

void pushStatus() {
  static char buf[320];
  bool any = false;
  for (int i = 0; i < MAX_WS; i++) {
    if (wsFd[i] < 0) continue;
    httpd_ws_client_info_t info = httpd_ws_get_fd_info(web, wsFd[i]);
    if (info == HTTPD_WS_CLIENT_INVALID) { wsFd[i] = -1; continue; }   // 닫힘
    if (info != HTTPD_WS_CLIENT_WEBSOCKET) { if (millis() - wsAt[i] > 2000) wsFd[i] = -1; continue; }   // 연결 중이거나 다른 요청
    if (!any) { buildStatus(buf, sizeof buf); any = true; }
    httpd_ws_frame_t f = {};
    f.type = HTTPD_WS_TYPE_TEXT; f.payload = (uint8_t*)buf; f.len = strlen(buf);
    httpd_ws_send_frame_async(web, wsFd[i], &f);
  }
  if (any) lastViewer = millis();          // 화면이 열려 있음 -> 차에 연결 신호 h 계속
}

void handleSet() {
  sensor_t* cam = esp_camera_sensor_get();
  if (hasArg("mir")) { camMirror = arg("mir") == "1"; cam->set_hmirror(cam, camMirror); prefs.putBool("mir", camMirror); }
  if (hasArg("flip")) { camFlip = arg("flip") == "1"; cam->set_vflip(cam, camFlip); prefs.putBool("flip", camFlip); }
  if (hasArg("gray") && !camReinit && (arg("gray") == "1") != camGray) { prefs.putBool("gray", !camGray); camReinit = true; }
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
  btAddr = prefs.getString("bt_addr", ""); btName = prefs.getString("bt_name", "");
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
  c.pixel_format = camGray ? PIXFORMAT_GRAYSCALE : PIXFORMAT_RGB565;   // 컬러면 선 찾기는 밝기만 뽑아서, 흑백이면 그대로
  c.frame_size = FRAMESIZE_QQVGA;          // 160 x 120
  c.fb_count = 1;                          // 1장 : 판단하고 바로 버림 (메모리 아낌)
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
  gray = (uint8_t*)ps_calloc(W * H, 1);
  color = (uint8_t*)ps_calloc(W * H * 2, 1);
  view = (uint8_t*)ps_malloc(W * H * 2);
  ledcAttachChannel(FLASH_LED, 5000, 8, 7); // 채널 7 (카메라 XCLK 가 채널 0 을 씀)
  prefs.begin("gulno", false);             // 저장된 설정 (핫스팟, 블루투스 기기, 화면 방향)
  camMirror = prefs.getBool("mir", true); camFlip = prefs.getBool("flip", false); camGray = prefs.getBool("gray", false);
  if (!startCamera()) {
    while (true) { Serial.println("E,camera"); delay(1000); }   // 카메라 리본 케이블 확인
  }
  startWifi();
  startServer();
  xTaskCreatePinnedToCore(btTask, "bt", 4096, nullptr, 1, nullptr, 0);
}

void loop() {
  if (camReinit) {                        // 컬러 <-> 흑백 : 카메라를 그 형식으로 다시 켬 (약 1초, 그동안 영상은 기다림)
    xSemaphoreTake(jpgLock, portMAX_DELAY);
    camGray = !camGray;
    esp_camera_deinit();
    startCamera();
    camReinit = false;
    xSemaphoreGive(jpgLock);
  }
  camera_fb_t* fb = esp_camera_fb_get();
  if (fb) {
    if (camGray && fb->len >= (size_t)(W * H)) {   // 흑백 : 받은 그대로 선 찾기 (변환 없음)
      memcpy(gray, fb->buf, W * H);
      trackLine(gray);
      frames++;
    } else if (!camGray && fb->len >= (size_t)(W * H * 2)) {
      const uint8_t* b = fb->buf;
      for (int i = 0; i < W * H; i++) {    // RGB565 -> 밝기 (빨강 30% 초록 59% 파랑 11%)
        uint16_t p = (b[2 * i] << 8) | b[2 * i + 1];
        gray[i] = (((p >> 11) & 31) * 79 + ((p >> 5) & 63) * 76 + (p & 31) * 29) >> 4;
      }
      trackLine(gray);
      memcpy(color, b, W * H * 2);
      frames++;
    }
    esp_camera_fb_return(fb);
  }
  static unsigned long lastPush = 0;
  if (millis() - lastPush >= 200) { lastPush = millis(); pushStatus(); }
  // 화면이 열려 있는 동안(웹소켓 연결) 차에 연결 신호 h 를 0.6초마다 대신 보냄.
  // 화면을 닫으면 끊겨서 차의 자동 주행이 1.5초 뒤 멈춘다 (앱과 같은 안전장치)
  if (btOn() && millis() - lastViewer < 2500 && millis() - lastHb >= 600) { lastHb = millis(); BT.write('h'); }
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
    Serial.printf("I,heap,%u,%u,psram,%u,%u\n", (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
                  (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL), (unsigned)ESP.getPsramSize(), (unsigned)ESP.getFreePsram());
  }
  readCar();
  // 상태 LED : 설정 모드 = 깜빡 (0.5초), 핫스팟 연결 = 켜짐, 연결됐다 끊김 = 빠르게 깜빡
  bool on = setupMode ? (millis() / 500) % 2 : WiFi.status() == WL_CONNECTED ? true : (millis() / 120) % 2;
  digitalWrite(STATUS_LED, on ? LOW : HIGH);
  // 앞 플래시 LED : 연결 중(설정 모드, 핫스팟 연결 중, 블루투스 연결 중) = 깜빡, 다 연결되면 = 계속 켜짐 (바닥 비춤)
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
