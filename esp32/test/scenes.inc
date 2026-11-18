static uint8_t img[W*H*2];
uint16_t rgb(int r,int g,int b){r=r<0?0:r>255?255:r;g=g<0?0:g>255?255:g;b=b<0?0:b>255?255:b;return ((r>>3)<<11)|((g>>2)<<5)|(b>>3);}
void put(int x,int y,uint16_t p){ if(x<0||x>=W||y<0||y>=H) return; img[2*(y*W+x)]=p>>8; img[2*(y*W+x)+1]=p&255; }
void bg(int r=150,int g=140,int b=120){ for(int y=0;y<H;y++) for(int x=0;x<W;x++) put(x,y,rgb(r,g,b)); }
void rect(int x0,int y0,int x1,int y1,uint16_t p){ for(int y=y0;y<y1;y++) for(int x=x0;x<x1;x++) put(x,y,p); }
bool inOct(double x,double y,double cx,double cy,double r){ double dx=fabs(x-cx),dy=fabs(y-cy); return dx<=r&&dy<=r&&dx+dy<=r*1.414; }
void stopSign(int cx,int cy,int r,int R,int G,int B,int wv,bool box=true){
  if(box) rect(cx-r-3,cy-r-3,cx+r+3,cy+r+3,rgb(10,10,10));
  for(int y=cy-r;y<=cy+r;y++) for(int x=cx-r;x<=cx+r;x++) if(inOct(x,y,cx,cy,r)) put(x,y, inOct(x,y,cx,cy,r-2)? rgb(R,G,B) : rgb(wv,wv,wv));
  int L=cx-r+4,Rr=cx+r-4,wd=(Rr-L)/4; for(int k=0;k<4;k++) rect(L+k*wd,cy-r/4,L+k*wd+wd-1,cy+r/4+1,rgb(wv,wv,wv));
}
void noise(int n,uint16_t p,unsigned seed){ for(int i=0;i<n;i++){ seed=seed*1103515245u+12345u; int x=(seed>>8)%W; seed=seed*1103515245u+12345u; int y=(seed>>8)%H; put(x,y,p); put(x+1,y,p);} }
void quad(int xa,int xb,int ya0,int ya1,int yb0,int yb1,uint16_t p){ for(int x=xa;x<xb;x++){ double t=double(x-xa)/(xb-xa); int y0=ya0+(yb0-ya0)*t, y1=ya1+(yb1-ya1)*t; for(int y=y0;y<y1;y++) put(x,y,p);} }
const char* N[]={"none","red","green","yellow","STOP"};
int ok=0,tot=0;
void run(const char* name,int want){ for(int i=0;i<4;i++) detectSignal(img,false); int n=sigState; tot++; ok+=n==want;
  printf("%-52s want %-6s got %-6s fill %3d%% white %3d%% %s\n",name,N[want],N[n],(int)sigFill,(int)sigWhite,n==want?"ok":"MISS"); }
// RLE 풀기 : 다시 칠한 색 종류가 표에서 본 것과 같은지
bool rleCheck(){ rleOn=true; detectSignal(img,false); rleOn=false; int n=rleN; if(n<7||rleBuf[0]!=0x82||rleBuf[1]!=126||((rleBuf[2]<<8)|rleBuf[3])!=n-4||rleBuf[4]!=CW||rleBuf[5]!=CH) return false;   // 웹소켓 머리 + 가로·세로
  int i=0; for(int k=6;k<n;k++){ int c=rleBuf[k]>>5, len=(rleBuf[k]&31)+1; for(;len>0;len--,i++){ if(i>=W*H) return false; uint16_t p=(img[2*i]<<8)|img[2*i+1]; if(colorLut[lutIndex(p)]!=c) return false; } }
  return i==W*H; }
