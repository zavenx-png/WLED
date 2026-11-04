// StopWatch V17.12 — STRING / Patchwork Playhouse festival collection.
// Ten independent 100% TIME-BASED animations, original IDs 41..50.
// No microphone, beat, FFT, audio levels, NVS writes, textures or heap use.
// Drawn directly into the existing C152 466x466 RGB565 PSRAM sprite.
// Designed as simple, high-contrast button-sized kinetic threadwork.
#pragma once

static inline uint16_t stCyan(){return rgb(78,244,222);}
static inline uint16_t stPink(){return rgb(255,91,182);}
static inline uint16_t stLilac(){return rgb(171,116,255);}
static inline uint16_t stGold(){return rgb(247,192,103);}
static inline uint16_t stIce(){return rgb(221,242,250);}
static inline uint16_t stMint(){return rgb(154,249,159);}

// Subtle dyed-thread halo, then a crisp 2-pixel luminous filament.
static inline void stCord(float x0,float y0,float x1,float y1,uint16_t c,float fade=1.f){
 line(x0,y0,x1,y1,tint(c,.19f*fade),3);
 line(x0,y0,x1,y1,tint(c,.91f*fade),2);
}
// One single yarn stroke, without extra halo for subdued background detail.
static inline void stFine(float x0,float y0,float x1,float y1,uint16_t c,float fade=.55f){
 line(x0,y0,x1,y1,tint(c,fade),1);
}
static inline void stKnot(float x,float y,uint16_t c,int radius=3){
 dot(x,y,radius+4,tint(c,.16f));
 dot(x,y,radius+1,tint(c,.42f));
 dot(x,y,radius,c);
}
// Discrete embroidery running-stitches along one physical seam.
static void stDashes(float x0,float y0,float x1,float y1,uint16_t c,int n,float phase=0){
 const float dx=x1-x0,dy=y1-y0;
 for(int j=0;j<n;j++){
  float a=(j+.14f)/n,b=(j+.67f)/n;
  float bright=.40f+.55f*(.5f+.5f*sinf(phase+j*.51f));
  stFine(x0+dx*a,y0+dy*a,x0+dx*b,y0+dy*b,c,bright);
 }
}
// Rotate a vector to make gentle, readable movement on the circular screen.
static inline void stRot(float x,float y,float a,float &ox,float &oy){
 const float co=cosf(a),si=sinf(a);
 ox=CX+x*co-y*si;oy=CY+x*si+y*co;
}
static bool renderStringFestivalV1712(uint8_t id){
 if(id<41||id>50)return false;
 // millis() drives each scene; never couple speed, brightness or geometry
 // to T(), beatClock, energy, FFT bins, mic state or tempo.
 const float t=millis()*.001f;
 black();
 switch(id){
 case 41:{ // 24 SILK ORBIT — satin ring segments braided into a slow breathing halo
  const uint16_t cs[3]={stCyan(),stPink(),stGold()};
  for(int layer=0;layer<3;layer++){
   const int n=50;
   const float rad=89+layer*47+5*sinf(t*.43f+layer*1.5f);
   const float spin=t*(layer%2?-0.17f:.14f)+layer*.63f;
   for(int j=0;j<n;j++){
    if((j+layer)%8==0)continue; // a recognisable tiny stitch gap
    const float a=TAU*j/n+spin,b=TAU*(j+.70f)/n+spin;
    const float fa=.70f+.23f*sinf(t*.71f+j*.20f+layer);
    stCord(CX+rad*cosf(a),CY+rad*sinf(a),
           CX+rad*cosf(b),CY+rad*sinf(b),cs[layer],fa);
   }
   float a=spin+t*.34f+layer*TAU/3;
   stKnot(CX+rad*cosf(a),CY+rad*sinf(a),cs[layer],4);
  }
  // The eye of the spool: clean, central silver opening.
  circ(CX,CY,31,tint(stIce(),.72f),2);
  circ(CX,CY,17,tint(stGold(),.48f),1);
  break;
 }
 case 42:{ // 25 CAT'S CRADLE — taut geometric cords shift between anchor fingers
  const float breath=7*sinf(t*.52f);
  float yl[6],yr[6];
  for(int i=0;i<6;i++){
   yl[i]=123+i*43+breath*sinf(t*.26f+i*.8f);
   yr[i]=123+i*43-breath*sinf(t*.26f+i*.8f);
  }
  for(int i=0;i<6;i++){
   const uint16_t c=i%2?stLilac():stCyan();
   stCord(83,yl[i],383,yr[5-i],c,.79f);
   if(i<5)stFine(83,yl[i],383,yr[(i+2)%6],stGold(),.37f);
  }
  // Two anchored cross-threads make the figure look woven, not like an EQ.
  stCord(105,114+breath,361,354-breath,stPink(),.73f);
  stCord(105,354-breath,361,114+breath,stPink(),.73f);
  for(int i=0;i<6;i++){
   stKnot(83,yl[i],i%2?stLilac():stCyan(),3);
   stKnot(383,yr[i],i%2?stCyan():stLilac(),3);
  }
  break;
 }
 case 43:{ // 26 NEEDLE CHASE — needle sewing a circular trail, no pixel-art props
  constexpr int N=64;
  const float head=frac(t*.085f)*N;
  for(int j=0;j<N;j++){
   const float a=TAU*j/N-.5f*PI_F;
   const float b=TAU*(j+.57f)/N-.5f*PI_F;
   const float x0=CX+170*cosf(a),y0=CY+144*sinf(a);
   const float x1=CX+170*cosf(b),y1=CY+144*sinf(b);
   const float dist=frac((head-j+N)/N)*N;
   const float alpha=dist<11?(.92f-.042f*dist):.29f;
   if(dist<11)stCord(x0,y0,x1,y1,stPink(),alpha);
   else stFine(x0,y0,x1,y1,stLilac(),alpha);
  }
  const float a=TAU*head/N-.5f*PI_F;
  const float x=CX+170*cosf(a),y=CY+144*sinf(a);
  float dx=-170*sinf(a),dy=144*cosf(a);
  float inv=1.f/sqrtf(dx*dx+dy*dy);dx*=inv;dy*=inv;
  // Smooth metallic needle, moving tangentially around the seam.
  stCord(x-dx*15,y-dy*15,x+dx*19,y+dy*19,stIce(),1.f);
  stKnot(x-dx*13,y-dy*13,stGold(),2);
  glow(x+dx*19,y+dy*19,2,stPink());
  break;
 }
 case 44:{ // 27 PUPPET STRINGS — five suspended satin charms obey invisible hands
  const uint16_t cols[5]={stCyan(),stPink(),stGold(),stLilac(),stMint()};
  stFine(94,86,372,86,stIce(),.44f);
  for(int j=0;j<5;j++){
   const float top=98+j*67;
   const float px=top+13*sinf(t*.61f+j*1.2f);
   const float py=256+27*sinf(t*.74f+j*.85f);
   stFine(top,86,px,py-26,cols[j],.76f);
   stKnot(top,86,cols[j],2);
   // Faceted fabric charms read clearly at arm's length (not tiny robots).
   const int x=(int)px,y=(int)py,rr=j==2?25:21;
   cv.fillTriangle(x,y-rr,x-rr,y,x,y+rr,tint(cols[j],.47f));
   cv.fillTriangle(x,y-rr,x+rr,y,x,y+rr,tint(cols[j],.76f));
   stFine(x,y-rr,x+rr,y,stIce(),.75f);
   stFine(x+rr,y,x,y+rr,stIce(),.48f);
   stKnot(x,y,stIce(),2);
  }
  break;
 }
 case 45:{ // 28 PATCHWORK WALTZ — luminous handmade quilt with moving seam glint
  const uint16_t colors[4]={stPink(),stGold(),stCyan(),stLilac()};
  const int x0=110,y0=110,S=82;
  for(int row=0;row<3;row++)for(int col=0;col<3;col++){
   int x=x0+col*S,y=y0+row*S;
   uint16_t c=colors[(row*2+col)%4];
   cv.fillTriangle(x+1,y+1,x+S-2,y+1,x+1,y+S-2,tint(c,.15f));
   cv.fillTriangle(x+S-2,y+1,x+S-2,y+S-2,x+1,y+S-2,tint(c,.28f));
   stFine(x,y,x+S,y,c,.48f);
   stFine(x,y,x,y+S,c,.48f);
   stDashes(x+9,y+9,x+S-9,y+S-9,stIce(),5,t*.65f+row+col);
  }
  stFine(x0+3*S,y0,x0+3*S,y0+3*S,stIce(),.56f);
  stFine(x0,y0+3*S,x0+3*S,y0+3*S,stIce(),.56f);
  // A needle-like highlight crawls around the central patch seam.
  const float u=frac(t*.10f)*4;
  int side=(int)u;float f=frac(u);
  float gx=CX,gy=CY;const float h=39.f;
  if(side==0){gx=CX-h+2*h*f;gy=CY-h;}
  else if(side==1){gx=CX+h;gy=CY-h+2*h*f;}
  else if(side==2){gx=CX+h-2*h*f;gy=CY+h;}
  else {gx=CX-h;gy=CY+h-2*h*f;}
  stKnot(gx,gy,stIce(),4);
  break;
 }
 case 46:{ // 29 UNRAVEL / RESTITCH — one traveling break unpicks a fine fabric seam
  const float cursor=105+255*(.5f+.5f*sinf(t*.54f));
  for(int row=0;row<6;row++){
   const float yy=122+row*45;
   const uint16_t c=row%2?stPink():stCyan();
   const float gap=17+7*sinf(t*.46f+row);
   for(int i=0;i<32;i++){
    const float xx=72+i*10.1f;
    float yA=yy+5*sinf(xx*.027f+row*.64f+t*.31f);
    float yB=yy+5*sinf((xx+7)*.027f+row*.64f+t*.31f);
    if(fabsf(xx-cursor)<gap)continue;
    const float b=.40f+.46f*(.5f+.5f*sinf(xx*.024f+t*.42f+row));
    stFine(xx,yA,xx+7,yB,c,b);
   }
   // Loose hanging curl precisely at the moving split in the thread.
   float ax=cursor,ay=yy;
   for(int k=1;k<=8;k++){
    float x=cursor+15*sinf(k*.48f+t*.85f+row*.8f);
    float y=yy+k*3.1f;
    stFine(ax,ay,x,y,c,.36f+.055f*k);ax=x;ay=y;
   }
   stKnot(cursor,yy,c,2);
  }
  break;
 }
 case 47:{ // 30 COSMIC LOOM — crossing warp and weft with rippling silk tension
  for(int k=0;k<10;k++){
   const float base=111+k*27;
   float px=base,py=99;
   const uint16_t c=k%2?stLilac():stCyan();
   for(int i=1;i<=16;i++){
    float y=99+i*16.5f;
    float x=base+7*sinf(y*.028f+t*.66f+k*.55f);
    stFine(px,py,x,y,c,.47f+.18f*sinf(t*.47f+k*.42f+i*.24f));
    px=x;py=y;
   }
  }
  for(int k=0;k<9;k++){
   const float base=117+k*28.5f;
   float px=101,py=base;
   const uint16_t c=k%2?stPink():stGold();
   for(int i=1;i<=16;i++){
    float x=101+i*16.5f;
    float y=base+5*sinf(x*.035f-t*.56f+k*.7f);
    stFine(px,py,x,y,c,.34f+.12f*sinf(t*.38f+i*.22f));
    px=x;py=y;
   }
  }
  // A single bright shuttle travels the warp diagonally.
  const float p=frac(t*.13f),xx=116+239*p;
  stKnot(xx,122+211*p,stIce(),3);
  break;
 }
 case 48:{ // 31 SPINDLE VORTEX — three smooth spiraling dyed yarn strands
  const uint16_t colors[3]={stPink(),stCyan(),stGold()};
  for(int strand=0;strand<3;strand++){
   const float shift=strand*TAU/3+t*.30f;
   float px=CX+13*cosf(shift),py=CY+13*sinf(shift);
   for(int k=1;k<=58;k++){
    const float p=(float)k/58;
    const float a=shift+p*TAU*2.5f;
    const float rad=13+177*p;
    const float xx=CX+rad*cosf(a),yy=CY+rad*sinf(a);
    stFine(px,py,xx,yy,colors[strand],.30f+.64f*p);
    if(k%13==0)stKnot(xx,yy,colors[strand],2);
    px=xx;py=yy;
   }
  }
  circ(CX,CY,16,tint(stIce(),.65f),2);
  break;
 }
 case 49:{ // 32 SEW-CLEW'S CLUES — living mystery-board thread connections
  const uint16_t colors[3]={stPink(),stGold(),stCyan()};
  float xx[7],yy[7];
  for(int i=0;i<7;i++){
   const float a=i*TAU/7-.5f*PI_F;
   xx[i]=CX+171*cosf(a);yy[i]=CY+167*sinf(a);
  }
  const uint8_t from[9]={0,1,2,4,6,0,3,5,1};
  const uint8_t to[9]  ={3,4,5,0,2,5,6,1,6};
  for(int j=0;j<9;j++){
   const int a=from[j],b=to[j];
   const uint16_t c=colors[j%3];
   stFine(xx[a],yy[a],xx[b],yy[b],c,j%3==0?.58f:.35f);
  }
  for(int j=0;j<7;j++){
   stKnot(xx[j],yy[j],colors[j%3],5);
   circ(xx[j],yy[j],9,tint(stIce(),.46f));
  }
  // Follow one clue at a time: a bright stitch glides along the chosen edge.
  const float journey=t*.43f;
  const int sel=((int)journey)%9;
  const float q=frac(journey);
  int a=from[sel],b=to[sel];
  stCord(xx[a],yy[a],xx[b],yy[b],colors[sel%3],1.f);
  stKnot(xx[a]+q*(xx[b]-xx[a]),yy[a]+q*(yy[b]-yy[a]),stIce(),5);
  break;
 }
 case 50:{ // 33 INFINITY KNOT — two softly woven figure-eight silk ribbons
  const uint16_t colors[2]={stCyan(),stPink()};
  for(int ribbon=0;ribbon<2;ribbon++){
   float px=0,py=0;
   for(int j=0;j<=100;j++){
    const float a=j*TAU/100.f;
    const float twist=a+t*.21f;
    const float x=174*sinf(a)/(1.f+.23f*cosf(a)*cosf(a));
    const float y=111*sinf(2*a);
    const float dx=x*cosf(t*.15f)-y*sinf(t*.15f);
    const float dy=x*sinf(t*.15f)+y*cosf(t*.15f);
    const float off=ribbon?6.f:-6.f;
    const float xx=CX+dx+off*sinf(twist);
    const float yy=CY+dy+off*cosf(twist);
    if(j)stFine(px,py,xx,yy,colors[ribbon],.58f+.32f*sinf(t*.53f+j*.04f+ribbon));
    if(j%20==0)stKnot(xx,yy,colors[ribbon],2);
    px=xx;py=yy;
   }
  }
  stKnot(CX,CY,stGold(),4);
  break;
 }
 }
 return true;
}
