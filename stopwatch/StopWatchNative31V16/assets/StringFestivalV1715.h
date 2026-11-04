// V17.15 — Five new high-contrast thread-and-textile scenes.
// Appended as IDs 49-53 (menu entries 27-31), preserving all V17.14 routines.
// Uses the fast native RGB565 M5GFX sprite / drawing primitives already
// defined by NativeApp.cpp and StringFestivalV1714.h. No dynamic allocation.
// All animations keep moving in silence; mic subtly changes size and flow.
#pragma once

static inline uint16_t sf15Ink(int k) {
  switch(k%6) {
    case 0: return rgb(255,75,163); // electric raspberry
    case 1: return rgb(68,234,231); // icy turquoise
    case 2: return rgb(253,193,88); // warm gold
    case 3: return rgb(173,101,255); // orchid
    case 4: return rgb(100,171,255); // periwinkle
    default:return rgb(255,138,108); // flame
  }
}
// A satin embroidery button: actual stitches and four holes, not a dot.
static void sf15Button(float x,float y,float r,float a,uint16_t ink) {
  dot(x+4,y+6,r+6,rgb(2,5,17));
  dot(x,y,r+4,tint(ink,.38f));
  dot(x,y,r,tint(ink,.73f));
  dot(x-5,y-6,r*.66f,tint(sfIvory(),.31f));
  circ(x,y,r,tint(sfIvory(),.76f),2);
  circ(x,y,r*.78f,tint(ink,.62f),2);
  const float ca=cosf(a),sa=sinf(a),hole=r*.27f;
  float hx[4],hy[4];
  for(int n=0;n<4;n++){
    const float xx=(n%2?1.f:-1.f)*hole,yy=(n<2?-hole:hole);
    hx[n]=x+ca*xx-sa*yy;hy[n]=y+sa*xx+ca*yy;
    dot(hx[n],hy[n],r*.095f,rgb(5,4,23));
  }
  sfLine(hx[0],hy[0],hx[3],hy[3],sfIvory(),.87f,3);
  sfLine(hx[1],hy[1],hx[2],hy[2],sfGold(),.89f,3);
  sfLight(x-r*.44f,y-r*.46f,sfIvory(),2);
}
static bool renderStringFestivalV1715(uint8_t id){
  if(id<49||id>53)return false;
  const float t=millis()*.001f;
  const float b=clampf(bass*.65f+kick*.27f,0.f,1.f);
  const float m=clampf(mids*.75f+highs*.28f,0.f,1.f);
  black();
  switch(id){
  case 49:{ // YARNFALL CATHEDRAL — a waterfall of tangible twisted skeins.
    sfStarfield(t,13);
    // The ribs of a distant midnight-velvet drape.
    for(int i=0;i<12;i++){
      const int x=17+i*39;
      sfLine(x,19,x+25,446,sfViolet(),.14f,2);
    }
    const float drift=t*.61f;
    for(int j=0;j<6;j++){
      const uint16_t c=sf15Ink(j);
      float oldx=0,oldy=0;
      for(int k=0;k<=42;k++){
        const float y=-16+k*12.f;
        const float x=47+j*75+26*sinf(y*.017f+drift+j*.72f)
                              +7*sinf(y*.062f-drift*.64f+j);
        if(k){
          sfLine(oldx+3,oldy+4,x+3,y+4,rgb(14,7,32),.81f,12);
          sfLine(oldx,oldy,x,y,c,.96f,10);
          sfLine(oldx-2,oldy-2,x-2,y-2,sfIvory(),.36f,2);
        }
        if(k>2&&k<42&&k%3==0){
          float spiral=sinf(y*.10f-t*1.5f+j);
          sfLight(x+spiral*3,y,sfIvory(),1);
        }
        oldx=x;oldy=y;
      }
      // Colored yarn spool caps near the top, distinctly recognizable.
      sf15Button(47+j*75+26*sinf(drift+j*.72f),31,16,j*.35f,sf15Ink(j));
    }
    // A long silver needle threading the waterfall at an angle.
    sfLine(317,102,437,18,sfIvory(),.81f,7);
    sfLine(331,93,438,18,sfCyan(),.39f,2);
    sfEllipse(329,92,11,7,-.60f,sfGold(),.9f,2);
    break;
  }
  case 50:{ // CELESTIAL BUTTONWORK — orbital mother-of-pearl sewn on velvet.
    sfStarfield(t,17);
    const float rot=t*.11f,breath=1.f+.09f*b;
    sfEllipse(CX,CY,206,206,.0f,sfViolet(),.48f,3);
    sfEllipse(CX,CY,193,193,.0f,sfPink(),.29f,2);
    for(int k=0;k<50;k++){
      float ang=TAU*k/50.f+rot*.2f;
      dot(CX+201*cosf(ang),CY+201*sinf(ang),2,tint(sfIvory(),.35f));
    }
    // Sewn radial connectors beneath all buttons.
    for(int j=0;j<7;j++){
      const float a=TAU*j/7.f+rot,xx=CX+150*cosf(a),yy=CY+150*sinf(a);
      sfBezier(CX+42*cosf(a),CY+42*sinf(a),CX+84*cosf(a+.10f),
        CY+84*sinf(a+.10f),CX+115*cosf(a-.16f),CY+115*sinf(a-.16f),
        xx,yy,sf15Ink(j),.69f,3);
    }
    for(int j=0;j<7;j++){
      float a=TAU*j/7.f+rot;
      float rr=(147+6*sinf(t*.43f+j))*breath;
      sf15Button(CX+rr*cosf(a),CY+rr*sinf(a),34,rot*.75f+j*.21f,sf15Ink(j));
    }
    sf15Button(CX,CY,62,rot*.26f,sfPink());
    for(int j=0;j<14;j++){
      float a=TAU*j/14.f-rot;
      sfLine(CX+77*cosf(a),CY+77*sinf(a),CX+90*cosf(a+.05f),
        CY+90*sinf(a+.05f),sfGold(),.65f,2);
    }
    break;
  }
  case 51:{ // SPIRAL SPINDLE v17.17: no button; faster, smooth flowing yarn.
    sfStarfield(t,6);
    // Smooth audio modulation of radius, not angle; music cannot make rotation jump.
    static float spiralBass=0.f;
    spiralBass+=(b-spiralBass)*.045f;
    const float rot=t*.64f; // 2.9x original .22 radians per second
    const float swell=1.f+.035f*spiralBass;
    for(int j=0;j<6;j++){
      float ox=0.f,oy=0.f;
      const uint16_t c=sf15Ink(j);
      for(int k=0;k<=94;k++){
        const float u=k/94.f;
        const float rad=(11.f+208.f*u)*swell;
        const float ang=rot+TAU*j/6.f+10.7f*u
              +.047f*sinf(t*.52f+u*7.f+j*.7f);
        const float x=CX+rad*cosf(ang),y=CY+rad*sinf(ang);
        if(k){
          sfLine(ox,oy,x,y,c,.86f,5);
          sfLine(ox-1.2f,oy-1.2f,x-1.2f,y-1.2f,sfIvory(),.29f,2);
        }
        ox=x;oy=y;
      }
    }
    // Intentionally no center button, circular hub, dots or pinned ornaments.
    break;
  }
  case 52:{ // AURORA FISHING NET — softly deformed, woven diamond lattice.
    sfStarfield(t,9);
    const float phase=t*.33f,rr=.69f+.09f*sinf(t*.26f);
    const float co=cosf(rr),si=sinf(rr);
    // A shared continuous deformation field: both directions actually weave.
    for(int dir=0;dir<2;dir++)for(int j=-7;j<=7;j++){
      float prevx=0,prevy=0;
      for(int k=0;k<=30;k++){
        float u=-1.11f+2.22f*k/30.f,v=j/7.1f;
        float x=dir==0?u:v,y=dir==0?v:u;
        float d=x*x+y*y;
        float lens=1.f-.15f*expf(-d*2.3f);
        float ripple=.064f*sinf(6.8f*(x+y)+phase*3.9f)
                     +.035f*sinf(8.1f*(x-y)-phase*2.2f);
        float ax=196*(x*lens+ripple),ay=196*(y*lens+ripple*.74f);
        float X=CX+ax*co-ay*si,Y=CY+ax*si+ay*co;
        if(k)sfLine(prevx,prevy,X,Y,dir==0?sfCyan():sfPink(),dir==0?.73f:.63f,3);
        prevx=X;prevy=Y;
      }
    }
    for(int j=-5;j<=5;j++)for(int k=-5;k<=5;k++){
      float x=j/7.1f,y=k/7.1f,d=x*x+y*y;
      if(d>1.0f)continue;
      float lens=1.f-.15f*expf(-d*2.3f);
      float ripple=.064f*sinf(6.8f*(x+y)+phase*3.9f)
                   +.035f*sinf(8.1f*(x-y)-phase*2.2f);
      float ax=196*(x*lens+ripple),ay=196*(y*lens+ripple*.74f);
      float X=CX+ax*co-ay*si,Y=CY+ax*si+ay*co;
      dot(X,Y,2,tint(sfGold(),.65f+.25f*sinf(t*1.5f+j+k)));
    }
    sfEllipse(CX,CY,211,211,0.f,sfCyan(),.40f,3);
    sfEllipse(CX,CY,218,218,0.f,sfPink(),.28f,2);
    break;
  }
  case 53:{ // TIDAL CABLE KNIT v17.17: flowing ropes, no clock-pin backdrop.
    // Nothing decorative behind the ropes: no hatch ticks, V pins or rim.
    static float knitBass=0.f;
    knitBass+=(b-knitBass)*.045f;
    const float motion=t*1.56f; // 1.9x original strand-driving rate
    for(int col=0;col<4;col++){
      const float base=66.f+col*110.f;
      const uint16_t c=sf15Ink(col);
      for(int side=-1;side<=1;side+=2){
        float ox=0.f,oy=0.f;
        for(int k=0;k<=70;k++){
          const float y=-18.f+k*7.2f; // denser vertices: smooth curved rope
          const float phi=y*.031f-motion*1.60f+col*.47f;
          const float sway=10.f*sinf(y*.016f+motion*.38f);
          const float x=base+sway+side*(28.f+1.8f*knitBass)*sinf(phi);
          if(k){
            // Wide satin body provides its own shading, avoiding extra backdrop.
            sfLine(ox,oy,x,y,c,.88f,8);
            const float shimmer=.24f+.18f*sinf(phi+side*.9f);
            sfLine(ox-1.8f,oy-1.8f,x-1.8f,y-1.8f,sfIvory(),shimmer,2);
          }
          ox=x;oy=y;
        }
      }
    }
    break;
  }
  }
  return true;
}
