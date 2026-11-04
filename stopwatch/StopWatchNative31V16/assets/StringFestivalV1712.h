// V17.13 — cinematic STRING / Patchwork Playhouse collection.
// Replaces the ten disliked V17.12 sketches, IDs 41..50, preserving all 23 originals.
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
 line(x,y,X,Y,tint(c,.19f*f),w+2);
 line(x,y,X,Y,tint(c,.91f*f),w);
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
static bool renderStringFestivalV1712(uint8_t id){
 if(id<41||id>50)return false;
 const float t=millis()*.001f;
 black();
 switch(id){
 case 41:{ // STITCHED EVENT HORIZON — dark star, warped twin accretion arcs.
  sfStarfield(t,44);
  // A dim radial space-time lens lies behind the bright, foreground disk.
  for(int i=0;i<10;i++)
   sfEllipse(CX,CY,203-i*9,130-i*5,.22f,sfViolet(),.08f+.018f*i,1);
  // Distant back half of the inclined accretion disk.
  for(int j=0;j<7;j++){
   float rx=207-j*10,ry=85-j*3;
   sfEllipse(CX,CY,rx,ry,-.20f,j%3?sfPink():sfGold(),
             .19f+.07f*j,2,PI_F,TAU);
  }
  // Deep black event horizon surrounded by three gravitational photon rings.
  dot(CX,CY,101,rgb(8,3,20));
  for(int i=7;i>=0;i--)
   circ(CX,CY,80+i*3,tint(i%2?sfPink():sfViolet(),.12f+.035f*(7-i)),2);
  dot(CX,CY,76,rgb(0,0,6));
  circ(CX,CY,81,sfGold(),2);
  circ(CX,CY,88,tint(sfPink(),.72f),3);
  circ(CX,CY,101,tint(sfCyan(),.24f),1);
  // Near-facing hot material covers the lower part of the shadow.
  for(int j=6;j>=0;j--){
   float rx=206-j*9,ry=84-j*2.2f;
   sfEllipse(CX,CY,rx,ry,-.20f,j%2?sfPink():sfGold(),
             .27f+.09f*(6-j),3,0,PI_F);
  }
  const float a=t*.35f;
  sfLight(CX+190*cosf(a),CY+74*sinf(a),sfGold(),4);
  // Clearly visible sewing in the orbital fabric: 14 tiny white dashes.
  for(int i=0;i<14;i++){
   float ang=i*TAU/14.f+t*.06f;
   float xx=CX+162*cosf(ang),yy=CY+57*sinf(ang);
   float xx2=CX+171*cosf(ang),yy2=CY+64*sinf(ang);
   line(xx,yy,xx2,yy2,tint(sfIvory(),.48f),2);
  }
  break;
 }
 case 42:{ // NEBULA NEEDLE — full-size metallic needle threads a star cloud.
  sfStarfield(t,40);
  // Large, layered nebula clouds; keep one recognizable needle silhouette.
  for(int k=0;k<10;k++){
   float a=k*2.39996f+t*.038f;
   int x=CX+113*cosf(a),y=CY+111*sinf(a);
   int r=55+(k%4)*22;
   dot(x,y,r,tint(k%2?sfViolet():sfBlue(),.09f));
   dot(x+9,y-13,r*.66f,tint(k%3?sfPink():sfCyan(),.12f));
  }
  // A ringed sewing portal at the base of the needle, atmospheric not flat.
  sfEllipse(235,238,141,119,.38f,sfViolet(),.45f,3);
  sfEllipse(235,238,130,109,.38f,sfCyan(),.23f,2);
  // Billowing loop of real magenta thread goes THROUGH the needle eye.
  sfBezier(71,409,55,256,190,327,242,202,sfPink(),.75f,3);
  sfBezier(242,202,272,108,353,76,389,110,sfPink(),.86f,3);
  sfBezier(389,110,429,160,401,246,343,247,sfPink(),.67f,3);
  // The silver sewing needle is thick with a shaded bevel and pointed end.
  // Its tip is near the lower-left, eye at upper-right.
  cv.fillTriangle(61,396,355,91,337,135,rgb(67,81,111));
  cv.fillTriangle(61,396,355,91,374,102,rgb(194,217,237));
  cv.fillTriangle(61,396,374,102,337,135,rgb(104,142,188));
  sfLine(71,385,358,99,sfIvory(),.88f,3);
  sfLine(81,390,346,120,sfBlue(),.45f,2);
  // Oversized oval eye, visibly threaded rather than a random glowing dot.
  sfEllipse(354,111,15,28,-.69f,rgb(6,16,39),1.f,7);
  sfEllipse(354,111,17,30,-.69f,sfIvory(),.92f,3);
  sfBezier(345,119,353,105,361,105,368,105,sfPink(),1.f,3);
  sfLight(357,89,sfIvory(),3);
  break;
 }
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
 case 45:{ // SATURN'S RED THREAD — glowing ringed gas giant tied to its moon.
  sfStarfield(t,33);
  const float a=t*.15f;
  // Faint orbital ring behind the planet, tilted as though in 3D.
  sfEllipse(CX,CY,214,72,-.24f,sfViolet(),.46f,5,PI_F,TAU);
  sfEllipse(CX,CY,198,58,-.24f,sfPink(),.32f,3,PI_F,TAU);
  // Planet fills nearly half the entire screen; shaded gaseous sphere.
  sfOrb(CX,CY,107,rgb(23,72,187),sfCyan());
  for(int dy=-80;dy<=80;dy+=16){
   float half=sqrtf(fmaxf(0.f,107.f*107-dy*dy));
   float wave=3*sinf(dy*.05f+t*.22f);
   sfLine(CX-half+10,CY+dy+wave,CX+half-10,CY+dy+wave,
          (dy%3)?sfBlue():sfViolet(),.16f+.08f*(.5f+.5f*sinf(t*.35f+dy)),2);
  }
  dot(CX-33,CY-37,24,tint(sfIvory(),.14f));
  // Front ring hides the lower half of sphere, creating real layer depth.
  sfEllipse(CX,CY,212,72,-.24f,sfGold(),.85f,6,0,PI_F);
  sfEllipse(CX,CY,195,59,-.24f,sfPink(),.62f,4,0,PI_F);
  for(int j=0;j<14;j++){
   float th=TAU*j/14.f;
   float x=CX+203*cosf(th),y=CY-49*sinf(th);
   float x2=CX+210*cosf(th),y2=CY-55*sinf(th);
   sfLine(x,y,x2,y2,sfIvory(),.55f,2);
  }
  // Moon connected by an actual continuous red-string arc from the ring.
  float mx=350+12*cosf(t*.48f),my=90+17*sinf(t*.48f);
  sfBezier(CX+176,CY-65,CX+230,96,mx+25,154,mx,my,sfRed(),.94f,3);
  sfOrb(mx,my,24,rgb(79,29,83),sfGold());
  sfLight(mx-9,my-8,sfIvory(),3);
  break;
 }
 case 46:{ // THE UNRAVELING — giant stitched curtain split to reveal a galaxy.
  sfStarfield(t,60);
  // The cosmos occupies the scar between two substantial cloth panels.
  for(int i=8;i>=0;i--){
   int r=118+i*13;
   dot(CX,CY,r,tint(i%2?sfPink():sfViolet(),.025f+.013f*(8-i)));
  }
  const float opening=64+31*(.5f+.5f*sinf(t*.46f));
  int XL[13],XR[13],YY[13];
  for(int j=0;j<13;j++){
   float y=18+j*36;
   float zig=(j%2?15.f:-14.f)+8*sinf(t*.42f+j*.64f);
   YY[j]=(int)y;
   XL[j]=(int)(CX-opening+zig);
   XR[j]=(int)(CX+opening-zig);
  }
  // Opaque material, large enough to read as fabric curtains from far away.
  for(int j=0;j<12;j++){
   cv.fillTriangle(0,YY[j],XL[j],YY[j],0,YY[j+1],rgb(32,15,84));
   cv.fillTriangle(0,YY[j+1],XL[j],YY[j],XL[j+1],YY[j+1],
                   j%2?rgb(46,17,98):rgb(61,25,112));
   cv.fillTriangle(W,YY[j],XR[j],YY[j],W,YY[j+1],rgb(23,41,86));
   cv.fillTriangle(W,YY[j+1],XR[j],YY[j],XR[j+1],YY[j+1],
                   j%2?rgb(21,64,112):rgb(25,41,96));
   // The ripped cloth edge is bright and uneven, not straight random lines.
   sfLine(XL[j],YY[j],XL[j+1],YY[j+1],sfPink(),.86f,4);
   sfLine(XR[j],YY[j],XR[j+1],YY[j+1],sfCyan(),.86f,4);
  }
  // Bold crossing stitches tug the two halves back together, partly undone.
  for(int j=2;j<11;j+=2){
   float xx=XL[j],xr=XR[j],yy=YY[j];
   float tension=.47f+.41f*(.5f+.5f*sinf(t*.63f+j));
   sfBezier(xx,yy,xr-23,yy-9,xx+23,yy+29,xr,yy+24,sfGold(),tension,3);
   sfLight(xx,yy,sfIvory(),2);
  }
  // One luminous needle climbs the seam, leading the repair.
  float p=frac(t*.089f);
  float ny=400-340*p;
  sfLine(CX-15,ny+20,CX+19,ny-23,sfIvory(),.9f,5);
  sfLight(CX+20,ny-24,sfGold(),4);
  break;
 }
 case 47:{ // MOBIUS WEAVE — wide 3-D single-sided ribbon with moving sewn hems.
  sfStarfield(t,24);
  const float turn=t*.24f;
  // Three very wide colored facets make the twist visible even at distance.
  for(int pass=0;pass<2;pass++){
   for(int k=0;k<90;k++){
    float u=(k+.0f)/90.f*TAU;
    float v=(k+1.f)/90.f*TAU;
    // Rear half drawn first; front half later. The ribbon twists 180 degrees.
    bool front=sinf(u+turn)>=0;
    if((pass==0)==front)continue;
    auto at=[&](float a,float q,float &x,float &y){
     const float r=126.f+q*57.f*cosf(a*.5f);
     x=CX+r*cosf(a+turn);
     y=CY+76.f*sinf(a+turn)+q*52.f*sinf(a*.5f);
    };
    float ax,ay,bx,by,cx,cy,dx,dy;
    at(u,-1,ax,ay);at(v,-1,bx,by);
    at(u,1,cx,cy);at(v,1,dx,dy);
    uint16_t col=(k/10)%3==0?rgb(77,41,163):(k/10)%3==1?rgb(27,145,188):rgb(190,56,146);
    if(front)col=(k/10)%2?rgb(184,66,215):rgb(79,215,232);
    cv.fillTriangle(ax,ay,bx,by,cx,cy,tint(col,.70f));
    cv.fillTriangle(bx,by,dx,dy,cx,cy,tint(col,.81f));
    // A single bright sewn edge prevents the band resembling thin geometry.
    if(k%2==0){
     sfLine(cx,cy,dx,dy,front?sfGold():sfCyan(),front?.82f:.42f,2);
     sfLine(ax,ay,bx,by,sfIvory(),front?.43f:.21f,1);
    }
    if(k%8==0&&front)sfLine(ax,ay,cx,cy,sfIvory(),.45f,2);
   }
  }
  // Traveling satin knot follows the single surface.
  float a=t*.48f;
  float px=CX+126*cosf(a+turn),py=CY+76*sinf(a+turn);
  sfLight(px,py,sfGold(),5);
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
 case 49:{ // SEW-CLEW'S LENS — magnifying glass revealing a key behind red yarn.
  sfStarfield(t,26);
  // Suspended investigation threads: background composition, not main subject.
  sfBezier(36,128,146,34,263,66,433,173,sfRed(),.39f,2);
  sfBezier(33,348,163,439,302,415,425,260,sfPink(),.33f,2);
  const float xx=211+11*sinf(t*.43f),yy=202+10*cosf(t*.41f);
  // Large physical silver lens handle cast behind the 240px glass circle.
  sfLine(xx+79,yy+90,398,406,rgb(71,79,118),.99f,28);
  sfLine(xx+80,yy+89,392,395,sfIvory(),.76f,8);
  sfLight(394,397,sfGold(),5);
  // Layered shaded glass occupies most of circular display.
  dot(xx,yy,129,rgb(41,33,87));
  dot(xx,yy,116,rgb(10,46,76));
  dot(xx-9,yy-10,97,rgb(8,33,71));
  for(int ring=0;ring<5;ring++)
   circ(xx,yy,118+ring*2,tint(ring%2?sfIvory():sfGold(),.37f+.10f*ring),2);
  // A large glowing missing-key silhouette inside magnified glass.
  const float kx=xx-13,ky=yy-31;
  sfEllipse(kx,ky-29,40,39,-.12f,sfGold(),.93f,11);
  dot(kx,ky-29,19,rgb(10,38,73));
  sfLine(kx+20,ky,kx+69,ky+105,sfGold(),1.f,19);
  sfLine(kx+20,ky,kx+69,ky+105,sfIvory(),.54f,5);
  sfLine(kx+53,ky+69,kx+86,ky+55,sfGold(),.89f,13);
  sfLine(kx+64,ky+96,kx+91,ky+83,sfGold(),.84f,12);
  // Glass glint sweeps across the key and reinforces curved lens.
  const float shift=30*sinf(t*.48f);
  sfEllipse(xx-48+shift,yy-50,31,79,-.45f,sfCyan(),.32f,3,2.8f,4.4f);
  sfLight(xx-85,yy-87,sfIvory(),3);
  break;
 }
 case 50:{ // THREADGLASS — full-height hourglass weaving a small galaxy in time.
  sfStarfield(t,24);
  // Substantial glass chambers: top wide, pinched in center, bottom wide.
  cv.fillTriangle(106,76,360,76,CX,CY,rgb(28,54,103));
  cv.fillTriangle(111,382,355,382,CX,CY,rgb(28,36,87));
  cv.fillTriangle(113,84,351,84,CX,CY,rgb(45,24,116));
  cv.fillTriangle(115,376,352,376,CX,CY,rgb(15,75,115));
  cv.fillTriangle(124,99,345,99,CX,CY,rgb(53,28,103));
  // The upper cosmic thread reservoir drains slowly; bottom builds a bright pool.
  float fill=.26f+.21f*(.5f+.5f*sinf(t*.19f));
  float topY=97+107*fill,botY=371-118*fill;
  cv.fillTriangle(127,topY,339,topY,CX,226,tint(sfPink(),.29f));
  cv.fillTriangle(127,botY,339,botY,CX,245,tint(sfCyan(),.37f));
  // Animated bundled filaments spiral through the narrow glass neck.
  for(int j=0;j<9;j++){
   const float phase=t*.63f+j*TAU/9.f;
   float x1=CX+86*cosf(phase),y1=132+16*sinf(phase);
   float x2=CX+8*cosf(phase+1.6f),y2=233;
   float x3=CX+86*cosf(phase+2.8f),y3=341+11*sinf(phase);
   sfBezier(x1,y1,CX+57*cosf(phase+.3f),166,x2-19,209,x2,y2,
            j%2?sfGold():sfPink(),.32f+.22f*(j%3),2);
   sfBezier(x2,y2,x2+17,259,CX+58*cosf(phase+2.5f),311,x3,y3,
            j%2?sfCyan():sfViolet(),.43f,2);
  }
  // The metallic casing and stitched seams make the hourglass readable.
  sfLine(110,75,356,75,sfGold(),.98f,9);
  sfLine(110,385,356,385,sfGold(),.98f,9);
  sfLine(110,75,CX-14,223,sfIvory(),.77f,5);
  sfLine(356,75,CX+14,223,sfIvory(),.77f,5);
  sfLine(CX-14,244,110,385,sfIvory(),.77f,5);
  sfLine(CX+14,244,356,385,sfIvory(),.77f,5);
  sfLine(CX-14,222,CX+14,222,sfGold(),.95f,4);
  sfLine(CX-14,245,CX+14,245,sfGold(),.95f,4);
  sfLight(CX,235,sfIvory(),4);
  break;
 }
 }
 return true;
}
