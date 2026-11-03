/*
 * StopWatch Font Laboratory V16.5: three independent implementations.
 * Each variant is built from the untouched V16.3 NativeApp.cpp, assets, and
 * renderer geometry. ONLY b5Char/b5Stamp are replaced in effects 1–5.
 *  1 = LovyanGFX native FreeSansBold + pushRotateZoomWithAA
 *  2 = FreeType Inter Black alpha atlas + PSRAM area-filtered mipmaps
 *  3 = native smooth vector wide lines/curves (no glyph bitmap)
 */
#pragma once
#ifndef STOPWATCH_FONT_TRIAL
#define STOPWATCH_FONT_TRIAL 1
#endif
#if STOPWATCH_FONT_TRIAL < 1 || STOPWATCH_FONT_TRIAL > 3
#error "STOPWATCH_FONT_TRIAL must be 1, 2, or 3"
#endif

// A. M5GFX built-in FreeSansBold font and the library's antialiased rotation.
#if STOPWATCH_FONT_TRIAL == 1
static lgfx::LGFX_Sprite trialTextSprite(&M5.Display);
static bool trialTextReady=false;
static bool trialNativeInit(){
 if(trialTextReady)return true;
 trialTextSprite.setColorDepth(16);
 trialTextSprite.setPsram(true);
 if(!trialTextSprite.createSprite(448,132))return false;
 trialTextSprite.setFont(&lgfx::fonts::FreeSansBold24pt7b);
 trialTextSprite.setTextDatum(middle_center);
 trialTextReady=true;
 return true;
}
static void trialNativeWord(const String &s,float x,float y,float width,float radians,uint16_t ink,float fade){
 if(s.length()==0||width<5||fade<.01f||!trialNativeInit())return;
 trialTextSprite.fillScreen(TFT_BLACK);
 trialTextSprite.setFont(&lgfx::fonts::FreeSansBold24pt7b);
 trialTextSprite.setTextSize(s.length()>8?1.12f:s.length()>6?1.55f:2.f);
 trialTextSprite.setTextDatum(middle_center);
 trialTextSprite.setTextColor(tint(ink,clampf(fade)),TFT_BLACK);
 trialTextSprite.drawString(s,224,66);
 float natural=trialTextSprite.textWidth(s);
 if(natural<1)return;
 // Native LovyanGFX filtered resampling (instead of custom nearest neighbor).
 float zoom=width/fminf(natural,435.f);
 trialTextSprite.setPivot(224,66);
 trialTextSprite.pushRotateZoomWithAA(&cv,x,y,radians*57.2957795f,zoom,zoom,TFT_BLACK);
}
static void trialChar(char ch,float x,float y,float scale,uint16_t ink,float radians){
 char cc[2]={ch,0};
 // Canvas's native letter height at 24pt, text size 2 is roughly 75px.
 trialNativeWord(String(cc),x,y,scale*75.f,radians,ink,1.f);
}
static void trialWord(const String &str,float x,float y,float width,float angle,uint16_t ink,float fade){
 trialNativeWord(str,x,y,width,angle,ink,fade);
}
#endif

