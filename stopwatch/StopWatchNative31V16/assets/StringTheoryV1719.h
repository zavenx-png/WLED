/*
 * String Theory — V17.19 high-fidelity kinetic textiles for M5Stack StopWatch C152.
 * ORIGINAL implementation; one integrated effect with six scenes (not six new menu IDs).
 * Uses the existing native renderer / 16 kHz microphone FFT and beat engine.
 * Sensors: BMI270 acceleration + gyro, CST820B touch, microphone analysis,
 * power/RTC through unchanged host firmware. No speaker conflicts, network use,
 * flash writes per frame, or vibration while switching effects.
 * Included inside NativeApp.cpp after common renderer functions are defined.
 */
#pragma once

namespace StringTheory {
static constexpr int STRANDS=24, KNOTS=27, MOTES=42, MODES=6, PALETTES=6;
static const char* const modeNames[MODES]={"PRISM LOOM","DREAMCATCHER","SILK VORTEX","CELESTIAL KNOT","MOIRE FABRIC","AURORA WEAVE"};
static const char* const paletteNames[PALETTES]={"ULTRAVIOLET","GOLD & INK","HOLOGRAPHIC","EMERALD","ROSE QUARTZ","GLACIER"};
struct Fiber { float x[KNOTS],y[KNOTS],vx[KNOTS],vy[KNOTS],phase; };
struct Ember {float u,v,rate,phase;};
static Fiber f[STRANDS];
static Ember em[MOTES];
static bool active=false,panel=false,pressed=false,audioOn=true,autoplay=true,cold=true;
static uint8_t mode=0,palette=0,detail=2;
static uint32_t lastPhysics=0,lastImu=0,hudUntil=0,lastShake=0,lastPluck=0;
static uint32_t lastTapAt=0;
static int lastTapX=0,lastTapY=0;
static float gX=0,gY=0,spin=0,shake=0,smoothedEnergy=0,travel=0;
static float touchX=233,touchY=233,priorX=233,priorY=233,dragDX=0,dragDY=0;
static float gravityAx=0,gravityAy=0,gravityAz=0,lastAx=0,lastAy=0,lastAz=0;
static bool hasGravity=false;
static float rippleX=233,rippleY=233,rippleAge=8,ripplePower=0;
static constexpr uint32_t DOUBLE_TAP_MIN_MS=55, DOUBLE_TAP_MAX_MS=380;
static constexpr int DOUBLE_TAP_RADIUS=72;
static void dismissUI(){panel=false;pressed=false;lastTapAt=0;dragDX=dragDY=0;}

static inline float stHash(int n) {return frac(sinf(n*127.1f+78.233f)*43758.5453f);}
static inline float stS(float a){return sinf(a);}
static inline float stC(float a){return cosf(a);}
static inline float stL(float a,float b,float t){return a+(b-a)*t;}
static void settingsLoad(){
 if(active)return;
 active=true;mode=clampi(prefs.getUChar("st18m",0),0,MODES-1);
 palette=clampi(prefs.getUChar("st18p",0),0,PALETTES-1);
 detail=clampi(prefs.getUChar("st18d",2),0,2);
 audioOn=prefs.getBool("st18a",true);autoplay=prefs.getBool("st18v",true);
 uint32_t now=millis();lastPhysics=now;lastImu=now;hudUntil=now+5000;
 hasGravity=false;lastTapAt=0;dragDX=dragDY=0;
 for(int s=0;s<STRANDS;s++){
  f[s].phase=TAU*stHash(s+87);
  for(int j=0;j<KNOTS;j++)f[s].x[j]=CX,f[s].y[j]=CY,f[s].vx[j]=f[s].vy[j]=0;
 }
 for(int i=0;i<MOTES;i++)em[i]={stHash(i+11),stHash(i+113),.11f+.38f*stHash(i+41),TAU*stHash(i+831)};
}
static void setMode(int offset){settingsLoad();mode=(mode+MODES+offset%MODES)%MODES;prefs.putUChar("st18m",mode);
 for(int s=0;s<STRANDS;s++)for(int j=0;j<KNOTS;j++)f[s].vx[j]*=.2f,f[s].vy[j]*=.2f;
 hudUntil=millis()+3000;
}
static void setPalette(int offset){settingsLoad();palette=(palette+PALETTES+offset%PALETTES)%PALETTES;
 prefs.putUChar("st18p",palette);hudUntil=millis()+3000;}
static uint16_t threadColor(float k,float shade=1.f){
 k=frac(k);float r=0,g=0,b=0;
 // Palette is a continuous dual-/tri-chromatic gradient, no pre-rendered bitmap.
 switch(palette){
 case 0:r=110+140*k;g=25+130*sinf(k*PI_F);b=200+55*(1-k);break;
 case 1:r=220+34*k;g=126+102*k;b=32+88*k;break;
 case 2:r=77+158*k;g=225-125*k;b=239-21*k;break;
 case 3:r=24+78*k;g=147+104*k;b=92+144*k;break;
 case 4:r=250-48*k;g=68+96*k;b=140+89*k;break;
 default:r=66+115*k;g=168+73*k;b=230+22*k;break;
 }
 return rgb(r*shade,g*shade,b*shade);
}
static void geometry(int s,float u,float t,float &x,float &y){
 float k=s/(float)STRANDS,a=TAU*k,rx=R-19;
 float spinBase=spin*.15f;
 float a1=a+spinBase, a2=0, r=0;
 switch(mode){
 case 0:{ // Opposing tensioned arcs: a lattice of crossing violin strings.
  a1=a+.17f*stS(t*.26f);a2=a+PI_F+(.27f+.10f*stS(t*.21f+s)) *stS(s*2.399f)+spinBase;
  float x0=CX+rx*stC(a1),y0=CY+rx*stS(a1);
  float x1=CX+rx*stC(a2),y1=CY+rx*stS(a2);
  float normal=(16+21*stS(s*.8f+t*.37f))*stS(u*PI_F);
  x=stL(x0,x1,u)+normal*stS(a1+PI_F*.5f);y=stL(y0,y1,u)+normal*stC(a1+PI_F*.5f);
  break;
 }
 case 1:{ // Circular dreamcatcher / hand-tied radial web.
  a1=a+t*.085f+spinBase;a2=a1+PI_F*(.67f+.05f*stS(s*.67f));
  float x0=CX+rx*stC(a1),y0=CY+rx*stS(a1);
  float x1=CX+rx*stC(a2),y1=CY+rx*stS(a2);
  x=stL(x0,x1,u)+15*stS(PI_F*u*2+s*.52f+t*.3f)*stS(PI_F*u);
  y=stL(y0,y1,u)+15*stC(PI_F*u*2+s*.52f+t*.3f)*stS(PI_F*u);
  break;
 }
 case 2:{ // Logarithmic spiral bundles forming a braided nautilus vortex.
  r=14+u*(rx-7);float phi=a+4.4f*u+t*.12f+spinBase;
  x=CX+r*stC(phi);y=CY+r*stS(phi);break;
 }
 case 3:{ // Knot/torus with negative-space central mandala.
  float phi=a+TAU*u*1.35f+t*.14f+spinBase;
  r=112+72*stS(u*PI_F*2+s*.41f)+22*stS(phi*3.0f+t*.29f);
  x=CX+r*stC(phi);y=CY+r*stS(phi);break;
 }
 case 4:{ // Polar-projected woven bands, a moire mechanical textile.
  float vertical=(k-.5f)*2.f;
  float h=vertical*178.f+18*stS(t*.18f+s*.4f);
  float full=sqrtf(fmaxf(1,rx*rx-h*h));
  x=CX+(2*u-1)*full;
  y=CY+h+(8+15*stS(s*.45f+t*.41f))*stS(PI_F*u)+5*stS(u*TAU*3+t+s*.8f);
  break;
 }
 default:{ // Aurora silk ribbons sweep between two polar anchors.
  a1=-PI_F*.85f+k*PI_F*1.7f+.16f*stS(t*.15f+s*.4f);
  a2=PI_F*.85f-k*PI_F*1.7f+.17f*stS(t*.12f+s*.4f);
  float x0=CX+rx*stC(a1),y0=CY+rx*stS(a1);
  float x1=CX+rx*stC(a2),y1=CY+rx*stS(a2);
  x=stL(x0,x1,u)+(22+35*stS(s*.37f+t*.29f))*stS(PI_F*u*2);
  y=stL(y0,y1,u)+23*stS(PI_F*u*2+s*.26f+t*.22f)*stS(PI_F*u);
  break;
 }
 }
 // Gently shear and drape the middle of each filament toward measured gravity.
 // The original 0.5px displacement was imperceptible on a 468px display.
 const float envelope=stS(PI_F*u);
 float ox=x-CX,oy=y-CY;
 x=CX+ox*(1-.018f*fabsf(gY))+envelope*(gX*28.f-gY*5.f);
 y=CY+oy*(1-.018f*fabsf(gX))+envelope*(gY*28.f+gX*5.f);
}
static void impact(float x,float y,float strength){
 rippleX=x;rippleY=y;rippleAge=0;ripplePower=clampf(strength,.15f,1.9f);
 lastPluck=millis();hudUntil=lastPluck+2000;
 for(int s=0;s<STRANDS;s++)for(int j=1;j<KNOTS-1;j++){
  float dx=f[s].x[j]-x,dy=f[s].y[j]-y;
  float rr=dx*dx+dy*dy;
  if(rr>175.f*175.f)continue;
  float inv=1.f/sqrtf(rr+100);
  // Wider, stronger impulse makes plucks visible even on a densely woven scene.
  float pulse=510.f*strength*expf(-rr/10700.f);
  f[s].vx[j]+=dx*inv*pulse;
  f[s].vy[j]+=dy*inv*pulse;
 }
}
static void touchStart(int x,int y){
 settingsLoad();
 if(panel){pressed=false;return;}
 pressed=true;touchX=priorX=x;touchY=priorY=y;dragDX=dragDY=0;
 // Immediate visual response, including very fast taps that land between frames.
 impact(x,y,.40f);
}
static void touchMove(int x,int y){
 if(!pressed || panel)return;
 float dx=clampf((float)x-touchX,-48.f,48.f),dy=clampf((float)y-touchY,-48.f,48.f);
 dragDX=clampf(dragDX+dx,-85.f,85.f);
 dragDY=clampf(dragDY+dy,-85.f,85.f);
 priorX=touchX;priorY=touchY;touchX=x;touchY=y;
}
static bool panelTap(int x,int y){
 // Match the visible button hit rectangles exactly, with generous touch margins.
 if(y<154){dismissUI();hudUntil=millis()+1800;return true;}
 if(y>=160&&y<268){if(x<CX)setMode(1);else setPalette(1);return true;}
 if(y>=272&&y<383){
  if(x<CX){audioOn=!audioOn;prefs.putBool("st18a",audioOn);}
  else{detail=(detail+1)%3;prefs.putUChar("st18d",detail);}
  return true;
 }
 if(y>=388){autoplay=!autoplay;prefs.putBool("st18v",autoplay);return true;}
 return true;
}
static bool touchEnd(int x,int y,uint32_t held,int dx,int dy){
 pressed=false;dragDX=dragDY=0;
 if(panel){return panelTap(x,y);}
 uint32_t now=millis();
 // Vertical swipe only changes the weave. Horizontal movement is ALWAYS art
 // interaction: the host must never use it to select another effect.
 if(abs(dy)>78&&abs(dy)>abs(dx)*1.2f){
  lastTapAt=0;setMode(dy>0?1:-1);return true;
 }
 bool isTap=held<=330&&abs(dx)<=37&&abs(dy)<=37;
 if(isTap){
  if(lastTapAt && (uint32_t)(now-lastTapAt)>=DOUBLE_TAP_MIN_MS &&
       (uint32_t)(now-lastTapAt)<=DOUBLE_TAP_MAX_MS &&
       sq((float)x-lastTapX)+sq((float)y-lastTapY)<=DOUBLE_TAP_RADIUS*DOUBLE_TAP_RADIUS){
    panel=true;lastTapAt=0;hudUntil=now+12000;return true;
  }
  lastTapAt=now;lastTapX=x;lastTapY=y;
  // Press already plucked immediately; a second light impulse improves tactility.
  impact(x,y,.68f);
  return true;
 }
 lastTapAt=0;
 // A long hold RELEASE does NOT open the settings menu anymore.
 float dist=sqrtf((float)dx*dx+(float)dy*dy);
 impact(x,y,clampf(.72f+dist*.006f,.72f,1.65f));
 return true;
}
static void sampleImu(uint32_t now){
 if((uint32_t)(now-lastImu)<28)return;
 float dt=clampf((now-lastImu)*.001f,.020f,.065f);
 lastImu=now;
 if(!M5.Imu.isEnabled())return;
 // M5Stack BMI270 requires an explicit IMU.update() prior to reading samples.
 // M5.update() alone updates the touch and button state, not the IMU FIFO.
 auto changed=M5.Imu.update();
 if(!changed)return;
 const auto& data=M5.Imu.getImuData();
 const float ax=data.accel.x,ay=data.accel.y,az=data.accel.z;
 const float gx=data.gyro.x,gy=data.gyro.y,gz=data.gyro.z;
 if(!isfinite(ax)||!isfinite(ay)||!isfinite(az))return;
 gX=stL(gX,clampf(ax,-1.4f,1.4f),.24f);
 gY=stL(gY,clampf(ay,-1.4f,1.4f),.24f);
 if(!hasGravity){
  gravityAx=lastAx=ax;gravityAy=lastAy=ay;gravityAz=lastAz=az;hasGravity=true;
 }
 const float lx=ax-gravityAx,ly=ay-gravityAy,lz=az-gravityAz;
 const float linear=sqrtf(lx*lx+ly*ly+lz*lz);
 const float jerk=sqrtf(sq(ax-lastAx)+sq(ay-lastAy)+sq(az-lastAz));
 const float accelDeviation=fabsf(sqrtf(ax*ax+ay*ay+az*az)-1.f);
 const float motion=fmaxf(fmaxf(linear,jerk*.85f),accelDeviation);
 // Lower shake threshold, detect fast changes even if |acceleration| stays
 // near 1g (a back-and-forth shake can otherwise cancel out).
 shake=stL(shake,motion,.35f);
 if(motion>.40f && (uint32_t)(now-lastShake)>550){
  lastShake=now;
  impact(CX,CY,1.60f);
  // Launch a radially distributed shock through the entire woven network.
  for(int s=0;s<STRANDS;s++)for(int j=1;j<KNOTS-1;j++){
   const float dx=f[s].x[j]-CX,dy=f[s].y[j]-CY;
   const float mag=sqrtf(dx*dx+dy*dy+90.f);
   const float kickV=80.f+95.f*stS((float)j/KNOTS*PI_F);
   f[s].vx[j]+=dx/mag*kickV;
   f[s].vy[j]+=dy/mag*kickV;
  }
 }
 // Adaptive low-pass gravity: slow orientation follows; transient shaking
 // survives in the high-pass component above.
 gravityAx=stL(gravityAx,ax,.07f);
 gravityAy=stL(gravityAy,ay,.07f);
 gravityAz=stL(gravityAz,az,.07f);
 lastAx=ax;lastAy=ay;lastAz=az;
 if(isfinite(gx)&&isfinite(gy)&&isfinite(gz)){
  // BMI270 gyroscope is degrees/s; integrate a bounded visible orbital twist.
  spin=clampf((spin+clampf(gz,-330.f,330.f)*dt*.012f)*.997f,-2.5f,2.5f);
  gX=clampf(gX+clampf(gy,-220.f,220.f)*dt*.004f,-1.7f,1.7f);
  gY=clampf(gY+clampf(gx,-220.f,220.f)*dt*.004f,-1.7f,1.7f);
 }
}
static void physics(uint32_t now){
 settingsLoad();sampleImu(now);
 float dt=clampf((now-lastPhysics)*.001f,.008f,.050f);
 lastPhysics=now;
 travel+=dt;
 float t=travel;
 if(cold){
  for(int s=0;s<STRANDS;s++)for(int j=0;j<KNOTS;j++){
   float x,y;geometry(s,j/(float)(KNOTS-1),t,x,y);
   f[s].x[j]=x;f[s].y[j]=y;f[s].vx[j]=f[s].vy[j]=0;
  }
  cold=false;
 }
 float sound=audioOn?clampf(.60f*energy+.65f*bass+.28f*highs):0;
 smoothedEnergy=stL(smoothedEnergy,sound,.12f);
 float bassPulse=audioOn?(beatPulse()*.45f+kick*.55f):0;
 rippleAge+=dt*1.0f;
 const float impulse=clampf((smoothedEnergy+bassPulse)*.9f,0,1.3f);
 for(int s=0;s<STRANDS;s++){
  Fiber &q=f[s];
  for(int j=0;j<KNOTS;j++){
   const float u=j/(float)(KNOTS-1);
   float bx,by;geometry(s,u,t,bx,by);
   if(j==0||j==KNOTS-1){q.x[j]=bx;q.y[j]=by;q.vx[j]=q.vy[j]=0;continue;}
   // Spring-to-rest + neighbor Laplacian + viscous drag.
   const float tether=clampf(1.8f+3.0f*(1.f-fabsf(.5f-u)*1.7f),1.5f,5.8f);
   float fx=(bx-q.x[j])*tether;
   float fy=(by-q.y[j])*tether;
   fx+=(q.x[j-1]+q.x[j+1]-2*q.x[j])*1.25f;
   fy+=(q.y[j-1]+q.y[j+1]-2*q.y[j])*1.25f;
   // Natural animated motion persists in silence or with disabled microphone.
   float ph=q.phase+u*TAU*2.5f+t*(autoplay?.9f:.1f);
   float env=stS(PI_F*u);
   fx+=env*stS(ph+q.phase)*29*(.40f+impulse);
   fy+=env*stC(ph*.76f+q.phase)*29*(.40f+impulse);
   // Stronger measured-gravity drape, still critically damped by the solver.
   fx+=env*gX*118.f;fy+=env*gY*118.f;
   if(pressed&&!panel){
    float dx=q.x[j]-touchX,dy=q.y[j]-touchY,d2=dx*dx+dy*dy;
    if(d2<170.f*170.f){
      const float influence=expf(-d2/11500.f);
      // Direct proximity attraction and accumulated finger motion, rather
      // than a 1-2 px force that was almost invisible in V17.18.
      fx+=(touchX-q.x[j])*44.f*influence+dragDX*115.f*influence;
      fy+=(touchY-q.y[j])*44.f*influence+dragDY*115.f*influence;
    }
   }
   if(rippleAge<1.8f){
    float dx=q.x[j]-rippleX,dy=q.y[j]-rippleY;
    float wave=sqrtf(dx*dx+dy*dy),front=rippleAge*230.f;
    float envelope=expf(-sq((wave-front)*.025f))*ripplePower*(1-rippleAge/2.0f);
    fx+=envelope*dx*.65f;fy+=envelope*dy*.65f;
   }
   q.vx[j]=(q.vx[j]+fx*dt)*.955f;
   q.vy[j]=(q.vy[j]+fy*dt)*.955f;
   q.x[j]+=q.vx[j]*dt*8.0f;
   q.y[j]+=q.vy[j]*dt*8.0f;
   q.x[j]=clampf(q.x[j],8,W-8);q.y[j]=clampf(q.y[j],8,H-8);
  }
 }
 // Finger velocity is consumed once per rendered frame, not silently lost
 // during the many M5.update() polls between graphics frames.
 dragDX*=.12f;dragDY*=.12f;
}
static void backdrop(float t){
 black();
 // Sparse high quality celestial dust + sparse concentric gravure, all original vectors.
 uint16_t dim=threadColor(.24f,.11f);
 for(int r=58;r<=204;r+=48)circ(CX,CY,r,dim);
 for(int i=0;i<72;i++){
  float a=TAU*stHash(i+117),rr=18+207*sqrtf(stHash(i+511));
  float x=CX+rr*stC(a+t*.007f*(i%2?1:-1));
  float y=CY+rr*stS(a+t*.007f*(i%2?1:-1));
  float shimmer=.2f+.24f*stS(t*1.2f+i*4.2f);
  if(shimmer>0)dot(x,y,(i%13==0?1.4f:.7f),tint(threadColor(i/72.f),shimmer));
 }
}
static void stroke(float x,float y,float xx,float yy,uint16_t co,int thick){
 // Two-pass glow is cheap enough for 466px, avoid extremely expensive full-screen blur.
 line(x,y,xx,yy,tint(co,.095f),thick>=2?4:3);
 line(x,y,xx,yy,co,thick);
}
static void strandsRender(float t){
 float beat=audioOn?beatPulse():0;
 // Cross-links are drawn first as an under/over knit. They make a visible net,
 // rather than a random collection of unrelated lines.
 if(detail>0&&(mode==1||mode==3||mode==4||mode==5)){
  for(int s=0;s<STRANDS;s++){
   int s2=(s+1)%STRANDS;
   for(int j=3;j<KNOTS-2;j+=detail==2?4:6){
    if((s+j)%3==0)continue;
    uint16_t co=tint(threadColor((s+.3f*j)/STRANDS),.14f+.07f*beat);
    line(f[s].x[j],f[s].y[j],f[s2].x[j],f[s2].y[j],co);
   }
  }
 }
 for(int s=0;s<STRANDS;s++){
  const Fiber &q=f[s];
  const float depth=.50f+.5f*stS(s*.69f+t*.18f);
  float intensity=.67f+.23f*depth+.13f*smoothedEnergy;
  for(int j=1;j<KNOTS;j++){
   float u=j/(float)(KNOTS-1);
   float hueShift=frac(s/(float)STRANDS+.2f*u+.04f*stS(t*.2f+s));
   uint16_t co=threadColor(hueShift,intensity);
   stroke(q.x[j-1],q.y[j-1],q.x[j],q.y[j],co,detail==2&&((j+s)%3==0)?2:1);
   if(detail==2&&(j+s)%7==0){
    // Specular highlight ticks suggest twisted filaments and light movement.
    line(q.x[j-1],q.y[j-1],q.x[j],q.y[j],tint(rgb(238,247,255),.18f+.14f*beat));
   }
  }
  // Path-following pearl droplets/braid markers move through the whole composition.
  for(int p=0;p<2;p++){
   float v=frac(t*(.09f+.015f*(s%3))+.17f*p+s*.067f);
   float fi=v*(KNOTS-1),ff=fi-floorf(fi);int j=clampi((int)fi,0,KNOTS-2);
   float x=stL(q.x[j],q.x[j+1],ff),y=stL(q.y[j],q.y[j+1],ff);
   float bright=.48f+.5f*stS(PI_F*v)*(.60f+.35f*smoothedEnergy);
   dot(x,y,detail==0?1.4f:3.8f,tint(threadColor(frac(s/(float)STRANDS+.28f)),bright*.16f));
   dot(x,y,1.2f,tint(rgb(241,247,255),bright));
  }
  // Anchoring pins are intentionally fine: no ring or button over the artwork.
  if(s%2==0){dot(q.x[0],q.y[0],1.9f,threadColor(s/(float)STRANDS,.83f));
    dot(q.x[KNOTS-1],q.y[KNOTS-1],1.9f,threadColor(s/(float)STRANDS,.83f));}
 }
}
static void renderRipple(){
 if(rippleAge>1.4f)return;
 float rad=rippleAge*230.f;
 if(rad<5)return;
 float fade=.3f*(1-rippleAge/1.4f)*ripplePower;
 for(int k=0;k<3;k++)circ(rippleX,rippleY,clampi((int)(rad+k*5),1,350),tint(threadColor(.3f+k*.19f),fade/(k+1)));
}
static void fittedText(const String& s,int x,int y,uint16_t color,float wanted,int maxWidth){
 cv.setFont(&lgfx::fonts::FreeSansBold12pt7b);
 cv.setTextSize(wanted);
 const int measured=cv.textWidth(s);
 if(measured>maxWidth && measured>0)cv.setTextSize(wanted*((float)maxWidth/measured));
 cv.setTextDatum(middle_center);cv.setTextColor(color);cv.drawString(s,x,y);
}
static void renderPanel(){
 // Enlarged typographic hierarchy and actual tap zones match panelTap().
 cv.fillRoundRect(39,67,388,358,28,rgb(7,14,29));
 cv.drawRoundRect(39,67,388,358,28,threadColor(.25f,.36f));
 cv.setFont(&lgfx::fonts::FreeSansBold18pt7b);cv.setTextSize(1.38f);
 int tw=cv.textWidth("STRING THEORY");
 if(tw>364)cv.setTextSize(1.38f*364.f/tw);
 cv.setTextDatum(middle_center);cv.setTextColor(rgb(237,242,255));
 cv.drawString("STRING THEORY",CX,112);
 fittedText("TAP HEADER TO CLOSE",CX,148,rgb(130,162,195),.82f,315);
 auto button=[&](int x,int y,int w,int h,const String &label,const String &value){
  cv.fillRoundRect(x,y,w,h,13,rgb(19,32,51));
  cv.drawRoundRect(x,y,w,h,13,threadColor(.35f,.48f));
  fittedText(label,x+w/2,y+28,rgb(165,187,218),1.05f,w-14);
  fittedText(value,x+w/2,y+72,rgb(246,247,253),1.28f,w-13);
 };
 button(62,162,165,103,"WEAVE",modeNames[mode]);
 button(238,162,165,103,"COLOR",paletteNames[palette]);
 button(62,277,165,103,"MIC / BEAT",audioOn?"ON":"OFF");
 button(238,277,165,103,"DETAIL",detail==2?"ULTRA":detail==1?"MID":"ECO");
 fittedText(String("AUTO MOTION: ")+(autoplay?"ON":"OFF")+"   [TAP]",CX,409,threadColor(.35f,.94f),.94f,335);
}
static void renderHud(uint32_t now){
 if(panel){renderPanel();return;}
 if((int32_t)(hudUntil-now)<=0)return;
 uint16_t accent=threadColor(.38f,.90f);
 fittedText("STRING THEORY",CX,46,accent,.84f,336);
 // Readable, larger mode name at the bottom of the round display.
 fittedText(modeNames[mode],CX,419,rgb(220,230,244),1.18f,368);
 fittedText("DOUBLE TAP: CONTROLS",CX,446,rgb(126,151,176),.74f,340);
}
static void render(){
 uint32_t now=millis();physics(now);
 float t=travel;
 backdrop(t);
 strandsRender(t);
 renderRipple();
 renderHud(now);
}
} // namespace StringTheory