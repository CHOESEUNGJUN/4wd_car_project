# 신호 판단 시험 (PC 에서) : .ino 에서 진짜 코드(상수, 색 종류표, detectSignal)를 뽑아 t.cpp 를 만듦.
# 실행 :  sh run.sh   (가짜 사진 27가지 + HSV 영상 묶음 풀기. 다 맞으면 끝 줄 27/27, 종료값 0)
import re,sys
import os
HERE=os.path.dirname(os.path.abspath(__file__))
src=open(os.path.join(HERE,'..','esp32cam_line','esp32cam_line.ino'),encoding='utf-8').read().split('\n')
D=next(k for k,l in enumerate(src) if l.startswith('const int CW = 160'))+1-38   # 줄 번호가 밀려도 맞게
L=lambda a,b: '\n'.join(src[a-1+D:b+D])
def fn(start):
    i=next(k for k,l in enumerate(src) if l.startswith(start)); j=next(k for k in range(i,len(src)) if src[k]=='}')
    return '\n'.join(src[i:j+1])
out='''#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <vector>
using std::max; using std::min;
#define HOT
volatile int camRot=0,viewW,viewH; static uint8_t colorb[160*120*2]; uint8_t* color=colorb;
'''
out+=L(38,39)+'\n'+L(41,41)+'\n'+L(48,51)+'\n'+L(54,65)+'\n'+fn('uint8_t colorClass')+'\n'+fn('void buildColorLut')+'\n'+L(84,85)+'\n'+L(88,88)+'\n'
out+='const int RLE_MAX = 8000; static uint8_t rleB[RLE_MAX+200]; uint8_t* rleBuf=rleB; volatile int rleN=0; bool rleOn=false;\n'
out+=fn('HOT bool detectSignal')+'\n#include "scenes.inc"\n'
open(os.path.join(HERE,'t.cpp'),'w').write(out)
