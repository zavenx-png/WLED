#pragma once
/* Gravity Lab / FluidBox Classic integrated renderer for Club StopWatch.
 * The self-contained FluidBox is an adaptation of the fluid simulation ideas in
 * omiya-bonsai/STOPWATCH-FluidBox (MIT); details in THIRD_PARTY_NOTICES_GRAVITY.md.
 * Effect 29 (ID 65) is classic black-screen 400-particle FluidBox-inspired mode;
 * effect 30 (ID 66) adds eight worlds, audio, and touch. B button remains NEXT.
 */
#include "GravityLabPhysics.h"
#include <new>
namespace GravityLab {
static const char* const WORLDS[]={"LIQUID MERCURY","NEBULA INK","ORBITAL ATLAS","PLASMA VORTEX","BLACK HOLE","FERROFLUID","PRISM STORM","QUANTUM SAND"};
static const char* const PAL[] = {"HOLOGRAPHIC","GOLDEN","ULTRAVIOLET","GLACIER","EMBER","EMERALD"};
static constexpr int WORLDS_N=8,PALS_N=6;
static GravityPhysics::Fluid* sol=nullptr;
static bool initialized=false,wasClassic=false,panel=false,pressed=false,audioOn=true,ambient=true,flip=false,lockWell=false;
static uint8_t world=0,palette=0,quality=2;
static uint32_t stamp=0,imuStamp=0,lastTap=0,lastShake=0,hudUntil=0,seed=113;
static int tapX=0,tapY=0;static float x=0,y=0,ox=0,oy=0,gX=0,gY=0,gZ=0;
static float bx=0,by=0,bz=1,prevAx=0,prevAy=0,prevAz=1,motion=0;
static float rippleX=0,rippleY=0,rippleR=0,rippleV=0,flash=0,sec=0,audioGlow=0;
static bool baseReady=false;
static float lockX=0,lockY=0;
static bool ready(bool classic){
 if(!sol){
   void* mem=heap_caps_malloc(sizeof(GravityPhysics::Fluid),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
   if(!mem){Serial.println("Gravity Lab: insufficient PSRAM");return false;}
   sol=new(mem) GravityPhysics::Fluid();sol->reset(seed);
 }
 if(!initialized){
  initialized=true;world=clampi(prefs.getUChar("grav20w",0),0,WORLDS_N-1);
  palette=clampi(prefs.getUChar("grav20p",0),0,PALS_N-1);
  quality=clampi(prefs.getUChar("grav20q",2),0,2);
  audioOn=prefs.getBool("grav20a",true);ambient=prefs.getBool("grav20m",true);
  flip=prefs.getBool("grav20f",false);
  stamp=imuStamp=millis();hudUntil=millis()+3800;
 }
 if(wasClassic!=classic){wasClassic=classic;panel=false;lastTap=0;pressed=false;lockWell=false;
   sol->scene=classic?0:world+1;sol->reset(++seed);hudUntil=millis()+4500;}
 return true;
}
static void dismissUI(){panel=false;pressed=false;lastTap=0;lockWell=false;}
static inline float clip(float v,float a,float b){return GravityPhysics::cl(v,a,b);}
static void setWorld(int delta){world=(world+WORLDS_N+delta%WORLDS_N)%WORLDS_N;
 prefs.putUChar("grav20w",world);if(sol){sol->scene=world+1;sol->reset(++seed);}hudUntil=millis()+3600;flash=.7f;}
static void setPalette(){palette=(palette+1)%PALS_N;prefs.putUChar("grav20p",palette);hudUntil=millis()+3000;}
static void reset(){if(sol)sol->reset(++seed);flash=.9f;hudUntil=millis()+1800;}
static uint16_t pal(float k,float v){
 k=k-floorf(k);v=clip(v,0,1.28f);
 float r,g,b;
 switch(palette){
  case 0:r=60+166*k;g=219-114*k;b=245-5*k;break;
  case 1:r=226+23*k;g=118+120*k;b=29+114*k;break;
  case 2:r=109+130*k;g=49+101*k;b=203+46*k;break;
  case 3:r=51+139*k;g=175+65*k;b=230+25*k;break;
  case 4:r=237+17*k;g=59+166*k;b=42+113*k;break;
  default:r=26+155*k;g=162+93*k;b=79+124*k;break;
 }
 return rgb(r*v,g*v,b*v);
}
static void label(const String& val,int xx,int yy,float scale,int maxWidth,uint16_t c){
 cv.setFont(&lgfx::fonts::FreeSansBold12pt7b);cv.setTextSize(scale);
 int width=cv.textWidth(val);if(width>maxWidth&&width>0)cv.setTextSize(scale*(float)maxWidth/width);
 cv.setTextDatum(middle_center);cv.setTextColor(c);cv.drawString(val,xx,yy);cv.setTextSize(1.f);
}
static void sensor(uint32_t now){
 if((uint32_t)(now-imuStamp)<12)return;
 imuStamp=now;
 if(!M5.Imu.isEnabled()||!M5.Imu.update())return;
 const auto& d=M5.Imu.getImuData();float ax=d.accel.x,ay=d.accel.y,az=d.accel.z,gz=d.gyro.z;
 if(!std::isfinite(ax)||!std::isfinite(ay)||!std::isfinite(az)||!std::isfinite(gz))return;
 if(!baseReady){bx=prevAx=ax;by=prevAy=ay;bz=prevAz=az;baseReady=true;}
 gX=clip(gX*.79f+(flip?-ax:ax)*.21f,-1.5f,1.5f);
 gY=clip(gY*.79f+(flip?-ay:ay)*.21f,-1.5f,1.5f);
 gZ=clip(gZ*.67f+gz*.33f,-470,470);
 float dx=ax-prevAx,dy=ay-prevAy,dz=az-prevAz;
 float deviation=sqrtf((ax-bx)*(ax-bx)+(ay-by)*(ay-by)+(az-bz)*(az-bz));
 float jerk=sqrtf(dx*dx+dy*dy+dz*dz);
 float shock=fmaxf(fmaxf(deviation,jerk*.87f),fabsf(sqrtf(ax*ax+ay*ay+az*az)-1.f));
 motion=clip(.75f*motion+.25f*shock,0,3);
 if(shock>.43f && (uint32_t)(now-lastShake)>520 && sol){
   lastShake=now;sol->burst(clip(.90f+shock*.80f,1.f,2.6f));
   rippleX=0;rippleY=0;rippleR=4;rippleV=1.25f;flash=1.1f;
 }
 bx=bx*.945f+ax*.055f;by=by*.945f+ay*.055f;bz=bz*.945f+az*.055f;
 prevAx=ax;prevAy=ay;prevAz=az;
}
static void tick(uint32_t now,bool classic){
 if(!ready(classic))return;
 sensor(now);
 if((uint32_t)(now-stamp)<(profile==2?38U:25U))return;
 float dt=clip((now-stamp)*.001f,.025f,.043f);stamp=now;
 sol->scene=classic?0:world+1;
 sol->motion(gX,gY,gZ,motion);
 if(lockWell&&!classic)sol->well(lockX,lockY,2.4f,155.f);
 float beat=audioOn&&!classic?clip(energy*.36f+bass*.7f+kick*.65f+beatPulse()*.5f,0,2.f):0;
 audioGlow=audioGlow*.85f+beat*.15f;
 sol->step(dt,beat,ambient);
 rippleR+=7.7f;rippleV*=.952f;flash*=.92f;sec+=dt;
}
static void startTouch(int px,int py,bool classic){
 if(!ready(classic)||panel)return;
 pressed=true;x=ox=px-CX;y=oy=py-CY;
 if(!classic){sol->well(x,y,3.f,175.f);rippleX=x;rippleY=y;rippleR=2;rippleV=1.1f;}
}
static void moveTouch(int px,int py,bool classic){
 if(!sol||!pressed||panel)return;
 float xx=px-CX,yy=py-CY,dx=clip(xx-x,-35,35),dy=clip(yy-y,-35,35);
 if(fabsf(dx)+fabsf(dy)>1.2f){sol->stir(xx,yy,dx*1.7f,dy*1.7f);
    if(!classic)sol->well(xx,yy,3.0f,165.f);}
 ox=x;oy=y;x=xx;y=yy;
}
static bool menuTap(int px,int py){
 if(py<147){panel=false;hudUntil=millis()+2000;return true;}
 if(py<263){if(px<CX)setWorld(1);else setPalette();return true;}
 if(py<373){if(px<CX){audioOn=!audioOn;prefs.putBool("grav20a",audioOn);}
  else{quality=(quality+1)%3;prefs.putUChar("grav20q",quality);}return true;}
 if(px<CX){ambient=!ambient;prefs.putBool("grav20m",ambient);}
 else{flip=!flip;prefs.putBool("grav20f",flip);}
 return true;
}
static bool endTouch(int px,int py,uint32_t held,int dx,int dy,bool classic){
 pressed=false;if(!sol)return true;
 if(classic){
   // Standalone FluidBox B reset conflicts with this firmware's B next-effect.
   // To preserve A/B navigation, quick touchscreen tap resets Classic instead.
   if(held<390&&abs(dx)<35&&abs(dy)<35)reset();
   return true;
 }
 if(panel)return menuTap(px,py);
 uint32_t now=millis();
 if(abs(dy)>80&&abs(dy)>abs(dx)*1.23f){lastTap=0;setWorld(dy>0?1:-1);return true;}
 if(held>660&&abs(dx)<55&&abs(dy)<55){lockWell=!lockWell;lockX=px-CX;lockY=py-CY;
   if(!lockWell)sol->well(0,0,0);
   hudUntil=now+2200;lastTap=0;return true;}
 if(held<=330&&abs(dx)<40&&abs(dy)<40){
   if(lastTap&&now-lastTap>50&&now-lastTap<=380&&
      (px-tapX)*(px-tapX)+(py-tapY)*(py-tapY)<73*73){
     panel=true;lockWell=false;sol->well(0,0,0);lastTap=0;return true;}
   lastTap=now;tapX=px;tapY=py;
   sol->well(px-CX,py-CY,3.9f,165.f);rippleX=px-CX;rippleY=py-CY;rippleR=1;rippleV=1.1f;
 }else lastTap=0;
 return true;
}
static void background(int scene){
 cv.fillScreen(TFT_BLACK);
 if(scene==0)return; // Classic shows only particles, no overlay or sci-fi scenery.
 uint16_t faded=pal(.42f,.12f);
 for(int r=62;r<=215;r+=49)cv.drawCircle(CX,CY,r,faded);
 for(int k=0;k<50;k++){
  float th=2.39996323f*k+sec*.012f*(k%2?1:-1);
  float rr=28+186*sqrtf(GravityPhysics::rnd(k*20+3));
  int xx=CX+(int)(rr*cosf(th)),yy=CY+(int)(rr*sinf(th));
  if(k%5==0)cv.drawPixel(xx,yy,pal(k*.08f,.37f));
 }
 if(scene==4||scene==5){
  for(int ring=1;ring<=4;ring++){
    int rad=ring*27+22;int chunks=44;
    for(int j=0;j<chunks;j++){
      float a=(float)j/chunks*TAU+sec*(scene==5?.30f:.12f)+ring*.3f;
      float b=a+TAU/chunks*.68f;
      cv.drawLine(CX+(int)(rad*cosf(a)),CY+(int)(rad*sinf(a)),
       CX+(int)(rad*cosf(b)),CY+(int)(rad*sinf(b)),pal(ring*.17f,.09f+ring*.022f));
    }
  }
 }
}
static void particles(bool classic){
 if(!sol)return;
 int scene=classic?0:world+1;
 int stride=!classic&&quality==0?2:1;
 for(int i=0;i<GravityPhysics::COUNT;i+=stride){
   const auto& p=sol->particles[i];
   float dep=(p.z+GravityPhysics::H)/(2*GravityPhysics::H);
   float scale=.80f+.30f*dep;
   int xx=CX+(int)(p.x*scale),yy=CY+(int)(p.y*scale);
   if(xx<8||yy<8||xx>=W-8||yy>=H-8)continue;
   int radius=2+(int)(dep*2.f)+(scene==1&&quality>0?1:0);
   float velocity=clip(p.speed*.055f,0,1.0f);
   if(scene==0){ // Original FluidBox aesthetic: velocity/depth RGB565 droplet.
    uint16_t c=rgb(30+180*velocity,100+115*dep,215-40*velocity);
    cv.fillCircle(xx,yy,radius+1,rgb(4,18,32));
    cv.fillCircle(xx,yy,radius,c);
    if(i%3==0)cv.drawPixel(xx-1,yy-1,rgb(219,248,255));
    continue;
   }
   uint16_t c=pal(i*.025f+sec*.023f+velocity*.13f,.55f+.35f*dep+.16f*velocity+.15f*audioGlow);
   if(scene==1){ // Iridescent mercury: metallic edges, bevel and highlight.
    cv.fillCircle(xx,yy,radius+3,pal(.08f,.10f));cv.fillCircle(xx,yy,radius,c);
    cv.fillCircle(xx-1,yy-1,radius>3?2:1,rgb(238,248,255));
    if(i%8==0)cv.drawCircle(xx,yy,radius+4,pal(.25f,.18f));
   }else if(scene==2){ // Nebula Ink: layered cloud halos and colored wisps.
    if(quality>0)cv.fillCircle(xx,yy,radius+5,pal(i*.03f,.12f));
    cv.fillCircle(xx,yy,radius+1,c);cv.drawPixel(xx,yy,rgb(232,247,255));
    if(i%7==0)cv.drawLine(CX+(int)p.px,CY+(int)p.py,xx,yy,pal(i*.04f,.27f));
   }else if(scene==3){ // Orbital Atlas: arcing comet trails and pearly planets.
    cv.drawLine(xx-(int)(p.vx*1.6f),yy-(int)(p.vy*1.6f),xx,yy,pal(i*.018f,.34f));
    cv.fillCircle(xx,yy,radius+2,pal(i*.04f,.11f));cv.fillCircle(xx,yy,radius,c);
    if(i%9==0)cv.drawPixel(xx-1,yy-1,rgb(250,249,255));
   }else if(scene==4){ // Plasma Vortex: plasma strands, white-hot cores.
    cv.fillCircle(xx,yy,radius+3,pal(i*.019f,.12f));cv.fillCircle(xx,yy,radius,c);
    if(i%3==0)cv.drawPixel(xx,yy,rgb(248,251,255));
   }else if(scene==5){ // Black Hole: streaking accretion disk.
    float ang=atan2f(p.y,p.x),rad=sqrtf(p.x*p.x+p.y*p.y)*scale;
    cv.drawLine(CX+(int)(rad*cosf(ang-.13f-velocity*.08f)),
                CY+(int)(rad*sinf(ang-.13f-velocity*.08f)),xx,yy,pal(i*.015f,.35f));
    cv.fillCircle(xx,yy,radius,c);
   }else if(scene==6){ // Ferrofluid: hard dark beads and spikes.
    cv.fillCircle(xx,yy,radius+2,pal(i*.025f,.14f));
    cv.fillCircle(xx,yy,radius,pal(i*.021f,.70f));
    if(i%7==0){int d=6+(int)(velocity*13);cv.drawLine(xx,yy,
     xx+(int)(cosf(i*1.4f)*d),yy+(int)(sinf(i*1.4f)*d),pal(.6f,.31f));}
   }else if(scene==7){ // Prism Storm: high energy neon comets.
    if(quality>0)cv.drawCircle(xx,yy,radius+5,pal(i*.024f,.24f));
    cv.drawLine(xx-(int)(p.vx*1.8f),yy-(int)(p.vy*1.8f),xx,yy,pal(i*.023f,.51f));
    cv.fillCircle(xx,yy,radius,c);
   }else{ // Quantum Sand: 400 tiny grains and gentle crystalline shimmer.
    cv.fillCircle(xx,yy,i%7==0?2:1,pal(i*.006f,.40f+.33f*dep));
    if(i%13==0)cv.drawPixel(xx,yy,rgb(242,244,255));
   }
 }
 // Extra structure only in the plasma and ferrofluid scenes.
 if((scene==4||scene==6)&&quality>0){
  for(int k=0;k<sol->pairCount&&k<(quality==2?840:420);k+=2){
   const auto &e=sol->pairs[k];if(e.q<.45f)continue;
   const auto &a=sol->particles[e.a],&b=sol->particles[e.b];
   cv.drawLine(CX+(int)a.x,CY+(int)a.y,CX+(int)b.x,CY+(int)b.y,pal(k*.017f,.23f));
  }
 }
}
static void panelDraw(){
 cv.fillRoundRect(38,57,390,369,26,rgb(7,13,26));
 cv.drawRoundRect(38,57,390,369,26,pal(.2f,.44f));
 label("GRAVITY LAB",CX,103,1.51f,374,rgb(238,245,255));
 label("TAP HEADER TO CLOSE",CX,144,.75f,345,rgb(123,161,193));
 auto tile=[](int tx,const String& name,const String& val){
  cv.fillRoundRect(tx,158,165,101,13,rgb(19,29,50));
  cv.drawRoundRect(tx,158,165,101,13,pal(.3f,.45f));
  label(name,tx+82,182,.98f,149,rgb(157,189,220));label(val,tx+82,225,1.22f,148,rgb(242,248,255));
 };
 tile(61,"WORLD",WORLDS[world]);tile(238,"PALETTE",PAL[palette]);
 cv.fillRoundRect(61,269,165,102,13,rgb(18,29,50));cv.drawRoundRect(61,269,165,102,13,pal(.28f,.42f));
 cv.fillRoundRect(238,269,165,102,13,rgb(18,29,50));cv.drawRoundRect(238,269,165,102,13,pal(.28f,.42f));
 label("MIC / BEAT",143,294,.96f,151,rgb(159,193,215));label(audioOn?"ON":"OFF",143,340,1.24f,151,rgb(247,247,255));
 label("QUALITY",321,294,.96f,152,rgb(159,193,215));label(quality==2?"ULTRA":quality==1?"MID":"ECO",321,340,1.24f,148,rgb(247,247,255));
 label(String("AMBIENT ")+(ambient?"ON":"OFF")+"  |  INVERT "+(flip?"ON":"OFF"),CX,403,.95f,349,pal(.28f,.91f));
 cv.drawLine(CX,382,CX,426,pal(.44f,.23f));
}
static void hud(bool classic){
 if(classic)return;
 if(panel){panelDraw();return;}
 if((int32_t)(hudUntil-millis())<=0)return;
 label("GRAVITY LAB",CX,47,.87f,335,pal(.27f,.97f));
 label(WORLDS[world],CX,417,1.19f,365,rgb(219,233,252));
 label(lockWell?"WELL LOCKED - HOLD TO RELEASE":"DOUBLE TAP - CONTROLS",CX,445,.72f,364,rgb(123,155,188));
}
static void touchStart(int px,int py,bool classic){startTouch(px,py,classic);}
static void touchMove(int px,int py,bool classic){moveTouch(px,py,classic);}
static bool touchEnd(int px,int py,uint32_t held,int dx,int dy,bool classic){return endTouch(px,py,held,dx,dy,classic);}
static void render(bool classic){
 if(!ready(classic))return;
 background(classic?0:world+1);
 particles(classic);
 if(!classic){
  if(rippleV>.04f&&rippleR<250){
    float intensity=rippleV*(1-rippleR/250.f);
    for(int k=0;k<3;k++)cv.drawCircle(CX+(int)rippleX,CY+(int)rippleY,(int)rippleR+k*5,pal(.2f+k*.18f,intensity*(k==0?.35f:.13f)));
  }
  if(flash>.16f)cv.drawCircle(CX,CY,204,pal(.45f,clip(flash*.33f,0,.34f)));
 }
 hud(classic);
}
} // namespace GravityLab