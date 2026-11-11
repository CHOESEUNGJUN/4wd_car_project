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
const int NEAR_Y0 = 85, NEAR_Y1 = 110;    // 가까운 띠 (화면 아래쪽 줄 범위)
const int FAR_Y0 = 35, FAR_Y1 = 55;       // 먼 띠 (화면 위쪽 줄 범위)
int flashDuty = 30;                      // 앞 플래시 LED 밝기 (0~255). 화면에서 바꿈. 최대로 오래 켜면 뜨겁고 전원이 약하면 재부팅
const int JPEG_Q = 40;                    // 화면 영상 압축 (낮을수록 많이 압축 = 작고 흐림). 지연보다 메모리·처리 부담을 줄이는 쪽
const int STREAM_MS = 100;                // 영상 한 장 간격 (초당 최대 10장)
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
volatile bool btScanReq = false, btScanning = false;
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
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover"><title>굴노 카메라</title>
<style>
:root{--bg:#0e1116;--card:#171b22;--line:#262c36;--fg:#e8ecf2;--mut:#8b95a5;--acc:#4f9dff;--ok:#3ecf8e;--warn:#f5b84a;--bad:#ff6b6b}
*{box-sizing:border-box}html,body{margin:0;background:var(--bg);color:var(--fg);font:15px/1.45 system-ui,-apple-system,"Apple SD Gothic Neo",sans-serif}
header{position:sticky;top:0;z-index:2;display:flex;flex-wrap:wrap;gap:8px;align-items:center;padding:12px 16px;background:rgba(14,17,22,.92);backdrop-filter:blur(8px);border-bottom:1px solid var(--line)}
h1{font-size:18px;margin:0 auto 0 0}.badge{font-size:13px;padding:4px 10px;border-radius:99px;background:var(--line);color:var(--mut);white-space:nowrap}
.badge.ok{background:rgba(62,207,142,.15);color:var(--ok)}.badge.warn{background:rgba(245,184,74,.15);color:var(--warn)}.badge.bad{background:rgba(255,107,107,.15);color:var(--bad)}
main{display:grid;gap:14px;padding:14px 16px 28px;max-width:1200px;margin:0 auto;grid-template-columns:1fr}
@media(min-width:900px){main{grid-template-columns:minmax(0,1.5fr) minmax(320px,1fr);align-items:start}.side{display:grid;gap:14px}}
.side{display:grid;gap:14px}.card{background:var(--card);border:1px solid var(--line);border-radius:14px;padding:14px}
.card h2{font-size:14px;color:var(--mut);font-weight:600;margin:0 0 10px;letter-spacing:.02em}
.video{position:relative;aspect-ratio:4/3;background:#000;border-radius:10px;overflow:hidden}
.video img{width:100%;height:100%;object-fit:contain;display:block}
.video .ph{position:absolute;inset:0;display:grid;place-items:center;color:var(--mut)}
.dir{position:absolute;left:10px;top:10px;display:flex;align-items:center;gap:8px;padding:6px 12px 6px 8px;border-radius:12px;background:rgba(0,0,0,.6);font-weight:600}
.dir b{font-size:34px;line-height:1}#s{color:var(--mut);font-size:13px;margin-top:8px}
.pad{display:grid;grid-template-columns:repeat(3,1fr);gap:10px;max-width:300px;margin:0 auto}
button{font:inherit;color:var(--fg);background:#232a35;border:1px solid #303846;border-radius:12px;padding:10px 14px;cursor:pointer;touch-action:manipulation}
button:active,button.on{background:var(--acc);border-color:var(--acc)}
.pad button{aspect-ratio:1;font-size:28px;touch-action:none;user-select:none;-webkit-user-select:none}.pad .stop{background:#3a2328;border-color:#5a2f37}
.row{display:flex;flex-wrap:wrap;gap:8px;margin-top:12px}.row button{flex:1}
#auto{background:#1f3a2e;border-color:#2b5a45}
table{width:100%;border-collapse:collapse}td{padding:6px 0;border-bottom:1px solid var(--line)}td:first-child{color:var(--mut);width:42%}
#raw{font:12px ui-monospace,Menlo,monospace;color:var(--mut);word-break:break-all;margin-top:8px}
#btl{display:grid;gap:6px;margin-top:10px}#btl button{text-align:left;display:flex;justify-content:space-between;gap:8px}#btl small{color:var(--mut)}
label{display:block;margin:6px 0 2px}input[type=range]{width:100%;accent-color:var(--acc)}.hint{color:var(--mut);font-size:12px;margin-top:8px}
</style></head><body>
<header><h1>굴노 카메라</h1><span id="bCam" class="badge">카메라</span><span id="bCar" class="badge">차 블루투스</span><span id="bFps" class="badge">- fps</span></header>
<main>
<section class="card"><div class="video"><div class="ph" id="ph">영상 연결 중…</div><img id="f" alt="">
<div class="dir"><b id="dir">…</b><span id="dirT">-</span></div></div><div id="s"></div></section>
<div class="side">
<section class="card"><h2>조종 (와이파이 → 블루투스)</h2>
<div class="pad"><button data-c="q">↺</button><button data-c="g">▲</button><button data-c="w">↻</button>
<button data-c="l">◀</button><button data-c="s" class="stop">■</button><button data-c="r">▶</button>
<span></span><button data-c="b">▼</button><span></span></div>
<div class="row"><button id="auto">자동으로</button><button id="man">수동으로</button></div><div id="cs" class="hint"></div></section>
<section class="card"><h2>차 상태</h2><table id="car"><tr><td>블루투스</td><td>연결 안 됨</td></tr></table><div id="raw"></div></section>
<section class="card"><h2>블루투스</h2><div id="bts" class="hint" style="margin:0 0 8px"></div>
<div class="row" style="margin:0"><button id="btScan">기기 찾기</button><button id="btOn">연결</button><button id="btOff">끊기</button></div><div id="btl"></div></section>
<section class="card"><h2>카메라 설정</h2>
<label>어두움 기준 <b id="v"></b></label><input id="o" type="range" min="5" max="120">
<label>앞 LED 밝기 <b id="lv"></b></label><input id="led" type="range" min="0" max="255">
<div class="hint">흰 가로줄 = 띠 범위 · 흑백 세로줄 = 찾은 선 · 점선 = 화면 가운데<br>앞 LED : 연결 중 깜빡, 연결되면 켜짐</div></section>
</div></main>
<script>
const $=i=>document.getElementById(i);
const badge=(id,t,c)=>{$(id).textContent=t;$(id).className='badge '+(c||'');};
let fails=0;
async function tick(){
  try{const s=await (await fetch('/status')).json(); fails=0; badge('bCam','카메라 연결','ok'); badge('bFps',s.fps+' fps');
    $('v').textContent=s.off; if(document.activeElement!==$('o'))$('o').value=s.off;
    $('lv').textContent=s.led; if(document.activeElement!==$('led'))$('led').value=s.led;
    showDir(s); showCar(s);}catch(e){ if(++fails>3)badge('bCam','카메라 끊김','bad'); }
}
// 카메라가 본 선 -> 직진 / 우회전 / 좌회전 (가까운 띠 70% + 먼 띠 30%)
function showDir(s){
  let t,a;
  if(!s.nf&&!s.ff){a='✕';t='선 없음';}
  else if(s.cross){a='╋';t='교차선';}
  else{const p=s.nf&&s.ff?s.np*0.7+s.fp*0.3:(s.nf?s.np:s.fp);
    if(p>25){a='➡';t='우회전';}else if(p<-25){a='⬅';t='좌회전';}else{a='⬆';t='직진';}}
  $('dir').textContent=a; $('dirT').textContent=t;
  $('s').textContent=`가까운 띠 ${s.nf?s.np:'없음'} · 먼 띠 ${s.ff?s.fp:'없음'}`;
}
// 아두이노 상태 줄 "ar,23,011,LINE,1,60" 풀어서 보여 주기
const ACT={g:'직진',b:'후진',l:'좌회전',r:'우회전',q:'제자리 좌',w:'제자리 우',s:'정지',o:'장애물 정지'};
let wasScan=0;
function showCar(s){
  const st=s.scan?'찾는 중…':s.bt?'연결됨':s.btw?'연결 중…':s.btn?'끊김':'기기를 찾아 고르세요';
  badge('bCar','차 '+(s.bt?'연결':s.btw?'연결 중':'끊김'),s.bt?'ok':s.btw?'warn':'');
  $('bts').textContent=(s.btn?s.btn+' · ':'')+st+(s.btw&&!s.bt?' (폰 앱이 그 기기에 붙어 있으면 앱에서 끊기)':'');
  if(wasScan&&!s.scan)loadList(); wasScan=s.scan;
  const f=(s.car||'').split(',');
  if(!s.bt||f.length<4){$('car').innerHTML='<tr><td>블루투스</td><td>'+(s.bt?'상태 기다리는 중':'연결 안 됨')+'</td></tr>';$('raw').textContent='';return;}
  const sen=f[2].split('').map(c=>c=='1'?'●':'○').join(' ');
  const rows=[['모드',f[0][0]=='a'?'자동':'수동'],['동작',ACT[f[0][1]]||f[0][1]],['앞 거리',f[1]=='999'?'-':f[1]+' cm'],['라인센서',sen],['단계',f[3]]];
  if(f.length>=6)rows.push(['오차 / 보정',f[4]+' / '+f[5]]);
  $('car').innerHTML=rows.map(r=>`<tr><td>${r[0]}</td><td>${r[1]}</td></tr>`).join(''); $('raw').textContent=s.car;
}
async function loadList(){
  const l=await (await fetch('/btlist')).json();
  $('btl').innerHTML=l.length?l.sort((a,b)=>b.r-a.r).map(d=>`<button data-a="${d.a}" data-n="${d.n}"><span>${d.n||'(이름 없음)'}</span><small>${d.a} · ${d.r}dBm</small></button>`).join(''):'<div class="hint">찾은 기기 없음 (차 전원, 폰 앱 연결 확인)</div>';
  $('btl').querySelectorAll('button').forEach(b=>b.onclick=()=>{fetch('/bt?a='+encodeURIComponent(b.dataset.a)+'&n='+encodeURIComponent(b.dataset.n));$('btl').innerHTML='';});
}
$('btScan').onclick=()=>{fetch('/bt?scan=1');$('btl').innerHTML='<div class="hint">찾는 중… (약 5초)</div>';};
$('btOn').onclick=()=>fetch('/bt?on=1');
$('btOff').onclick=()=>fetch('/bt?on=0');
// 영상 : 같은 주소의 /stream 연속 영상. 끊기면 2초 뒤 다시
const stream=()=>{$('f').src='/stream?'+Date.now();};
$('f').onload=()=>{$('ph').style.display='none';};
$('f').onerror=()=>{$('ph').style.display='grid';setTimeout(stream,2000);};
stream();
$('o').onchange=()=>fetch('/set?off='+$('o').value);
$('led').onchange=()=>fetch('/set?led='+$('led').value);
setInterval(tick,400);
// 앱과 같은 규칙 : 누르는 동안 0.2초마다 같은 글자, 떼면 s 두 번, 0.6초마다 h (연결 신호)
const send=c=>fetch('/cmd?c='+c).then(r=>{$('cs').textContent=r.status==503?'차 블루투스가 연결되지 않았습니다':'';}).catch(()=>{});
let held=null,rep=null,hb=null;
function press(b){release(false);held=b;b.classList.add('on');send(b.dataset.c);rep=setInterval(()=>send(b.dataset.c),200);}
function release(stop=true){if(rep){clearInterval(rep);rep=null;} if(held){held.classList.remove('on'); if(stop){send('s');setTimeout(()=>send('s'),60);}} held=null;}
document.querySelectorAll('.pad button').forEach(b=>{
  b.onpointerdown=e=>{e.preventDefault();b.setPointerCapture(e.pointerId);press(b);};
  b.onpointerup=b.onpointercancel=b.onlostpointercapture=()=>release();
});
const KEY={ArrowUp:'g',ArrowDown:'b',ArrowLeft:'l',ArrowRight:'r',q:'q',e:'w',' ':'s'};   // 노트북 키보드
addEventListener('keydown',e=>{const c=KEY[e.key];if(!c||e.repeat||e.target.tagName=='INPUT')return;e.preventDefault();press(document.querySelector(`.pad [data-c="${c}"]`));});
addEventListener('keyup',e=>{if(KEY[e.key])release();});
$('auto').onclick=()=>{if(confirm('자동 모드로 전환하시겠습니까?'))send('a');};
$('man').onclick=()=>send('m');
setInterval(()=>send('h'),600);
document.addEventListener('visibilitychange',()=>{if(document.hidden)release();});
</script></body></html>)HTML";

// 지금 프레임 + 띠·선 표시를 JPEG 로 (화면 한 장, 스트림 공용)
inline void px(int x, int y, uint8_t v) { view[(y * W + x) * 2] = view[(y * W + x) * 2 + 1] = v; }   // 흰색 255 / 검정 0

bool makeJpeg(uint8_t** jpg, size_t* len) {
  memcpy(view, color, W * H * 2);
  for (int x = 0; x < W; x++) {           // 띠 경계
    px(x, NEAR_Y0, 255); px(x, NEAR_Y1 - 1, 255);
    px(x, FAR_Y0, 255); px(x, FAR_Y1 - 1, 255);
  }
  for (int y = 0; y < H; y += 3) px(W / 2, y, (y & 1) ? 0 : 255);   // 화면 가운데
  const Band* bs[2] = {&lastNear, &lastFar};
  const int ys[2][2] = {{NEAR_Y0, NEAR_Y1}, {FAR_Y0, FAR_Y1}};
  for (int i = 0; i < 2; i++) if (bs[i]->found)                // 찾은 선 가운데
    for (int y = ys[i][0]; y < ys[i][1]; y++)
      for (int dx = -1; dx <= 1; dx++) { int x = bs[i]->cx + dx; if (x >= 0 && x < W) px(x, y, (y & 1) ? 0 : 255); }
  return fmt2jpg(view, W * H * 2, W, H, PIXFORMAT_RGB565, JPEG_Q, jpg, len);
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
    vTaskDelay(pdMS_TO_TICKS(STREAM_MS));
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
  if (httpd_start(&web, &cfg) != ESP_OK) return;
  struct { const char* u; httpd_method_t m; void (*f)(); } routes[] = {
    {"/", HTTP_GET, pageMain}, {"/cam", HTTP_GET, pageCam}, {"/save", HTTP_POST, pageSave},
    {"/status", HTTP_GET, handleStatus}, {"/set", HTTP_GET, handleSet}, {"/cmd", HTTP_GET, handleCmd},
    {"/bt", HTTP_GET, handleBt}, {"/btlist", HTTP_GET, pageList}};
  for (auto& x : routes) { httpd_uri_t u = {x.u, x.m, route, (void*)x.f}; httpd_register_uri_handler(web, &u); }
  httpd_uri_t st = {"/stream", HTTP_GET, streamHandler, nullptr};
  httpd_register_uri_handler(web, &st);
  httpd_register_err_handler(web, HTTPD_404_NOT_FOUND, notFound);
}

// 블루투스 : 따로 도는 작업에서 HC-06 에 붙고, 끊기면 다시 붙음 (찾는 데 몇 초 걸려서 loop 를 막지 않게)
void btTask(void*) {
  // 블루투스는 메모리를 많이 써서 (와이파이 + 카메라와 같이 켜면 모자람) "찾기"나 "연결"을 처음 누를 때만 켠다
  while (!btWant && !btScanReq) vTaskDelay(pdMS_TO_TICKS(200));
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
    if (btWant && btAddr.length() && !BT.connected()) BT.connect(BTAddress(btAddr));   // 몇 초 걸림
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

void handleStatus() {
  char buf[300];
  snprintf(buf, sizeof buf, "{\"nf\":%d,\"np\":%d,\"ff\":%d,\"fp\":%d,\"cross\":%d,\"off\":%d,\"led\":%d,\"fps\":%.1f,\"bt\":%d,\"btw\":%d,\"scan\":%d,\"btn\":\"%s\",\"car\":\"%s\"}",
           lastNear.found, lastNear.pos, lastFar.found, lastFar.pos, lastNear.cross, darkOffset, flashDuty, fps,
           btOn() ? 1 : 0, btWant ? 1 : 0, btScanning || btScanReq ? 1 : 0, esc(btName.length() ? btName : btAddr).c_str(),
           esc(carLine).c_str());
  send(200, "application/json", buf);
}

// 웹 조종 글자를 선(U0T)으로 아두이노에 "K,g" 처럼 전달. 버전 D 가 블루투스 글자와 똑같이 처리
void handleCmd() {
  String c = arg("c");
  if (setupMode || c.length() != 1 || !strchr("gblrsqwamh", c[0])) { send(403); return; }
  if (!btOn()) { send(503, "text/plain", "car bluetooth not connected"); return; }
  BT.write(c[0]);                          // 앱과 똑같이 글자 하나 (아두이노는 버전 C 그대로)
  send(204);
}

void handleSet() {
  if (hasArg("off")) darkOffset = constrain(arg("off").toInt(), 5, 120);
  if (hasArg("led")) flashDuty = constrain(arg("led").toInt(), 0, 255);
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
  prefs.begin("gulno", false);
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
  c.pixel_format = PIXFORMAT_RGB565;       // 컬러로 찍고, 선 찾기는 밝기만 뽑아서
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
  pinMode(STATUS_LED, OUTPUT); digitalWrite(STATUS_LED, HIGH);
  gray = (uint8_t*)ps_calloc(W * H, 1);
  color = (uint8_t*)ps_calloc(W * H * 2, 1);
  view = (uint8_t*)ps_malloc(W * H * 2);
  ledcAttachChannel(FLASH_LED, 5000, 8, 7); // 채널 7 (카메라 XCLK 가 채널 0 을 씀)
  if (!startCamera()) {
    while (true) { Serial.println("E,camera"); delay(1000); }   // 카메라 리본 케이블 확인
  }
  startWifi();
  startServer();
  xTaskCreatePinnedToCore(btTask, "bt", 4096, nullptr, 1, nullptr, 0);
}

void loop() {
  camera_fb_t* fb = esp_camera_fb_get();
  if (fb) {
    if (fb->len >= (size_t)(W * H * 2)) {
      const uint8_t* b = fb->buf;
      for (int i = 0; i < W * H; i++) {    // RGB565 -> 밝기 (빨강 30% 초록 59% 파랑 11%)
        uint16_t p = (b[2 * i] << 8) | b[2 * i + 1];
        gray[i] = (((p >> 11) & 31) * 79 + ((p >> 5) & 63) * 76 + (p & 31) * 29) >> 4;
      }
      lastNear = findLine(gray, NEAR_Y0, NEAR_Y1);
      lastFar = findLine(gray, FAR_Y0, FAR_Y1);
      memcpy(color, b, W * H * 2);
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
    Serial.printf("I,heap,%u,%u\n", (unsigned)ESP.getFreeHeap(), (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));
  }
  readCar();
  // 상태 LED : 설정 모드 = 깜빡 (0.5초), 핫스팟 연결 = 켜짐, 연결됐다 끊김 = 빠르게 깜빡
  bool on = setupMode ? (millis() / 500) % 2 : WiFi.status() == WL_CONNECTED ? true : (millis() / 120) % 2;
  digitalWrite(STATUS_LED, on ? LOW : HIGH);
  // 앞 플래시 LED : 연결 중(설정 모드, 핫스팟 연결 중, 블루투스 연결 중) = 깜빡, 다 연결되면 = 계속 켜짐 (바닥 비춤)
  bool connecting = setupMode || WiFi.status() != WL_CONNECTED || (btWant && !btOn());
  ledcWrite(FLASH_LED, connecting && (millis() / 400) % 2 ? 0 : flashDuty);
  if (setupMode) {
    dns.processNextRequest();
    if (savedSsid.length() && WiFi.status() == WL_CONNECTED) {   // 핫스팟이 나타나서 붙음 -> 설정 모드 끝
      Serial.printf("I,joined,%s\n", savedSsid.c_str());
      delay(300);
      ESP.restart();                       // 다시 켜면서 핫스팟 모드로 (켤 때 바로 붙음)
    }
  }
}
