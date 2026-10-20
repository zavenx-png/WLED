// StopWatch Club Visualizer v1 - M5Stack StopWatch C152 / ESP32-S3
// Requires M5Stack Arduino board package >= 3.3.8, M5Unified >= 0.2.19.
// Built-in mic, 468px-class AMOLED, BMI270, vibration motor and ESP-NOW.
// 20 independent visual programs and a direct Z-effects WLED remote.
#include <Arduino.h>
#include <M5Unified.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_now.h>
#include <Preferences.h>
#include <math.h>

constexpr int FFT_N = 512;
constexpr int SAMPLE_RATE = 16000;
constexpr int BANDS = 32;
constexpr int VISUAL_COUNT = 20;
constexpr int Z_COUNT = 28;
constexpr float PI2 = 6.2831853071795864769f;
static_assert((FFT_N & (FFT_N - 1)) == 0, "FFT_N must be a power of two");

static const char* const visualNames[VISUAL_COUNT] = {
  "BLUE NOTE VU", "PRISM EQ", "PSYCHEDELIC BLOOM", "VAPOR SCOPE",
  "MATRIX PHOSPHOR", "DANCING RIBBONS", "FREQUENCY TOWERS",
  "RADIAL SPECTRUM", "HEARTBEAT NEON", "ROUND RESONATOR",
  "KALEIDOSCOPE REACTOR", "FIBONACCI SUNFLOWER", "INFINITY TUNNEL",
  "CYMATIC MANDALA", "FRACTAL LIGHTNING", "HYPERSPACE STARFIELD",
  "LIQUID CHROME", "ORBITAL HARMONICS", "QUANTUM MOIRE",
  "NEBULA PARTICLES"
};

// This order is exactly the Z - effect registration order in the harness branch.
// Command 9 finds the Nth "Z -" effect on the harness: no WLED presets required.
static const char* const zNames[Z_COUNT] = {
  "Nokia Snake", "Chunchun Harness", "Neural Pulse", "Plasma Veins", "Electric Organism", "Gravity Well",
  "Wormhole", "Rainbow Fracture", "Starlight Nervous System",
  "Phoenix Flight", "Comet Storm", "Deep Current",
  "Tidal Interference", "Ember Drift", "Orbital Waltz",
  "Silk Ribbon", "Twin Moons", "Liquid Mercury", "Echo Chamber",
  "Prism Cascade", "Gravity Lens", "Quiet Storm", "Infinite Corridor",
  "Meteor Circuit", "Meteor Twin Sync", "Meteor Ricochet",
  "Meteor Crossover", "Meteor Echo Rain"
};

lgfx::LGFX_Sprite canvas(&M5.Display);
Preferences nv;
int16_t sampleBuf[FFT_N] = {};
float re[FFT_N] = {}, im[FFT_N] = {};
float spectrum[BANDS] = {}, peaks[BANDS] = {}, scope[256] = {};
float envelope = 0, bass = 0, treble = 0, transient = 0, beatLevel = 0;
float noiseFloor = 0.005f, autoGain = 1.0f;
float previousEnergy = 0, phaseClock = 0;
bool audioPending = false, audioReady = false, haveAudio = false;
uint32_t beatWhen = 0, frameWhen = 0, vibUntil = 0, sentWhen = 0;
uint32_t lastSignal = 0, seqCounter = 0;
uint32_t flatSince = 0, uprightSince = 0;
bool faceUp = false, palmMode = false, overrideMode = false;
bool haptics = true, radioOK = false, awaitingAck = false, ackOK = false;
bool ackError = false, dimmed = false;
uint8_t visualIndex = 0, zIndex = 0, channel = 1;
uint8_t remoteBrightness = 120, remoteSpeed = 128, remoteIntensity = 128;
uint8_t page = 0; // 0=Z library, 1=presets, 2=adjust, 3=pair/channel
uint8_t presetID = 1;
uint8_t lastCmd = 0, ackBrightness = 0, ackPreset = 0;
uint32_t ackSeq = 0;
int W = 468, H = 468, CX = 234, CY = 234;
bool haveSprite = false;
portMUX_TYPE ackMux = portMUX_INITIALIZER_UNLOCKED;
struct Ack { uint32_t seq; uint8_t status, brightness, preset; bool ready; };
Ack incoming = {};
const uint8_t BCAST_MAC[6] = {255,255,255,255,255,255};

