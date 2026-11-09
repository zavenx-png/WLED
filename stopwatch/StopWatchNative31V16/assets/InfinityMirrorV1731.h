#pragma once
// ZAVEN StopWatch V17.31 — five originally authored circular infinity mirror effects.
// Designed for the 466px round StopWatch, RGB565 sprite and continuous, non-audio time.
// IDs 71..75 are appended. No Wi-Fi, microphone amplitude or external assets required.
namespace InfinityMirrorV1731 {
  static constexpr int LUT=96;
  static constexpr float IM_TAU=6.2831853071795864769f;
  static float sn[LUT],cs[LUT];
  static bool ready=false;

  static void init(){
    if(ready)return;
    for(int i=0;i<LUT;i++){
      const float a=IM_TAU*i/LUT;
      sn[i]=sinf(a);cs[i]=cosf(a);
    }
    ready=true;
  }
  static inline float wrap(float a){return a-floorf(a);}
  // Perspective depth: z=0 arrives at the outer ring; z=1 falls into the center.
  // Radius contracts with squared distance, so inward travel never jumps at the
  // reset: each new mirrored ring appears only at the outer rim.
  static inline float depthRadius(float z){
    float near=1.f-z;
    return 5.f+211.f*near*near;
  }
  static inline float brilliance(float z){
    const float near=1.f-z;
    return .09f+.90f*sqrtf(near);
  }
  static inline void node(int x,int y,int radius,uint16_t color,bool halo){
    // Small bright LED die with a 1px reflection rim; never flat screen-size glow.
    if(halo&&radius>=2)cv.fillCircle(x,y,radius+2,tint(color,.21f));
    cv.fillCircle(x,y,radius,color);
    if(radius>=3)cv.drawPixel(x-1,y-1,rgb(245,249,255));
  }
  static void backdrop(uint8_t kind,float seconds){
    cv.fillScreen(TFT_BLACK);
    // Optical cavity: keep an almost-black interior rather than a 2D gradient.
    uint16_t core=kind==1?rgb(2,3,4):kind==3?rgb(5,1,12):rgb(1,3,9);
    cv.fillCircle(CX,CY,220,core);
    // A restrained glass/metal lip, inside the physical circle.
    cv.drawCircle(CX,CY,225,tint(rgb(119,177,212),.32f));
    cv.drawCircle(CX,CY,222,tint(rgb(88,118,163),.25f));
    for(int k=0;k<24;k++){
      int i=(k*4)%LUT;
      const float shimmer=.32f+.25f*sinf(seconds*.32f+k*.70f);
      const uint16_t c=tint(kind==1?rgb(209,223,229):hue(k/24.f+.15f),shimmer);
      cv.drawLine(CX+cs[i]*225,CY+sn[i]*225,
                  CX+cs[i]*220,CY+sn[i]*220,c);
    }
  }
  static void finish(float seconds,uint8_t mode){
    // Deep black vanishing aperture and tiny specular infinity point.
    cv.fillCircle(CX,CY,5,rgb(0,0,0));
    cv.drawCircle(CX,CY,7,tint(mode==1?rgb(191,207,217):rgb(83,169,225),.14f));
    if(mode!=1&&((int)(seconds*1.7f)%17)<4)
      cv.drawPixel(CX,CY,tint(rgb(225,238,255),.19f));
  }

  // #34 PRISM DROP — the multicolored concentric infinity-well from the photo:
  // separated RGB diode rings; an uninterrupted inward depth descent.
  static void prismDrop(float seconds){
    backdrop(0,seconds);
    const int rings=24;
    for(int k=0;k<rings;k++){
      float z=wrap(k/(float)rings+seconds*.064f);
      float r=depthRadius(z);
      float br=brilliance(z);
      const int step=(r>126?1:(r>62?2:3));
      const float twist=seconds*.050f+z*.32f;
      const float co=cosf(twist),si=sinf(twist);
      for(int i=0;i<LUT;i+=step){
        const float x=co*cs[i]-si*sn[i],y=si*cs[i]+co*sn[i];
        const int px=(int)(CX+r*x),py=(int)(CY+r*y);
        const float colorPosition=wrap(i/(float)LUT+.10f+.035f*z+seconds*.009f);
        uint16_t c=tint(hue(colorPosition),br);
        const int size=r>100?3:(r>38?2:1);
        node(px,py,size,c,(r>140&&(i%6)==0));
      }
      if(k%3==0&&r>42)cv.drawCircle(CX,CY,(int)r,tint(hue(.59f+z*.27f),br*.11f));
    }
    finish(seconds,0);
  }

