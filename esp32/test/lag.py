# SPDX-License-Identifier: MIT · Copyright (c) 2026 4wd-car Team 2 "Gulno" (굴러라 노란바퀴) · see LICENSE, NOTICE.md
# 보드 시험 : 웹 화면의 차 상태 줄이 얼마나 자주 오는지 (HSV 영상을 받으면서). 실행 : python3 lag.py 15 1   (15초, 영상마다 답 k 보냄)
# 상태 줄(글자 프레임)이 얼마나 고르게 오는지 : HSV 영상을 받으면서. ack=1 이면 영상 한 장마다 'k' 를 보냄
import socket,os,base64,struct,time,json,sys,subprocess,statistics
secs=float(sys.argv[1]); ack=sys.argv[2]=='1'
subprocess.run(['curl','-s','-m','5','http://gulno-cam.local/set?hsv=1'],capture_output=True)
s=socket.create_connection((os.environ.get('ESP','gulno-cam.local'),80),timeout=12)   # ESP=172.20.10.2 처럼 주소를 바꿀 수 있음
key=base64.b64encode(os.urandom(16)).decode()
s.sendall(f'GET /ws HTTP/1.1\r\nHost: gulno-cam.local\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Key: {key}\r\nSec-WebSocket-Version: 13\r\n\r\n'.encode())
buf=b''
while b'\r\n\r\n' not in buf: buf+=s.recv(4096)
buf=buf.split(b'\r\n\r\n',1)[1]
def sendc(c):
    m=os.urandom(4); s.sendall(bytes([0x81,0x81])+m+bytes([ord(c)^m[0]]))
sendc('h')
def need(n):
    global buf
    while len(buf)<n:
        d=s.recv(65536)
        if not d: raise EOFError
        buf+=d
def frame():
    global buf
    need(2); op=buf[0]&15; ln=buf[1]&127; o=2
    if ln==126: need(4); ln=struct.unpack('>H',buf[2:4])[0]; o=4
    need(o+ln); p=buf[o:o+ln]; buf=buf[o+ln:]; return op,p
t0=time.time(); tt=[]; nb=0
while time.time()-t0<secs:
    op,p=frame()
    if op==1: tt.append(time.time())
    elif op==2:
        nb+=1
        if ack: sendc('k')
gaps=[b-a for a,b in zip(tt,tt[1:])]
print(f"ack={int(ack)} status {len(tt)} in {secs:.0f}s, gap avg {statistics.mean(gaps):.2f}s max {max(gaps):.2f}s | video {nb/secs:.1f} fps")
