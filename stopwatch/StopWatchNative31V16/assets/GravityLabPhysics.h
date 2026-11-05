#pragma once
/* Gravity Lab particle core — fixed storage, PSRAM allocated by host.
   Original port inspired by the MIT licensed STOPWATCH-FluidBox, omiya-bonsai,
   and esp32-fluidbox, V4C38. See THIRD_PARTY_NOTICES_GRAVITY.md. */
#include <cmath>
#include <cstdint>
#include <cstring>
namespace GravityPhysics {
constexpr int COUNT=400, GRID_X=13,GRID_Y=13,GRID_Z=3;
constexpr int CELLS=GRID_X*GRID_Y*GRID_Z, PAIR_MAX=14000;
constexpr float R=208.f,H=36.f,SMOOTH=39.f;
inline float cl(float x,float lo,float hi){return x<lo?lo:x>hi?hi:x;}
inline uint32_t hash(uint32_t x){x^=x>>16;x*=0x7feb352dU;x^=x>>15;x*=0x846ca68bU;x^=x>>16;return x;}
inline float rnd(uint32_t k){return (hash(k)&0xffffff)/16777215.f;}
struct Particle {float x,y,z,vx,vy,vz,px,py,pz,speed;};
struct Pair {uint16_t a,b;float nx,ny,nz,q;};
struct Fluid {
 Particle particles[COUNT];Pair pairs[PAIR_MAX];
 float density[COUNT],nearDensity[COUNT];int16_t head[CELLS],next[COUNT];
 float gravityX=0,gravityY=.5f,angular=0,shake=0,wellX=0,wellY=0,wellPower=0,wellRadius=150;
 float t=0,restDensity=4.0f;
 uint32_t overflow=0,invalid=0;int pairCount=0,scene=0;bool ready=false;
 void reset(uint32_t seed=1){
  ready=true;pairCount=0;
  for(int i=0;i<COUNT;i++){
   float theta=2.39996323f*i;
   float radius=sqrtf((i+.5f)/COUNT)*134.f;
   float x=cosf(theta)*radius+(rnd(seed+i*4)-.5f)*7;
   float y=sinf(theta)*radius+22+(rnd(seed+i*4+1)-.5f)*7;
   float z=(rnd(seed+i*4+2)-.5f)*50.f;
   particles[i]={x,y,z,0,0,0,x,y,z,0};
  }
 }
 void motion(float x,float y,float gyro,float shak){gravityX=cl(x,-1.55f,1.55f);gravityY=cl(y,-1.55f,1.55f);angular=cl(gyro,-500,500);shake=cl(shak,0,4);}
 void well(float x,float y,float amount,float radius=150){wellX=x;wellY=y;wellPower=cl(amount,-6,6);wellRadius=cl(radius,20,240);}
 void burst(float amount){
  for(int i=0;i<COUNT;i++){
   auto &p=particles[i];float rad=sqrtf(p.x*p.x+p.y*p.y+18.f);float f=(3.2f+3.4f*rnd(i*31+(int)(t*30)))*amount;
   p.vx+=p.x/rad*f;p.vy+=p.y/rad*f;p.vz+=(rnd(i*13+31)-.5f)*f;
  }
 }
 void stir(float x,float y,float dx,float dy){
  dx=cl(dx,-42,42);dy=cl(dy,-42,42);
  for(auto &p:particles){float xx=p.x-x,yy=p.y-y;float d2=xx*xx+yy*yy;
   if(d2<195*195){float wt=expf(-d2/9800.f);p.vx+=dx*.42f*wt;p.vy+=dy*.42f*wt;}
  }
 }
 int index(int x,int y,int z)const{return (z*GRID_Y+y)*GRID_X+x;}
 void buckets(){
  for(auto &v:head)v=-1;
  for(int i=0;i<COUNT;i++){
   const auto &p=particles[i];
   int x=(int)cl((p.x+226)/35,0,GRID_X-1),y=(int)cl((p.y+226)/35,0,GRID_Y-1),z=(int)cl((p.z+42)/28,0,GRID_Z-1);
   int k=index(x,y,z);next[i]=head[k];head[k]=i;
  }
 }
 void neighborhoods(){
  pairCount=0;memset(density,0,sizeof(density));memset(nearDensity,0,sizeof(nearDensity));
  for(int i=0;i<COUNT;i++){
   const auto &p=particles[i];int ix=(int)cl((p.x+226)/35,0,GRID_X-1),iy=(int)cl((p.y+226)/35,0,GRID_Y-1),iz=(int)cl((p.z+42)/28,0,GRID_Z-1);
   for(int zz=-1;zz<=1;zz++)for(int yy=-1;yy<=1;yy++)for(int xx=-1;xx<=1;xx++){
    int cx=ix+xx,cy=iy+yy,cz=iz+zz;if(cx<0||cy<0||cz<0||cx>=GRID_X||cy>=GRID_Y||cz>=GRID_Z)continue;
    for(int j=head[index(cx,cy,cz)];j>=0;j=next[j]){
     if(j<=i)continue;
     const auto &q=particles[j];float dx=q.x-p.x,dy=q.y-p.y,dz=(q.z-p.z)*.72f;
     float d2=dx*dx+dy*dy+dz*dz;if(d2>=SMOOTH*SMOOTH)continue;
     float d=sqrtf(d2+.01f),r=1-d/SMOOTH,r2=r*r,r3=r2*r;
     density[i]+=r2;density[j]+=r2;nearDensity[i]+=r3;nearDensity[j]+=r3;
     if(pairCount<PAIR_MAX)pairs[pairCount++]={(uint16_t)i,(uint16_t)j,dx/d,dy/d,dz/d,r};else overflow++;
    }
   }
  }
 }
 void wall(Particle& p){
  float rr=sqrtf(p.x*p.x+p.y*p.y);
  if(rr>R){float x=p.x/rr,y=p.y/rr;p.x=x*R;p.y=y*R;
   float outward=p.vx*x+p.vy*y;if(outward>0){p.vx-=outward*x*1.42f;p.vy-=outward*y*1.42f;}}
  if(p.z>H){p.z=H;p.vz=-fabsf(p.vz)*.42f;}
  if(p.z<-H){p.z=-H;p.vz=fabsf(p.vz)*.42f;}
 }
 void step(float dt,float audio=0,bool ambient=true){
  if(!ready)reset();
  dt=cl(dt,.008f,.055f);t+=dt;
  float f=cl(dt*38.f,.28f,1.55f);
  float swirl=angular*.00085f;
  for(int i=0;i<COUNT;i++){
   auto &p=particles[i];p.px=p.x;p.py=p.y;p.pz=p.z;
   float x=p.x,y=p.y,rr=sqrtf(x*x+y*y+28);
   float gx=gravityX+(ambient?.15f*sinf(t*.47f):0),gy=gravityY+(ambient?.14f*cosf(t*.37f):0);
   if(scene==3||scene==4||scene==5){float orb=(scene==5?1.3f:.70f)+audio*.23f;p.vx-=y/rr*orb*f;p.vy+=x/rr*orb*f;}
   if(scene==5){p.vx-=x/rr*.29f*f;p.vy-=y/rr*.29f*f;}
   if(scene==6){float pole=90*sinf(t*.27f),xx=pole-x,yy=-y,dd=sqrtf(xx*xx+yy*yy+300);
    p.vx+=xx/dd*.67f*f;p.vy+=yy/dd*.67f*f;}
   if(scene==8){gx*=.50f;gy*=1.50f;}
   p.vx+=(gx*.36f-y*swirl)*f;p.vy+=(gy*.36f+x*swirl)*f;
   p.vz-=p.z*.0017f*f;
   if(wellPower!=0){float xx=wellX-x,yy=wellY-y,dd=xx*xx+yy*yy+120,dist=sqrtf(dd);
    float attraction=wellPower*1.28f/(1+dd/(wellRadius*wellRadius));
    p.vx+=xx/dist*attraction*f;p.vy+=yy/dist*attraction*f;}
   float motion=(scene==0?.015f:.065f)+audio*.11f;
   p.vx+=sinf(t*.66f+i*.93f)*motion*f;p.vy+=cosf(t*.52f+i*1.2f)*motion*f;
   p.vx=cl(p.vx,-23,23);p.vy=cl(p.vy,-23,23);p.vz=cl(p.vz,-13,13);
   p.x+=p.vx*f;p.y+=p.vy*f;p.z+=p.vz*f;wall(p);
  }
  buckets();neighborhoods();
  for(int n=0;n<pairCount;n++){
   const auto &e=pairs[n];auto &a=particles[e.a],&b=particles[e.b];
   float ka=fmaxf(0,density[e.a]-restDensity)*.45f+nearDensity[e.a]*.65f;
   float kb=fmaxf(0,density[e.b]-restDensity)*.45f+nearDensity[e.b]*.65f;
   float correction=cl((ka+kb)*e.q*e.q*.43f,0,2.8f);
   a.x-=e.nx*correction;b.x+=e.nx*correction;
   a.y-=e.ny*correction;b.y+=e.ny*correction;
   a.z-=e.nz*correction*.7f;b.z+=e.nz*correction*.7f;
   float approach=(b.vx-a.vx)*e.nx+(b.vy-a.vy)*e.ny+(b.vz-a.vz)*e.nz;
   if(approach<0){float visc=approach*e.q*(scene==8?.14f:.065f);
    a.vx+=visc*e.nx;b.vx-=visc*e.nx;a.vy+=visc*e.ny;b.vy-=visc*e.ny;}
  }
  float damping=scene==8?.83f:.95f;
  for(int i=0;i<COUNT;i++){
   auto &p=particles[i];wall(p);
   p.vx=cl((p.x-p.px)*damping/f,-23,23);p.vy=cl((p.y-p.py)*damping/f,-23,23);
   p.vz=cl((p.z-p.pz)*damping/f,-13,13);
   p.speed=sqrtf(p.vx*p.vx+p.vy*p.vy+p.vz*p.vz);
   if(!std::isfinite(p.x)||!std::isfinite(p.y)||!std::isfinite(p.z)||!std::isfinite(p.speed)){
     invalid++;float angle=i*2.39996323f;p={cosf(angle)*60,sinf(angle)*60,0,0,0,0,0,0,0,0};}
  }
  wellPower*=.992f;
 }
};
} // namespace GravityPhysics