// B. Independent FreeType font atlas: 102px source coverage + adaptive PSRAM mipmaps.
#if STOPWATCH_FONT_TRIAL == 2
#include "TrueTypeAtlas.h"
static uint8_t* trialFTPacked=nullptr;
static uint8_t* trialMip[FT_COUNT][3]={};
static bool trialFTAttempted=false,trialFTReady=false;
static bool trialFTInit(){
 if(trialFTAttempted)return trialFTReady;
 trialFTAttempted=true;
 trialFTPacked=(uint8_t*)heap_caps_malloc(FT_BYTES,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
 if(!trialFTPacked)return false;
 trialFTReady=b5Load(FT_DATA,trialFTPacked,FT_BYTES);
 Serial.printf("Font Trial B: TrueType atlas=%s, source=%u bytes\n",trialFTReady?"OK":"FAILED",(unsigned)FT_BYTES);
 return trialFTReady;
}
static bool trialFTGlyph(char cc,FTGlyph &g,int &index){
 for(int i=0;i<FT_COUNT;i++){
  memcpy_P(&g,&FT_GLYPHS[i],sizeof(g));
  if(g.ch==(uint8_t)cc){index=i;return true;}
 }
 return false;
}
static int trialFTW(const FTGlyph&g,int level){return max(1,((int)g.w+(1<<level)-1)>>level);}
static int trialFTH(const FTGlyph&g,int level){return max(1,((int)g.h+(1<<level)-1)>>level);}
static uint8_t trialFTReadBase(const FTGlyph &g,int x,int y){
 if(x<0||y<0||x>=g.w||y>=g.h)return 0;
 return (uint8_t)(b5Nib(trialFTPacked,((int)g.start*2)+y*g.w+x)*17);
}
static uint8_t trialFTRead(const FTGlyph &g,int gi,int level,int x,int y){
 if(x<0||y<0||x>=trialFTW(g,level)||y>=trialFTH(g,level))return 0;
 return level==0?trialFTReadBase(g,x,y):trialMip[gi][level-1][y*trialFTW(g,level)+x];
}
static bool trialFTMakeMip(const FTGlyph &g,int gi,int level){
 if(!level)return true;
 if(trialMip[gi][level-1])return true;
 if(!trialFTMakeMip(g,gi,level-1))return false;
 int w=trialFTW(g,level),h=trialFTH(g,level);
 uint8_t*p=(uint8_t*)heap_caps_malloc((size_t)w*h,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
 if(!p)return false;
 for(int yy=0;yy<h;yy++)for(int xx=0;xx<w;xx++){
  int v=0;
  for(int sy=0;sy<2;sy++)for(int sx=0;sx<2;sx++)
   v+=trialFTRead(g,gi,level-1,xx*2+sx,yy*2+sy);
  p[yy*w+xx]=(uint8_t)((v+2)/4);
 }
 trialMip[gi][level-1]=p;
 return true;
}
static uint8_t trialFTSample(const FTGlyph&g,int gi,int level,float x,float y){
 if(x<0||y<0||x>trialFTW(g,level)-1||y>trialFTH(g,level)-1)return 0;
 int x0=(int)x,y0=(int)y;
 float fx=x-x0,fy=y-y0;
 float v00=trialFTRead(g,gi,level,x0,y0);
 float v10=trialFTRead(g,gi,level,x0+1,y0);
 float v01=trialFTRead(g,gi,level,x0,y0+1);
 float v11=trialFTRead(g,gi,level,x0+1,y0+1);
 return (uint8_t)clampi((int)(v00*(1-fx)*(1-fy)+v10*fx*(1-fy)+v01*(1-fx)*fy+v11*fx*fy+.5f),0,255);
}
static void trialFTBlit(const FTGlyph&g,int gi,float cx,float cy,float scale,float angle,uint16_t color,float fade){
 if(scale<=.02f||fade<.015f)return;
 int level=scale<.20f?3:scale<.37f?2:scale<.68f?1:0;
 if(!trialFTMakeMip(g,gi,level))return;
 float ca=cosf(angle),sa=sinf(angle),hw=g.w*scale*.5f,hh=g.h*scale*.5f;
 float rx=fabsf(ca)*hw+fabsf(sa)*hh+1.5f,ry=fabsf(sa)*hw+fabsf(ca)*hh+1.5f;
 int l=max(0,(int)floorf(cx-rx)),r=min(W-1,(int)ceilf(cx+rx));
 int t=max(0,(int)floorf(cy-ry)),b=min(H-1,(int)ceilf(cy+ry));
 float mult=1.f/(scale*(1<<level));
 for(int y=t;y<=b;y++)for(int x=l;x<=r;x++){
  float dx=(x+.5f)-cx,dy=(y+.5f)-cy;
  float u=(dx*ca+dy*sa)*mult+(g.w*.5f)/(1<<level)-.5f;
  float v=(-dx*sa+dy*ca)*mult+(g.h*.5f)/(1<<level)-.5f;
  uint8_t a=trialFTSample(g,gi,level,u,v);
  if(a<7)continue;
  int ia=clampi((int)(a*fade),0,255);
  const int pos=y*W+x;fb[pos]=b5Mix(fb[pos],color,ia);
 }
}
static void trialChar(char ch,float x,float y,float scale,uint16_t ink,float radians){
 if(!trialFTInit())return;
 FTGlyph g;int gi;
 if(!trialFTGlyph(ch,g,gi))return;
 float sc=scale*78.f/83.f; // match native glyph cap-height, not source image width.
 trialFTBlit(g,gi,x,y,sc,radians,ink,1.f);
}
static void trialWord(const String &str,float x,float y,float width,float angle,uint16_t ink,float fade){
 if(str.length()==0||!trialFTInit())return;
 const int n=min((int)str.length(),12);
 float advance=0;
 for(int i=0;i<n;i++){FTGlyph g;int gi;advance+=(trialFTGlyph(str[i],g,gi)?g.advance:50);}
 if(advance<=1)return;
 float sc=width/advance,pen=-advance*.5f,ca=cosf(angle),sa=sinf(angle);
 for(int i=0;i<n;i++){
  FTGlyph g;int gi;
  if(trialFTGlyph(str[i],g,gi)){
   float ox=(pen+g.x+g.w*.5f)*sc,oy=(g.y+g.h*.5f)*sc;
   trialFTBlit(g,gi,x+ca*ox-sa*oy,y+sa*ox+ca*oy,sc,angle,ink,fade);
   pen+=g.advance;
  }else pen+=50;
 }
}
#endif

// C. Direct M5GFX vector drawing: real smooth lines and spots in the final
// framebuffer. No font masks, no resizing, and no raw RGB565 pixel writes.
#if STOPWATCH_FONT_TRIAL == 3
static inline void trialVSegment(float ax,float ay,float bx,float by,float cx,float cy,float sz,float radians,float thick,uint16_t c){
 float ca=cosf(radians),sa=sinf(radians);
 int x0=(int)roundf(cx+(ax*ca-ay*sa)*sz);
 int y0=(int)roundf(cy+(ax*sa+ay*ca)*sz);
 int x1=(int)roundf(cx+(bx*ca-by*sa)*sz);
 int y1=(int)roundf(cy+(bx*sa+by*ca)*sz);
 cv.drawWideLine(x0,y0,x1,y1,thick,c);
}
static void trialVArc(float cx0,float cy0,float rx,float ry,float a0,float a1,float x,float y,float sz,float angle,float thick,uint16_t ink){
 float p0x=cx0+cosf(a0)*rx,p0y=cy0+sinf(a0)*ry;
 for(int k=1;k<=12;k++){
  float a=a0+(a1-a0)*k/12.f;
  float px=cx0+cosf(a)*rx,py=cy0+sinf(a)*ry;
  trialVSegment(p0x,p0y,px,py,x,y,sz,angle,thick,ink);
  p0x=px;p0y=py;
 }
}
static void trialVChar(char ch,float cx,float cy,float height,float angle,uint16_t ink,float fade){
 if(height<2||fade<.015f)return;
 float s=height/11.f,rad=fmaxf(.6f,height*.085f);
 uint16_t c=tint(ink,fade);
 // Lowercase 'zaven' is the critical test text. Other letters get a clean
 // stroke-alphabet fallback; all names remain accepted, never fall back to V16.4.
 char cc=(ch>='A'&&ch<='Z')?ch-'A'+'a':ch;
 #define VSEG(a,b,d,e) trialVSegment(a,b,d,e,cx,cy,s,angle,rad,c)
 #define VARC(x,y,rx,ry,a,b) trialVArc(x,y,rx,ry,a,b,cx,cy,s,angle,rad,c)
 switch(cc){
 case 'z':VSEG(-4,-4,4,-4);VSEG(4,-4,-4,4);VSEG(-4,4,4,4);break;
 case 'a':VARC(-.3,0,3.5,4,0,TAU);VSEG(3.2,-4,3.2,4);break;
 case 'v':VSEG(-4,-4,0,4);VSEG(0,4,4,-4);break;
 case 'e':VARC(0,0,3.8,4,-.35f,TAU-.6f);VSEG(-3.6,0,3,0);break;
 case 'n':VSEG(-4,-4,-4,4);VARC(0,-.3,4,3.7,PI_F,TAU);VSEG(4,-.3,4,4);break;
 case 'm':VSEG(-4,-4,-4,4);VARC(-2.1,-.3,1.9,3.7,PI_F,TAU);VSEG(0,-.3,0,4);VARC(2,-.3,2,3.7,PI_F,TAU);VSEG(4,-.3,4,4);break;
 case 'o':VARC(0,0,3.9,4,0,TAU);break;
 case 'c':VARC(0,0,3.9,4,.7f,TAU-.7f);break;
 case 'u':VSEG(-4,-4,-4,1);VARC(0,1,4,3,0,PI_F);VSEG(4,-4,4,4);break;
 case 's':VARC(0,-2.1,3.7,2.2,-.2,PI_F*1.17f);VARC(0,2.1,3.7,2.2,-PI_F*.95f,PI_F*.97f);break;
 case 'r':VSEG(-3.5,-4,-3.5,4);VARC(.5,-1,3.5,3,PI_F,PI_F*1.88f);break;
 case 'l':VSEG(-1,-5,-1,4);break;
 case 'i':VSEG(0,-3,0,4);cv.drawSpot((int)roundf(cx),(int)roundf(cy-5*s),rad,c);break;
 case 't':VSEG(0,-5,0,4);VSEG(-3,-3,3,-3);break;
 case 'x':VSEG(-4,-4,4,4);VSEG(4,-4,-4,4);break;
 case 'h':VSEG(-4,-5,-4,4);VARC(0,-.3,4,3.7,PI_F,TAU);VSEG(4,-.3,4,4);break;
 case 'd':VARC(-.5,0,3.5,4,0,TAU);VSEG(3.2,-5,3.2,4);break;
 case 'p':VSEG(-4,-4,-4,6);VARC(0,-.8,4,3.3,0,TAU);break;
 case 'b':VSEG(-4,-5,-4,4);VARC(0,0,4,4,0,TAU);break;
 case 'g':VARC(-.3,0,3.5,4,0,TAU);VSEG(3.3,-4,3.3,6);break;
 case 'f':VSEG(0,-4,0,5);VSEG(-3,-3,4,-3);break;
 case 'k':VSEG(-4,-5,-4,4);VSEG(-3,0,4,-4);VSEG(-3,0,4,4);break;
 case 'y':VSEG(-4,-4,0,3);VSEG(4,-4,-2,6);break;
 case 'w':VSEG(-4,-4,-2,4);VSEG(-2,4,0,-1);VSEG(0,-1,2,4);VSEG(2,4,4,-4);break;
 case '0':VARC(0,0,3.7,4.6,0,TAU);break;
 case '1':VSEG(-1,-4,0,-5);VSEG(0,-5,0,4);break;
 case '2':VARC(0,-1.7,3.7,3,-PI_F*.96f,PI_F*.2f);VSEG(3,0,-4,4);VSEG(-4,4,4,4);break;
 case '3':VARC(0,-2,3.4,2.4,-1.7,1.7);VARC(0,2,3.4,2.4,-1.7,1.7);break;
 case '4':VSEG(-3,-4,-3,1);VSEG(-3,1,4,1);VSEG(3,-4,3,4);break;
 case '5':VSEG(4,-4,-3,-4);VSEG(-3,-4,-3,0);VARC(0,2,4,2.5,-2.8,2.1);break;
 case '6':VARC(0,1.5,3.8,3,0,TAU);VSEG(2,-4,-3,1);break;
 case '7':VSEG(-4,-4,4,-4);VSEG(4,-4,-1,4);break;
 case '8':VARC(0,-2,3.1,2.3,0,TAU);VARC(0,2,3.3,2.3,0,TAU);break;
 case '9':VARC(0,-1.5,3.8,3,0,TAU);VSEG(3,-1,-2,4);break;
 case '-':VSEG(-3,0,3,0);break;
 case '_':VSEG(-4,5,4,5);break;
 case '.':cv.drawSpot((int)cx,(int)(cy+4*s),rad,c);break;
 case ' ':break;
 default:VSEG(-3,-4,3,-4);VSEG(3,-4,3,4);VSEG(3,4,-3,4);VSEG(-3,4,-3,-4);break;
 }
 #undef VSEG
 #undef VARC
}
static void trialChar(char ch,float x,float y,float scale,uint16_t ink,float radians){
 trialVChar(ch,x,y,scale*71.f,radians,ink,1.f);
}
static void trialWord(const String &word,float x,float y,float width,float angle,uint16_t ink,float fade){
 int n=min((int)word.length(),12);if(n<1||fade<.015f)return;
 // Monospace vector strokes, proportionally sized to the specified word width.
 float pitch=width/n,sz=fminf(80.f,pitch*1.07f),ca=cosf(angle),sa=sinf(angle);
 for(int i=0;i<n;i++){
  float offset=((i+.5f)-n*.5f)*pitch;
  trialVChar(word[i],x+ca*offset,y+sa*offset,sz,angle,ink,fade);
 }
}
#endif