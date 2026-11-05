#pragma once
/* Beat Laboratory V17.21 - host-testable beat and spectrum analysis.
 * Original implementation; uses the host firmware's existing FFT frames.
 * No audio task, SPI, Wi-Fi or speaker ownership. The host supplies 32 bands.
 */
#include <stdint.h>
#include <math.h>
#include <string.h>
namespace BeatLabDSP {
inline float lim(float v,float a,float b){return v<a?a:(v>b?b:v);}
inline float decay(float a,float b,float k){return a+(b-a)*k;}
struct Engine {
 float previous[32]={},smoothBands[32]={},trails[32]={};
 float smoothedFlux=0.01f,noise=0.008f,rawFlux=0, transient=0,lowPulse=0;
 float level=0,bass=0,mid=0,treble=0;
 float tempo=124.f,confidence=0.f,anchorMs=0.f; // ms as float: relative timestamps good for <49 days
 float visualClock=0.f,phase=0.f,pulse=0.f;
 uint32_t lastAudio=0,lastTick=0,lastOnset=0,lastEstimate=0;
 uint32_t onsets[48]={}; float strengths[48]={};int head=0,count=0;
 uint32_t taps[8]={};int tapsCount=0,tapsHead=0;
 bool manual=false,signal=false;
 uint8_t meter=4;
 float onsetFlash=0.f;
 void clear(){*this=Engine();}
 void setTempo(float v,bool tapMode){tempo=lim(v,58.f,204.f);manual=tapMode;confidence=tapMode?1.f:0.f;lastTick=0;}
 void tap(uint32_t now){
   if(tapsCount&&now-taps[(tapsHead+7)&7]>2100)tapsCount=0;
   taps[tapsHead]=now;tapsHead=(tapsHead+1)&7;tapsCount=tapsCount<8?tapsCount+1:8;
   if(tapsCount>=3){
     uint32_t vals[7]={};int n=0;
     for(int k=1;k<tapsCount;k++){
       const uint32_t a=taps[(tapsHead-1-k+8)&7],b=taps[(tapsHead-k+8)&7];
       uint32_t d=b-a;if(d>=280&&d<=1020)vals[n++]=d;
     }
     if(n>=2){for(int i=1;i<n;i++){uint32_t v=vals[i];int j=i-1;while(j>=0&&vals[j]>v){vals[j+1]=vals[j];j--;}vals[j+1]=v;}
       const uint32_t period=vals[n/2];setTempo(60000.f/period,true);
     }
   }
   anchorMs=(float)now;phase=0;visualClock=0;onsetFlash=1.f;lastOnset=now;
 }
 void autoTempo(){manual=false;confidence=0;tapsCount=0;lastEstimate=0;}
 void estimate(uint32_t now){
   if(manual||count<5)return;
   int n=count<28?count:28;
   float best=-1.f,runner=-1.f;int bestB=124;
   // Periodicity of strong onsets, supporting half/double-time energy and
   // resilient to a missed kick. Penalize candidates with no adjacent beats.
   for(int trial=72;trial<=194;trial++){
     float period=60000.f/trial,score=0,weight=0,adj=0;
     for(int i=0;i<n;i++){
       int ia=(head-1-i+48)%48;uint32_t a=onsets[ia];
       if(now-a>9000)break;
       for(int j=i+1;j<n;j++){
         int ib=(head-1-j+48)%48;uint32_t b=onsets[ib];
         uint32_t dt=a-b;if(dt>3300)break;if(dt<260)continue;
         float beats=dt/period,nearest=floorf(beats+.5f);
         if(nearest<1||nearest>6)continue;
         float err=fabsf(beats-nearest),s=lim(1.f-err*4.2f,0,1);
         float w=(.4f+.6f*lim(strengths[ia]*8.f,0,1))*(.5f+.5f*lim(strengths[ib]*8.f,0,1))/(1.f+.19f*(j-i));
         score+=s*s*w;weight+=w;
         if(nearest<1.45f)adj+=s*w;
       }
     }
     if(weight<.3f)continue;
     score=(score/weight)*(.75f+.25f*lim(adj/(weight*.20f),0.f,1.f));
     // gentle continuity: avoid bouncing between octave-equivalent candidates
     score-=.012f*lim(fabsf(trial-tempo)/80.f,0,1);
     if(score>best){runner=best;best=score;bestB=trial;}
     else if(score>runner&&abs(trial-bestB)>5)runner=score;
   }
   float evidence=lim((count-4.f)/9.f,0.f,1.f);
   float certainty=lim((best-.36f)*1.9f,0,1)*evidence;
   if(!signal)certainty=0;
   confidence=decay(confidence,certainty,.19f);
   if(certainty>.23f){
     float diff=bestB-tempo;
     // Faster reacquisition after silence, conservative rate in continuous sets.
     tempo+=lim(diff,-7.f,7.f)*(.15f+.17f*certainty);
     if(tempo<58)tempo=58;
     if(tempo>204)tempo=204;
   }
   lastEstimate=now;
 }
 void consume(const float in[32],float low,float middle,float high,float amplitude,uint32_t now){
   bass=decay(bass,lim(low,0,1),.3f);mid=decay(mid,lim(middle,0,1),.3f);treble=decay(treble,lim(high,0,1),.3f);
   level=decay(level,lim(amplitude,0,1),.27f);signal=level>.035f||low>.055f;
   float f=0.f;
   for(int i=0;i<32;i++){
     float v=lim(in[i],0,1.25f);
     float d=v-previous[i];
     float w=i<8?1.95f:(i<20?.95f:.66f);
     if(d>0)f+=d*w;
     previous[i]=v;
     smoothBands[i]=decay(smoothBands[i],v,v>smoothBands[i]?.44f:.12f);
     trails[i]=fmaxf(smoothBands[i],trails[i]*.959f);
   }
   rawFlux=f/32.f;
   smoothedFlux=decay(smoothedFlux,rawFlux,.075f);
   noise=decay(noise,rawFlux,rawFlux<noise?.035f:.007f);
   // Flux compared to local mean and noise, with a bass-sensitive guard.
   const float gate=fmaxf(.0042f,noise*2.3f+smoothedFlux*.78f);
   float punch=lim((rawFlux-gate)/(gate+.009f),0.f,2.5f);
   transient=decay(transient,punch,punch>transient?.61f:.15f);
   lowPulse=decay(lowPulse,lim(low*.5f+punch*.42f,0,1.2f),.28f);
   lastAudio=now;
   if(signal&&rawFlux>gate&&punch>.12f&&now-lastOnset>=270){
     onsets[head]=now;strengths[head]=lim(rawFlux*13.f,0,1);head=(head+1)%48;count=count<48?count+1:48;
     onsetFlash=1.f;lastOnset=now;
     if(!manual&&count>=6 && now-lastEstimate>950) estimate(now);
     // correct phase using low-frequency attack; do not hard snap on hats
     if(!manual&&confidence>.27f){
       float period=60000.f/tempo;
       if(anchorMs==0)anchorMs=(float)now;
       float ph=fmodf((now-anchorMs)/period,1.f);if(ph<0)ph+=1.f;
       float e=ph>.5f?ph-1.f:ph;
       anchorMs+=lim(e*period*.17f,-26.f,26.f);
     }else if(anchorMs==0)anchorMs=(float)now;
   }
 }
 void update(uint32_t now){
   if(!lastTick){lastTick=now;anchorMs=(float)now;return;}
   float dt=lim((now-lastTick)*.001f,0,.08f);lastTick=now;
   visualClock+=dt;
   float period=60000.f/lim(tempo,58,204);
   float cycles=(now-anchorMs)/period;
   phase=cycles-floorf(cycles);
   if(phase<0)phase+=1.f;
   float beatEnv=expf(-phase*8.2f);
   pulse=decay(pulse,beatEnv,.32f);
   onsetFlash*=expf(-dt*9.f);
   if(!manual){
     if(now-lastAudio>1800||!signal)confidence*=expf(-dt*.32f);
     if(now-lastOnset>12000){confidence*=expf(-dt*.35f);if(now-lastOnset>24000)tempo=decay(tempo,124.f,dt*.015f);}
   }
 }
};
static Engine engine;
} // BeatLabDSP