float clampf(float x, float a = 0, float b = 1) { return x < a ? a : (x > b ? b : x); }
int cxi(float x) { return CX + (int)roundf(x); }
int cyi(float y) { return CY + (int)roundf(y); }
uint16_t color565(int r, int g, int b) {
  r = constrain(r,0,255); g = constrain(g,0,255); b = constrain(b,0,255);
  return (uint16_t)(((r & 248)<<8) | ((g & 252)<<3) | (b>>3));
}
uint16_t hsv(float hue, float sat=1, float value=1) {
  hue -= floorf(hue); sat = clampf(sat); value = clampf(value);
  float h = hue*6, f = h-floorf(h);
  float p=value*(1-sat), q=value*(1-f*sat), t=value*(1-(1-f)*sat);
  float r,g,b;
  switch ((int)h % 6) {
    case 0: r=value;g=t;b=p;break;
    case 1: r=q;g=value;b=p;break;
    case 2: r=p;g=value;b=t;break;
    case 3: r=p;g=q;b=value;break;
    case 4: r=t;g=p;b=value;break;
    default:r=value;g=p;b=q;break;
  }
  return color565(int(r*255),int(g*255),int(b*255));
}
uint16_t mixColor(uint16_t a,uint16_t b,float t) {
  t=clampf(t); int ar=(a>>11)&31,ag=(a>>5)&63,ab=a&31;
  int br=(b>>11)&31,bg=(b>>5)&63,bb=b&31;
  return (uint16_t)((int(ar+(br-ar)*t)<<11)|
                    (int(ag+(bg-ag)*t)<<5)|int(ab+(bb-ab)*t));
}
void haptic(uint8_t strength=190, uint16_t duration=55) {
  if (!haptics) return;
  M5.Power.setVibration(strength);
  vibUntil=millis()+duration;
}
void tickVibration() {
  if (vibUntil && (int32_t)(millis()-vibUntil)>=0) {
    M5.Power.setVibration(0); vibUntil=0;
  }
}
void fft512() {
  // Cooley-Tukey complex radix-2 FFT; input carries a Hann window.
  for (int i=1,j=0;i<FFT_N;i++) {
    int bit=FFT_N>>1;
    for (;j&bit;bit>>=1) j^=bit;
    j^=bit;
    if(i<j) { float t=re[i];re[i]=re[j];re[j]=t;
               t=im[i];im[i]=im[j];im[j]=t; }
  }
  for (int len=2;len<=FFT_N;len<<=1) {
    float angle=-PI2/len, wr0=cosf(angle),wi0=sinf(angle);
    for(int i=0;i<FFT_N;i+=len) {
      float wr=1,wi=0;
      for(int j=0;j<len/2;j++){
        int a=i+j,b=a+len/2;
        float vr=re[b]*wr-im[b]*wi, vi=re[b]*wi+im[b]*wr;
        re[b]=re[a]-vr;im[b]=im[a]-vi;
        re[a]+=vr;im[a]+=vi;
        float nw=wr*wr0-wi*wi0;wi=wr*wi0+wi*wr0;wr=nw;
      }
    }
  }
}
void processAudio() {
  float dc=0, energy=0;
  for(int i=0;i<FFT_N;i++) dc+=sampleBuf[i];
  dc/=FFT_N;
  for(int i=0;i<FFT_N;i++){
    float v=(sampleBuf[i]-dc)/32768.f;
    energy+=v*v;
    if(i<256) scope[i]=scope[i]*0.30f+v*0.70f;
    float w=0.5f*(1-cosf(PI2*i/(FFT_N-1)));
    re[i]=v*w; im[i]=0;
  }
  energy=sqrtf(energy/FFT_N);
  // Adaptive noise gating and AGC to work in quiet rooms and loud clubs.
  if (energy<noiseFloor*1.8f) noiseFloor=noiseFloor*0.997f+energy*0.003f;
  noiseFloor=clampf(noiseFloor,0.0003f,0.05f);
  float targetGain=0.22f/fmaxf(energy,0.004f);
  autoGain=autoGain*0.99f+clampf(targetGain,0.7f,14.f)*0.01f;
  float level=clampf((energy-noiseFloor*0.6f)*autoGain*3.f);
  envelope+=(level-envelope)*(level>envelope?0.46f:0.09f);
  transient=fmaxf(0,level-previousEnergy);
  previousEnergy=previousEnergy*0.78f+level*0.22f;
  fft512();
  float raw[BANDS]={};
  for(int b=0;b<BANDS;b++) {
    // Log-ish spacing: useful bass discrimination and high-frequency extension.
    float f0=45.f*powf(8000.f/45.f,float(b)/BANDS);
    float f1=45.f*powf(8000.f/45.f,float(b+1)/BANDS);
    int k0=constrain(int(f0*FFT_N/SAMPLE_RATE),1,FFT_N/2-1);
    int k1=constrain(int(ceilf(f1*FFT_N/SAMPLE_RATE)),k0+1,FFT_N/2);
    float power=0;
    for(int k=k0;k<k1;k++) power+=sqrtf(re[k]*re[k]+im[k]*im[k]);
    raw[b]=clampf(log1pf(power*2.8f*autoGain)/2.8f);
    spectrum[b]+= (raw[b]-spectrum[b])*(raw[b]>spectrum[b]?0.56f:0.14f);
    peaks[b]=fmaxf(spectrum[b],peaks[b]*0.968f);
  }
  bass=clampf((spectrum[1]+spectrum[2]+spectrum[3]+spectrum[4])*0.40f);
  treble=clampf((spectrum[20]+spectrum[23]+spectrum[27]+spectrum[30])*0.46f);
  beatLevel=fmaxf(bass,level*0.76f);
  uint32_t now=millis();
  if (transient>0.075f && beatLevel>0.18f && now-beatWhen>185) beatWhen=now;
  lastSignal=now; haveAudio=true;
}
void sampleAudio() {
  if (!M5.Mic.isEnabled()) return;
  if (audioPending) {
    if (!M5.Mic.isRecording()) {
      audioPending=false;
      processAudio();
      audioReady=true;
    }
  } else {
    audioPending=M5.Mic.record(sampleBuf,FFT_N,SAMPLE_RATE);
  }
}
void putPixel(int x,int y,uint16_t c) {
  if(x>=8 && x<W-8 && y>=8 && y<H-8) canvas.drawPixel(x,y,c);
}
void neonDot(float x,float y,float size,uint16_t c) {
  int xi=cxi(x),yi=cyi(y),r=constrain(int(size),1,12);
  canvas.fillCircle(xi,yi,r+3,mixColor(0,c,0.17f));
  canvas.fillCircle(xi,yi,r+1,mixColor(0,c,0.48f));
  canvas.fillCircle(xi,yi,r,c);
}
void ring(float radius,uint16_t col,int thickness=1) {
  for(int t=0;t<thickness;t++)canvas.drawCircle(CX,CY,int(radius)+t,col);
}
void drawNeedleVU() {
  // McIntosh-inspired blue analog meter, physical needle with logarithmic ticks.
  uint16_t cyan=color565(9,162,245), gold=color565(250,189,86);
  canvas.fillRoundRect(CX-184,CY-136,368,274,26,color565(2,24,48));
  canvas.drawRoundRect(CX-184,CY-136,368,274,26,cyan);
  for(int i=0;i<8;i++) canvas.drawLine(CX-174,CY-128+i,CX+174,CY-128+i,
                                              color565(2,32+i*2,65+i*3));
  canvas.setTextColor(cyan);canvas.setTextDatum(middle_center);
  canvas.drawString("POWER OUTPUT",CX,CY-104,2);
  const int OY=CY+98; const float R=186;
  for(int i=0;i<=12;i++){
    float a=3.87f + i*2.55f/12;
    float x0=CX+cosf(a)*(R-24),y0=OY+sinf(a)*(R-24);
    float x1=CX+cosf(a)*(R-8), y1=OY+sinf(a)*(R-8);
    canvas.drawLine(int(x0),int(y0),int(x1),int(y1),i>9?gold:cyan);
    if (i%2==0){
      int lab=i-10;
      canvas.setTextColor(i>9?gold:color565(154,200,255));
      canvas.drawString(String(lab*2),int(CX+cosf(a)*(R-46)),int(OY+sinf(a)*(R-46)),1);
    }
  }
  static float needle=0;
  float db=20.f*log10f(fmaxf(envelope,0.004f));
  float v=clampf((db+40.f)/40.f);
  needle+= (v-needle)*(v>needle?0.35f:0.075f);
  float a=3.87f+needle*2.55f;
  int endX=int(CX+cosf(a)*(R-6)), endY=int(OY+sinf(a)*(R-6));
  canvas.drawLine(CX+1,OY+1,endX+1,endY+1,color565(3,15,25));
  canvas.drawLine(CX,OY,endX,endY,color565(246,251,255));
  canvas.fillCircle(CX,OY,9,color565(212,231,242));
  canvas.setTextColor(gold);canvas.drawString("dB",CX,OY-68,4);
  canvas.setTextColor(cyan);canvas.drawString("REFERENCE",CX,OY-38,2);
}
void drawPrismEQ() {
  for(int b=0;b<BANDS;b++) {
    float h=spectrum[b]*288;int x=CX-190+b*12, y=CY+134;
    for(int yy=0;yy<(int)h;yy+=9) {
      uint16_t c=hsv(float(b)/BANDS+float(yy)*0.0014f+phaseClock*0.06f,.95f,0.65f+yy/850.f);
      canvas.fillRoundRect(x,y-yy-7,9,7,2,c);
    }
    canvas.fillRect(x,y-int(peaks[b]*288)-6,9,3,color565(240,240,255));
  }
  ring(205,color565(33,17,67));
}
void drawPsyBloom(){
  float t=phaseClock;
  for(int layer=0;layer<8;layer++) {
    float rad=32+layer*18+envelope*35;
    for(int j=0;j<110;j++) {
      float a=PI2*j/110;
      float mod=1+0.21f*sinf(a*(5+layer%5)+t*(1.3f+layer*.15f));
      mod+=0.22f*bass*sinf(a*13-t*2);
      float r=rad*mod;
      if (j>0) {
        float a0=PI2*(j-1)/110,rr=rad*(1+.21f*sinf(a0*(5+layer%5)+t*(1.3f+layer*.15f))
                                   +.22f*bass*sinf(a0*13-t*2));
        canvas.drawLine(cxi(cosf(a0)*rr),cyi(sinf(a0)*rr),
                        cxi(cosf(a)*r),cyi(sinf(a)*r),hsv(.1f*layer+t*.04f+j*.001f));
      }
    }
  }
}
void drawVaporScope(){
  for(int y=-150;y<=150;y+=48) {
    for(int x=-176;x<176;x+=8) {
      float f=phaseClock*.8f+x*.022f+y*.013f;
      float y0=y+35*sinf(f)+scope[(x+176)*255/352]*240;
      float f2=phaseClock*.8f+(x+8)*.022f+y*.013f;
      float y1=y+35*sinf(f2)+scope[(x+184)*255/352]*240;
      canvas.drawLine(cxi(x),cyi(y0),cxi(x+8),cyi(y1),
                      hsv(y/350.f+phaseClock*.06f+x*.0006f));
    }
  }
  canvas.drawLine(34,CY,W-34,CY,color565(34,36,87));
}
void drawMatrix(){
  uint16_t green=color565(22,255,119);
  for(int y=-153;y<=153;y+=23) {
    int prevY=0;
    for(int x=-172;x<=172;x+=5){
      float sample=scope[(x+172)*255/344];
      int py=cyi(y+sinf(phaseClock*1.3f+y*.1f+x*.03f)*5+sample*(25+envelope*160));
      if(x>-172)canvas.drawLine(cxi(x-5),prevY,cxi(x),py,green);
      prevY=py;
    }
  }
  for(int i=0;i<28;i++){
    int x=(i*71+int(phaseClock*24))%380+44;
    int y=(i*101+int(phaseClock*91))%385+42;
    canvas.drawFastVLine(x,y,9,color565(8,70,40));
  }
}
void drawDancing(){
  for(int k=0;k<10;k++){
    int px=0,py=0;
    for(int i=0;i<92;i++){
      float xx=-175.f+i*350.f/91;
      float m=spectrum[(i*BANDS/92+k*3)%BANDS];
      float yy=(k-4.5f)*27+48*sinf(xx*.026f+phaseClock*(.7f+k*.065f)+k*.6f)
                +m*140.f*cosf(xx*.011f+k);
      int x=cxi(xx),y=cyi(yy);
      if(i)canvas.drawLine(px,py,x,y,hsv(k/10.f+phaseClock*.025f+m*.2f));
      px=x;py=y;
    }
  }
}
void drawTowers(){
  for(int b=0;b<BANDS;b++){
    float a=PI2*b/BANDS;
    float h=spectrum[b]*146;
    float r=54+h;
    uint16_t color=hsv(.62f+b/float(BANDS)*.54f+phaseClock*.04f);
    canvas.drawLine(cxi(cosf(a)*53),cyi(sinf(a)*53),
                    cxi(cosf(a)*r),cyi(sinf(a)*r),color);
    neonDot(cosf(a)*r,sinf(a)*r,2,color);
  }
  ring(51,color565(95,86,255),2);
}
void drawRadial(){
  for(int i=0;i<160;i++){
    int b=i*BANDS/160;
    float a=PI2*i/160+phaseClock*.14f;
    float r=85+175*spectrum[b];
    float inner=82+12*sinf(i*0.4f+phaseClock);
    canvas.drawLine(cxi(cosf(a)*inner),cyi(sinf(a)*inner),
                    cxi(cosf(a)*r),cyi(sinf(a)*r),
                    hsv(i/160.f+.35f+phaseClock*.05f));
  }
  ring(67,color565(78,135,245),2);
  canvas.setTextDatum(middle_center);canvas.setTextColor(TFT_WHITE);
  canvas.drawString(String("BASS ")+String(int(bass*100))+"%",CX,CY,2); // artistic level cue, not frequency estimate
}
void drawHeart(){
  float beat=clampf(1.f-(millis()-beatWhen)/480.f);
  float scale=7.0f*(1+0.16f*beat+0.1f*envelope);
  for(int glow=3;glow>=0;glow--){
    int px=0,py=0;
    for(int i=0;i<=230;i++){
      float t=PI2*i/230.f;
      float hx=16*powf(sinf(t),3);
      float hy=13*cosf(t)-5*cosf(2*t)-2*cosf(3*t)-cosf(4*t);
      float perturb=1+0.035f*sinf(phaseClock*1.1f+3*t)*envelope;
      int x=cxi(hx*(scale+glow*.30f)*perturb),
          y=cyi(-hy*(scale+glow*.30f)*perturb);
      if(i)canvas.drawLine(px,py,x,y,
           glow==0?hsv(.92f+.05f*beat,.88f,1.f):
           color565(glow==1?100:35,0,glow==1?93:35));
      px=x;py=y;
    }
  }
  for(int i=0;i<46;i++){
    float x=-155+i*310.f/45;
    float v=scope[i*255/46];
    float ekg=(i%23==12)?-(bass*42):((i%23==13)?bass*70:0);
    canvas.drawLine(cxi(x),cyi(133+v*35+ekg),
        cxi(x+310.f/45),cyi(133+scope[min(255,(i+1)*255/46)]*35),color565(245,43,145));
  }
}
void drawResonator(){
  for(int layer=0;layer<22;layer++){
    float rad=30+layer*7.6f+spectrum[layer%BANDS]*28;
    for(int p=0;p<100;p++){
      float a=PI2*p/100.f+layer*.10f;
      float bend=4*sinf(a*8-phaseClock*1.5f+layer*.32f)*(0.15f+bass);
      putPixel(cxi(cosf(a)*(rad+bend)),cyi(sinf(a)*(rad+bend)),
                  hsv(layer*.047f+phaseClock*.04f));
    }
  }
}
void drawKaleidoscope(){
  for(int petal=0;petal<12;petal++){
    float a=PI2*petal/12.f;
    for(int r=24;r<193;r+=7){
      float wobble=16*sinf(r*.06f-phaseClock*1.4f)+spectrum[(r/7)%BANDS]*32;
      float rr=r+wobble;
      float phi=a+.16f*sinf(r*.045f+phaseClock*.6f);
      uint16_t co=hsv(.8f*petal/12.f+r*.001f+phaseClock*.035f);
      neonDot(cosf(phi)*rr,sinf(phi)*rr,1,co);
      neonDot(cosf(phi+PI2/24)*rr,sinf(phi+PI2/24)*rr,1,co);
    }
  }
}
void drawFibonacci(){
  // Golden angle phyllotaxis, each seed acts as a music-driven luminous point.
  constexpr float goldenAngle=2.39996322972865f;
  for(int n=0;n<410;n++){
    float a=n*goldenAngle+phaseClock*.2f;
    float rad=9.1f*sqrtf(n)*(1+.15f*envelope);
    float freq=spectrum[(n*11)%BANDS];
    float spin=rad+freq*29;
    uint16_t co=hsv(fmodf(n*.0618f+phaseClock*.04f,1.f),.85f,.30f+.70f*freq);
    putPixel(cxi(cosf(a)*spin),cyi(sinf(a)*spin),co);
    if(n%9==0)neonDot(cosf(a)*spin,sinf(a)*spin,1+freq*2,co);
  }
}
void drawTunnel(){
  for(int layer=0;layer<23;layer++){
    float k=fmodf(layer*0.067f+phaseClock*.12f,1.f);
    float rad=12+220*k*k;
    uint16_t co=hsv(.68f+k*.40f+phaseClock*.03f,.9f,k);
    for(int i=0;i<10;i++){
      float a=PI2*i/10+phaseClock*.10f+layer*.11f;
      int x=cxi(cosf(a)*rad),y=cyi(sinf(a)*rad);
      int nx=cxi(cosf(a+PI2/10)*rad),ny=cyi(sinf(a+PI2/10)*rad);
      canvas.drawLine(x,y,nx,ny,co);
    }
  }
}
void drawCymatic(){
  for(int j=0;j<145;j++){
    float angle=PI2*j/145.f;
    float theta=angle+phaseClock*.08f;
    for(int r=40;r<208;r+=5){
      float node=sinf(5*angle+phaseClock*.7f)*cosf(r*.075f)
               +sinf(8*angle-phaseClock*.3f)*bass;
      if(fabsf(node)>.72f){
        putPixel(cxi(cosf(theta)*(r+node*10)),
                 cyi(sinf(theta)*(r+node*10)),hsv(.57f+node*.17f+phaseClock*.02f));
      }
    }
  }
}
void drawFractal(){
  // Recursive lightning forks (iterative breadth; branches follow spectral energy).
  struct Branch { float x,y,a,l; int depth; };
  Branch jobs[96];int head=0,tail=1;
  jobs[0]={0.f,120.f,-1.57f,64.f,0};
  while(head<tail && tail<94){
    Branch b=jobs[head++];
    float jitter=sinf(phaseClock*2+b.depth*1.7f+head)*.45f;
    float nx=b.x+cosf(b.a+jitter)*b.l;
    float ny=b.y+sinf(b.a+jitter)*b.l;
    canvas.drawLine(cxi(b.x),cyi(b.y),cxi(nx),cyi(ny),
                    hsv(.55f+b.depth*.08f+phaseClock*.05f));
    if(b.depth<6) {
      float energy=spectrum[(head*3)%BANDS];
      float length=b.l*(.72f+energy*.10f);
      jobs[tail++]={nx,ny,b.a+.47f+jitter*.2f,length,b.depth+1};
      if(tail<95) jobs[tail++]={nx,ny,b.a-.49f-jitter*.2f,length,b.depth+1};
    }
  }
}
void drawStarfield(){
  for(int i=0;i<160;i++){
    float seed=fmodf(i*.618033989f,1.f), ang=PI2*seed*17.7f;
    float progress=fmodf(i*.027f+phaseClock*(.13f+envelope*.32f),1.f);
    float r=5+208*progress*progress;
    float x=cosf(ang)*r,y=sinf(ang)*r;
    uint16_t co=hsv(i*.014f+phaseClock*.05f,.75f,.4f+progress*.6f);
    canvas.drawLine(cxi(x*.84f),cyi(y*.84f),cxi(x),cyi(y),co);
  }
}
void drawChrome(){
  for(int k=0;k<15;k++){
    float y=(k-7)*22.f;
    for(int x=-170;x<171;x+=6){
      float v=sin(phaseClock+float(x)*.026f+k*.54f);
      float height=y+v*(12+envelope*45)+spectrum[(x+170)*BANDS/341]*35;
      float v2=sinf(phaseClock+(x+6)*.026f+k*.54f);
      float nextY=y+v2*(12+envelope*45)+spectrum[(x+176)*BANDS/347]*35;
      canvas.drawLine(cxi(x),cyi(height),cxi(x+6),cyi(nextY),
          mixColor(color565(56,88,125),color565(228,239,255),fabsf(v)));
    }
  }
}
void drawOrbitals(){
  for(int o=0;o<13;o++){
    float tilt=o*.46f+phaseClock*.11f;
    float r=46+o*11;
    uint16_t co=hsv(o/13.f+phaseClock*.035f);
    for(int p=0;p<120;p++){
      float a=PI2*p/120;
      float x=cosf(a)*r, y=sinf(a)*r*(.31f+.12f*spectrum[o*2%BANDS]);
      float xx=x*cosf(tilt)-y*sinf(tilt);
      float yy=x*sinf(tilt)+y*cosf(tilt);
      if(p%2==0)putPixel(cxi(xx),cyi(yy),co);
    }
    float a=phaseClock*(0.3f+o*.03f)+o*1.1f;
    float px=cosf(a)*r,py=sinf(a)*r*.37f;
    neonDot(px*cosf(tilt)-py*sinf(tilt),px*sinf(tilt)+py*cosf(tilt),
               2+spectrum[o*2%BANDS]*5,co);
  }
}
void drawMoire(){
  for(int i=-180;i<180;i+=9){
    for(int j=-180;j<180;j+=9){
      float d1=hypotf(i+34.f*sinf(phaseClock),j);
      float d2=hypotf(i-34.f*cosf(phaseClock*.8f),j);
      float v=sinf(d1*.21f+phaseClock)+sinf(d2*.22f-phaseClock*.7f);
      if(v>.18f){
        float a=clampf(.5f+v*.25f+envelope*.18f);
        putPixel(cxi(i),cyi(j),hsv(.52f+v*.14f+phaseClock*.05f,.9f,a));
      }
    }
  }
}
void drawNebula(){
  for(int i=0;i<340;i++){
    float seed=i*2.399963f;
    float drift=phaseClock*(.05f+(i%13)*.005f);
    float r=190*sqrtf(fmodf(i*.61803f,1.f));
    float angle=seed+drift+envelope*.7f;
    float scatter=14*sinf(i*.7f+phaseClock*.6f)+spectrum[i%BANDS]*40;
    uint16_t co=hsv(.7f+i*.005f+phaseClock*.025f,.85f,.25f+.7f*spectrum[i%BANDS]);
    putPixel(cxi(cosf(angle)*(r+scatter)),cyi(sinf(angle)*(r+scatter)),co);
    if(i%29==0)neonDot(cosf(angle)*(r+scatter),sinf(angle)*(r+scatter),2,co);
  }
}
void drawVisual() {
  canvas.fillScreen(TFT_BLACK);
  phaseClock=millis()/1000.f;
  switch(visualIndex) {
    case 0:drawNeedleVU();break;       case 1:drawPrismEQ();break;
    case 2:drawPsyBloom();break;       case 3:drawVaporScope();break;
    case 4:drawMatrix();break;         case 5:drawDancing();break;
    case 6:drawTowers();break;         case 7:drawRadial();break;
    case 8:drawHeart();break;          case 9:drawResonator();break;
    case 10:drawKaleidoscope();break;  case 11:drawFibonacci();break;
    case 12:drawTunnel();break;        case 13:drawCymatic();break;
    case 14:drawFractal();break;       case 15:drawStarfield();break;
    case 16:drawChrome();break;        case 17:drawOrbitals();break;
    case 18:drawMoire();break;         default:drawNebula();break;
  }
  canvas.setTextDatum(middle_center);
  canvas.fillRoundRect(CX-155,H-64,310,43,13,color565(8,13,28));
  canvas.setTextColor(color565(204,228,255));
  canvas.drawString(visualNames[visualIndex],CX,H-43,2);
  canvas.drawCircle(CX,CY,225,color565(25,32,64));
  for(int i=0;i<20;i++) canvas.fillCircle(CX-95+i*10,CY-209,
     2,hsv(i/20.f,1.f,i==visualIndex?.95f:.12f));
}
void header(const char* title,const char* sub) {
  canvas.fillScreen(TFT_BLACK);
  ring(222,color565(23,40,65));
  canvas.setTextDatum(middle_center);
  canvas.setTextColor(color565(38,236,254));canvas.drawString(title,CX,51,4);
  canvas.setTextColor(color565(145,167,203));canvas.drawString(sub,CX,91,2);
}
void pill(int x,int y,int w,int h,const String& label,uint16_t col){
  canvas.fillRoundRect(x,y,w,h,15,mixColor(0,col,.16f));
  canvas.drawRoundRect(x,y,w,h,15,col);
  canvas.setTextDatum(middle_center);canvas.setTextColor(col);
  canvas.drawString(label,x+w/2,y+h/2,2);
}
void drawRemote(){
  if(page==0){
    header("Z / HARNESS","DIRECT EFFECT SELECTION");
    canvas.setTextDatum(middle_center);
    canvas.setTextColor(color565(244,245,255));
    canvas.drawString(String(zIndex+1)+" / "+String(Z_COUNT),CX,128,2);
    canvas.setTextColor(hsv(zIndex/float(Z_COUNT)+.5f));
    canvas.drawString(zNames[zIndex],CX,187,3);
    pill(CX-178,245,98,58,"< PREV",color565(80,159,250));
    pill(CX+80,245,98,58,"NEXT >",color565(80,159,250));
    pill(CX-79,237,158,78,"APPLY",color565(11,245,176));
    pill(CX-136,330,272,48,awaitingAck?"SENDING...":ackOK?"ACK OK":"SEND Z EFFECT",
                                     awaitingAck?color565(242,193,54):color565(159,113,244));
  } else if(page==1){
    header("PRESETS","EXISTING WLED SAVED PRESETS");
    canvas.setTextDatum(middle_center);canvas.setTextColor(TFT_WHITE);
    canvas.drawString(String(presetID),CX,194,7);
    pill(CX-167,238,102,61,"<",color565(79,149,255));
    pill(CX+65,238,102,61,">",color565(79,149,255));
    pill(CX-62,239,124,61,"RECALL",color565(19,241,171));
  } else if(page==2){
    header("TUNE","BRIGHTNESS / SPEED / INTENSITY");
    canvas.setTextDatum(middle_center);
    canvas.setTextColor(TFT_WHITE);
    canvas.drawString("BRI "+String(remoteBrightness),CX,140,2);
    canvas.drawString("SPD "+String(remoteSpeed),CX,230,2);
    canvas.drawString("INT "+String(remoteIntensity),CX,320,2);
    for(int i=0;i<3;i++){
      int y=165+i*90;
      uint8_t value=i==0?remoteBrightness:(i==1?remoteSpeed:remoteIntensity);
      canvas.fillRoundRect(CX-154,y,308,20,7,color565(19,26,47));
      canvas.fillRoundRect(CX-154,y,max(5,int(value*308/255)),20,7,
                          hsv(.53f+i*.20f));
    }
  } else {
    header("PAIR / RADIO","ENTER THIS MAC IN WLED REMOTE LIST");
    canvas.setTextDatum(middle_center);canvas.setTextColor(color565(245,247,255));
    canvas.drawString(WiFi.macAddress(),CX,167,3);
    canvas.setTextColor(color565(64,220,246));
    canvas.drawString("2.4 GHz CHANNEL "+String(channel),CX,228,3);
    canvas.setTextColor(color565(176,194,229));
    canvas.drawString("TAP CENTER: NEXT CHANNEL",CX,279,2);
    pill(CX-98,380,196,42,"POWER TOGGLE",color565(250,164,80));
    canvas.drawString("WLED CHANNEL MUST MATCH",CX,321,2);
    canvas.drawString("LAST ACK: "+String(ackOK?"YES":"NO"),CX,359,2);
  }
  pill(CX-105,H-71,210,48,"PAGE "+String(page+1)+"/4",color565(92,97,211));
  canvas.setTextDatum(middle_center);
  canvas.setTextColor(color565(130,139,169));
  canvas.drawString("B: VIZ   HOLD B: AUTO",CX,H-97,2);
  if(ackError)canvas.drawString("COMMAND REJECTED",CX,406,2);
  else if(awaitingAck&&millis()-sentWhen>850)canvas.drawString("NO ACK / CHECK PAIRING",CX,406,2);
}
void radioReceive(const esp_now_recv_info_t* info,const uint8_t* data,int len) {
  if(!info||!data||len!=13||memcmp(data,"ZHA1",4)!=0||data[4]!=1)return;
  Ack a;
  a.seq=uint32_t(data[9])|(uint32_t(data[10])<<8)|
        (uint32_t(data[11])<<16)|(uint32_t(data[12])<<24);
  a.status=data[6];a.brightness=data[7];a.preset=data[8];a.ready=true;
  portENTER_CRITICAL(&ackMux);incoming=a;portEXIT_CRITICAL(&ackMux);
}
bool startRadio(){
  esp_now_deinit(); // harmless if not yet initialised
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  esp_wifi_set_promiscuous(true);
  esp_err_t ch=esp_wifi_set_channel(channel,WIFI_SECOND_CHAN_NONE);
  esp_wifi_set_promiscuous(false);
  if(ch!=ESP_OK)return false;
  if(esp_now_init()!=ESP_OK)return false;
  if(esp_now_register_recv_cb(radioReceive)!=ESP_OK)return false;
  esp_now_peer_info_t peer={};
  memcpy(peer.peer_addr,BCAST_MAC,6);
  peer.channel=channel;
  peer.ifidx=WIFI_IF_STA;
  peer.encrypt=false;
  return esp_now_add_peer(&peer)==ESP_OK;
}
void radioSend(uint8_t command,uint8_t value=0) {
  if(!radioOK){ackOK=false;haptic(90,160);return;}
  uint32_t id=++seqCounter;
  uint8_t msg[12]={'Z','H','R','1',1,command,value,0,
                   uint8_t(id),uint8_t(id>>8),uint8_t(id>>16),uint8_t(id>>24)};
  awaitingAck=esp_now_send(BCAST_MAC,msg,12)==ESP_OK;
  ackOK=false;ackError=false;
  sentWhen=millis();lastCmd=command;
  if(!awaitingAck)haptic(90,120);
}
void readAck(){
  Ack a={};
  portENTER_CRITICAL(&ackMux);
  if(incoming.ready){a=incoming;incoming.ready=false;}
  portEXIT_CRITICAL(&ackMux);
  if(a.ready&&a.seq==seqCounter){
    awaitingAck=false;ackOK=(a.status==0);ackError=(a.status!=0);
    ackBrightness=a.brightness;ackPreset=a.preset;
    if(ackOK)haptic(145,35);else haptic(100,135);
  }
  if(awaitingAck&&millis()-sentWhen>1700)awaitingAck=false;
}
void remoteTouch(int x,int y){
  if(y>H-90){page=(page+1)%4;haptic();return;}
  if(page==0) {
    if(y>225&&y<330) {
      if(x<CX-65)zIndex=(zIndex+Z_COUNT-1)%Z_COUNT;
      else if(x>CX+65)zIndex=(zIndex+1)%Z_COUNT;
      else radioSend(9,zIndex);haptic();
    } else if(y>330&&y<390)radioSend(9,zIndex);
  } else if(page==1) {
    if(x<CX-60)presetID=presetID<=1?250:presetID-1;
    else if(x>CX+60)presetID=presetID>=250?1:presetID+1;
    else radioSend(1,presetID);
    haptic();
  } else if(page==2) {
    int band=(y>=115&&y<206)?0:((y>=206&&y<296)?1:((y>=296&&y<383)?2:-1));
    if(band>=0) {
      uint8_t val=constrain((x-(CX-154))*255/308,0,255);
      if(band==0){remoteBrightness=val;radioSend(2,val);}
      if(band==1){remoteSpeed=val;radioSend(10,val);}
      if(band==2){remoteIntensity=val;radioSend(11,val);}
      haptic(110,32);
    }
  } else if(page==3 && y>374&&y<433) {
    radioSend(3,0);haptic(170,75);
  } else if(page==3 && y>190&&y<310) {
    channel=channel>=11?1:channel+1;
    nv.putUChar("channel",channel);radioOK=startRadio();haptic(180,85);
  }
}
void checkPose(){
  static float smoothZ=0;
  float x=0,y=0,z=0;
  if(!M5.Imu.isEnabled())return;
  if(!M5.Imu.getAccel(&x,&y,&z))return;
  smoothZ=smoothZ*.80f+z*.20f;
  uint32_t now=millis();
  bool flat=smoothZ>.86f&&fabsf(x)<.38f&&fabsf(y)<.38f;
  bool upright=smoothZ<.66f;
  if(flat){if(!flatSince)flatSince=now;}else flatSince=0;
  if(upright){if(!uprightSince)uprightSince=now;}else uprightSince=0;
  if(!overrideMode){
    if(flatSince&&now-flatSince>1250&&!palmMode){
      palmMode=true;page=0;haptic(200,85);
    }
    if(uprightSince&&now-uprightSince>1050&&palmMode){
      palmMode=false;haptic(130,50);
    }
  }
}
void handleControls(){
  if(M5.BtnB.wasHold()){
    overrideMode=false;
    haptic(230,110);
  } else if(M5.BtnB.wasClicked()) {
    overrideMode=true;palmMode=!palmMode;
    if(palmMode)page=0;
    haptic(210,70);
  }
  if(M5.BtnA.wasClicked()){
    if(palmMode) {
      zIndex=(zIndex+1)%Z_COUNT;
      haptic(135,35);
    } else {
      visualIndex=(visualIndex+1)%VISUAL_COUNT;
      nv.putUChar("visual",visualIndex);
      haptic(160,42);
    }
  }
  if(M5.BtnA.wasHold()&&palmMode) {
    radioSend(9,zIndex);
    haptic(210,75);
  }
  auto touch=M5.Touch.getDetail(0);
  if(touch.wasClicked()){
    if(palmMode)remoteTouch(touch.x,touch.y);
    else {
      if(touch.x<CX)visualIndex=(visualIndex+VISUAL_COUNT-1)%VISUAL_COUNT;
      else visualIndex=(visualIndex+1)%VISUAL_COUNT;
      nv.putUChar("visual",visualIndex);haptic(120,32);
    }
  }
}
void setup(){
  Serial.begin(115200);
  auto config=M5.config();
  config.internal_mic=true;
  config.internal_spk=false; // microphone and speaker share the audio codec
  M5.begin(config);
  M5.Speaker.end();
  M5.Mic.begin();
  M5.Display.setRotation(0);
  M5.Display.setBrightness(154);
  W=M5.Display.width();H=M5.Display.height();CX=W/2;CY=H/2;
  canvas.setColorDepth(16);
  haveSprite=canvas.createSprite(W,H)!=nullptr;
  if(!haveSprite) {
    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.setTextColor(TFT_RED);
    M5.Display.drawString("DISPLAY MEMORY ERROR",30,CY);
    while(true)delay(1000);
  }
  nv.begin("clubviz",false);
  visualIndex=nv.getUChar("visual",0)%VISUAL_COUNT;
  channel=constrain(nv.getUChar("channel",1),1,11);
  radioOK=startRadio();
  M5.Power.setVibration(0);
  haptic(200,70);
  Serial.printf("Club Visualizer v1; display %dx%d, STA %s, channel %u, radio %d\n",
     W,H,WiFi.macAddress().c_str(),channel,radioOK);
}
void loop(){
  M5.update();
  sampleAudio();
  checkPose();
  handleControls();
  readAck();
  tickVibration();
  uint32_t now=millis();
  if(now-frameWhen<34){delay(1);return;}
  frameWhen=now;
  if(palmMode)drawRemote(); else drawVisual();
  canvas.pushSprite(0,0);
}