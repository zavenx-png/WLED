// V17.14 — REBUILT cinematic STRING / Patchwork Playhouse collection.
// Replaces the three disliked V17.12 sketches, IDs 41..50, preserving all 23 originals.
// Full-display graphic subjects, layered satin shading, false 3-D perspective.
// TIME-ONLY: millis() animation. No FFT, microphone, tempo, touch, heap allocation.
// Each scene has a recognizable silhouette readable at a distance.
#pragma once

static inline uint16_t sfPink(){return rgb(255,72,165);}
static inline uint16_t sfViolet(){return rgb(143,83,255);}
static inline uint16_t sfBlue(){return rgb(36,155,255);}
static inline uint16_t sfCyan(){return rgb(63,237,244);}
static inline uint16_t sfGold(){return rgb(255,196,85);}
static inline uint16_t sfIvory(){return rgb(240,247,255);}
static inline uint16_t sfRed(){return rgb(255,58,92);}
static inline float sfClamp(float v){return clampf(v,0.f,1.f);}
static inline void sfLine(float x,float y,float X,float Y,uint16_t c,float f=1.f,int w=2){
 if(w<=3){ // lightweight fiber detail
  line(x,y,X,Y,tint(c,.16f*f),3);
  line(x,y,X,Y,tint(c,.90f*f),w);
  return;
 }
 // Physically wide shaded satin/material strokes, NOT a fake thick-line flag.
 // The base renderer's generic line() caps out at 3px, so broad puppet limbs,
 // needle handles and metal rims must be filled as proper quadrilaterals.
 const float dx=X-x,dy=Y-y;
 const float inv=1.f/fmaxf(1.f,sqrtf(dx*dx+dy*dy));
 const float nx=-dy*inv,ny=dx*inv;
 for(int pass=0;pass<2;pass++){
  const float r=(w+(pass==0?5:0))*.5f;
  const uint16_t ink=tint(c,pass==0?.17f*f:.87f*f);
  const float ax=x+nx*r,ay=y+ny*r,bx=x-nx*r,by=y-ny*r;
  const float cx=X+nx*r,cy=Y+ny*r,ex=X-nx*r,ey=Y-ny*r;
  cv.fillTriangle(ax,ay,bx,by,cx,cy,ink);
  cv.fillTriangle(bx,by,ex,ey,cx,cy,ink);
  cv.fillCircle((int)x,(int)y,(int)(r+.5f),ink);
  cv.fillCircle((int)X,(int)Y,(int)(r+.5f),ink);
 }
}
static inline void sfLight(float x,float y,uint16_t c,int r=4){
 dot(x,y,r+9,tint(c,.11f));
 dot(x,y,r+4,tint(c,.26f));
 dot(x,y,r,c);
}
static inline void sfStarfield(float t,int count=33){
 // Deterministic coordinates: no random flicker, visual noise or memory.
 for(int i=0;i<count;i++){
  const int x=25+(i*109+37)%421;
  const int y=25+(i*181+19)%416;
  if((x-CX)*(x-CX)+(y-CY)*(y-CY)>218*218)continue;
  const float a=.22f+.42f*(.5f+.5f*sinf(t*.38f+i*1.84f));
  dot(x,y,(i%13==0)?2:1,tint(i%5==0?sfGold():sfIvory(),a));
 }
}
static void sfEllipse(float x,float y,float rx,float ry,float rot,uint16_t c,float f=1.f,int thick=2,float lo=0,float hi=TAU){
 const int seg=(int)(64*(hi-lo)/TAU)+2;
 float px=0,py=0;
 const float cr=cosf(rot),sr=sinf(rot);
 for(int j=0;j<=seg;j++){
  const float a=lo+(hi-lo)*j/seg;
  const float xx=rx*cosf(a),yy=ry*sinf(a);
  const float X=x+xx*cr-yy*sr,Y=y+xx*sr+yy*cr;
  if(j)sfLine(px,py,X,Y,c,f,thick);
  px=X;py=Y;
 }
}
static void sfBezier(float ax,float ay,float bx,float by,float cx,float cy,float dx,float dy,uint16_t color,float fade=1.f,int w=2){
 float px=ax,py=ay;
 for(int j=1;j<=30;j++){
  float t=j/30.f,s=1.f-t;
  float x=s*s*s*ax+3*s*s*t*bx+3*s*t*t*cx+t*t*t*dx;
  float y=s*s*s*ay+3*s*s*t*by+3*s*t*t*cy+t*t*t*dy;
  sfLine(px,py,x,y,color,fade,w);
  px=x;py=y;
 }
}
static void sfOrb(int x,int y,int r,uint16_t shade,uint16_t light){
 // Layered circular material instead of a flat, pixel-art outline.
 for(int k=0;k<12;k++){
  float q=k/11.f;
  dot(x,y,(int)(r*(1.f-q*.94f)),tint(shade,.27f+.64f*q));
 }
 dot(x-r*.25f,y-r*.29f,r*.49f,tint(light,.47f));
 circ(x,y,r,tint(light,.89f),2);
 circ(x,y,r+4,tint(light,.18f));
}
static void sfStitch(float x0,float y0,float x1,float y1,uint16_t c,int n,float motion){
 for(int j=0;j<n;j++){
  float u=(j+.16f)/n,v=(j+.61f)/n;
  float a=.36f+.61f*(.5f+.5f*sinf(motion+j*.32f));
  line(x0+(x1-x0)*u,y0+(y1-y0)*u,
       x0+(x1-x0)*v,y0+(y1-y0)*v,tint(c,a),2);
 }
}
static bool renderStringFestivalV1714(uint8_t id){
 if(id!=43&&id!=44&&id!=48)return false;
 const float t=millis()*.001f;
 black();
 switch(id){
 case 43:{ // STAR MARIONETTE — a sentient stitched doll controlled from above.
  sfStarfield(t,24);
  const float sway=sinf(t*.87f)*15.f;
  const float left=149+sinf(t*.71f)*19.f,right=315-sinf(t*.72f+1.f)*20.f;
  const int hy=144+(int)(4*sinf(t*.60f));
  const float handY=303+11*sinf(t*.91f);
  // Threads stretch from offscreen puppeteer's hands to real articulated joints.
  sfLine(101,14,left,255,sfCyan(),.75f,2);
  sfLine(365,14,right,255,sfPink(),.75f,2);
  sfLine(CX,12,CX+sway*.25f,113,sfGold(),.75f,3);
  sfLine(82,20,left-20,handY,sfViolet(),.48f,1);
  sfLine(384,20,right+20,handY,sfViolet(),.48f,1);
  // Body: broad patchwork coat with colored panels and a cinched waist.
  cv.fillTriangle(171,205,295,205,233,342,rgb(50,20,104));
  cv.fillTriangle(171,205,231,225,233,342,rgb(142,48,190));
  cv.fillTriangle(296,205,234,225,233,342,rgb(44,118,163));
  cv.fillTriangle(172,205,128,282,203,249,rgb(255,66,144));
  cv.fillTriangle(294,205,263,249,337,280,rgb(63,205,241));
  // Patch panels are joined by bright seams, NOT an abstract web.
  sfStitch(233,224,233,338,sfIvory(),12,t);
  sfLine(182,211,151,263,sfPink(),.9f,6);
  sfLine(281,211,314,265,sfCyan(),.9f,6);
  sfLine(151,263,left-20,handY,sfPink(),1.f,7);
  sfLine(314,265,right+20,handY,sfCyan(),1.f,7);
  sfLight(left-20,handY,sfGold(),7);
  sfLight(right+20,handY,sfGold(),7);
  // Articulated legs in a dramatic dancing pose.
  sfLine(213,330,184,373,sfViolet(),.95f,11);
  sfLine(250,330,279,372,sfCyan(),.95f,11);
  sfLine(183,374,170,410,sfPink(),1.f,11);
  sfLine(279,374,309,402,sfBlue(),1.f,11);
  // Huge ivory button face, two unmistakable sewn-through eyes.
  dot(CX,hy,56,rgb(216,215,229));
  dot(CX-10,hy-11,43,rgb(249,235,222));
  circ(CX,hy,56,sfGold(),3);
  for(int d=-1;d<=1;d+=2){
   for(int v=-1;v<=1;v+=2){
    dot(CX+d*17,hy+v*16,6,rgb(19,15,42));
   }
  }
  sfLine(CX-17,hy-16,CX+17,hy+16,sfRed(),.89f,3);
  sfLine(CX+17,hy-16,CX-17,hy+16,sfRed(),.89f,3);
  sfLight(CX,hy+4,sfGold(),3);
  break;
 }
 case 44:{ // WARP LOOM — immersive perspective weaving machine, flying shuttle.
  sfStarfield(t,18);
  // Wide perspective loom frames, moving toward the observer.
  const float travel=frac(t*.15f);
  for(int j=8;j>=0;j--){
   float p=frac((j+travel)/9.f);
   float k=.14f+.86f*p*p;
   float hw=206*k,hh=173*k;
   uint16_t c=j%3==0?sfPink():j%3==1?sfBlue():sfGold();
   sfLine(CX-hw,CY-hh,CX+hw,CY-hh,c,.35f+.5f*k,3);
   sfLine(CX+hw,CY-hh,CX+hw,CY+hh,c,.36f+.52f*k,3);
   sfLine(CX+hw,CY+hh,CX-hw,CY+hh,c,.33f+.48f*k,3);
   sfLine(CX-hw,CY+hh,CX-hw,CY-hh,c,.34f+.49f*k,3);
   for(int u=-2;u<=2;u++){
    float xx=CX+hw*u/3.f;
    sfLine(xx,CY-hh,xx,CY+hh,sfCyan(),.08f+.15f*k,1);
   }
  }
  for(int k=-6;k<=6;k++){
   float ax=CX+k*35.f;
   sfLine(CX+k*9,CY-36,ax,CY-194,k%2?sfCyan():sfViolet(),.31f,2);
   sfLine(CX+k*9,CY+36,ax,CY+194,k%2?sfPink():sfGold(),.31f,2);
  }
  // A physical loom: broad metal top/bottom beams and upright silver columns.
  // The large filled structures add material and depth missing in the old sketch.
  sfLine(87,77,379,77,sfGold(),.78f,12);
  sfLine(87,387,379,387,sfGold(),.78f,12);
  sfLine(88,77,88,387,sfIvory(),.67f,9);
  sfLine(378,77,378,387,sfIvory(),.67f,9);
  for(int i=0;i<9;i++){
   float x=107+i*31;
   sfLine(x,98,x,366,i%2?sfCyan():sfViolet(),.43f,2);
  }
  // Substantial satin weft ribbons cross the warp, with a cyclic shuttle pass.
  for(int row=0;row<5;row++){
   const float y=128+row*53+10*sinf(t*.63f+row*.71f);
   sfBezier(105,y,174,y+16*sinf(row+t*.28f),309,y-14*sinf(row+t*.28f),363,y,
            row%2?sfPink():sfCyan(),.47f+row*.095f,4);
  }
  // Gold shuttle: a strong diamond silhouette flying across the portal.
  const float sh=CX+154*sinf(t*.9f);
  const float sy=CY+28*sinf(t*1.4f);
  cv.fillTriangle(sh-47,sy,sh,sy-18,sh+47,sy,sfGold());
  cv.fillTriangle(sh-47,sy,sh,sy+19,sh+47,sy,rgb(142,68,201));
  sfLine(sh-47,sy,sh+47,sy,sfIvory(),.94f,2);
  sfLight(sh,sy,sfIvory(),3);
  sfBezier(sh-41,sy+1,CX,sy+68,CX+81,sy-32,CX+166,sy+65,sfPink(),.8f,3);
  break;
 }
 case 48:{ // COSMIC PATCHWORK MOTH — huge sentient stitched wings and needle body.
  sfStarfield(t,16);
  const float flap=.91f+.09f*sinf(t*.75f);
  // Draw symmetrical opaque fabric panels, not outlines or tiny characters.
  for(int side=-1;side<=1;side+=2){
   float x0=CX+side*12;
   float tip=CX+side*207*flap;
   float mid=CX+side*143*flap;
   float low=CX+side*164*flap;
   cv.fillTriangle(x0,208,tip,76,mid,244,rgb(86,26,151));
   cv.fillTriangle(x0,208,tip,76,CX+side*75,155,rgb(238,64,166));
   cv.fillTriangle(x0,243,mid,244,low,388,rgb(19,145,174));
   cv.fillTriangle(x0,243,low,388,CX+side*57,350,rgb(31,54,144));
   cv.fillTriangle(CX+side*75,155,tip,76,mid,244,rgb(146,49,220));
   // Large shaded appliqué panels, not just four flat kite triangles.
   cv.fillTriangle(CX+side*75,164,CX+side*177*flap,105,
                   CX+side*126*flap,215,rgb(233,112,190));
   cv.fillTriangle(CX+side*66,282,CX+side*135*flap,335,
                   CX+side*74*flap,354,rgb(54,194,236));
   sfLine(CX+side*78,160,CX+side*151*flap,116,sfIvory(),.40f,2);
   sfLine(CX+side*75,283,CX+side*125*flap,345,sfIvory(),.40f,2);
   for(int stitch=1;stitch<=8;stitch++){
    float p=stitch/9.f;
    float px=x0+(tip-x0)*p,py=208+(76-208)*p;
    dot(px+side*6,py,2,tint(sfIvory(),.66f));
   }
   // 2 luminous eye spots evoke an actual moth's wings.
   sfOrb(CX+side*(106*flap),169,27,rgb(39,28,106),sfCyan());
   dot(CX+side*(106*flap),169,12,rgb(14,11,45));
   sfOrb(CX+side*(100*flap),307,21,rgb(106,35,145),sfGold());
   // Satin running stitches along outer edges; large triangles read from afar.
   sfLine(x0,208,tip,76,sfPink(),.67f,3);
   sfLine(tip,76,mid,244,sfGold(),.70f,3);
   sfLine(mid,244,low,388,sfCyan(),.81f,3);
   sfLine(low,388,x0,243,sfViolet(),.71f,3);
   sfLine(CX+side*70,154,CX+side*96,232,sfIvory(),.57f,2);
   sfLine(CX+side*60,285,CX+side*92,352,sfIvory(),.46f,2);
  }
  // Metallic sewing-needle thorax and button-head, with two antennae.
  sfLine(CX,146,CX,381,sfGold(),.99f,11);
  sfLine(CX-4,156,CX-4,350,sfIvory(),.65f,3);
  dot(CX,184,31,rgb(238,216,235));
  dot(CX-7,175,21,rgb(255,242,235));
  circ(CX,184,32,sfPink(),2);
  for(int i=-1;i<=1;i+=2){
   dot(CX+i*10,184,5,rgb(31,20,62));
   sfBezier(CX+i*13,163,CX+i*23,122,CX+i*31,108,CX+i*46,92,sfGold(),.69f,2);
  }
  sfLight(CX,384,sfCyan(),4);
  break;
 }
 }
 return true;
}