int main(){ buildColorLut(); const uint16_t RD=rgb(255,0,0), WH=rgb(255,255,255);
 // 예전 17가지
 bg(); rect(55,20,105,100,RD); run("phone red (full bright)",1);
 bg(); rect(55,20,105,100,rgb(140,10,10)); run("phone red, dark (55%)",1);
 bg(); rect(55,20,105,100,rgb(255,70,40)); run("phone red, orange tint (AWB)",1);
 bg(); rect(55,20,105,100,rgb(255,110,110)); run("phone red, washed out",1);
 bg(); rect(55,20,105,100,rgb(0,255,0)); run("phone green (full)",2);
 bg(); rect(55,20,105,100,rgb(40,200,120)); run("phone green, bluish (camera)",2);
 bg(); rect(55,20,105,100,rgb(10,110,30)); run("phone green, dark",2);
 bg(); rect(55,20,105,100,rgb(255,255,0)); run("phone yellow (full)",3);
 bg(); rect(55,20,105,100,rgb(230,200,40)); run("phone yellow, warm (camera)",3);
 bg(); rect(55,20,105,100,rgb(130,120,10)); run("phone yellow, dark",3);
 bg(); stopSign(80,60,30,255,0,0,255); run("stop sign (full)",4);
 bg(); stopSign(80,60,30,150,15,15,170); run("stop sign, dark",4);
 bg(); stopSign(80,60,14,230,50,40,220); run("stop sign far, tinted",4);
 bg(); stopSign(80,60,11,255,0,0,255); run("stop sign r=11",4);
 bg(); run("floor beige",0);
 bg(180,120,70); run("wood floor (brownish)",0);
 bg(60,60,60); run("dark gray room",0);
 bg(); rect(0,0,W,H,WH); run("white wall",0);
 // 2026-11-18 : 실제 화면에서 빨간불을 표지판으로 본 경우들
 bg(255,255,255); rect(50,70,95,110,RD); put(75,45,RD); put(76,45,RD); put(76,46,RD); put(140,20,RD); run("red on white wall + stray red dots",1);
 bg(255,255,255); rect(50,70,95,110,RD); rect(88,60,120,120,rgb(30,30,30)); noise(25,RD,7); run("red, partly hidden, white wall, 25 red specks",1);
 bg(255,255,255); rect(40,30,120,100,RD); run("big red on white wall",1);
 bg(255,255,255); quad(40,110,30,100,45,85,RD); run("red screen seen at an angle, white wall",1);
 bg(255,255,255); rect(55,20,105,100,RD); rect(55,92,105,100,rgb(40,40,40)); run("red screen with dark button bar, white wall",1);
 bg(255,255,255); rect(55,20,105,100,RD); rect(70,30,76,36,WH); run("red screen with small glare spot",1);
 bg(255,255,255); stopSign(80,60,30,255,0,0,255,false); run("stop sign on white wall (no dark box)",4);
 bg(255,255,255); stopSign(60,70,20,255,0,0,255,false); noise(25,RD,3); run("stop sign on white wall + red specks",4);
 bg(); stopSign(110,40,16,200,30,30,230); rect(10,90,30,110,rgb(255,255,0)); run("stop sign + small yellow thing",4);
 printf("\n%d/%d\n",ok,tot);
 // SD 기록은 HSV 보기를 따라감 : 켜면 ESP 가 본 색 종류, 끄면 실제 컬러 (기록이 같은 버퍼를 씀)
 bg(); stopSign(80,60,30,255,0,0,255);
 hsvView=true;  recOn=true; detectSignal(img,true); bool recHsv  = memcmp(color,img,W*H*2)!=0;
 hsvView=false; recOn=true; detectSignal(img,true); bool recReal = memcmp(color,img,W*H*2)==0;
 recOn=false;
 printf("recording follows HSV view : on = HSV %s, off = real colour %s\n",recHsv?"ok":"FAIL",recReal?"ok":"FAIL");
 if(!recHsv||!recReal) return 1;
 bg(); stopSign(80,60,30,255,0,0,255); noise(300,rgb(0,255,0),5); bool r1=rleCheck(); int n1=rleN;
 bg(255,255,255); rect(50,70,95,110,RD); bool r2=rleCheck(); int n2=rleN;
 printf("RLE roundtrip : sign+noise %s (%d bytes), red on white %s (%d bytes)\n",r1?"ok":"FAIL",n1,r2?"ok":"FAIL",n2);
 return !(ok==tot&&r1&&r2); }
