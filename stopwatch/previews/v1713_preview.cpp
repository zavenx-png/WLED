// Portable pixel-frame QA of the ACTUAL V17.13 C++ effect renderer.
// Stand-in LGFX software raster operations, no reinterpretation of scene math.
// Compile: g++ -O2 -std=c++17 v1713_preview.cpp -o preview && ./preview
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>
using std::max;using std::min;
static constexpr float PI_F=3.14159265358979323846f,TAU=6.2831853071795864769f;
static int W=466,H=466,CX=233,CY=233,R=230;
static float tm=0;
static uint32_t millis(){return (uint32_t)(tm*1000.f);}
static inline float clampf(float x,float a=0.f,float b=1.f){return max(a,min(b,x));}
static inline float frac(float v){return v-std::floor(v);}
static inline uint16_t rgb(int r,int g,int b){
 r=max(0,min(255,r));g=max(0,min(255,g));b=max(0,min(255,b));
 return (uint16_t)(((r>>3)<<11)|((g>>2)<<5)|(b>>3));
}
static inline uint16_t tint(uint16_t c,float f){
 f=clampf(f);return rgb((int)(((c>>11)&31)*255/31*f),(int)(((c>>5)&63)*255/63*f),(int)((c&31)*255/31*f));
}
static std::vector<uint16_t> fb(W*H,0);
static inline void pix(int x,int y,uint16_t c){if((unsigned)x<(unsigned)W&&(unsigned)y<(unsigned)H)fb[y*W+x]=c;}
static void drawLine(int x0,int y0,int x1,int y1,uint16_t c){
 int dx=abs(x1-x0),sx=x0<x1?1:-1,dy=-abs(y1-y0),sy=y0<y1?1:-1,err=dx+dy;
 for(;;){pix(x0,y0,c);if(x0==x1&&y0==y1)break;int e=2*err;if(e>=dy){err+=dy;x0+=sx;}if(e<=dx){err+=dx;y0+=sy;}}
}
static void fillCircle(int cx,int cy,int radius,uint16_t c){
 if(radius<0)return;
 int y0=max(0,cy-radius),y1=min(H-1,cy+radius);
 for(int y=y0;y<=y1;y++){
  const int dy=y-cy;const int d=(int)std::sqrt((double)max(0,radius*radius-dy*dy));
  int x0=max(0,cx-d),x1=min(W-1,cx+d);
  for(int x=x0;x<=x1;x++)pix(x,y,c);
 }
}
static void drawCircle(int cx,int cy,int r,uint16_t c){
 if(r<0)return;
 int x=r,y=0,err=0;
 while(x>=y){
  pix(cx+x,cy+y,c);pix(cx+y,cy+x,c);pix(cx-y,cy+x,c);pix(cx-x,cy+y,c);
  pix(cx-x,cy-y,c);pix(cx-y,cy-x,c);pix(cx+y,cy-x,c);pix(cx+x,cy-y,c);
  y++;if(err<=0)err+=2*y+1; if(err>0){x--;err-=2*x+1;}
 }
}
static inline int edge(int ax,int ay,int bx,int by,int px,int py){return (px-ax)*(by-ay)-(py-ay)*(bx-ax);}
static void fillTriangle(int ax,int ay,int bx,int by,int cx,int cy,uint16_t c){
 int x0=max(0,min({ax,bx,cx})),x1=min(W-1,max({ax,bx,cx}));
 int y0=max(0,min({ay,by,cy})),y1=min(H-1,max({ay,by,cy}));
 int ori=edge(ax,ay,bx,by,cx,cy);
 if(ori==0){drawLine(ax,ay,bx,by,c);drawLine(ax,ay,cx,cy,c);return;}
 for(int y=y0;y<=y1;y++)for(int x=x0;x<=x1;x++){
  int d1=edge(ax,ay,bx,by,x,y),d2=edge(bx,by,cx,cy,x,y),d3=edge(cx,cy,ax,ay,x,y);
  if(ori>0?(d1>=0&&d2>=0&&d3>=0):(d1<=0&&d2<=0&&d3<=0))pix(x,y,c);
 }
}
struct DummySprite {
 void fillTriangle(int ax,int ay,int bx,int by,int cx,int cy,uint16_t c){::fillTriangle(ax,ay,bx,by,cx,cy,c);}
 void fillCircle(int x,int y,int r,uint16_t c){::fillCircle(x,y,r,c);}
}cv;
static void black(){std::fill(fb.begin(),fb.end(),0);}
static void dot(float x,float y,float r,uint16_t c){fillCircle((int)x,(int)y,(int)r,c);}
static void line(float x,float y,float X,float Y,uint16_t c,int w=1){
 drawLine(x,y,X,Y,c);
 if(w>1)drawLine(x,y+1,X,Y+1,c);
 if(w>2)drawLine(x+1,y,X+1,Y,c);
}
static void circ(float x,float y,int r,uint16_t c,int w=1){
 for(int j=0;j<w;j++)drawCircle((int)x,(int)y,r+j,c);
}
#include "../StopWatchNative31V16/assets/StringFestivalV1713.h"
static void writePpm(const std::string &filename){
 std::ofstream f(filename,std::ios::binary);f<<"P6\n"<<W<<" "<<H<<"\n255\n";
 for(int y=0;y<H;y++)for(int x=0;x<W;x++){
  const int dx=x-CX,dy=y-CY;
  uint16_t c=(dx*dx+dy*dy<=229*229)?fb[y*W+x]:0;
  unsigned char p[3]={(unsigned char)(((c>>11)&31)*255/31),
                      (unsigned char)(((c>>5)&63)*255/63),
                      (unsigned char)((c&31)*255/31)};
  f.write((char*)p,3);
 }
}
int main(){
 for(int id=41;id<=50;id++){
  for(int at=0;at<2;at++){
   tm=at?12.f:2.5f;
   if(!renderStringFestivalV1713(id))return 2;
   char name[80];std::snprintf(name,sizeof(name),"scene_%02d_frame_%d.ppm",id-17,at);
   writePpm(name);
   std::printf("%s\n",name);
  }
 }
 return 0;
}
