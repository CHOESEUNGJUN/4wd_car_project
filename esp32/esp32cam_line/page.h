// 굴노 카메라 웹 화면 (esp32cam_line.ino 에서 씀)
#pragma once
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
.video img,.video canvas{width:100%;height:100%;object-fit:contain;display:block}.video canvas{image-rendering:pixelated;display:none}
.video .ph{position:absolute;inset:0;display:grid;place-items:center;color:var(--mut)}
#s{color:var(--mut);font-size:13px;margin-top:8px}
.pad{display:grid;grid-template-columns:repeat(3,1fr);gap:10px;max-width:300px;margin:0 auto}
button{font:inherit;color:var(--fg);background:#232a35;border:1px solid #303846;border-radius:12px;padding:10px 14px;cursor:pointer;touch-action:manipulation}
button:active,button.on{background:var(--acc);border-color:var(--acc)}
.pad button{aspect-ratio:1;font-size:28px;touch-action:none;user-select:none;-webkit-user-select:none}.pad .stop{background:#3a2328;border-color:#5a2f37}
.row{display:flex;flex-wrap:wrap;gap:8px;margin-top:12px}.row button{flex:1}.row .badge{align-self:center;white-space:normal;max-width:100%}
#auto{background:#1f3a2e;border-color:#2b5a45}
table{width:100%;border-collapse:collapse}td{padding:6px 0;border-bottom:1px solid var(--line)}td:first-child{color:var(--mut);width:42%}
#raw{font:12px ui-monospace,Menlo,monospace;color:var(--mut);word-break:break-all;margin-top:8px}
#btl{display:grid;gap:6px;margin-top:10px}#btl button{text-align:left;display:flex;justify-content:space-between;gap:8px}#btl small{color:var(--mut)}
label{display:block;margin:6px 0 2px}select{font:inherit;color:var(--fg);background:#232a35;border:1px solid #303846;border-radius:8px;padding:4px 8px}input[type=range]{width:100%;accent-color:var(--acc)}.hint{color:var(--mut);font-size:12px;margin-top:8px}
</style></head><body>
<header><h1>굴노 카메라</h1><span id="bCam" class="badge">카메라</span><span id="bCar" class="badge">차 블루투스</span><span id="bFps" class="badge">- fps</span></header>
<main>
<section class="card"><div class="video"><div class="ph" id="ph">영상 연결 중…</div><img id="f" alt=""><canvas id="hc"></canvas></div><div id="s"></div></section>
<div class="side">
<section class="card"><h2>조종 (와이파이 → 블루투스)</h2>
<div class="pad"><button data-c="q">↺</button><button data-c="g">▲</button><button data-c="w">↻</button>
<button data-c="l">◀</button><button data-c="s" class="stop">■</button><button data-c="r">▶</button>
<span></span><button data-c="b">▼</button><span></span></div>
<div class="row"><button id="auto">라인센서 주행</button><button id="man">정지 · 수동</button></div>
<div class="row"><button id="sigBtn">신호등 인식 : 끔</button><span id="sigS" class="badge">-</span></div>
<div class="row"><button id="recBtn">SD 기록 : 끔</button><span id="recS" class="badge">-</span></div>
<div class="hint">라인은 차가 라인센서로 따라가고, 카메라는 신호등만 봅니다. 신호등 : 아이폰 앱 굴노의 <b>신호등 화면</b>을 카메라 앞에 두기. 빨강 = 정지, 초록 = 다시 주행, 노랑 = 서행, <b>정지 표지판</b> = 3초 서 있다가 출발</div><div id="cs" class="hint"></div></section>
<section class="card"><h2>접속 주소 (다른 폰·PC 에서)</h2><div id="addr" class="hint" style="margin:0">-</div></section>
<section class="card"><h2>차 상태</h2><table id="car"><tr><td>블루투스</td><td>연결 안 됨</td></tr></table><div id="raw"></div></section>
<section class="card"><h2>블루투스</h2><div id="bts" class="hint" style="margin:0 0 8px"></div>
<div class="row" style="margin:0"><button id="btScan">기기 찾기</button><button id="btOn">연결</button><button id="btOff">끊기</button></div><div id="btl"></div></section>
<section class="card"><h2>카메라 설정</h2>
<div class="row"><label><input type="checkbox" id="mir"> 좌우 반전</label><label><input type="checkbox" id="flip"> 위아래 뒤집기</label><label><input type="checkbox" id="hsv"> HSV 보기</label><label>회전 <select id="rot"><option value="0">0°</option><option value="90">90°</option><option value="180">180°</option><option value="270">270°</option></select></label></div>
<div class="hint">신호등 인식을 켜면 영상 테두리가 지금 신호 색 (빨강 / 초록 / 노랑, 정지 표지판 = 자홍, 없음 = 회색). 정지 표지판은 3초 서 있다가 출발. 아래 숫자는 사진에서 센 각 색 픽셀 수 : 기준(SIG_MIN)을 넘고 가장 많은 색이 신호<br>HSV 보기 : 영상을 ESP 가 본 색 종류로 칠함 (빨강·초록·노랑·흰색, 회색 = 신호 아님). 기준을 맞출 때 켜기<br>앞 LED : 연결 중 깜빡, 연결되면 켜짐</div></section>
</div></main>
<script>
const $=i=>document.getElementById(i);
const badge=(id,t,c)=>{$(id).textContent=t;$(id).className='badge '+(c||'');};
// 웹소켓 하나로 주고받기 : 상태는 ESP32 가 0.2초마다 밀어 주고, 조종 글자는 바로 보냄. 끊기면 1초 뒤 다시
let ws=null;
function connect(){
  ws=new WebSocket('ws://'+location.host+'/ws'); ws.binaryType='arraybuffer';
  ws.onopen=()=>{badge('bCam','카메라 연결','ok'); ws.send('h');};   // 첫 글자를 보내야 ESP32 가 이 화면을 등록함
  ws.onmessage=e=>{
    if(typeof e.data!=='string'){drawHsv(new Uint8Array(e.data));return;}   // HSV 영상 (색 종류 묶음)
    const s=JSON.parse(e.data); badge('bFps',s.fps+' fps'); cur=s; setHsv(!!s.hsv);
    $('sigBtn').textContent='신호등 인식 : '+(s.sigm?'켬':'끔'); $('sigBtn').classList.toggle('on',!!s.sigm);
    $('sigS').textContent=!s.sigm?'-':s.sig==1?'🔴 빨강 : 정지':s.sig==2?'🟢 초록 : 주행':s.sig==3?'🟡 노랑 : 서행':s.sig==4?'🛑 정지 표지판':'신호 없음';
    if(s.sigm&&s.stop>0)$('sigS').textContent='🛑 정지 표지판 : '+(s.stop/1000).toFixed(1)+'초 뒤 출발';
    $('s').textContent=s.sigm?`센 픽셀 : 빨강 ${s.rc} · 초록 ${s.gc} · 노랑 ${s.yc} (기준 ${s.smin} 넘고 가장 많은 색) · 빨강 덩어리 채움 ${s.sf}% 흰색 ${s.sw}% (채움 80% 아래 + 가운데 흰색 4% 넘으면 정지 표지판)`:'신호등 인식 꺼짐';
    $('recBtn').textContent='SD 기록 : '+(s.rec?'켬':'끔'); $('recBtn').classList.toggle('on',!!s.rec); $('recBtn').disabled=!s.sd;
    $('recS').textContent=!s.sd?'SD 카드 없음':s.rec?('rec_'+String(s.recn).padStart(4,'0')+' · '+s.recf+'장'):'-';
    $('addr').innerHTML=(s.ip?`이 핫스팟에서 : <b>http://${s.ip}</b> (아이폰·맥·PC 는 <b>gulno-cam.local</b> 도 됨)`:'')+(s.apn?`<br>어디서든 (핫스팟 없이도, 삼성폰 등) : 와이파이 <b>${s.apn}</b> 암호 <b>${s.apk}</b> 에 붙고 <b>http://192.168.4.1</b> (화면이 바로 뜨기도 함)`:'');
    $('mir').checked=!!s.mir; $('flip').checked=!!s.flip; if(document.activeElement!==$('rot'))$('rot').value=String(s.rot||0); $('hsv').checked=!!s.hsv; showCar(s);};
  ws.onclose=()=>{badge('bCam','카메라 끊김','bad'); setTimeout(connect,1000);};
}
connect();
// 아두이노 상태 줄 "ar,23,011,LINE,1,60" 풀어서 보여 주기
const ACT={g:'직진',b:'후진',l:'좌회전',r:'우회전',q:'제자리 좌',w:'제자리 우',s:'정지',o:'장애물 정지'};
let wasScan=0;
function showCar(s){
  if(s.btfail){badge('bCar','블루투스 불가','bad');$('bts').textContent='메모리 부족 : 이 카메라 보드는 와이파이·카메라와 블루투스를 같이 켤 수 없습니다';$('btl').innerHTML='';return;}
  const st=s.scan?'찾는 중…':s.bt?'연결됨':s.btw?'연결 중…':s.btn?'끊김':'기기를 찾아 고르세요';
  badge('bCar','차 '+(s.bt?'연결':s.btw?'연결 중':'끊김'),s.bt?'ok':s.btw?'warn':'');
  $('cs').textContent=s.bt?'':'차 블루투스가 연결되지 않아 조종 버튼이 동작하지 않습니다';
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
$('btScan').onclick=()=>{if(!confirm('기기를 찾는 동안 차와의 블루투스 연결이 잠깐 끊깁니다 (주행 중이면 차가 멈춤). 찾을까요?'))return; fetch('/bt?scan=1');$('btl').innerHTML='<div class="hint">찾는 중… (약 5초)</div>';};
$('btOn').onclick=()=>fetch('/bt?on=1');
$('btOff').onclick=()=>fetch('/bt?on=0');
// 화면 : /stream 연속 영상(MJPEG). 끊기면 2초 뒤 다시
const stream=()=>{if(!hsvOn)$('f').src='/stream?'+Date.now();};
// HSV 보기 : JPEG 영상을 끊고, 웹소켓으로 오는 색 종류 묶음(종류 3비트 + 칸 수 5비트)을 캔버스에 칠함 (ESP 가 압축을 안 해서 빠름)
let hsvOn=false,cur={};
const PAL=[[41,40,41],[255,36,0],[41,227,90],[255,215,0],[255,255,255]],SIGC=['#808080','#ff0000','#00ff00','#ffff00','#ff00ff'];
function setHsv(on){ if(on===hsvOn)return; hsvOn=on; $('hc').style.display=on?'block':'none'; $('f').style.display=on?'none':'block';
  if(on){$('f').removeAttribute('src');}else{$('ph').style.display='grid';stream();} }
function drawHsv(a){ if(!hsvOn||a.length<3)return; const w=a[0],h=a[1],r=cur.rot|0,ow=r&1?h:w,oh=r&1?w:h,cv=$('hc');
  if(cv.width!==ow||cv.height!==oh){cv.width=ow;cv.height=oh;}
  const ctx=cv.getContext('2d'),im=ctx.createImageData(ow,oh),d=im.data; let i=0;
  for(let k=2;k<a.length;k++){ const p=PAL[a[k]>>5]||PAL[0];
    for(let n=(a[k]&31)+1;n>0&&i<w*h;n--,i++){ const x=i%w,y=(i/w)|0,dx=r==1?h-1-y:r==2?w-1-x:r==3?y:x,dy=r==1?x:r==2?h-1-y:r==3?w-1-x:y,o=(dy*ow+dx)*4;
      d[o]=p[0];d[o+1]=p[1];d[o+2]=p[2];d[o+3]=255; } }
  ctx.putImageData(im,0,0);
  if(cur.sigm){ctx.strokeStyle=SIGC[cur.sig]||SIGC[0];ctx.lineWidth=6;ctx.strokeRect(0,0,ow,oh);}
  $('ph').style.display='none'; }
$('f').onload=()=>{$('ph').style.display='none';};
$('f').onerror=()=>{if(hsvOn)return;$('ph').style.display='grid';setTimeout(stream,2000);};
stream();
$('mir').onchange=()=>fetch('/set?mir='+($('mir').checked?1:0));
$('flip').onchange=()=>fetch('/set?flip='+($('flip').checked?1:0));
$('rot').onchange=()=>fetch('/set?rot='+$('rot').value);
$('hsv').onchange=()=>fetch('/set?hsv='+($('hsv').checked?1:0));   // ESP 가 본 색 종류로 칠한 영상 (빨강·초록·노랑·흰색, 회색 = 신호 아님)   // 카메라를 90도 돌려 달았을 때 (영상만 돌림, 신호 판단은 같음)
// 앱과 같은 규칙 : 누르는 동안 0.2초마다 같은 글자, 떼면 s 두 번. 연결 신호 h 는 ESP32 가 대신 보냄
const send=c=>{if(ws&&ws.readyState===1)ws.send(c);};
let held=null,rep=null;
function press(b){release(false);held=b;b.classList.add('on');send(b.dataset.c);rep=setInterval(()=>send(b.dataset.c),200);}
function release(stop=true){if(rep){clearInterval(rep);rep=null;} if(held){held.classList.remove('on'); if(stop){send('s');setTimeout(()=>send('s'),60);}} held=null;}
document.querySelectorAll('.pad button').forEach(b=>{
  b.onpointerdown=e=>{e.preventDefault();b.setPointerCapture(e.pointerId);press(b);};
  b.onpointerup=b.onpointercancel=b.onlostpointercapture=()=>release();
});
const KEY={ArrowUp:'g',ArrowDown:'b',ArrowLeft:'l',ArrowRight:'r',q:'q',e:'w',' ':'s'};   // 노트북 키보드
addEventListener('keydown',e=>{const c=KEY[e.key];if(!c||e.repeat||e.target.tagName=='INPUT')return;e.preventDefault();press(document.querySelector(`.pad [data-c="${c}"]`));});
addEventListener('keyup',e=>{if(KEY[e.key])release();});
// 라인센서 주행 = 아두이노 자동 모드(a, 라인센서 3개 + PID). 정지 = 수동(m)
$('auto').onclick=()=>{if(confirm('라인센서 자동 주행을 시작할까요?'))send('a');};
$('man').onclick=()=>send('m');
$('recBtn').onclick=()=>fetch('/set?rec='+($('recBtn').classList.contains('on')?0:1));
$('sigBtn').onclick=()=>fetch('/set?sig='+($('sigBtn').classList.contains('on')?0:1));
document.addEventListener('visibilitychange',()=>{if(document.hidden)release();});
</script></body></html>)HTML";
