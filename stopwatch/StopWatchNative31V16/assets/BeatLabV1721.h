#pragma once
/* BEAT LABORATORY — V17.21 native M5StopWatch renderer.
 * Effect #31 (original ID 67). Eight independent authored environments.
 * Uses native sprite, existing 32-band FFT, ESP32 BMI270, CST820B touchscreen.
 * A/B remain global effect navigation. Double tap opens settings, no haptics.
 * Every world has an animated clock when quiet. No network/speaker required.
 */
namespace BeatLab {
static const char* const WORLDS[]={"ORBIT DECK","SPECTRAL BLOOM","SONIC CATHEDRAL","PRISM REACTOR",
 "CYMATIC LENS","LIQUID SCOPE","DRUM CONSTELLATION","DJ CHRONOGRAPH"};
static const char* const PALETTES[]={"ULTRAVIOLET","CYBER ICE","CHAMPAGNE","SOLAR FLARE","ACID CLUB","MONOCHROME"};
static constexpr int WORLDS_N=8,PALS_N=6;
static bool loaded=false,panel=false,pressed=false,tiltOn=true,ambient=true;
static uint8_t world=0,palette=0,detail=2;
static uint32_t lastTap=0,hudUntil=0,imuLast=0,lastShock=0;
static int tapX=0,tapY=0,pressX=0,pressY=0;
static float gyro=0,moveX=0,moveY=0,ox=0,oy=0,flash=0,shock=0;
static float sparkle=0,ring=0,ringX=233,ringY=233,rx=0,ry=0;
static float lastAx=0,lastAy=0,lastAz=1;
static const float BEAT_LAB_TAU=6.28318530718f;
static inline float clamp(float x,float a=0,float b=1){return BeatLabDSP::lim(x,a,b);}
static inline float s(float x){return sinf(x);}static inline float c(float x){return cosf(x);}
static inline float hsh(int n){float x=s((float)n*127.1f+78.233f)*43758.5453f;return x-floorf(x);}
static inline float clockt(){return BeatLabDSP::engine.visualClock;}
static inline float phase(){return BeatLabDSP::engine.phase;}
static inline float hit(){return clamp(BeatLabDSP::engine.pulse*.55f+BeatLabDSP::engine.onsetFlash*.75f+flash*.3f,0,1.4f);}
static inline float band(int k){return BeatLabDSP::engine.smoothBands[clampi(k,0,31)];}
static inline float level(){return BeatLabDSP::engine.level;}
static void ensure(){
 if(loaded)return;
 loaded=true;
 world=clampi(prefs.getUChar("bl21w",0),0,7);
 palette=clampi(prefs.getUChar("bl21p",0),0,5);
 detail=clampi(prefs.getUChar("bl21q",2),0,2);
 tiltOn=prefs.getBool("bl21t",true);ambient=prefs.getBool("bl21a",true);
 auto& e=BeatLabDSP::engine;
 e.tempo=clamp(prefs.getFloat("bl21b",124.f),58.f,204.f);
 e.manual=prefs.getBool("bl21m",false);if(e.manual)e.confidence=1.f;
 e.meter=clampi(prefs.getUChar("bl21bar",4),3,8);
 hudUntil=millis()+4300;
}
static void dismissUI(){panel=false;pressed=false;lastTap=0;}
static void changeWorld(int dir){world=(world+WORLDS_N+dir%WORLDS_N)%WORLDS_N;
 prefs.putUChar("bl21w",world);hudUntil=millis()+3100;flash=.7f;}
static void changePalette(int dir){palette=(palette+PALS_N+dir%PALS_N)%PALS_N;
 prefs.putUChar("bl21p",palette);hudUntil=millis()+3100;flash=.7f;}
static uint16_t color(float k,float v=1.f){
 k=k-floorf(k);v=clamp(v,0,1.8f);
 float r,g,b;
 switch(palette){
 case 0:r=96+157*k;g=34+166*s(k*3.14159f);b=220+35*(1-k);break;
 case 1:r=40+125*k;g=176+76*k;b=253-44*k;break;
 case 2:r=212+42*k;g=119+127*k;b=42+155*k;break;
 case 3:r=253-22*k;g=53+165*k;b=32+74*k;break;
 case 4:r=72+148*k;g=245-30*k;b=32+168*k;break;
 default:r=g=b=142+105*k;break;
 }
 return rgb(r*v,g*v,b*v);
}
static uint16_t white(float v=1.f){return rgb(255*clamp(v,0,1),250*clamp(v,0,1),255*clamp(v,0,1));}
static void stroke(float x,float y,float xx,float yy,uint16_t col,int w=1){
 cv.drawLine((int)x,(int)y,(int)xx,(int)yy,col);
 if(w>=2)cv.drawLine((int)x+1,(int)y,(int)xx+1,(int)yy,col);
 if(w>=3)cv.drawLine((int)x,(int)y+1,(int)xx,(int)yy+1,col);
}
static void arc(int x,int y,float r,float start,float width,uint16_t col,int steps=35,int thick=1){
 if(r<0||width<=0)return;
 steps=clampi(steps,3,96);float px=x+r*c(start),py=y+r*s(start);
 for(int j=1;j<=steps;j++){float a=start+width*j/steps;
   float nx=x+r*c(a),ny=y+r*s(a);stroke(px,py,nx,ny,col,thick);px=nx;py=ny;}
}
static void spot(float x,float y,float r,float shade,float hue){
 if(r<.7f)return;
 cv.fillCircle((int)x,(int)y,(int)(r+4),color(hue,shade*.08f));
 cv.fillCircle((int)x,(int)y,(int)(r+2),color(hue,shade*.19f));
 cv.fillCircle((int)x,(int)y,(int)r,color(hue,shade*.77f));
 cv.fillCircle((int)x-1,(int)y-1,1,white(shade*.9f));
}
static void text(const String &value,int x,int y,float scale,int width,uint16_t co){
 cv.setFont(&lgfx::fonts::FreeSansBold12pt7b);cv.setTextSize(scale);
 int w=cv.textWidth(value);if(w>width&&w>0)cv.setTextSize(scale*width/w);
 cv.setTextDatum(middle_center);cv.setTextColor(co);cv.drawString(value,x,y);cv.setTextSize(1.f);
}
static void background(int x,int y,float t){
 cv.fillScreen(TFT_BLACK);
 // Optical depth from progressively dimmer concentric offset rings, no images.
 for(int r=61;r<224;r+=27)cv.drawCircle(x,y,r,color(.20f+.008f*r,.015f+(r%5)*.004f));
 for(int i=0;i<44;i++){
  float a=BEAT_LAB_TAU*hsh(i*3+4)+t*.009f*(i%2?1.f:-1.f),rad=43.f+170.f*sqrtf(hsh(i*17+12));
  int px=x+(int)(rad*c(a)),py=y+(int)(rad*s(a));
  if((unsigned)px<(unsigned)W&&(unsigned)py<(unsigned)H)cv.drawPixel(px,py,color(hsh(i+21),.18f+(i%7==0?.36f:0.f)));
 }
}
static void orbitDeck(float t,float b,int x,int y){
 // Triple-lens orbit deck: a carefully balanced radial stage, clear BPM center.
 for(int k=0;k<4;k++){
  float r=57+k*36.f+b*(k<2?8.f:16.f);
  arc(x,y,r,t*(k%2?.14f:-.17f)+k*.67f,1.7f+k*.23f,color(.06f+k*.17f,.12f),42,2);
  arc(x,y,r,t*(k%2?-.09f:.12f)+2.3f+k*.33f,1.0f,color(.28f+k*.11f,.38f),30,2);
 }
 for(int k=0;k<64;k++){
  float a=(BEAT_LAB_TAU*k/64)-1.5707963f;
  float amp=band(k/2);
  float r0=176,r1=180+clamp(amp*.95f+b*.25f,0,1)*41;
  stroke(x+r0*c(a),y+r0*s(a),x+r1*c(a),y+r1*s(a),color(k/64.f,.20f+amp*.72f),1);
 }
 for(int k=0;k<4;k++){
  float a=-1.5707963f+k*BEAT_LAB_TAU/4;
  arc(x,y,152,a+.04f,BEAT_LAB_TAU/4-.09f,color(k*.22f,.16f+(k==((int)floorf(t*BeatLabDSP::engine.tempo/60.f))%4)?.4f:0.f),25,2);
 }
 float a=-1.5707963f+BEAT_LAB_TAU*phase();
 arc(x,y,152,a-.23f,.24f,color(.16f,1.f),20,3);
 spot(x+152*c(a),y+152*s(a),5+2*b,1.f,.08f);
}
static void spectralBloom(float t,float b,int x,int y){
 // Evolving phyllotactic neon chrysanthemum: bass opens the petals, highs glitter.
 const int layers=detail==0?2:4;
 for(int layer=0;layer<layers;layer++)for(int k=0;k<80;k++){
  float a=BEAT_LAB_TAU*k/80+t*(layer%2?.09f:-.075f)+layer*.33f;
  float m=band((k+layer*5)%32),len=24.f+layer*24.f+45.f*clamp(m*2.f+b*.6f);
  float inn=24+layer*35+6*s(k*.3f+t*.18f);
  float taper=1.f+.10f*s(a*7+t*.27f);
  float r=inn+len*taper;
  float f=1.f+.15f*s(k*1.72f+t*.61f);
  float x0=x+(inn-6)*c(a-.055f*f),y0=y+(inn-6)*s(a-.055f*f);
  float x1=x+r*c(a),y1=y+r*s(a);
  float x2=x+(inn-6)*c(a+.055f*f),y2=y+(inn-6)*s(a+.055f*f);
  stroke(x0,y0,x1,y1,color(k/80.f+layer*.08f,.08f+layer*.055f+m*.28f));
  stroke(x1,y1,x2,y2,color(.2f+k/100.f,.18f+layer*.052f+m*.35f));
  if((k+layer)%8==0)spot(x1,y1,1.7f+m*2.f,.35f+m*.4f,k/80.f);
 }
 for(int j=0;j<6;j++)arc(x,y,32+j*28+b*5,t*.06f+j*.58f,.8f,color(j*.18f,.18f),24);
}
static void sonicCathedral(float t,float b,int x,int y){
 // 3D concert architecture - receding synth pylons / perspective resonance.
 for(int depth=7;depth>=0;depth--){
  float z=(depth+1)/8.f,r=26+z*186+14*s(t*.19f+depth);
  int yy=(int)(y+40*s(depth*.7f+t*.13f));
  arc(x,yy,r,3.14f,3.14f,color(depth*.12f,.09f+(7-depth)*.019f),38,2);
  for(int side=-1;side<=1;side+=2){
   float sx=x+side*(26+z*178);
   stroke(sx,y+135*z,sx-side*30,y-138*z,color(depth*.13f,.10f+.08f*b));
  }
 }
 for(int i=0;i<32;i++){
  float q=(i-15.5f)/16.f;
  float d=1.f-fabsf(q),a=band(i);
  float px=x+q*206;
  float base=y+140.f-15.f*d;
  float h=27+clamp(a*1.75f+b*.3f)*174.f*d;
  stroke(px,base,px,base-h,color(.06f+i/45.f,.22f+a*.54f),2);
  stroke(px+3,base,px+3,base-h*.92f,color(.46f+i/65.f,.09f+a*.2f));
  if(i%4==0)spot(px,base-h,2.7f,.4f+a*.5f,i/32.f);
 }
}
static void prismReactor(float t,float b,int x,int y){
 for(int ring=0;ring<6;ring++){
  float r=36+ring*29+b*5*(ring%2?1:0),rot=t*(ring%2?-.24f:.21f);
  int n=12+ring*4;
  for(int j=0;j<n;j++){
   float a=BEAT_LAB_TAU*j/n+rot+ring*.34f;
   float v=band((j*3+ring*4)%32);
   arc(x,y,r,a,.028f+BEAT_LAB_TAU/n*(.25f+.50f*clamp(v*2.f)),color(j/(float)n+ring*.08f,.15f+.40f*v+.12f*b),5,2);
   if(j%7==0)spot(x+r*c(a),y+r*s(a),2.0f+.6f*b,.3f,ring*.2f);
  }
 }
 for(int j=0;j<8;j++){
  float a=t*.17f+j*BEAT_LAB_TAU/8;
  float r=42+6*s(t*.4f+j);
  spot(x+r*c(a),y+r*s(a),2.4f,0.8f,j/8.f);
 }
}
static void cymaticLens(float t,float b,int x,int y){
 // Interfering polar harmonics (5-mode Lissajous-like Chladni field).
 int spokes=detail==0?112:184;
 for(int k=0;k<5;k++){
  float px=0,py=0;
  for(int i=0;i<=spokes;i++){
   float a=BEAT_LAB_TAU*i/spokes;
   float harmonic=s((k+3)*a+t*.22f)+.48f*s((k+7)*a-t*.19f);
   float r=39+30*k+harmonic*(8+19*b+5*BeatLabDSP::engine.mid);
   float xx=x+r*c(a+t*.015f*(k%2?1:-1)),yy=y+r*s(a+t*.015f*(k%2?1:-1));
   if(i>0)stroke(px,py,xx,yy,color(k*.165f+i*.001f,.13f+k*.06f+b*.15f),k==2?2:1);
   px=xx;py=yy;
  }
 }
 for(int j=0;j<72;j++){
  float a=BEAT_LAB_TAU*j/72+t*.042f;
  float m=band(j%32),r=170+28*m;
  if(j%3==0)spot(x+r*c(a),y+r*s(a),1.3f+.8f*m,.23f+m*.42f,j/72.f);
 }
}
static void liquidScope(float t,float b,int x,int y){
 // Bright glass VU ribbons: 4 independent audio wave bodies and glow ribbons.
 float p[160]={};
 for(int i=0;i<160;i++)p[i]=scopeY[i];
 int n=detail==0?100:160;
 for(int k=0;k<5;k++){
  int px=0,py=0;
  for(int i=0;i<n;i++){
   float u=i/(float)(n-1),sample=p[(int)(u*159)];
   float x1=x-200+400*u;
   float env=(1.f-fabsf(2*u-1));env=clamp(env,0,1);
   float sinwave=24*s(u*12.57f+t*(.48f+.17f*k)+k*.8f);
   float y1=y+(k-2)*37 + (sinwave*.28f+sample*210+13*s(u*33+t+k*.3f))*(.4f+env*.6f)+b*10*s(u*6.28f+k);
   if(i>0)stroke(px,py,x1,y1,color(k*.14f+u*.13f,.15f+k*.055f+b*.13f),k==2?2:1);
   px=x1;py=y1;
  }
 }
 for(int i=0;i<33;i++){
  float u=i/32.f,m=band(i%32);
  float xx=x-197+394*u;
  spot(xx,y+160-m*56,1.1f+m*2.f,.3f+m*.4f,u);
 }
}
static void drumConstellation(float t,float b,int x,int y){
 // 16-beat step sequencer as an orbital constellation. The visual grid is
 // genuinely metrical, but only observed onsets are illuminated as played.
 const int n=16;
 for(int i=0;i<n;i++){
  float a=BEAT_LAB_TAU*i/n-1.5707963f+t*.025f;
  float r=146.f+9*s(i*.7f+t*.2f);
  float nx=x+r*c(a),ny=y+r*s(a);
  float v=band(i*2);
  if(i>0){float ap=BEAT_LAB_TAU*(i-1)/n-1.5707963f+t*.025f;
   float rp=146+9*s((i-1)*.7f+t*.2f);
   stroke(x+rp*c(ap),y+rp*s(ap),nx,ny,color(i/16.f,.18f));}
  spot(nx,ny,4.4f+5*clamp(v*1.8f)+2*b,.45f+v*.35f,i/16.f);
  for(int j=1;j<=2;j++){
   int k=(i+j*5)%16;
   float b1=BEAT_LAB_TAU*k/16-1.5707963f+t*.025f;
   float rk=146+9*s(k*.7f+t*.2f);
   stroke(nx,ny,x+rk*c(b1),y+rk*s(b1),color(.18f+i/32.f,.045f+.045f*v));
  }
 }
 for(int j=0;j<56;j++){
  float u=(j+.5f)/56.f,ang=BEAT_LAB_TAU*(u*3.0f+t*.043f);
  float rr=19+u*100;
  float strength=band((int)(u*31));
  spot(x+rr*c(ang),y+rr*s(ang),.8f+strength*2.f,.12f+strength*.7f,j/56.f);
 }
}
static void djChronograph(float t,float b,int x,int y){
 // Ultra-legible timecode dial and metrical tick divisions for the shirt.
 for(int i=0;i<64;i++){
  float a=BEAT_LAB_TAU*i/64-1.5707963f;
  float r0=i%16==0?172:(i%4==0?182:191);
  float r1=203+band(i/2)*15;
  stroke(x+r0*c(a),y+r0*s(a),x+r1*c(a),y+r1*s(a),color(i/64.f,i%16==0?.66f:.18f),i%16==0?2:1);
 }
 for(int i=0;i<4;i++){
  float a=-1.5707963f+i*BEAT_LAB_TAU/4;
  arc(x,y,151,a+.07f,BEAT_LAB_TAU/4-.14f,color(i*.21f,.16f+(i==((int)(t*BeatLabDSP::engine.tempo/60.f))%4?.35f:0.f)),31,3);
 }
 arc(x,y,151,-1.5707963f,phase()*BEAT_LAB_TAU,color(.08f,.85f),clampi((int)(phase()*160),4,86),3);
 float a=-1.5707963f+phase()*BEAT_LAB_TAU;
 spot(x+151*c(a),y+151*s(a),5+b*3,1,.16f);
 // VU arc: 32 bars as a smooth luminous meter.
 for(int k=0;k<32;k++){
  float a=-2.3f+k*4.6f/31;
  float r0=101,r1=101+clamp(band(k)*1.5f+b*.17f)*31;
  stroke(x+r0*c(a),y+r0*s(a),x+r1*c(a),y+r1*s(a),color(.14f+k/40.f,.30f),2);
 }
}
static void imu(uint32_t now){
 if(now-imuLast<24)return;
 imuLast=now;
 if(!tiltOn||!M5.Imu.isEnabled()||!M5.Imu.update())return;
 const auto& im=M5.Imu.getImuData();float ax=im.accel.x,ay=im.accel.y,az=im.accel.z,gz=im.gyro.z;
 if(!std::isfinite(ax)||!std::isfinite(ay)||!std::isfinite(az)||!std::isfinite(gz))return;
 float jerk=sqrtf((ax-lastAx)*(ax-lastAx)+(ay-lastAy)*(ay-lastAy)+(az-lastAz)*(az-lastAz));
 moveX=moveX*.9f+clamp(ax,-1,1)*1.2f;moveY=moveY*.9f+clamp(ay,-1,1)*1.2f;
 gyro=gyro*.9f+clamp(gz,-300,300)*.001f;
 if(jerk>.8f&&now-lastShock>600){shock=1.f;flash=1.f;lastShock=now;ring=1.f;ringX=CX;ringY=CY;}
 lastAx=ax;lastAy=ay;lastAz=az;
}
static void tick(uint32_t now){
 ensure();BeatLabDSP::engine.update(now);imu(now);
 flash*=.92f;shock*=.93f;ring*=.94f;
 if(!ambient&&now-BeatLabDSP::engine.lastAudio>3000)BeatLabDSP::engine.pulse*=.90f;
}
static void onAudio(uint32_t now){
 if(!loaded)ensure();
 BeatLabDSP::engine.consume(bins,bass,mids,highs,energy,now);
}
static void touchStart(int x,int y){ensure();pressed=true;pressX=x;pressY=y;rx=x;ry=y;}
static void touchMove(int x,int y){if(!pressed||panel)return;float d=fabsf(x-rx)+fabsf(y-ry);if(d>2.f){flash=fminf(.7f,flash+.02f);gyro+=(x-rx)*.0016f;rx=x;ry=y;}}
static void toggleAuto(){auto& e=BeatLabDSP::engine;if(e.manual)e.autoTempo();else e.setTempo(e.tempo,true);
 prefs.putBool("bl21m",e.manual);prefs.putFloat("bl21b",e.tempo);}
static void adjustTempo(float mult){auto& e=BeatLabDSP::engine;e.setTempo(e.tempo*mult,true);prefs.putBool("bl21m",true);prefs.putFloat("bl21b",e.tempo);}
static bool menuTap(int x,int y){
 if(y<138){panel=false;hudUntil=millis()+1400;return true;}
 if(y<205){changeWorld(x<CX?-1:1);return true;}
 if(y<276){changePalette(x<CX?-1:1);return true;}
 if(y<342){if(x<CX)toggleAuto();else {detail=(detail+1)%3;prefs.putUChar("bl21q",detail);}return true;}
 if(x<CX){BeatLabDSP::engine.meter=BeatLabDSP::engine.meter==4?3:BeatLabDSP::engine.meter==3?6:BeatLabDSP::engine.meter==6?8:4;
  prefs.putUChar("bl21bar",BeatLabDSP::engine.meter);}
 else {tiltOn=!tiltOn;prefs.putBool("bl21t",tiltOn);}return true;
}
static bool touchEnd(int x,int y,uint32_t held,int dx,int dy){
 pressed=false;ensure();
 if(panel)return menuTap(x,y);
 uint32_t now=millis();
 if(abs(dy)>75&&abs(dy)>abs(dx)*1.15f){lastTap=0;changeWorld(dy>0?1:-1);return true;}
 if(abs(dx)>75&&abs(dx)>abs(dy)*1.15f){lastTap=0;changePalette(dx>0?1:-1);return true;}
 if(held>680&&abs(dx)<42&&abs(dy)<42){
  if(x<CX-65)adjustTempo(.5f);       // half-time interpretation
  else if(x>CX+65)adjustTempo(2.f); // double-time interpretation
  else BeatLabDSP::engine.anchorMs=(float)now; // downbeat phase reset
  flash=.75f;lastTap=0;hudUntil=now+1300;return true;
 }
 if(held<360&&abs(dx)<42&&abs(dy)<42){
  if(lastTap && now-lastTap>=50 && now-lastTap<=390 && (x-tapX)*(x-tapX)+(y-tapY)*(y-tapY)<72*72){
   panel=true;lastTap=0;return true;
  }
  lastTap=now;tapX=x;tapY=y;
  if(y>318&&y<398&&x>CX-105&&x<CX+105){
   float prev=BeatLabDSP::engine.tempo;
   BeatLabDSP::engine.tap(now);
   if(BeatLabDSP::engine.tapsCount==3){prefs.putBool("bl21m",BeatLabDSP::engine.manual);prefs.putFloat("bl21b",BeatLabDSP::engine.tempo);}
   else if(BeatLabDSP::engine.tapsCount>3&&fabsf(prev-BeatLabDSP::engine.tempo)>1.0f)prefs.putFloat("bl21b",BeatLabDSP::engine.tempo);
  }
  ring=1.f;ringX=x;ringY=y;flash=.48f;
 }
 return true;
}
static void menuBox(int xx,int yy,int ww,int hh,const String& val,bool active=false){
 cv.fillRoundRect(xx,yy,ww,hh,11,active?rgb(30,71,86):rgb(15,24,41));
 cv.drawRoundRect(xx,yy,ww,hh,11,active?color(.25f,.76f):color(.27f,.35f));
 text(val,xx+ww/2,yy+hh/2,1.0f,ww-18,white(.94f));
}
static void menu(){
 cv.fillScreen(TFT_BLACK);
 for(int r=119;r<=222;r+=34)cv.drawCircle(CX,CY,r,color(.18f,.13f));
 text("BEAT LAB",CX,83,1.9f,390,white(1.f));
 text("TAP HEADER TO CLOSE",CX,124,.7f,315,color(.24f,.68f));
 menuBox(66,158,160,51,String("< ")+WORLDS[world],true);menuBox(241,158,160,51,"NEXT  >",true);
 menuBox(66,228,160,51,String("< ")+PALETTES[palette]);menuBox(241,228,160,51,"PALETTE >");
 menuBox(66,298,160,51,BeatLabDSP::engine.manual?"TAP SYNC":"AUTO SYNC",true);
 menuBox(241,298,160,51,detail==0?"ECO":detail==1?"BALANCED":"ULTRA");
 menuBox(66,367,160,51,String(BeatLabDSP::engine.meter)+" / BAR");
 menuBox(241,367,160,51,tiltOn?"TILT ON":"TILT OFF");
}
static void hud(int x,int y){
 auto& e=BeatLabDSP::engine;
 // Center is deliberately clean and high-contrast from several feet away.
 if(world==2||world==5)cv.fillCircle(x,y,61,rgb(2,4,12));
 if(world!=1&&world!=4)cv.fillCircle(x,y,56,rgb(1,4,12));
 cv.drawCircle(x,y,55,color(.18f,.19f));
 char num[20];snprintf(num,sizeof(num),"%d",(int)roundf(e.tempo));
 text(num,x,y-5,2.7f,124,white(1.f));
 text("BPM",x,y+39,.64f,96,color(.27f,.84f));
 if(world==7){
  text(e.manual?"TAP LOCK":"AUTO TRACK",x,y-64,.6f,150,color(.34f,.58f));
 }
 if(millis()<hudUntil){
  text("BEAT LABORATORY",CX,59,.87f,290,color(.22f,.7f));
 }
 // Explicit status: never label the fallback beat as a verified measurement.
 String status=e.manual?"TAP LOCK":(e.confidence>.38f&&millis()-e.lastAudio<2200?"BEAT LOCK":(e.signal?"ANALYZING":"AMBIENT"));
 text(status,CX,336,.80f,235,color(.24f,.71f));
 // Tap region is y~350-390. BPM numeric value above isn't a touch button.
 text("TAP  /  SYNC",CX,369,.82f,210,color(.40f,.70f));
 text(WORLDS[world],CX,410,1.12f,350,white(.94f));
 // Confidence ring only when there is actual evidence.
 const float conf=e.manual?1.f:e.confidence;
 arc(CX,CY,218,-1.570796f,clamp(conf)*BEAT_LAB_TAU,color(.06f,.44f),clampi((int)(conf*132),3,80),2);
}
static void render(){
 ensure();if(panel){menu();return;}
 auto& e=BeatLabDSP::engine;
 float t=clockt(),b=clamp(e.lowPulse*.38f+e.pulse*.38f+e.onsetFlash*.43f+shock*.42f),mx=clamp(moveX,-14,14),my=clamp(moveY,-14,14);
 int x=CX+(int)mx,y=CY+(int)my;
 background(x,y,t);
 switch(world){
  case 0:orbitDeck(t,b,x,y);break;
  case 1:spectralBloom(t,b,x,y);break;
  case 2:sonicCathedral(t,b,x,y);break;
  case 3:prismReactor(t,b,x,y);break;
  case 4:cymaticLens(t,b,x,y);break;
  case 5:liquidScope(t,b,x,y);break;
  case 6:drumConstellation(t,b,x,y);break;
  default:djChronograph(t,b,x,y);break;
 }
 if(ring>.012f){float r=(1.f-ring)*142;
   arc((int)ringX,(int)ringY,r,0,BEAT_LAB_TAU,color(.12f,ring*.36f),72,2);
 }
 // Accent ambient motion independent of detected sound. Restrict particle budget.
 const int motes=detail==0?18:detail==1?28:40;
 for(int i=0;i<motes;i++){
  float a=hsh(i*91)*BEAT_LAB_TAU+t*(.025f+.005f*(i%5))*(i%2?1:-1),r=39+171*hsh(i*78+11);
  float px=x+r*c(a),py=y+r*s(a);
  if((px-CX)*(px-CX)+(py-CY)*(py-CY)<220*220 && ((i*7+(int)(t*5))%7)<4)
    spot(px,py,.8f+1.2f*hsh(i+9),.13f+.24f*hsh(i+8),hsh(i+2));
 }
 hud(x,y);
}
} // namespace BeatLab