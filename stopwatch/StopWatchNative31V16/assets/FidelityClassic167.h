// StopWatch V16.7 browser-geometry fidelity pass.
// Purpose: native interpretations of browser math, not screenshot sprites.
// Uses original C152 framebuffer/audio/beat engine. No heap allocation per frame.
#pragma once

static inline float fxSine(float a,float b,float v){return a+b*sinf(v);}
static inline uint16_t fxInk(float h,float level){return tint(hue(frac(h)),clampf(level));}
static inline void fxPolarDot(float radius,float angle,float size,uint16_t color){
 dot(CX+radius*cosf(angle),CY+radius*sinf(angle),size,color);
}
static inline void fxPoly(int sides,float radius,float turn,uint16_t color,int width=1){
 float px=CX+radius*cosf(turn),py=CY+radius*sinf(turn);
 for(int j=1;j<=sides;j++){
  float a=turn+j*TAU/sides;float x=CX+radius*cosf(a),y=CY+radius*sinf(a);
  line(px,py,x,y,color,width);px=x;py=y;
 }
}
static inline float fxECG(float p){
 auto g=[](float x,float y,float w)->float{float z=(x-y)/w;return expf(-.5f*z*z);};
 return .18f*g(p,.18f,.044f)-.12f*g(p,.355f,.013f)+1.15f*g(p,.386f,.008f)
       -.31f*g(p,.416f,.015f)+.35f*g(p,.675f,.076f);
}
// True for all reworked classic effects; 1 (Prism EQ) and 22 (Robot Shuffle)
// are deliberately routed to their UNCHANGED V16.6 code.
static bool fidelityClassic167(uint8_t id){
 float t=T(),kick=clampf(bass*.65f+energy*.45f),pulse=beatPulse();
 switch(id){
 case 2:{ // 11 Psychedelic Bloom: remove solid purple center; browser 9 breathing contours
  black();
  for(int k=0;k<9;k++){
   float ra=24+k*20+energy*30,ax=0,ay=0;
   for(int j=0;j<=124;j++){
    float a=j*TAU/124.f;
    float r=ra*(1+.20f*sinf(a*(5+k%5)+t*(1.2f+k*.13f))
           +.23f*bass*sinf(a*13-t*2));
    float x=CX+cosf(a)*r,y=CY+sinf(a)*r;
    if(j)line(ax,ay,x,y,fxInk(k*.103f+t*.044f,.42f+.30f*energy),1);
    ax=x;ay=y;
   }
  }return true;
 }
 case 3:{ // 12 Vapor Scope: forward-moving neon highway, NO car; sound-reactive foreground
  black();
  // Fully circular neon sunset, including true rounded top and bottom.
  // Draw filled base FIRST; clipped bands never truncate its silhouette.
  const int sunX=CX,sunY=149,sunR=66;
  dot(sunX,sunY,sunR,rgb(239,96,164));
  for(int yy=-sunR;yy<=sunR;yy+=4){
   float half=sqrtf(fmaxf(0.f,(float)(sunR*sunR-yy*yy)));
   uint16_t co=fxInk(.92f-yy*.0014f,.73f);
   line(sunX-half,sunY+yy,sunX+half,sunY+yy,co,2);
  }
  // Horizon starts BELOW the sun; no horizontal crop through its base.
  for(int h=0;h<7;h++){
   int y=224+h*4;
   line(0,y,W,y,tint(rgb(93,38,104),.22f+h*.035f));
  }
  // Asphalt expands from the horizon toward the viewer; drawing to the circular bezel.
  const int horizon=254;
  for(int y=horizon;y<H;y+=3){
   float z=(y-horizon)/(float)(H-horizon);
   float half=16+228*z;
   line(CX-half,y,CX+half,y,rgb(7+int(6*z),8+int(3*z),19+int(11*z)),3);
  }
  // Animated neon road grid. Render AFTER asphalt so gridlines stay visible.
  // A perspective cross-hatch streams from the horizon to the circular bezel.
  for(int k=-7;k<=7;k++){
   float edgeX=CX+k*34.f;
   line(CX+k*3.f,horizon,edgeX,H,tint(rgb(67,153,222),.49f+.18f*energy));
  }
  // V16.12: a single travel clock and the EXACT same perspective transform
  // drive BOTH neon grid cross-lines and the white lane markers.
  const float travel=t*4.02f; // V17: 33% slower; grid and white lines share this clock
  for(int k=0;k<13;k++){
   float z=frac((k+travel)/13.f),depth=z*z;
   float y=horizon+207*depth,half=16+228*depth;
   if(y>H-2)continue;
   line(CX-half,y,CX+half,y,
        tint(rgb(195,60,214),.28f+.38f*depth+.09f*bass),depth>.55f?2:1);
  }
  // Fixed road edges converge at the distant horizon.
  for(int side=-1;side<=1;side+=2){
   line(CX+side*16,horizon,CX+side*234,H,fxInk(side>0?.87f:.53f,.78f),3);
   line(CX+side*25,horizon,CX+side*245,H,fxInk(side>0?.88f:.54f,.19f),5);
  }
  // Lane markers travel toward us with perspective acceleration.
  // Same travel variable and depth transform as the neon road grid above.
  for(int i=0;i<13;i++){
   float z=frac((i+travel)/13.f),depth=z*z;
   int y=horizon+int(207*depth),dh=3+int(24*depth);
   float k=.55f+.45f*depth;
   if(y+dh>H)dh=H-y;
   line(CX-4*depth,y,CX-4*depth,y+dh,tint(rgb(255,241,219),k),1+int(depth*3));
   line(CX+4*depth,y,CX+4*depth,y+dh,tint(rgb(255,241,219),k),1+int(depth*3));
  }
  // The waveform sits above the road, responding to actual mic samples and kicks.
  float drift=t*.36f;
  for(int k=0;k<4;k++){
   float oldX=0,oldY=230;
   for(int i=0;i<=132;i++){
    float u=i/132.f,x=u*(W+16)-8.f;
    int bi=clampi(int(u*31),0,31);
    float env=clampf(.18f+.90f*bass+.50f*energy+.30f*band(bi));
    float ripple=(17+28*env)*sinf(u*TAU*(1.15f+k*.14f)+drift+k*.6f)
               +(11+16*env)*sinf(u*TAU*3.2f-drift*.6f+k*.4f);
    int wi=clampi(int(u*153),0,153);
    float y=206+k*18+ripple+scopeY[wi]*autoGain*micGain*34.f;
    if(i)line(oldX,oldY,x,y,fxInk(.49f+k*.12f+t*.011f,k==1?.92f:.51f),k==1?3:1);
    oldX=x;oldY=y;
   }
  }
  return true;
 }
 case 4:{ // 13 Matrix Phosphor: same timing, pink/magenta rain and scope
  black();
  for(int i=0;i<34;i++){
   float x=7+i*14,py=fmodf(i*91.f+t*(29+i%6*14),610.f);
   for(int j=0;j<10;j++){
    float y=fmodf(py-j*18+610.f,610.f)-65;
    if(y<0||y>=H)continue;
    textC(((i+j)&1)?"1":"0",x,y,tint(rgb(255,68,171),.09f+.75f*(10-j)/10.f),2);
   }
  }
  line(0,CY,W,CY,tint(rgb(255,35,141),.24f));
  scopeLine(rgb(255,92,187),CY,125,true);
  return true;
 }
 case 6:{ // 14 Frequency Towers: browser's radial spikes, NOT vertical EQ bars
  black();
  for(int b=0;b<32;b++){
    float a=TAU*b/32.f,outer=55+clampf(band(b))*160.f;
    line(CX+52*cosf(a),CY+52*sinf(a),CX+outer*cosf(a),CY+outer*sinf(a),
         fxInk(.53f+b*.036f+t*.036f,.56f+.40f*band(b)),4);
  }
  circ(CX,CY,53,rgb(161,166,255),2);
  return true;
 }
 case 7:{ // 15 Radial Spectrum: thin outline + guaranteed BASS / number in center
  black();
  for(int i=0;i<120;i++){
   int bi=clampi(i*32/120,0,31);float a=TAU*i/120+t*.14f;
   float inner=80+12*sinf(i*.4f+t),r=86+170*band(bi);
   line(CX+cosf(a)*inner,CY+sinf(a)*inner,
        CX+cosf(a)*r,CY+sinf(a)*r,fxInk(i*.0083f+t*.042f,.38f+.58f*band(bi)),2);
  }
  // Opaque central plate prevents radial spokes hiding the labels.
  dot(CX,CY,68,rgb(4,6,18));
  circ(CX,CY,68,rgb(83,126,243),1);
  textC("BASS",CX,CY-15,rgb(217,231,255),2);
  textC(String(clampi((int)lroundf(clampf(bass)*100),0,100))+"%",CX,CY+18,rgb(227,242,255),4);
  return true;
 }
 case 8:{ // 16 Heartbeat Neon: recognisable heart and LIVE audio ECG
  black();
  uint16_t pink=rgb(255,96,175);
  for(int j=17;j<W;j+=19){line(j,0,j,H,tint(pink,j%76==17?.17f:.085f));line(0,j,W,j,tint(pink,j%76==17?.17f:.085f));}
  float thump=pulse+.34f*expf(-.5f*sq((beatPhase()-.17f)/.065f));
  float scale=5.0f*(1+.085f*thump+.045f*energy+.050f*bass),px=0,py=0;
  for(int j=0;j<=144;j++){
   float a=j*TAU/144.f;
   float x=16*powf(sinf(a),3)*scale;
   float y=-(13*cosf(a)-5*cosf(2*a)-2*cosf(3*a)-cosf(4*a))*scale;
   float nx=CX+x,ny=157+y;
   if(j){line(px,py,nx,ny,tint(pink,.29f),5);line(px,py,nx,ny,rgb(255,158,210),2);}
   px=nx;py=ny;
  }
  // Keep ECG sharp and recognisable but let actual mic wave + bass control its travel.
  float lastX=0,lastY=355;
  const float audioAmp=52+100*clampf(bass)+56*clampf(energy);
  for(int x=0;x<=W;x+=2){
   float p=frac(beatClock*.44f-(x/(float)W)*1.8f);
   int wi=clampi((x*153)/W,0,153);
   float live=clampf(scopeY[wi]*autoGain*micGain,-1.f,1.f);
   float y=355-fxECG(p)*audioAmp-live*(10+34*clampf(energy));
   if(x){line(lastX,lastY,x,y,tint(pink,.24f),6);line(lastX,lastY,x,y,rgb(255,115,186),2);}
   lastX=x;lastY=y;
  }
  line(0,355,W,355,tint(pink,.19f));
  return true;
 }
 case 9:{ // 17 Round Resonator: browser dotted wavy concentric oscillations
  black();
  for(int l=0;l<22;l++){
   float r=30+l*7.5f+band(l)*25.f;
   for(int j=0;j<90;j++){
    float a=TAU*j/90.f,rr=r+4*sinf(a*8-t*1.5f+l*.32f)*(.15f+bass);
    fxPolarDot(rr,a,1.2f,fxInk(l*.053f+t*.033f,.45f+.45f*band(l)));
   }
  }return true;
 }
 case 10:{ // 18 Kaleidoscope Reactor: moving 12-fold paired dotted symmetry
  black();
  for(int k=0;k<12;k++){
   float a=TAU*k/12.f;
   for(int r=24;r<200;r+=10){
    float rr=r+15*sinf(r*.06f-t*1.4f)+band((r/8)%32)*32;
    float ang=a+.16f*sinf(r*.045f+t*.6f);
    fxPolarDot(rr,ang,2,fxInk(k*.078f+r*.0011f+t*.033f,.55f));
    fxPolarDot(rr,ang+TAU/24,2,fxInk(k*.078f+r*.0011f+t*.033f,.55f));
   }
  }return true;
 }
 case 11:{ // 19 Fibonacci Sunflower: fewer, double-size dots, full circular coverage
  black();
  const float gold=2.39996323f;
  for(int i=0;i<220;i++){
   float a=i*gold+t*.20f;
   float rr=15.f*sqrtf((float)i)*(1+.07f*energy)+band((i*11)%32)*12;
   float level=.24f+band((i*11)%32)*.68f;
   fxPolarDot(rr,a,(i%9)?2.5f:5.1f,fxInk(i*.0618f+t*.036f,level));
  }return true;
 }
 case 12:{ // 20 Infinity Tunnel: correct 10-sided perspective polygons
  black();
  for(int l=0;l<23;l++){
   float k=frac(l*.067f+t*(.12f+.034f*bass)),r=12+220*k*k;
   fxPoly(10,r,t*.10f+l*.11f,fxInk(.65f+k*.38f+t*.035f,.22f+.69f*k),k>.65f?2:1);
  }
  return true;
 }
 case 13:{ // 21 Cymatic Mandala: polar standing-wave nodes
  black();
  for(int j=0;j<128;j++){
   float a=TAU*j/128.f,th=a+t*.08f;
   for(int r=40;r<209;r+=7){
    float node=sinf(5*a+t*.7f)*cosf(r*.075f)+sinf(8*a-t*.3f)*bass;
    if(fabsf(node)>.72f)fxPolarDot(r+node*10,th,1.6f,fxInk(.56f+node*.14f+t*.022f,.52f+.35f*bass));
   }
  }return true;
 }
 case 17:{ // 23 Orbital Harmonics: rotated elliptical orbits instead of circles
  black();
  for(int o=0;o<13;o++){
   float tilt=o*.46f+t*.11f,r=46+o*11;
   float flatten=.31f+.12f*band((o*2)%32);
   for(int j=0;j<90;j+=2){
    float a=j*TAU/90.f,x=cosf(a)*r,y=sinf(a)*r*flatten;
    float xx=x*cosf(tilt)-y*sinf(tilt),yy=x*sinf(tilt)+y*cosf(tilt);
    dot(CX+xx,CY+yy,1.4f,fxInk(o*.083f+t*.042f,.54f+.3f*band(o*2)));
   }
   float a=t*(.3f+o*.03f)+o*1.1f,xx=cosf(a)*r,yy=sinf(a)*r*.37f;
   glow(CX+xx*cosf(tilt)-yy*sinf(tilt),CY+xx*sinf(tilt)+yy*cosf(tilt),
        2+int(band((o*2)%32)*4),fxInk(o*.083f+t*.042f,.74f));
  }return true;
 }
 case 19:{ // 25 Nebula Particles: browser 340 dispersed glowing particles
  black();
  for(int i=0;i<340;i++){
   float seed=i*2.399963f,drift=t*(.05f+(i%13)*.005f);
   float r=190*sqrtf(frac(i*.61803f)),a=seed+drift+energy*.7f;
   float scatter=14*sinf(i*.7f+t*.6f)+band(i%32)*40;
   float s=(i%29)?1.5f:3.f;
   fxPolarDot(r+scatter,a,s,fxInk(.69f+i*.005f+t*.025f,.24f+.64f*band(i%32)));
  }return true;
 }
 case 20:{ // 26 Warehouse House Duo: perspective floor grid + lasers BEHIND dancers
  black();
  // V16.9: remove overhead spiral/fan rays; keep floor, EQ lighting and dancers unchanged.
  // Floor: horizon at y310, perspective rays and compressed depth lines.
  for(int k=-10;k<=10;k++)line(CX,307,CX+k*47,H,tint(rgb(79,74,208),.38f));
  for(int j=1;j<=9;j++){float v=j/9.f,y=307+161*v*v;
   line(CX-235*v,y,CX+235*v,y,tint(rgb(155,85,235),.28f+.22f*v));
  }
  for(int k=0;k<18;k++){
   int x=8+k*25;int h=15+int(band(k+3)*120);
   cv.fillRect(x,H-15-h,13,h,fxInk(k*.055f+t*.016f,.20f+.28f*band(k+3)));
  }
  figure(150,303,1.f,beatClock+.2f,rgb(255,104,214));
  figure(318,303,1.f,beatClock+2.f,rgb(77,226,255));
  return true;
 }
 case 21:{ // 27 Ribbon Pirouette: two performers, overhead sweeping CLUB SPOTLIGHTS
  black();
  // Motorized club beams sweep back and forth independently of the dancers.
  // Three strands per lamp form a visible moving light cone; brighter on kicks.
  const float spotDrive=clampf(.28f+.62f*bass+.34f*energy);
  for(int beam=0;beam<9;beam++){
    float originX=42+beam*47,phase=t*(.57f+beam*.019f)+beam*1.31f;
    float sweep=sinf(phase)*(.48f+.27f*spotDrive);
    float targetX=originX+tan(fminf(1.15f,fmaxf(-1.15f,sweep)))*230.f;
    uint16_t col=fxInk(.56f+beam*.064f+t*.009f,.22f+.48f*spotDrive);
    for(int side=-2;side<=2;side++){
      line(originX+side*2,29,targetX+side*21,408,tint(col,side==0?.90f:.20f),side==0?2:1);
    }
    glow(originX,28,3+int(3*spotDrive),col);
    // Reflected floor pool follows the beam.
    dot(targetX,410,4+int(4*spotDrive),tint(col,.37f));
  }
  for(int j=0;j<2;j++){
   float baseX=j?330:138,p=beatClock*.20f+j*1.6f;
   // The spiraling ribbon strands are intentionally removed in V16.11.
   // V17: 60% larger figures; re-center them vertically so legs stay in view.
   figure(baseX+sinf(p)*8,337,1.36f,beatClock+j*1.5f,
          j?rgb(235,99,255):rgb(64,247,216));
  }
  return true;
 }
 default: return false;
 }
}
