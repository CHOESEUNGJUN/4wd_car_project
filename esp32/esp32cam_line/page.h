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
.video img,.video canvas{width:100%;height:100%;object-fit:contain;display:block}.video canvas{display:none;background:#fff}
.video .ph{position:absolute;inset:0;display:grid;place-items:center;color:var(--mut)}
.dir{position:absolute;left:10px;top:10px;display:flex;align-items:center;gap:8px;padding:6px 12px 6px 8px;border-radius:12px;background:rgba(0,0,0,.6);font-weight:600}
.dir b{font-size:34px;line-height:1}#s{color:var(--mut);font-size:13px;margin-top:8px}
.pad{display:grid;grid-template-columns:repeat(3,1fr);gap:10px;max-width:300px;margin:0 auto}
button{font:inherit;color:var(--fg);background:#232a35;border:1px solid #303846;border-radius:12px;padding:10px 14px;cursor:pointer;touch-action:manipulation}
button:active,button.on{background:var(--acc);border-color:var(--acc)}
.pad button{aspect-ratio:1;font-size:28px;touch-action:none;user-select:none;-webkit-user-select:none}.pad .stop{background:#3a2328;border-color:#5a2f37}
.row{display:flex;flex-wrap:wrap;gap:8px;margin-top:12px}.row button{flex:1}
#auto,#camGo{background:#1f3a2e;border-color:#2b5a45}#camGo:disabled{opacity:.4}#camGo.on{background:var(--acc)}
table{width:100%;border-collapse:collapse}td{padding:6px 0;border-bottom:1px solid var(--line)}td:first-child{color:var(--mut);width:42%}
#raw{font:12px ui-monospace,Menlo,monospace;color:var(--mut);word-break:break-all;margin-top:8px}
#btl{display:grid;gap:6px;margin-top:10px}#btl button{text-align:left;display:flex;justify-content:space-between;gap:8px}#btl small{color:var(--mut)}
label{display:block;margin:6px 0 2px}input[type=range]{width:100%;accent-color:var(--acc)}.hint{color:var(--mut);font-size:12px;margin-top:8px}
</style></head><body>
<header><h1>굴노 카메라</h1><span id="bCam" class="badge">카메라</span><span id="bCar" class="badge">차 블루투스</span><span id="bFps" class="badge">- fps</span></header>
<main>
<section class="card"><div class="video"><div class="ph" id="ph">영상 연결 중…</div><img id="f" alt=""><canvas id="cv" width="160" height="120"></canvas>
<div class="dir"><b id="dir">…</b><span id="dirT">-</span></div></div><div id="s"></div></section>
<div class="side">
<section class="card"><h2>조종 (와이파이 → 블루투스)</h2>
<div class="pad"><button data-c="q">↺</button><button data-c="g">▲</button><button data-c="w">↻</button>
<button data-c="l">◀</button><button data-c="s" class="stop">■</button><button data-c="r">▶</button>
<span></span><button data-c="b">▼</button><span></span></div>
<div class="row"><button id="auto">라인센서 주행</button><button id="camGo">카메라 주행</button><button id="man">정지 · 수동</button></div><div id="cs" class="hint"></div></section>
<section class="card"><h2>차 상태</h2><table id="car"><tr><td>블루투스</td><td>연결 안 됨</td></tr></table><div id="raw"></div></section>
<section class="card"><h2>블루투스</h2><div id="bts" class="hint" style="margin:0 0 8px"></div>
<div class="row" style="margin:0"><button id="btScan">기기 찾기</button><button id="btOn">연결</button><button id="btOff">끊기</button></div><div id="btl"></div></section>
<section class="card"><h2>카메라 설정</h2>
<div class="row"><label><input type="checkbox" id="mir"> 좌우 반전</label><label><input type="checkbox" id="flip"> 위아래 뒤집기</label><label><input type="checkbox" id="morph"> 모폴로지 (빠름)</label></div>
<div class="hint">컬러 : 초록 = 인식한 라인 · 노란 점 = 경로 · 빨간 선·점 = 향할 곳 · 흰 점선 = 차 가운데<br>모폴로지 : 영상 대신 숫자로 받아 그림 (가벼움) · 흰 바탕에 정리된 검정 라인 · 노란 점 = 경로 · 빨간 선·점 = 향할 곳 · 파란 점선 = 차 가운데<br>앞 LED : 연결 중 깜빡, 연결되면 켜짐</div></section>
</div></main>
<script>
const $=i=>document.getElementById(i);
const badge=(id,t,c)=>{$(id).textContent=t;$(id).className='badge '+(c||'');};
// 웹소켓 하나로 주고받기 : 상태는 ESP32 가 0.2초마다 밀어 주고, 조종 글자는 바로 보냄. 끊기면 1초 뒤 다시
let ws=null;
function connect(){
  ws=new WebSocket('ws://'+location.host+'/ws');
  ws.binaryType='arraybuffer';
  ws.onopen=()=>{badge('bCam','카메라 연결','ok'); ws.send('h');};   // 첫 글자를 보내야 ESP32 가 이 화면을 등록함
  ws.onmessage=e=>{
    if(typeof e.data!=='string'){drawMask(new Uint8Array(e.data));return;}   // 모폴로지 화면 (RLE)
    const s=JSON.parse(e.data); badge('bFps',s.fps+' fps'); setView(!!s.morph);
    $('camGo').disabled=!s.morph||!s.bt; $('camGo').classList.toggle('on',!!s.cam);
    $('camGo').title=!s.morph?'카메라 설정에서 모폴로지로 바꾼 뒤 쓸 수 있습니다':!s.bt?'차 블루투스 연결이 필요합니다':'';
    $('mir').checked=!!s.mir; $('flip').checked=!!s.flip; $('morph').checked=!!s.morph; showDir(s); showCar(s);};
  ws.onclose=()=>{badge('bCam','카메라 끊김','bad'); setTimeout(connect,1000);};
}
connect();
// 카메라가 본 선 -> 직진 / 우회전 / 좌회전 (가까운 띠 70% + 먼 띠 30%)
function showDir(s){
  let t,a;
  if(!s.nf&&!s.ff){a='✕';t='선 없음';}
  else if(s.cross){a='╋';t='교차선';}
  else{const p=s.nf&&s.ff?s.np*0.7+s.fp*0.3:(s.nf?s.np:s.fp);
    if(p>25){a='➡';t='우회전';}else if(p<-25){a='⬅';t='좌회전';}else{a='⬆';t='직진';}}
  $('dir').textContent=a; $('dirT').textContent=t;
  $('s').textContent=`가까운 곳 ${s.nf?s.np:'없음'} · 먼 곳 ${s.ff?s.fp:'없음'}`;
}
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
// 화면 : 컬러 = /stream 연속 영상(MJPEG), 모폴로지 = 웹소켓으로 온 마스크(RLE)를 캔버스에 그림 (영상 안 받음)
let morph=null;
const stream=()=>{if(morph===false)$('f').src='/stream?'+Date.now();};
$('f').onload=()=>{$('ph').style.display='none';};
$('f').onerror=()=>{if(morph===false){$('ph').style.display='grid';setTimeout(stream,2000);}};
function setView(m){
  if(m===morph)return; morph=m;
  $('f').style.display=m?'none':'block'; $('cv').style.display=m?'block':'none';
  if(m){$('f').removeAttribute('src');$('ph').style.display='none';} else stream();
}
const cx=$('cv').getContext('2d'), img=cx.createImageData(160,120);
function drawMask(b){
  if(b[0]!==77)return;                                     // 'M'
  const d=img.data; d.fill(255);                           // 흰 바탕
  const path=[]; let k=3;
  for(let y=0;y<120&&k<b.length;y++){
    const c=b[k++], n=b[k++];
    for(let i=0;i<n;i++){const s=b[k++],e=b[k++]; for(let x=s;x<=e;x++){const p=(y*160+x)*4; d[p]=d[p+1]=d[p+2]=0;}}   // 검정 라인
    if(c!==255)path.push([c,y]);
  }
  cx.putImageData(img,0,0);
  cx.fillStyle='#f5b84a'; path.forEach(([x,y],i)=>{if(i%2==0)cx.fillRect(x,y,1,1);});   // 경로 (노랑)
  cx.fillStyle='#4f9dff'; for(let y=104;y<120;y+=2)cx.fillRect(80,y,1,1);                // 차 가운데 (파랑)
  if(b[1]!==255){cx.strokeStyle='#ff4b4b';cx.lineWidth=1.5;cx.beginPath();cx.moveTo(80,119);cx.lineTo(b[1],b[2]);cx.stroke();
    cx.fillStyle='#ff4b4b';cx.fillRect(b[1]-2,b[2]-2,5,5);}                               // 향할 곳 (빨강)
}
$('mir').onchange=()=>fetch('/set?mir='+($('mir').checked?1:0));
$('flip').onchange=()=>fetch('/set?flip='+($('flip').checked?1:0));
$('morph').onchange=()=>{if(confirm('화면 모드를 바꾸면 카메라가 다시 켜집니다 (몇 초). 바꿀까요?'))fetch('/set?morph='+($('morph').checked?1:0));else $('morph').checked=!$('morph').checked;};   // 라인 확인이 끝나면 켜서 더 빠르게
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
// 라인센서 주행 = 아두이노 자동 모드(a). 카메라 주행 = ESP32 가 카메라 판단으로 조종 (모폴로지 화면일 때만). 정지 = 둘 다 끄고 수동
$('auto').onclick=()=>{if(confirm('라인센서 자동 주행을 시작할까요?')){send('X');send('a');}};
$('camGo').onclick=()=>{if(confirm('카메라 주행을 시작할까요? (ESP32 가 카메라로 보고 조종합니다)'))send('C');};
$('man').onclick=()=>{send('X');send('m');};
document.addEventListener('visibilitychange',()=>{if(document.hidden)release();});
</script></body></html>)HTML";