  // #35 SILVER LIGHTWELL — high density reflected white-dot tunnel,
  // charcoal/metal rim, soft ice-blue alternating rows and deep black center.
  static void silverLightwell(float seconds){
    backdrop(1,seconds);
    for(int k=0;k<28;k++){
      float z=wrap(k/28.f+seconds*.047f);
      float r=depthRadius(z);
      const float br=brilliance(z);
      const int step=r>117?1:(r>61?2:3);
      const float offset=(k&1)?IM_TAU/(2*LUT):0.f;
      float co=cosf(offset),si=sinf(offset);
      const uint16_t pearl=(k%5==0)?rgb(119,185,217):((k&1)?rgb(214,232,245):rgb(243,248,251));
      const int dotr=r>116?3:(r>34?2:1);
      for(int i=0;i<LUT;i+=step){
        int x=(int)(CX+r*(cs[i]*co-sn[i]*si));
        int y=(int)(CY+r*(sn[i]*co+cs[i]*si));
        node(x,y,dotr,tint(pearl,br),r>110&&(i%10==0));
      }
      if(k%4==0&&r>28)
        cv.drawCircle(CX,CY,(int)r,tint(rgb(197,212,229),br*.10f));
    }
    finish(seconds,1);
  }

  // #36 NEON HELIX CHAMBER — two counter-twisting spiral rails made of
  // glowing LED beads, with interlaced angular lanes converging at the center.
  static void neonHelix(float seconds){
    backdrop(2,seconds);
    const int rings=22;
    const float drift=seconds*.12f;
    for(int k=0;k<rings;k++){
      float z=wrap(k/(float)rings+seconds*.073f);
      float r=depthRadius(z);
      float br=brilliance(z);
      const float twist=2.8f*z+drift;
      const float co=cosf(twist),si=sinf(twist);
      int step=r>105?2:3;
      for(int i=0;i<LUT;i+=step){
        const float x=cs[i]*co-sn[i]*si;
        const float y=sn[i]*co+cs[i]*si;
        const float quadrant=wrap(i/(float)LUT+.25f+z*.23f);
        const uint16_t base=(quadrant<.5f)?rgb(13,237,243):rgb(253,46,198);
        const float shimmer=.74f+.26f*sinf(i*.27f+seconds*1.5f+z*5.f);
        node((int)(CX+r*x),(int)(CY+r*y),r>105?3:2,
             tint(base,br*shimmer),r>160&&(i%12==0));
      }
    }
    // Six ruled helix tracks are architectural tunnel ribs (not random rays).
    for(int arm=0;arm<8;arm++){
      float prevX=0,prevY=0;
      for(int step=0;step<=44;step++){
        float z=step/44.f;
        float r=depthRadius(z);
        float theta=(arm*IM_TAU/8)+z*3.1f+drift;
        float x=CX+r*cosf(theta),y=CY+r*sinf(theta);
        if(step){
          uint16_t co=tint((arm&1)?rgb(255,58,189):rgb(36,192,246),
                            (.12f+.27f*(1.f-z)));
          line(prevX,prevY,x,y,co,1);
        }
        prevX=x;prevY=y;
      }
    }
    // Keep the LED mirrors in front of thin rails for a dimensional finish.
    for(int k=0;k<10;k++){
      float z=wrap(k/10.f+seconds*.073f);
      float r=depthRadius(z);
      float a=seconds*.12f+z*2.8f;
      float co=cosf(a),si=sinf(a);
      for(int n=0;n<8;n++){
        int i=n*12;
        node((int)(CX+r*(co*cs[i]-si*sn[i])),
             (int)(CY+r*(si*cs[i]+co*sn[i])),
             r>88?4:2,tint((n&1)?rgb(255,84,219):rgb(53,237,252),brilliance(z)),
             r>136);
      }
    }
    finish(seconds,2);
  }

