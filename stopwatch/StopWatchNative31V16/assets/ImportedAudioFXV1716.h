// V17.16: six renderers ported from stopwatch-fx.zip, with four explicit
// variants. Uses ONLY NativeApp.cpp's mic/FFT, bins[32], kick/bass/mids/highs,
// and beatStamp. The alternate mic and transform implementations supplied in the ZIP are NOT included.
// Adapted for the existing 466x466 RGB565 PSRAM sprite and round OLED.
#pragma once
#include <esp_heap_caps.h>
static constexpr int AF_G=116;
static lgfx::LGFX_Sprite afSmall(&M5.Display);
static bool afSmallOK=false,afSmallBad=false;
static uint8_t *afHeat=nullptr;
static int16_t *afWave=nullptr,*afPrev=nullptr;
static float afX[9],afY[9],afVX[9],afVY[9],afRad[9];
static float afPX[300],afPY[300],afPZ[300],afEX[300],afEY[300],afEZ[300];
static uint16_t afLut[256];
static uint32_t afSeed=0x9a359afU,afLastBeat=0,afBeatCount=0;
static uint8_t afLastID=255;
static float afTime=0,afDt=.033f,afBassPulse=0,afAngle=0,afMotion=0;
static float afSine[1024];
static bool afTrigReady=false;
static int afKaleidFold=8;
static uint32_t afFoldBeat=0;
static inline uint32_t afRnd(){afSeed^=afSeed<<13;afSeed^=afSeed>>17;afSeed^=afSeed<<5;return afSeed;}
static inline float afRand(){return (afRnd()>>8)*(1.f/16777216.f);}
static inline uint16_t afSwap(uint16_t c){return (c>>8)|(c<<8);}
static inline float afSin(float a){return afSine[((int)floorf(a*162.9746617f))&1023];}
static inline float afCos(float a){return afSine[(((int)floorf(a*162.9746617f))+256)&1023];}
static inline float afLevel(float v){
 float k=clampf(v,0.f,63.f)*31.f/63.f;int i=(int)k;
 return clampf(bins[i]+(bins[i<31?i+1:i]-bins[i])*(k-i),0.f,1.f);
}
static inline float afMin(float a,float b){return a<b?a:b;}
static bool afGrid(){
 if(afSmallOK)return true;
 if(afSmallBad)return false;
 afSmall.setColorDepth(16);afSmall.setPsram(true);
 afSmallOK=bool(afSmall.createSprite(AF_G,AF_G));
 afSmallBad=!afSmallOK;return afSmallOK;
}
static inline void afBlit(){
 afSmall.pushRotateZoomWithAA(&cv,float(CX),float(CY),0.f,float(W)/AF_G,float(H)/AF_G);
}
static void afFade(){
 uint16_t *p=(uint16_t*)cv.getBuffer();if(!p)return;
 const int count=W*H;
 for(int i=0;i<count;i++){uint16_t v=afSwap(p[i]);v=(v>>1)&0x7bef;p[i]=afSwap(v);}
}
static void afColors(int pal){
 for(int i=0;i<256;i++){
  float u=i/255.f,rr=0,gg=0,bb=0;
  if(pal==0){rr=clampf(u*1.66f);gg=clampf((u-.26f)*2.0f);bb=clampf(.09f+u*1.48f);}
  else if(pal==1){rr=clampf(u*2.22f);gg=clampf((u-.22f)*1.68f);bb=clampf((u-.85f)*4.f);}
  else if(pal==2){rr=clampf((u-.53f)*2.0f);gg=clampf(u*1.6f);bb=clampf(.15f+u*1.6f);}
  else if(pal==3){rr=clampf((u-.72f)*2.0f);gg=clampf(u*1.85f);bb=clampf(u*.25f);}
  else {
   float h=fmodf(u+afTime*.02f,1.f)*6.f,ss=clampf(u*1.4f),q=ss*(1.f-fabsf(fmodf(h,2.f)-1.f));
   if(h<1){rr=ss;gg=q;}else if(h<2){rr=q;gg=ss;}
   else if(h<3){gg=ss;bb=q;}else if(h<4){gg=q;bb=ss;}
   else if(h<5){rr=q;bb=ss;}else{rr=ss;bb=q;}
  }
  afLut[i]=afSwap(rgb((int)(rr*255),(int)(gg*255),(int)(bb*255)));
 }
}
static void afResetBlobs(){
 for(int i=0;i<9;i++){
  float a=TAU*i/9.f,d=.13f+.23f*((i*37)%11)/11.f;
  afX[i]=cosf(a)*d;afY[i]=sinf(a)*d;afVX[i]=afVY[i]=0;afRad[i]=.16f;
 }
}
static void afDrop(float x,float y,int radius,int amount){
 if(!afWave)return;
 for(int dy=-radius;dy<=radius;dy++)for(int dx=-radius;dx<=radius;dx++){
  int X=(int)x+dx,Y=(int)y+dy;float r=sqrtf(float(dx*dx+dy*dy));
  if(X<1||X>=AF_G-1||Y<1||Y>=AF_G-1||r>radius)continue;
  int i=Y*AF_G+X;
  afWave[i]=(int16_t)clampi(afWave[i]+int(amount*(.5f+.5f*cosf(PI_F*r/radius))),-28000,28000);
 }
}
static void afInit(uint8_t id){
 if(afLastID==id)return;
 afLastID=id;afTime=millis()*.001f;afDt=.033f;
 afLastBeat=beatStamp;afBassPulse=0;afAngle=0;afMotion=0;
 cv.fillScreen(0);
 if(!afTrigReady){for(int k=0;k<1024;k++)afSine[k]=sinf(TAU*k/1024.f);afTrigReady=true;}
 if(id==55||id==61)afResetBlobs();
 if(id==56||id==62){
  if(!afWave)afWave=(int16_t*)heap_caps_calloc(AF_G*AF_G,sizeof(int16_t),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
  if(!afPrev)afPrev=(int16_t*)heap_caps_calloc(AF_G*AF_G,sizeof(int16_t),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
  if(afWave)memset(afWave,0,AF_G*AF_G*2);
  if(afPrev)memset(afPrev,0,AF_G*AF_G*2);
 }
 if(id==58||id==62){
  if(!afHeat)afHeat=(uint8_t*)heap_caps_calloc(AF_G*AF_G,1,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
  if(afHeat)memset(afHeat,0,AF_G*AF_G);
 }
 afFoldBeat=afBeatCount;
}
static void afCorona(bool prism,bool onset){
 afFade();
 float spin=(prism?-.16f:.12f)*afTime;
 float levelSum=0;
 for(int i=0;i<64;i++)levelSum+=afLevel(float(i));
 float intensity=clampf(levelSum/64.f);
 int arcs=prism?4:2;
 for(int arc=0;arc<arcs;arc++){
  const float center=spin+(arc*TAU/arcs);
  for(int j=0;j<64;j++){
   const float a=center+(-.5f+j/63.f)*2.46f;
   const float lv=afLevel(float(j));
   float r0=prism?68.f:84.f;
   float rr=r0+(prism?112:148)*powf(lv,.78f)+afBassPulse*17.f;
   float x0=CX+r0*afCos(a),y0=CY+r0*afSin(a);
   float x1=CX+rr*afCos(a),y1=CY+rr*afSin(a);
   uint16_t c=hue((prism?0.02f:0.72f)+(j/64.f)*(prism?.82f:.33f)+afTime*.008f);
   line(x0,y0,x1,y1,tint(c,.32f),3);
   line(x0,y0,x1,y1,tint(c,.72f+.24f*lv),2);
   if(lv>.24f)dot(x1,y1,2,tint(rgb(245,252,255),.7f));
  }
 }
 int core=int(27+intensity*18+afBassPulse*17);
 for(int r=core+17;r>2;r-=5){
  dot(CX,CY,r,tint(hue(prism?.34f:.85f),clampf(1.f-r/float(core+28))));
 }
 circ(CX,CY,core+12,tint(rgb(202,238,255),.36f),3);
 if(onset)for(int i=0;i<3;i++)circ(CX,CY,140+i*21,tint(rgb(245,235,255),.46f/(i+1)),2);
}
static void afMagma(bool reactor){
 if(!afGrid())return;
 afColors(reactor?2:1);
 const float grav=.50f*afDt,now=afTime;
 for(int i=0;i<9;i++){
  int b=i*7;
  float level=0;for(int j=0;j<7;j++)level+=afLevel(float(b+j));level/=7;
  afRad[i]=.10f+(i%3)*.018f+level*(reactor?.20f:.15f)+afBassPulse*.027f;
  afVX[i]+=(afRand()-.5f)*.006f;
  afVY[i]+=(reactor?-grav:grav);
  afVX[i]*=.98f;afVY[i]*=.98f;
  afX[i]+=afVX[i];afY[i]+=afVY[i];
  float d=sqrtf(afX[i]*afX[i]+afY[i]*afY[i]),lim=.86f;
  if(d>lim){afX[i]*=lim/d;afY[i]*=lim/d;afVX[i]*=-.68f;afVY[i]*=-.68f;}
  if(afBassPulse>.78f){afVX[i]+=.018f*afCos(now*4.f+i);afVY[i]+=.02f*afSin(now*4.f+i);}
 }
 float xx[9][AF_G],rr[9];
 for(int i=0;i<9;i++){
  rr[i]=afRad[i]*afRad[i];for(int x=0;x<AF_G;x++){
   float dx=(x+.5f)*2.f/AF_G-1.f-afX[i];xx[i][x]=dx*dx;
  }
 }
 uint16_t *pix=(uint16_t*)afSmall.getBuffer();
 for(int y=0;y<AF_G;y++){
  float dy[9];float Y=(y+.5f)*2.f/AF_G-1.f;
  for(int i=0;i<9;i++){float d=Y-afY[i];dy[i]=d*d+.003f;}
  for(int x=0;x<AF_G;x++){
   float fld=0;for(int k=0;k<9;k++)fld+=rr[k]/(xx[k][x]+dy[k]);
   float halo=clampf(fld*.45f),body=clampf((fld-.87f)*2.f),core=clampf((fld-1.35f)*.65f);
   float value=halo*halo*.22f+body*.55f+core*.27f;
   pix[y*AF_G+x]=afLut[clampi((int)(value*255),0,255)];
  }
 }
 afBlit();
 if(reactor)for(int i=0;i<5;i++)circ(CX,CY,104+i*25,tint(rgb(77,206,255),.10f+.05f*afBassPulse),2);
}
static void afRipple(bool echo,bool onset){
 if(!afGrid()||!afWave||!afPrev)return;
 if(onset){afDrop(58,58,4,int(3200+2100*bass));if(echo)afDrop(35,69,3,2400);}
 if(afRand()<mids*.3f){float a=TAU*afRand(),r=10+35*afRand();afDrop(58+r*cosf(a),58+r*sinf(a),2,1700);}
 for(int step=0;step<2;step++){
  for(int y=1;y<AF_G-1;y++)for(int x=1;x<AF_G-1;x++){
   int i=y*AF_G+x;
   int v=((afWave[i-1]+afWave[i+1]+afWave[i-AF_G]+afWave[i+AF_G])>>1)-afPrev[i];
   float a=(x-57.5f)/57.5f,b=(y-57.5f)/57.5f,d=sqrtf(a*a+b*b);
   float absorb=clampf((1.0f-d)/.15f);
   afPrev[i]=(int16_t)clampi((int)(v*.982f*absorb),-29000,29000);
  }
  int16_t *tmp=afWave;afWave=afPrev;afPrev=tmp;
 }
 afColors(echo?0:2);
 uint16_t *pix=(uint16_t*)afSmall.getBuffer();
 for(int y=0;y<AF_G;y++)for(int x=0;x<AF_G;x++){
  int i=y*AF_G+x;
  if(x<1||x>=AF_G-1||y<1||y>=AF_G-1){pix[i]=0;continue;}
  int dx=afWave[i+1]-afWave[i-1],dy=afWave[i+AF_G]-afWave[i-AF_G];
  float glint=clampf((dx-dy)*.0021f),contrast=clampf((abs(dx)+abs(dy))*.0012f);
  float val=.05f+.29f*contrast+.67f*glint;
  pix[i]=afLut[clampi((int)(val*255),0,255)];
 }
 afBlit();
 if(echo)circ(CX,CY,190,tint(rgb(225,127,255),.27f),2);
}
static void afKaleido(bool hyper,bool onset){
 if(!afGrid())return;
 if(onset&&afBeatCount-afFoldBeat>=(hyper?8:24)){
  afFoldBeat=afBeatCount;
  static const int folds[]={6,8,12,10,16};
  afKaleidFold=folds[(afBeatCount/8)%5];
 }
 afColors(hyper?4:0);
 const float sectors=float(afKaleidFold),seg=TAU/sectors,rot=afTime*(hyper?.36f:.13f);
 const float z=(hyper?9.f:6.f)+5.f*bass+2.f*afBassPulse;
 uint16_t *pix=(uint16_t*)afSmall.getBuffer();
 for(int y=0;y<AF_G;y++)for(int x=0;x<AF_G;x++){
  float X=(x+.5f)*2.f/AF_G-1.f,Y=(y+.5f)*2.f/AF_G-1.f;
  float r=sqrtf(X*X+Y*Y);if(r>1.f){pix[y*AF_G+x]=0;continue;}
  float a=atan2f(Y,X)+rot;
  a-=seg*floorf(a/seg);a=fabsf(a-seg*.5f);
  float U=r*afCos(a)*z,V=r*afSin(a)*z;
  float value=(afSin(U*1.9f+afTime*.86f)+afSin(V*2.4f-afTime*.74f)
    +afSin((U+V)*1.2f+afTime*.52f)+afSin(r*z*2.1f-afTime*2.f))*.25f;
  float ridge=clampf(1.f-fabsf(value)*7.f)*.55f;
  float lv=afLevel(r*47.f);
  float br=.28f+.72f*clampf(lv*1.3f+afBassPulse*.3f);
  float intensity=br*(.31f+.69f*(value*.5f+.5f))+ridge*br;
  pix[y*AF_G+x]=afLut[clampi((int)(intensity*255.f),0,255)];
 }
 afBlit();
}
static void afInferno(bool blue,bool onset){
 if(!afGrid()||!afHeat)return;
 afColors(blue?2:1);
 for(int row=0;row<2;row++)for(int x=0;x<AF_G;x++){
  float k=fabsf(x-57.5f)/57.5f,lv=afLevel(k*63.f);
  float v=lv*210.f+afRand()*40.f+afBassPulse*62.f+12.f;
  afHeat[(AF_G-1-row)*AF_G+x]=(uint8_t)clampi((int)v,0,255);
 }
 for(int y=0;y<AF_G-2;y++)for(int x=0;x<AF_G;x++){
  int l=x>0?x-1:x,r=x<AF_G-1?x+1:x;
  int j=(y+1)*AF_G,k=(y+2)*AF_G;
  int v=((afHeat[j+l]+afHeat[j+x]+afHeat[j+r]+afHeat[k+x])>>2)-(afRnd()%4);
  afHeat[y*AF_G+x]=(uint8_t)clampi(v,0,255);
 }
 uint16_t *pix=(uint16_t*)afSmall.getBuffer();
 for(int i=0;i<AF_G*AF_G;i++)pix[i]=afLut[afHeat[i]];
 afBlit();
 int n=clampi((int)(highs*10)+ (onset?18:0),0,22);
 for(int i=0;i<n;i++){
  float x=CX+(afRand()-.5f)*380.f,y=CY+(afRand()-.5f)*380.f;
  dot(x,y,1+afRand()*2,tint(blue?rgb(133,243,255):rgb(255,194,72),.44f+.43f*afRand()));
 }
}
static void afSpike(bool nova,bool onset){
 afFade();
 afAngle+=afDt*(.34f+mids*1.5f);
 float co=afCos(afAngle),si=afSin(afAngle);
 float co2=afCos(afAngle*.7f),si2=afSin(afAngle*.7f);
 int count=nova?300:250;
 float rad=80.f+10.f*bass;
 for(int layer=0;layer<2;layer++)for(int i=0;i<count;i++){
  float y=1.f-2.f*(i+.5f)/count,p=i*2.399963f;
  float R=sqrtf(clampf(1.f-y*y)),X=R*afCos(p),Z=R*afSin(p);
  float rx=co*X+si*Z,rz=co*Z-si*X;
  float ry=co2*y-si2*rz,depth=si2*y+co2*rz;
  bool front=depth>0;
  if(front!=(layer==1))continue;
  float bandID=fabsf(y)*63,lv=afLevel(bandID);
  float extent=rad+(nova?175.f:145.f)*powf(lv,1.12f)+afBassPulse*16.f;
  float perspective=1.f/(1.f-depth*.22f);
  float sx=CX+rx*rad*perspective,sy=CY+ry*rad*perspective;
  float ex=CX+rx*extent*perspective,ey=CY+ry*extent*perspective;
  uint16_t color=hue((nova?.39f:.83f)+bandID/63.f*(nova?.63f:.27f));
  line(sx,sy,ex,ey,tint(color,front?.92f:.24f),front?2:1);
  if(front&&lv>.20f)dot(ex,ey,2,rgb(235,248,255));
 }
 for(int r=int(rad);r>0;r-=9)
  dot(CX,CY,r,tint(nova?rgb(81,209,243):rgb(222,116,255),.25f*(1.f-r/float(rad+1))));
 if(onset)circ(CX,CY,rad+13,rgb(218,244,255),2);
}
static bool renderImportedAudioV1716(uint8_t id){
 if(id<54||id>63)return false;
 afInit(id);
 if(!afTrigReady)return true;
 float now=millis()*.001f;
 afDt=clampf(now-afTime,.012f,.08f);afTime=now;
 bool onset=beatStamp&&beatStamp!=afLastBeat;
 if(onset){afLastBeat=beatStamp;afBeatCount++;afBassPulse=1.f;}
 afBassPulse*=powf(.016f,afDt);
 switch(id){
 case 54:afCorona(false,onset);break;
 case 55:afMagma(false);break;
 case 56:afRipple(false,onset);break;
 case 57:afKaleido(false,onset);break;
 case 58:afInferno(false,onset);break;
 case 59:afSpike(false,onset);break;
 case 60:afCorona(true,onset);break;
 case 61:afMagma(true);break;
 case 62:afInferno(true,onset);break;
 case 63:afSpike(true,onset);break;
 }
 return true;
}