  // #37 ULTRAVIOLET REACTOR — segmented LED panels, rotating azimuth,
  // strong magenta / cobalt / cyan color separation and layered mirror rings.
  static void ultravioletReactor(float seconds){
    backdrop(3,seconds);
    const int rings=22;
    for(int k=0;k<rings;k++){
      float z=wrap(k/(float)rings+seconds*.056f);
      float r=depthRadius(z);
      if(r<7)continue;
      float br=brilliance(z);
      const float rot=seconds*.21f+(k&1?-.22f:.22f)+z*.80f;
      const float co=cosf(rot),si=sinf(rot);
      const int segments=16;
      for(int seg=0;seg<segments;seg++){
        int j0=(seg*LUT)/segments, j1=((seg+1)*LUT)/segments;
        // Leave a consistent black reflective gap between LED panels.
        const float shrink=.81f;
        int jEnd=j0+(int)((j1-j0)*shrink);
        uint16_t color=(seg%4==0)?rgb(250,56,200):
                        (seg%4==1)?rgb(46,122,252):
                        (seg%4==2)?rgb(40,230,244):rgb(165,57,255);
        color=tint(color,br);
        for(int j=j0;j<jEnd;j++){
          int next=j+1;
          float x0=CX+r*(cs[j]*co-sn[j]*si),y0=CY+r*(sn[j]*co+cs[j]*si);
          float x1=CX+r*(cs[next]*co-sn[next]*si),y1=CY+r*(sn[next]*co+cs[next]*si);
          line(x0,y0,x1,y1,color,r>85?3:2);
        }
        if(r>85&&seg%3==0){
          int ix=j0+1;
          node((int)(CX+r*(cs[ix]*co-sn[ix]*si)),
               (int)(CY+r*(sn[ix]*co+cs[ix]*si)),3,
               tint(rgb(244,237,255),br*.75f),false);
        }
      }
      if(k%3==0)cv.drawCircle(CX,CY,(int)r,tint(rgb(83,47,133),br*.20f));
    }
    finish(seconds,3);
  }

  // #38 DISCO STARGATE — a physically structured twelve-sided infinite
  // polygonal mirror with luminous vertex bulbs and long repeating edge seams.
  static void discoStargate(float seconds){
    backdrop(4,seconds);
    const int sides=12,rings=22;
    const float rotationalDrift=seconds*.075f;
    for(int k=0;k<rings;k++){
      const float z=wrap(k/(float)rings+seconds*.065f);
      const float r=depthRadius(z);
      const float br=brilliance(z);
      const float a=rotationalDrift+z*.53f;
      const float co=cosf(a),si=sinf(a);
      float vx[sides],vy[sides];
      for(int p=0;p<sides;p++){
        const int i=p*LUT/sides;
        vx[p]=CX+r*(cs[i]*co-sn[i]*si);
        vy[p]=CY+r*(sn[i]*co+cs[i]*si);
      }
      for(int p=0;p<sides;p++){
        const int next=(p+1)%sides;
        uint16_t color=tint(hue(p/(float)sides+.07f*z+seconds*.017f),br);
        line(vx[p],vy[p],vx[next],vy[next],tint(color,.25f),r>75?3:2);
        if(r>38)line(vx[p],vy[p],vx[next],vy[next],color,1);
        node((int)vx[p],(int)vy[p],r>95?4:2,color,r>124);
        // LED diamonds at the midpoints give each facet a structured cadence.
        if(r>55&&((p+k)&1)==0){
          float x=(vx[p]+vx[next])*.5f,y=(vy[p]+vy[next])*.5f;
          node((int)x,(int)y,r>130?3:2,tint(rgb(237,247,255),br*.90f),false);
        }
      }
    }
    // Symmetric rails recede in perspective, linking matching mirror vertices.
    for(int p=0;p<12;p+=2){
      float oldx=0,oldy=0;
      for(int k=0;k<=35;k++){
        float z=k/35.f,r=depthRadius(z);
        float a=p*IM_TAU/12+rotationalDrift+z*.53f;
        float x=CX+r*cosf(a),y=CY+r*sinf(a);
        if(k>0)line(oldx,oldy,x,y,tint(rgb(169,179,250),.12f*(1.f-z)),1);
        oldx=x;oldy=y;
      }
    }
    finish(seconds,4);
  }

  // seconds from the monotonic clock, not beatClock: always animates silently.
  static void render(uint8_t variant){
    init();
    const float seconds=millis()*.001f;
    switch(variant){
      case 0:prismDrop(seconds);break;
      case 1:silverLightwell(seconds);break;
      case 2:neonHelix(seconds);break;
      case 3:ultravioletReactor(seconds);break;
      default:discoStargate(seconds);break;
    }
  }
}
