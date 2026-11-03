/*
 * Club StopWatch — Native31 V17 · Favorites / Scenes / Adaptive Power (ESP32-S3 / M5Stack StopWatch C152)
 * Native rendering, compact precomputed art, 31 curated effects, automatic BPM clock, A+B remote.
 * Hardware-targeted source; not a conversion of browser Canvas APIs.
 * M5Stack board package >=3.3.8, M5Unified>=0.2.19, M5GFX>=0.2.21.
 * Licensing: new original source; see README for validation limitations.
 */
#include <Arduino.h>
#include <M5Unified.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_now.h>
#include <Preferences.h>
#include <math.h>
#include <esp_heap_caps.h>
#include "assets/NativeTextures.h"

constexpr uint8_t VIS_COUNT=31, Z_COUNT=28, REMOTE_PAGES=5;
constexpr int NFFT=512, FS=16000, NBANDS=32;
constexpr float PI_F=3.14159265358979323846f, TAU=6.28318530717958647692f;
static const char *const EFFECT_NAMES[VIS_COUNT] = {
 "zaven Chromatic EQ",
 "zaven Psychedelic Halo",
 "zaven Kaleidoscope",
 "zaven Round Resonator",
 "zaven Fibonacci Spiral",
 "zaven Hyperspace Portal",
 "zaven Electric Aurora",
 "zaven Flowing Halo",
 "zaven Orbit Portal",
 "Prism EQ",
 "Psychedelic Bloom",
 "Vapor Scope",
 "Matrix Phosphor",
 "Frequency Towers",
 "Radial Spectrum",
 "Heartbeat Neon",
 "Round Resonator",
 "Kaleidoscope Reactor",
 "Fibonacci Sunflower",
 "Infinity Tunnel",
 "Cymatic Mandala",
 "Liquid Chrome",
 "Orbital Harmonics",
 "Quantum Moire",
 "Nebula Particles",
 "Warehouse House Duo",
 "Ribbon Pirouette",
 "Robot Shuffle",
 "Living Cybernetic Eye",
 "Impossible Geometry",
 "Personal Dance Aura",
};
static constexpr uint8_t ORIGINAL_EFFECT_ID[VIS_COUNT]={23,24,25,26,27,28,29,30,31,1,2,3,4,6,7,8,9,10,11,12,13,16,17,18,19,20,21,22,32,37,40};
static inline uint8_t originalEffect(uint8_t i);
static constexpr uint8_t V12_INDEX_TO_V13[VIS_COUNT]={9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,0,1,2,3,4,5,6,7,8,28,29,30};
static inline uint8_t migrateEffectIndex(uint8_t n){return n<VIS_COUNT?V12_INDEX_TO_V13[n]:0;}


static const char *const Z_NAMES[Z_COUNT]={
 "Nokia Snake","Chunchun Harness","Neural Pulse","Plasma Veins","Electric Organism","Gravity Well",
 "Wormhole","Rainbow Fracture","Starlight Nervous System","Phoenix Flight","Comet Storm","Deep Current",
 "Tidal Interference","Ember Drift","Orbital Waltz","Silk Ribbon","Twin Moons","Liquid Mercury",
 "Echo Chamber","Prism Cascade","Gravity Lens","Quiet Storm","Infinite Corridor","Meteor Circuit",
 "Meteor Twin Sync","Meteor Ricochet","Meteor Crossover","Meteor Echo Rain"
};
// Native WLED mode IDs follow Z effects in the on-watch remote.
// Names are a compact best-effort subset; the mode number is the authoritative value.
static const char *const WLED_NAMES[] = {
 "Solid","Blink","Breathe","Wipe","Wipe Random","Random Colors","Sweep","Dynamic",
 "Colorloop","Rainbow","Scan","Scan Dual","Fade","Theater","Theater Rainbow","Running","Saw",
 "Twinkle","Dissolve","Dissolve Rnd","Sparkle","Sparkle Dark","Sparkle+","Strobe","Strobe Rainbow",
 "Strobe Mega","Blink Rainbow","Android","Chase","Chase Random","Chase Rainbow","Chase Flash",
 "Chase Flash Rnd","Rainbow Runner","Colorful","Traffic Light","Sweep Random","Chase 2",
 "Aurora","Stream","Scanner","Lighthouse","Fireworks","Rain","Tetrix","Fire Flicker",
 "Gradient","Loading","Rolling Balls","Fairy","Two Dots","Fairytwinkle","Running Dual",
 "Image","Chase 3","Tri Wipe","Tri Fade","Lightning","ICU","Multi Comet","Scanner Dual",
 "Stream 2","Oscillate","Pride 2015","Juggle","Palette","Fire 2012","Colorwaves","Bpm",
 "Fill Noise","Noise 1","Noise 2","Noise 3","Noise 4","Colortwinkles","Lake","Meteor",
 "Copy Segment","Railway","Ripple","Twinklefox","Twinklecat","Halloween Eyes","Solid Pattern",
 "Solid Pattern Tri","Spots","Spots Fade","Glitter","Candle","Fireworks Starburst",
 "Fireworks 1D","Bouncing Balls","Sinelon","Sinelon Dual","Sinelon Rainbow","Popcorn",
 "Drip","Plasma","Percent","Ripple Rainbow","Heartbeat","Pacifica","Candle Multi",
 "Solid Glitter","Sunrise","Phased","Twinkleup","Noise Pal","Sine","Phased Noise",
 "Flow","Chunchun","Dancing Shadows","Washing Machine","Rotozoomer","Blends","TV Simulator",
 "Dynamic Smooth","Spaceships","Crazy Bees","Ghost Rider","Blobs","Scrolling Text","Drift Rose",
 "Distortion Waves","Soap","Octopus","Waving Cell","Pixels","Pixelwave","Juggles",
 "Matripix","Gravimeter","Plasmoid","Puddles","Midnoise","Noisemeter","Freqwave","Freqmatrix",
 "GEQ","Waterfall","Freqpixels","Binmap","Noisefire","Puddlepeak","Noisemove","Noise2D",
 "Perlin Move","Ripple Peak","Firenoise","Squared Swirl","PacMan","DNA","Matrix",
 "Metaballs","Freqmap","Gravcenter","Gravcentric","Gravfreq","DJ Light","Funky Plank",
 "Shimmer","Pulser","Blurz","Drift","Waverly","Sun Radiation","Colored Bursts",
 "Julia","Poolnoise","Twister","Cellular","Game Of Life","Tartan","Polar Lights",
 "Swirl","Lissajous","Frizzles","Plasma Ball","Flow Stripe","Hiphotic","Sindots",
 "DNA Spiral","Black Hole","Wavesins","Rocktaves","Akemi","PS Volcano","PS Fire",
 "PS Fireworks","PS Vortex","PS Fuzzy Noise","PS Ballpit","PS Box","PS Attractor",
 "PS Impact","PS Waterfall","PS Spray","Particles GEQ","Particle Center GEQ",
 "PS Ghost Rider","PS Blobs","PS Drip","PS Pinball","PS Dancing Shadows","PS Fireworks 1D",
 "PS Sparkler","PS Hourglass","PS Spray 1D","PS Balance","PS Chase","PS Starburst",
 "PS 1D GEQ","PS Fire 1D","PS Sonic Stream","PS Sonic Boom","PS Springy","PS Galaxy",
 "Color Clouds","Slow Transition"
};
constexpr int WLED_COUNT=sizeof(WLED_NAMES)/sizeof(WLED_NAMES[0]);
static const char *const PAL_NAMES[]={
 "Default","Random Cycle","Primary Color","Based on Primary","Set Colors","Based on Set",
 "Party","Cloud","Lava","Ocean","Forest","Rainbow",
 "Rainbow Bands","Sunset","Rivendell","Breeze","Red & Blue","Yellowout",
 "Analogous","Splash","Pastel","Sunset 2","Beech","Vintage",
 "Departure","Landscape","Beach","Sherbet","Hult","Hult 64",
 "Drywet","Jul","Grintage","Rewhi","Tertiary","Fire",
 "Icefire","Cyane","Light Pink","Autumn","Magenta","Magred",
 "Yelmag","Yelblu","Orange & Teal","Tiamat","April Night","Orangery",
 "C9","Sakura"
};
constexpr uint8_t PAL_COUNT=sizeof(PAL_NAMES)/sizeof(PAL_NAMES[0]);

lgfx::LGFX_Sprite cv(&M5.Display);
Preferences prefs;
int16_t pcm[NFFT]={}; float xr[NFFT]={},xi[NFFT]={};
float bins[NBANDS]={},peak[NBANDS]={},scopeY[160]={};
float bass=0,mids=0,highs=0,energy=0,kick=0,flux=0,oldBass=0;
float autoGain=1,noiseFloor=.003f,micGain=1.35f;
static float fftWindow[NFFT];static uint16_t binBegin[NBANDS],binEnd[NBANDS];
uint32_t onsets[40]={};int onsetHead=0,onsetNum=0;
float bpm=125.f,beatClock=0.f,beatConfidence=0.f;
bool haveSamples=false,pendingMic=false;
uint32_t audioStamp=0,beatStamp=0,frameStamp=0,renderStamp=0,micStamp=0;
int W=466,H=466,CX=233,CY=233,R=230;
uint8_t effectID=0,brightness=52,profile=1;
uint8_t rxChannel=1,remotePage=0,remoteSelected=0,selectedPalette=0,remoteBrightness=155,remoteSpeed=128,remoteIntensity=128;
int remotePreset=1;bool remote=false,radioReady=false,radioPending=false,radioSuccess=false;
uint32_t remoteSeq=0,remoteLast=0;
uint8_t gainPct[VIS_COUNT]={};
// Each effect is either AUTO or explicitly MANUAL until the user presses
// RETURN AUTO. A bitmask occupies only 8 bytes of Preferences storage.
uint64_t micManualMask=0;
static bool gainIsManual(uint8_t id){return id<VIS_COUNT && (micManualMask&(1ULL<<id));}
static void setGainManual(uint8_t id,bool manual){
 if(id>=VIS_COUNT)return;
 if(manual)micManualMask|=(1ULL<<id);else micManualMask&=~(1ULL<<id);
 prefs.putBytes("micmodes",&micManualMask,sizeof(micManualMask));
}
String userText[VIS_COUNT];String blueRobot="",pinkRobot="";static const String DEFAULT_WORD="zaven";
enum Overlay : uint8_t {NONE,NAME,GAIN,BRIGHTNESS,RHYTHM,LIBRARY};
Overlay overlay=NONE;
String inputName="";uint8_t robotField=0;bool keyboardUpper=false;
uint8_t candidateBrightness=52;float candidateBpm=125;
// Persistent, compact NVS-backed favorites and complete per-scene parameters.
static constexpr uint64_t FAVORITE_DEFAULT=(1ULL<<4)|(1ULL<<9)|(1ULL<<19)|(1ULL<<27)|(1ULL<<28)|(1ULL<<29)|(1ULL<<30);
static constexpr uint32_t SCENE_MAGIC=0x3253435A; // "ZCS1" scene format version
static const char *const SCENE_LABELS[4]={"WARMUP","PEAK HOUR","HYPNOTIC","AFTERHOURS"};
struct SceneData {
 uint32_t magic=0;
 uint8_t effect=0,gain=135,bright=55,profile=1;
 uint8_t palette=0,harnessBrightness=155,speed=128,intensity=128,remoteFx=0;
 uint8_t adaptive=1,remoteIdle=20;
 char name[13]={},blue[13]={},pink[13]={};
};
SceneData scenes[4]={};
uint64_t favoriteBits=FAVORITE_DEFAULT;
bool favoritesOnly=false,adaptiveRender=true,displayBlank=false;
uint8_t libraryPage=0,remoteIdleSecs=20;
uint32_t lastRemoteActivity=0;
static bool isFavorite(uint8_t i){return i<VIS_COUNT && (favoriteBits&(1ULL<<i));}
static void storeLibrary(){prefs.putBytes("favs",&favoriteBits,sizeof(favoriteBits));prefs.putBool("favonly",favoritesOnly);}
static void vibration(uint8_t strength,int ms);
static inline int clampi(int v,int a,int b);
static void saveNames();
static void saveScene(uint8_t slot){
 if(slot>=4)return;SceneData v;v.magic=SCENE_MAGIC;v.effect=effectID;v.gain=gainPct[effectID];v.bright=brightness;v.profile=profile;
 v.palette=selectedPalette;v.harnessBrightness=remoteBrightness;v.speed=remoteSpeed;v.intensity=remoteIntensity;v.remoteFx=remoteSelected;
 v.adaptive=adaptiveRender?1:0;v.remoteIdle=remoteIdleSecs;
 userText[effectID].toCharArray(v.name,sizeof(v.name));blueRobot.toCharArray(v.blue,sizeof(v.blue));pinkRobot.toCharArray(v.pink,sizeof(v.pink));
 scenes[slot]=v;prefs.putBytes((String("scn")+String(slot)).c_str(),&v,sizeof(v));
 // Keep original SceneData binary layout compatible with V11-V15.
 prefs.putBool((String("scna")+String(slot)).c_str(),gainIsManual(effectID));
 vibration(170,72);
}
static void loadScene(uint8_t slot){
 if(slot>=4||scenes[slot].magic!=SCENE_MAGIC||scenes[slot].effect>=VIS_COUNT)return;
 const SceneData &v=scenes[slot];effectID=v.effect;brightness=clampi(v.bright,8,100);profile=v.profile%3;
 gainPct[effectID]=clampi(v.gain,20,250);micGain=gainPct[effectID]/100.f;
 setGainManual(effectID,prefs.getBool((String("scna")+String(slot)).c_str(),false));
 userText[effectID]=String(v.name);blueRobot=String(v.blue);pinkRobot=String(v.pink);
 selectedPalette=clampi(v.palette,0,PAL_COUNT-1);remoteBrightness=v.harnessBrightness;remoteSpeed=v.speed;
 remoteIntensity=v.intensity;remoteSelected=clampi(v.remoteFx,0,Z_COUNT+WLED_COUNT-1);
 adaptiveRender=v.adaptive!=0;remoteIdleSecs=v.remoteIdle==0?0:clampi(v.remoteIdle,10,120);favoritesOnly=false;
 prefs.putUChar("effect",effectID);prefs.putUChar("bright",brightness);prefs.putUChar("profile",profile);
 prefs.putBool("adaptive",adaptiveRender);prefs.putUChar("idle",remoteIdleSecs);prefs.putBytes("gains",gainPct,sizeof(gainPct));
 storeLibrary();saveNames();M5.Display.setBrightness((uint8_t)(brightness*255/100));
 // No automatic transmissions from a recalled scene: avoid unexpected harness changes.
 if(!remote)setCpuFrequencyMhz(profile==2?160:240);
 overlay=NONE;vibration(180,85);
}
static void clearScene(uint8_t slot){if(slot>=4)return;scenes[slot]=SceneData();prefs.remove((String("scn")+String(slot)).c_str());vibration(140,70);}
static uint8_t nextFavorite(uint8_t start,int dir){
 for(int n=0;n<VIS_COUNT;n++){start=(start+VIS_COUNT+(dir<0?-1:1))%VIS_COUNT;if(isFavorite(start))return start;}return start;
}
static uint8_t powerFrameCap();

uint32_t buzzUntil=0;bool buzzEnabled=true;
uint32_t keyADown=0,keyBDown=0;bool keyChord=false,keyAHeld=false,keyBHeld=false;
int touchStartX=0,touchStartY=0,touchLastX=0,touchLastY=0;
uint32_t touchStart=0,lastArtTouchMs=0;float userSpin=0;
bool touchDown=false;
bool wakeKeyConsumed=false;
uint16_t *fb=nullptr;

static const uint8_t BROADCAST[]={0xff,0xff,0xff,0xff,0xff,0xff};
portMUX_TYPE ackMux=portMUX_INITIALIZER_UNLOCKED;
struct Ack {uint32_t seq=0;uint8_t status=255,power=0,brightness=0,preset=0;bool ready=false;};
volatile bool newAck=false;Ack rxAck;

static inline float clampf(float v,float a=0.f,float b=1.f){return v<a?a:(v>b?b:v);}
static inline int clampi(int v,int a,int b){return v<a?a:(v>b?b:v);}
static inline float frac(float v){return v-floorf(v);}
static inline float smooth(float a,float b,float t){return a+(b-a)*clampf(t);}
static inline float sq(float a){return a*a;}
static inline uint16_t rgb(int r,int g,int b){return (uint16_t)((clampi(r,0,255)>>3)<<11 | (clampi(g,0,255)>>2)<<5 |(clampi(b,0,255)>>3));}
static inline uint16_t tint(uint16_t c,float k){k=clampf(k);return rgb(((c>>11)&31)*255/31*k,((c>>5)&63)*255/63*k,(c&31)*255/31*k);}
static inline uint16_t blend(uint16_t a,uint16_t b,float t){t=clampf(t);return rgb((((a>>11)&31)*(1-t)+((b>>11)&31)*t)*255/31,
 (((a>>5)&63)*(1-t)+((b>>5)&63)*t)*255/63,((a&31)*(1-t)+(b&31)*t)*255/31);}
static inline uint16_t hue(float v){float h=frac(v);float r=clampf(fabsf(h*6-3)-1);float g=clampf(2-fabsf(h*6-2));float b=clampf(2-fabsf(h*6-4));return rgb(int(255*r),int(255*g),int(255*b));}
static inline float phase4(){return frac(beatClock/4.f);}
static inline float phase8(){return frac(beatClock/8.f);}
static inline float phase16(){return frac(beatClock/16.f);}
static inline float beatPhase(){return frac(beatClock);}
static inline float beatPulse(){return expf(-beatPhase()*8.f);}
static inline float T(){return beatClock*60.f/125.f;} // BPM-linked art time; smooth 125 BPM fallback in silence
static inline float sinBeat(float cycles=1){return sinf(beatClock*TAU*cycles);}
static inline void pix(int x,int y,uint16_t c){if((unsigned)x<(unsigned)W&&(unsigned)y<(unsigned)H) cv.drawPixel(x,y,c);}
static inline void dot(float x,float y,float r,uint16_t c){if(r>0)cv.fillCircle((int)x,(int)y,(int)r,c);}
static inline void line(float x,float y,float x2,float y2,uint16_t c,int thick=1){
 cv.drawLine((int)x,(int)y,(int)x2,(int)y2,c);
 if(thick>1){cv.drawLine((int)x,(int)y+1,(int)x2,(int)y2+1,c);if(thick>2)cv.drawLine((int)x+1,(int)y,(int)x2+1,(int)y2,c);}
}
static inline void circ(float x,float y,int r,uint16_t c,int thick=1){if(r<1)return;for(int i=0;i<thick;i++)cv.drawCircle((int)x,(int)y,r+i,c);}
static inline void glow(float x,float y,int r,uint16_t c){dot(x,y,r+8,tint(c,.12f));dot(x,y,r+4,tint(c,.33f));dot(x,y,r,c);}
static inline float band(int b){return bins[clampi(b,0,NBANDS-1)];}
static inline void textC(const String& s,int x,int y,uint16_t c,int size=2){cv.setTextDatum(middle_center);cv.setTextColor(c);cv.drawString(s,x,y,size);}
static inline void black(){cv.fillScreen(TFT_BLACK);}
static inline uint8_t originalEffect(uint8_t i){return ORIGINAL_EFFECT_ID[i%VIS_COUNT];}
static inline bool isTextEffect(uint8_t i){uint8_t id=originalEffect(i);return id>=23&&id<=31;}
static inline const String& artName(uint8_t i){return userText[i].length()?userText[i]:DEFAULT_WORD;}
static inline void glyph(char ch,int x,int y,uint16_t c,int size=4){char b[2]={ch,0};cv.setTextDatum(middle_center);cv.setTextColor(c);cv.drawString(b,x,y,size);}
static inline void vibration(uint8_t strength=145,int ms=38){if(!buzzEnabled)return;M5.Power.setVibration(strength);buzzUntil=millis()+ms;}
static void vibrationTick(){if(buzzUntil&&(int32_t)(millis()-buzzUntil)>=0){M5.Power.setVibration(0);buzzUntil=0;}}

static void fft512(){
 for(int i=1,j=0;i<NFFT;i++){int bit=NFFT>>1;for(;j&bit;bit>>=1)j^=bit;j^=bit;if(i<j){float a=xr[i];xr[i]=xr[j];xr[j]=a;a=xi[i];xi[i]=xi[j];xi[j]=a;}}
 for(int len=2;len<=NFFT;len<<=1){float a=-TAU/len,ur=cosf(a),ui=sinf(a);
  for(int i=0;i<NFFT;i+=len){float wr=1,wi=0;for(int k=0;k<len/2;k++){
   int p=i+k,q=p+len/2;float tr=wr*xr[q]-wi*xi[q],ti=wr*xi[q]+wi*xr[q];xr[q]=xr[p]-tr;xi[q]=xi[p]-ti;xr[p]+=tr;xi[p]+=ti;
   float nw=wr*ur-wi*ui;wi=wr*ui+wi*ur;wr=nw;
  }}
 }
}
// Auto-only musical clock: low-band kick onsets, weighted phase coherence, 8s window.
// Distinct non-neighbor tempo peaks reject random strikes and false double-speed locks.
static void estimateTempo(uint32_t now){
 const int n=clampi(onsetNum,0,25);
 if(n<6)return;
 const uint32_t anchor=onsets[(onsetHead+39)%40];
 float scores[151]={};float best=0;
 int bestB=125;
 // Narrower tempo range optimizes for 90–165 BPM house/techno while retaining
 // a continuously animated 125 BPM fallback when no musical beat is stable.
 for(int candidate=90;candidate<=165;candidate++){
  float period=60000.f/candidate,xx=0,yy=0,weight=0;
  for(int j=0;j<n;j++){
   uint32_t stamp=onsets[(onsetHead-1-j+40)%40];
   uint32_t ageMs=anchor-stamp;
   if(ageMs>9500)continue;
   float age=ageMs/period;
   float w=1.f/(1.f+j*.13f);
   xx+=cosf(TAU*age)*w;yy+=sinf(TAU*age)*w;weight+=w;
  }
  float coherence=sqrtf(xx*xx+yy*yy)/(weight+1e-4f);
  // Require multiple consistent onsets, not a brief coincidence.
  scores[candidate-90]=coherence;
  if(coherence>best){best=coherence;bestB=candidate;}
 }
 float runner=0;
 for(int candidate=90;candidate<=165;candidate++)
  if(abs(candidate-bestB)>=6)runner=fmaxf(runner,scores[candidate-90]);
 float distinct=clampf((best-runner)*3.2f);
 float evidence=clampf((n-4.f)/8.f);
 float confidence=clampf((best-.36f)*1.75f)*(.60f+.40f*distinct)*evidence;
 beatConfidence=smooth(beatConfidence,confidence,.17f);
 if(confidence>.37f){
  float difference=bestB-bpm;
  // Smoothly settle to an observed tempo over a few beats.
  bpm+=clampf(difference,-5.0f,5.0f)*(.10f+.16f*confidence);
 }
}
static void registerOnset(uint32_t now){
 if((now-beatStamp)<250)return;
 beatStamp=now;onsets[onsetHead]=now;onsetHead=(onsetHead+1)%40;onsetNum=clampi(onsetNum+1,0,40);
 if(onsetNum>=6 && (onsetNum%2==0))estimateTempo(now);
 if(beatConfidence>.43f){float f=beatPhase();float err=f<.5f?-f:1-f;beatClock+=clampf(err,-.11f,.11f)*.09f;}
}

// V14: adaptive *microphone* sensitivity for effects with audio-scaled geometry.
// IDs refer to the V13 curated menu; 0 = fixed geometry (no automatic changes).
// Spectrum: upward/ radial bars; bass: widening rings/portals; energy: reactive
// expansions; wave: the oscilloscope trace. Rendering functions are unchanged.
enum AutoMicType : uint8_t {MIC_FIXED=0,MIC_SPECTRUM=1,MIC_BASS=2,MIC_ENERGY=3,MIC_WAVE=4};
static constexpr uint8_t AUTO_MIC_TYPES[VIS_COUNT]={
 MIC_SPECTRUM, MIC_BASS, MIC_ENERGY, MIC_SPECTRUM, MIC_BASS,
 MIC_BASS, MIC_ENERGY, MIC_ENERGY, MIC_BASS,
 MIC_SPECTRUM, MIC_ENERGY, MIC_ENERGY, MIC_WAVE, MIC_SPECTRUM,
 MIC_SPECTRUM, MIC_FIXED, MIC_ENERGY, MIC_ENERGY, MIC_ENERGY,
 MIC_BASS, MIC_BASS, MIC_ENERGY, MIC_SPECTRUM, MIC_BASS,
 MIC_FIXED, MIC_FIXED, MIC_FIXED, MIC_FIXED, MIC_FIXED,
 MIC_FIXED, MIC_FIXED
};
static_assert(sizeof(AUTO_MIC_TYPES)/sizeof(AUTO_MIC_TYPES[0])==VIS_COUNT,"auto mic profile count");
static constexpr float AUTO_MIC_TARGET=.80f;
// V15: peak-window microphone AGC. Use the ACTUAL ungained control inputs
// that drive each effect (FFT bins/bass/energy/wave) rather than an arbitrary
// hard minimum. About 85% of the 150-ms music peaks is aimed at 80% travel.
// This never changes any effect's drawing code or the automatic beat engine.
static constexpr int AUTO_BUCKETS=32;
struct AutoMicHistory {
 float peaks[AUTO_BUCKETS]={},bucketMax=0;
 uint8_t count=0,next=0;
 uint32_t bucketAt=0,lastSignal=0,lastAdjust=0;
 bool capped=false;
};
static AutoMicHistory micHist[VIS_COUNT];
// No timer: manual sensitivity persists until an explicit return to Auto.
static void resetMicHistory(AutoMicHistory &h){
 h.count=h.next=0;h.bucketMax=0;h.bucketAt=0;h.capped=false;
}
static void adaptMicSensitivity(float rms,float spectrum,float lowBass,float wave){
 if(remote||effectID>=VIS_COUNT||AUTO_MIC_TYPES[effectID]==MIC_FIXED)return;
 const uint32_t now=millis();
 if(gainIsManual(effectID))return;
 if(!isfinite(rms)||!isfinite(spectrum)||!isfinite(lowBass)||!isfinite(wave))return;
 AutoMicHistory &h=micHist[effectID];
 // V16: quiet MUSIC must increase sensitivity. V15's level gate
 // rejected gentle passages before any gain learning occurred. Only
 // near-absolute digital silence is ignored; low-level noise can reach
 // the hardware gain cap if no reliable music is present.
 const bool trulySilent=rms<.00003f && spectrum<.00035f && lowBass<.00035f && wave<.0001f;
 if(trulySilent){
   if(h.lastSignal&&now-h.lastSignal>2600)resetMicHistory(h);
   return;
 }
 const uint8_t kind=AUTO_MIC_TYPES[effectID];
 const float base=kind==MIC_SPECTRUM?spectrum:
                  kind==MIC_BASS?lowBass:
                  kind==MIC_WAVE?fmaxf(wave,rms*autoGain*.52f):rms*autoGain*2.f;
 if(!isfinite(base)||base<.0001f)return;
 if(h.lastSignal&&now-h.lastSignal>9000)resetMicHistory(h);
 h.lastSignal=now;
 if(!h.bucketAt)h.bucketAt=now;
 h.bucketMax=fmaxf(h.bucketMax,base);
 if(now-h.bucketAt>=150){
   h.peaks[h.next]=h.bucketMax;
   h.next=(h.next+1)%AUTO_BUCKETS;
   if(h.count<AUTO_BUCKETS)h.count++;
   h.bucketMax=base;
   h.bucketAt=now;
 }
 if(h.count<7||now-h.lastAdjust<370)return;
 h.lastAdjust=now;
 float sorted[AUTO_BUCKETS];
 for(int i=0;i<h.count;i++)sorted[i]=h.peaks[i];
 for(int i=1;i<h.count;i++){const float v=sorted[i];int j=i-1;
   while(j>=0&&sorted[j]>v){sorted[j+1]=sorted[j];j--;}
   sorted[j+1]=v;
 }
 const float ref=fmaxf(.000001f,sorted[(int)((h.count-1)*.85f)]);
 const float current=ref*micGain;
 if(current>=.72f&&current<=.90f){h.capped=false;return;}
 // Existing native NVS stores per-effect gains as uint8, range 20..250%.
 // Preserve this layout; the 250% limit may prevent reaching 80% with
 // very low signals. Never falsely claim calibration when at the cap.
 const float wanted=clampf(AUTO_MIC_TARGET*100.f/ref,20.f,250.f);
 const float old=(float)gainPct[effectID];
 const float step=clampf((wanted-old)*.32f,-14.f,14.f);
 if(fabsf(step)>=.6f){
   gainPct[effectID]=(uint8_t)clampi((int)roundf(old+step),20,250);
   micGain=gainPct[effectID]/100.f;
 }
 h.capped=(wanted>=249.f&&ref*2.5f<.72f)||(wanted<=21.f&&current>.90f);
 // No Preferences writes in the audio callback, so no extra flash wear.
}

static void processMic(){
 float power=0,powerBass=0,powerMid=0,powerHi=0;
 float strongest[5]={},ungainedBass=0,rawWave=0;
 for(int i=0;i<NFFT;i++){
  float s=(float)pcm[i]/32768.f;xr[i]=s*fftWindow[i];xi[i]=0;power+=s*s;
  if(i<160){scopeY[i]=s;rawWave=fmaxf(rawWave,fabsf(s));}
 }
 fft512();
 for(int k=0;k<NBANDS;k++){
  int low=binBegin[k],hi=binEnd[k];
  float mag=0;for(int b=low;b<hi;b++)mag+=sqrtf(xr[b]*xr[b]+xi[b]*xi[b]);mag/=(hi-low);
  const float ungained=mag*.055f*autoGain;
  if(k<8)ungainedBass+=ungained/8.f;
  for(int j=0;j<5;j++)if(ungained>strongest[j]){
   for(int i=4;i>j;i--) strongest[i]=strongest[i-1];
   strongest[j]=ungained;
   break;
  }
  float v=clampf(ungained*micGain,0,1.25f);
  bins[k]=smooth(bins[k],v,v>bins[k]?.55f:.17f);peak[k]=fmaxf(bins[k],peak[k]*.978f);
  if(k<8)powerBass+=v;else if(k<21)powerMid+=v;else powerHi+=v;
 }
 float raw=sqrtf(power/NFFT);noiseFloor=smooth(noiseFloor,raw,raw<noiseFloor?.025f:.001f);
 float bandReference=0;for(float strength:strongest)bandReference+=strength*.2f;
 adaptMicSensitivity(raw,bandReference,ungainedBass,rawWave*autoGain);
 float desired=.19f/fmaxf(raw-noiseFloor,.001f);autoGain=smooth(autoGain,clampf(desired,.18f,9.f),.013f);
 bass=smooth(bass,clampf(powerBass/8),.25f);mids=smooth(mids,clampf(powerMid/13),.23f);highs=smooth(highs,clampf(powerHi/11),.30f);
 float total=clampf(raw*micGain*autoGain*2.f);energy=smooth(energy,total,total>energy?.40f:.08f);
 flux=fmaxf(0,bass-oldBass);oldBass=smooth(oldBass,bass,.075f);
 kick=smooth(kick,clampf(flux*2.5f+bass*.28f),.4f);
 uint32_t now=millis();if(flux>.042f && bass>.105f && now-beatStamp>260)registerOnset(now);
 haveSamples=true;micStamp=now;
}
static void audioTick(){
 if(remote || !M5.Mic.isEnabled())return;
 if(pendingMic){if(!M5.Mic.isRecording()){pendingMic=false;processMic();}}
 else pendingMic=M5.Mic.record(pcm,NFFT,FS);
}
static void beatTick(){static uint32_t last=0;uint32_t now=millis();if(!last){last=now;return;}uint32_t d=now-last;last=now;
 beatClock+=clampf(d*.001f,0,.05f)*bpm/60.f;
 if(now-micStamp>2500){beatConfidence=smooth(beatConfidence,0,.009f);
  // Continue animating in silence; reacquire tempo without sudden jumps when music returns.
  if(now-beatStamp>12000){bpm=smooth(bpm,125.f,.0013f);onsetNum=0;}
 }
}
static void saveNames(){for(int i=0;i<VIS_COUNT;i++)if(isTextEffect(i))prefs.putString((String("t")+String(i)).c_str(),userText[i]);prefs.putString("bluerobot",blueRobot);prefs.putString("pinkrobot",pinkRobot);}
static void loadSettings(){
 prefs.begin("clubnative31",false);
 // V12 stored effect indexes, NVS names, and favorites by old presentation order.
 // V13 reorders only the presentation. Migrate exactly once when prior settings exist.
 const bool hadV12=prefs.isKey("effect")||prefs.isKey("favs")||prefs.isKey("gains")||prefs.isKey("scn0");
 const bool migrate=hadV12 && prefs.getUChar("sortrev",12)!=13;
 effectID=prefs.getUChar("effect",0)%VIS_COUNT;
 if(migrate)effectID=migrateEffectIndex(effectID);
 brightness=prefs.getUChar("bright",52);profile=prefs.getUChar("profile",1)%3;
 uint64_t oldFavorites=FAVORITE_DEFAULT;
 if(prefs.getBytes("favs",&oldFavorites,sizeof(oldFavorites))!=sizeof(oldFavorites)){
  // V12's default bitmap is positional and must be mapped just like a saved one.
  oldFavorites=migrate?((1ULL<<0)|(1ULL<<10)|(1ULL<<18)|(1ULL<<23)|(1ULL<<28)|(1ULL<<29)|(1ULL<<30)):FAVORITE_DEFAULT;
 }
 if(migrate){
  favoriteBits=0;
  for(uint8_t i=0;i<VIS_COUNT;i++)if(oldFavorites&(1ULL<<i))favoriteBits|=1ULL<<migrateEffectIndex(i);
 }else favoriteBits=oldFavorites;
 favoriteBits&=((1ULL<<VIS_COUNT)-1ULL);
 favoritesOnly=prefs.getBool("favonly",false)&&favoriteBits;
 if(prefs.getBytes("micmodes",&micManualMask,sizeof(micManualMask))!=sizeof(micManualMask))micManualMask=0;
 micManualMask&=((1ULL<<VIS_COUNT)-1ULL);
 adaptiveRender=prefs.getBool("adaptive",true);remoteIdleSecs=prefs.getUChar("idle",20);
 for(uint8_t j=0;j<4;j++){
  SceneData v;size_t n=prefs.getBytes((String("scn")+String(j)).c_str(),&v,sizeof(v));
  if(n==sizeof(v)&&v.magic==SCENE_MAGIC&&v.effect<VIS_COUNT){
   if(migrate)v.effect=migrateEffectIndex(v.effect);
   scenes[j]=v;
  }
 }
 rxChannel=clampi(prefs.getUChar("radio",1),1,11);
 uint8_t loadedGain[VIS_COUNT]={};prefs.getBytes("gains",loadedGain,sizeof(loadedGain));
 for(uint8_t i=0;i<VIS_COUNT;i++){
  const uint8_t dest=migrate?migrateEffectIndex(i):i;
  gainPct[dest]=clampi(loadedGain[i],20,250);
  if(loadedGain[i]<20||loadedGain[i]>250)gainPct[dest]=135;
 }
 String oldLabels[VIS_COUNT];
 for(uint8_t i=0;i<VIS_COUNT;i++)oldLabels[i]=prefs.getString((String("t")+String(i)).c_str(),"");
 for(uint8_t i=0;i<VIS_COUNT;i++){
  uint8_t dest=migrate?migrateEffectIndex(i):i;
  if(isTextEffect(dest))userText[dest]=oldLabels[i];
 }
 blueRobot=prefs.getString("bluerobot","");pinkRobot=prefs.getString("pinkrobot","");
 micGain=gainPct[effectID]/100.f;
 if(migrate){
  prefs.putUChar("effect",effectID);prefs.putBytes("favs",&favoriteBits,sizeof(favoriteBits));
  prefs.putBytes("gains",gainPct,sizeof(gainPct));
  for(uint8_t i=0;i<VIS_COUNT;i++)if(isTextEffect(i))prefs.putString((String("t")+String(i)).c_str(),userText[i]);
  for(uint8_t j=0;j<4;j++)if(scenes[j].magic==SCENE_MAGIC)prefs.putBytes((String("scn")+String(j)).c_str(),&scenes[j],sizeof(SceneData));
 }
 // Commit marker only after all migrated values are written.
 if(!prefs.isKey("sortrev")||prefs.getUChar("sortrev",12)!=13)prefs.putUChar("sortrev",13);
}
static void persistEffect(){prefs.putUChar("effect",effectID);micGain=gainPct[effectID]/100.f;}
static void radioSend(uint8_t cmd,uint8_t value){
 if(!radioReady)return;
 uint32_t seq=++remoteSeq;uint8_t payload[12]={'Z','H','R','1',1,cmd,value,0,
   (uint8_t)seq,(uint8_t)(seq>>8),(uint8_t)(seq>>16),(uint8_t)(seq>>24)};
 radioPending=(esp_now_send(BROADCAST,payload,sizeof(payload))==ESP_OK);remoteLast=millis();radioSuccess=false;
}
#if ESP_IDF_VERSION_MAJOR >= 5
static void radioCallback(const esp_now_recv_info_t *info,const uint8_t *data,int n)
#else
static void radioCallback(const uint8_t *mac,const uint8_t *data,int n)
#endif
{
 if(n!=13||memcmp(data,"ZHA1",4)!=0||data[4]!=1)return;
 Ack a; a.seq=(uint32_t)data[9]|((uint32_t)data[10]<<8)|((uint32_t)data[11]<<16)|((uint32_t)data[12]<<24);
 a.status=data[6];a.brightness=data[7];a.preset=data[8];a.power=0;a.ready=true;
 portENTER_CRITICAL(&ackMux);rxAck=a;newAck=true;portEXIT_CRITICAL(&ackMux);
}
static void radioTick(){if(newAck){Ack a;portENTER_CRITICAL(&ackMux);a=rxAck;newAck=false;portEXIT_CRITICAL(&ackMux);
 if(a.ready&&a.seq==remoteSeq){radioPending=false;radioSuccess=a.status==0;vibration(radioSuccess?135:90,radioSuccess?45:100);}}
 if(radioPending&&millis()-remoteLast>1500)radioPending=false;}
static void startRadio(){
 WiFi.mode(WIFI_STA);WiFi.disconnect(false,false);WiFi.setSleep(false);
 esp_wifi_set_promiscuous(true);
 esp_err_t channelStatus=esp_wifi_set_channel(rxChannel,WIFI_SECOND_CHAN_NONE);
 esp_wifi_set_promiscuous(false);if(channelStatus!=ESP_OK){radioReady=false;return;}
 radioReady=esp_now_init()==ESP_OK;
 if(radioReady){esp_now_register_recv_cb(radioCallback);esp_now_peer_info_t peer={};memcpy(peer.peer_addr,BROADCAST,6);peer.channel=rxChannel;peer.encrypt=false;esp_now_add_peer(&peer);}
}
static void setRemote(bool on){if(remote==on)return;remote=on;overlay=NONE;remotePage=0;displayBlank=false;lastRemoteActivity=millis();
 M5.Display.setBrightness((uint8_t)(brightness*255/100));radioSuccess=false;radioPending=false;
 if(remote)M5.Mic.end();else{M5.Mic.begin();pendingMic=false;}
 vibration(190,82);}

// ------------------ Low-overhead native render primitives ------------------
static void spoke(float cx,float cy,float r0,float r1,float a,uint16_t c,int w=1){line(cx+cosf(a)*r0,cy+sinf(a)*r0,cx+cosf(a)*r1,cy+sinf(a)*r1,c,w);}
static void arc(float cx,float cy,float radius,float a0,float a1,uint16_t c,int w=1){
 int n=clampi(int(fabsf(a1-a0)*radius/7),5,128);float x=cx+radius*cosf(a0),y=cy+radius*sinf(a0);
 for(int i=1;i<=n;i++){float a=a0+(a1-a0)*i/n;float xx=cx+radius*cosf(a),yy=cy+radius*sinf(a);line(x,y,xx,yy,c,w);x=xx;y=yy;}
}
static void lens(float cx,float cy,int radius,uint16_t c){for(int r=radius+12;r>radius;r-=3)circ(cx,cy,r,tint(c,(radius+13-r)*.035f));circ(cx,cy,radius,c,2);}
static void stars(int count,float time,float s,float pulse=0){for(int i=0;i<count;i++){
 float a=frac(i*.61803398f)*TAU;float r=sqrtf(frac(i*.7548776f))*R;
 float x=CX+r*cosf(a+time*.045f*(i%3?1:-1));float y=CY+r*sinf(a+time*.045f*(i%3?1:-1));
 int d=1+(i%7==0?1:0);uint16_t c=hue(frac(i*.1618f+.53f));dot(x,y,d+(pulse>0&&i%11==0?pulse*2:0),tint(c,.17f+(i%7)*.075f));
}}
static void glassText(const String &s,float x,float y,uint16_t c,int font=4){
 cv.setTextDatum(middle_center);cv.setTextColor(tint(c,.19f));
 for(int dx=-3;dx<=3;dx+=3)for(int dy=-3;dy<=3;dy+=3)cv.drawString(s,(int)x+dx,(int)y+dy,font);
 cv.setTextColor(c);cv.drawString(s,(int)x,(int)y,font);
}
static void figure(float x,float y,float scale,float t,uint16_t c,bool robot=false){
 // Smooth periodic choreographed posing, paired with footwork and torso twists.
 float sway=18*sinf(TAU*t/4),step=sinf(TAU*t/2),bounce=6*sinf(TAU*t);
 float hx=x+sway,hy=y-79*scale+bounce;float shoulderX=hx+9*scale*sinf(TAU*t/8);
 int rr=(int)(11*scale);
 if(robot){cv.fillRoundRect(hx-rr,hy-rr,rr*2,rr*2,4,tint(c,.25f));cv.drawRoundRect(hx-rr,hy-rr,rr*2,rr*2,4,c);
  dot(hx-4*scale,hy,2,c);dot(hx+4*scale,hy,2,c);}
 else {circ(hx,hy,rr,c,2);}
 float sy=hy+rr+8*scale,hip=sy+55*scale;
 line(shoulderX,sy,hx,hip,c,3);line(shoulderX-21*scale,sy,shoulderX+21*scale,sy,c,2);
 float elbowsY=sy+28*scale, handsY=sy+33*scale+28*scale*sinf(TAU*t/4+.8f);
 line(shoulderX-21*scale,sy,shoulderX-39*scale-17*scale*step,elbowsY,c,2);
 line(shoulderX-39*scale-17*scale*step,elbowsY,shoulderX-43*scale-23*scale*step,handsY,c,2);
 line(shoulderX+21*scale,sy,shoulderX+39*scale+17*scale*step,elbowsY,c,2);
 line(shoulderX+39*scale+17*scale*step,elbowsY,shoulderX+43*scale+18*scale*step,sy+2*scale-20*scale*sinf(TAU*t/4),c,2);
 float footBase=hip+61*scale;
 line(hx,hip,hx-22*scale-19*scale*step,hip+35*scale-13*scale*fmaxf(0,step),c,3);
 line(hx-22*scale-19*scale*step,hip+35*scale-13*scale*fmaxf(0,step),hx-32*scale-20*scale*step,footBase-20*scale*fmaxf(0,step),c,3);
 line(hx,hip,hx+22*scale+19*scale*step,hip+35*scale+13*scale*fminf(0,step),c,3);
 line(hx+22*scale+19*scale*step,hip+35*scale+13*scale*fminf(0,step),hx+32*scale+20*scale*step,footBase+20*scale*fminf(0,step),c,3);
}
static void eqBars(bool mirrored=false){
 int count=29,span=W-12,slot=span/count;int ground=mirrored?CY:CY+143;
 for(int i=0;i<count;i++){
  float v=bins[clampi((int)(i*NBANDS/(float)count),0,NBANDS-1)];
  int h=18+(int)(clampf(v*.9f+beatPulse()*.05f)*300);
  int x=6+i*slot;uint16_t co=hue(i*.023f+.43f);
  for(int yy=0;yy<h;yy+=10){
    uint16_t col=yy>h*.8f?rgb(253,112,174):yy>h*.46f?hue(.45f+i*.015f):co;
    cv.fillRoundRect(x,ground-yy-8,slot-2,8,2,tint(col,.35f+.55f*(float)yy/fmaxf(h,1)));
    if(mirrored)cv.fillRoundRect(x,ground+yy,slot-2,7,2,tint(col,.46f));
  }
 }
}
static void scopeLine(uint16_t color,int y0,int amp=105,bool heavy=false){
 float t=T();float prevx=4,prevy=y0;
 for(int i=1;i<154;i++){
  float x=4+i*(W-8)/153.f;
  float v=scopeY[i]*autoGain*micGain;
  float yy=y0-clampf(v,-.85f,.85f)*amp;
  if(!haveSamples)yy=y0-12*sinf(t*2+i*.22f);
  if(heavy)line(prevx,prevy,x,yy,tint(color,.20f),6);
  line(prevx,prevy,x,yy,color,heavy?3:2);prevx=x;prevy=yy;
 }
}
static void effectsClassic(uint8_t id){
 float t=T(),bp=beatPulse(),e=clampf(energy+.25f*bp);black();
 switch(id){
 case 0:{ // Blue Note: entire screen blue, no square bezel.
  uint16_t dark=rgb(2,27,76),blue=rgb(6,80,159),gold=rgb(255,217,146);cv.fillScreen(rgb(1,36,104));
  for(int yy=0;yy<H;yy+=19)line(2,yy,W-2,yy,tint(blue,.55f),1);
  for(int j=0;j<13;j++){float a=3.53f+j*(TAU-1.72f)/12.f;spoke(CX,CY+88,145,159,a,gold,j%3==0?3:1);}
  float angle=3.53f+clampf(energy+.3f*bass)*(TAU-1.72f);
  line(CX,CY+88,CX+166*cosf(angle),CY+88+166*sinf(angle),rgb(255,250,222),3);
  dot(CX,CY+88,11,gold);textC("dB",CX,CY-113,gold,4);break;}
 case 1:{eqBars(false);break;}
 case 2:{ // Bloom, 20 radial blossoming filaments.
  for(int k=0;k<11;k++){float rr=22+k*17+e*45;circ(CX,CY,rr,tint(hue(t*.045f+k*.07f),.22f),2);}
  for(int p=0;p<25;p++){float a=TAU*p/25+t*.24f;
    for(int j=0;j<4;j++){float r=36+j*28+e*68,theta=a+.27f*sinf(r*.031f+t);
      dot(CX+r*cosf(theta),CY+r*sinf(theta),j%2?3:2,tint(hue(p*.05f+j*.08f+t*.03f),.5f));}}
  glow(CX,CY,18+e*18,rgb(236,87,255));break;}
 case 3:{ // Vapor scope slow 8/16-beat grid waves.
  for(int i=0;i<19;i++){int y=CY+35+i*12;float sk=(y-CY)/215.f;
    line(CX-205*sk,y,CX+205*sk,y,tint(rgb(220,42,171),.32f),1);
  }
  for(int i=-8;i<=8;i++){line(CX,CY+35,CX+i*25,CY+208,tint(rgb(41,170,227),.38f));}
  dot(CX,CY-115,67,rgb(235,74,145));for(int i=0;i<5;i++)line(CX-76,CY-150+i*19,CX+76,CY-150+i*19,rgb(11,18,44),2);
  for(int k=0;k<3;k++){float oldx=3,oldy=CY+20;
    for(int i=1;i<155;i++){float x=i*(W-8)/155.f+4;
      float v=sinf(i*.087f+beatClock*.11f+k*.9f)*sinf(i*.018f+beatClock*.06f);
      float y=CY+30+v*(28+15*e)*(k==1?-1:1)+k*13;
      line(oldx,oldy,x,y,tint(hue(.83f+k*.1f),k==0?.8f:.42f),k==0?3:1);oldx=x;oldy=y;}}
  break;}
 case 4:{ // MATRIX PHOSPHOR: binary rain, single coherent trace, calibrated-reference dB label.
  for(int i=0;i<38;i++){float px=frac(i*.6180f)*W;float py=fmodf((i*83.f+t*(31+i%4*8)),(float)H);
   for(int j=0;j<8;j++){float yy=fmodf(py-j*18+H,H);textC(((i+j)&1)?"1":"0",px,yy,tint(rgb(42,255,84),.10f+.65f*(8-j)/8.f),2);}}
  line(0,CY,W,CY,tint(rgb(23,125,44),.3f));scopeLine(rgb(65,255,91),CY,125,true);
  break;}
 case 5:{for(int k=0;k<12;k++){float oldx=0,oldy=CY;for(int i=1;i<90;i++){float x=i*W/90.f;
    float y=CY+sinf(i*.095f+t*.7f+k*.51f)*(30+e*83)+sinf(i*.032f+beatClock*.12f+k)*(28+k*4);
    line(oldx,oldy,x,y,tint(hue(k/12.f+t*.025f),.35f+k*.04f),2);oldx=x;oldy=y;}}break;}
 case 6:{for(int k=0;k<22;k++){float x=12+k*20,v=band(k+3);float h=15+v*345;cv.fillRoundRect(x,CY+155-h,13,h,4,tint(hue(k*.035f+.48f),.35f+v*.65f));dot(x+6,CY+155-h,3,rgb(230,240,255));}break;}
 case 7:{for(int k=0;k<64;k++){float a=TAU*k/64-PI_F/2;
  float v=band(k*NBANDS/64);spoke(CX,CY,67,90+v*140,a,tint(hue(k/64.f+.42f),.42f+.4f*v),3);}
  lens(CX,CY,56,rgb(101,238,244));break;}
 case 8:{ // House tempo ECG, synced trace rather than chaotic motion.
  for(int y=25;y<H;y+=22)line(0,y,W,y,tint(rgb(247,47,141),.16f));
  for(int x=25;x<W;x+=22)line(x,0,x,H,tint(rgb(247,47,141),.16f));
  float prevx=0,prevy=CY;for(int i=1;i<=W;i++){
   float p=frac((float)i/W*2.1f-beatClock);
   float z=-8*expf(-sq((p-.29f)/.035f))+14*expf(-sq((p-.41f)/.03f))-
      97*expf(-sq((p-.49f)/.017f))+26*expf(-sq((p-.55f)/.04f));
   float y=CY+z;line(prevx,prevy,i,y,tint(rgb(255,59,161),.18f),6);
   line(prevx,prevy,i,y,rgb(255,96,171),3);prevx=i;prevy=y;
  }
  glow(CX,CY-120,24+bp*13,rgb(255,66,151));break;}
 case 9:{for(int k=0;k<9;k++){float r=24+k*21+7*sinf(beatClock*.3f+k*.6f);
  circ(CX,CY,r,tint(hue(k*.08f+.56f),.25f+e*.32f),2);}
  for(int k=0;k<24;k++){float a=TAU*k/24+t*.15f,r=35+e*154;dot(CX+r*cosf(a),CY+r*sinf(a),3,hue(k*.043f+.55f));}break;}
 case 10:{for(int k=0;k<16;k++){float a=k*TAU/16;for(int j=0;j<18;j++){
   float r=24+j*11,ang=a+.2f*sinf(j*.24f+t*.35f);spoke(CX,CY,r,r+7,ang,tint(hue(k*.065f+j*.01f+.1f),.2f+.5f*e),2);
  }}break;}
 case 11:{for(int j=0;j<230;j++){float a=j*2.3999632f+t*.075f;float rr=9*sqrtf((float)j)*(1+.12f*e);
   dot(CX+rr*cosf(a),CY+rr*sinf(a),1+(j%9==0?2:0),hue(j*.031f+t*.03f));}break;}
 case 12:{ // Infinity Tunnel — always glides, bass expands geometry, highs add glints.
  const float drive=clampf(.18f+.70f*bass+.20f*energy);
  const float travel=beatClock*3.3f + bass*16.f;
  for(int j=0;j<27;j++){
   float distance=frac((j*18.f+travel)/486.f);
   int rr=int(10+distance*245.f*(1.f+.12f*bass));
   float br=clampf(.10f+distance*.45f+drive*.23f);
   uint16_t co=tint(hue(j*.048f+t*.013f+.58f),br);
   circ(CX,CY,rr,co,j%6==0?3:1);
   if(j%4==0){float a=t*.26f+j*TAU*.618f;
    glow(CX+rr*cosf(a),CY+rr*sinf(a),1+int(highs*4.f),tint(co,.65f));}
  }
  // Vortex spokes expand on beats, with slow ambient movement when quiet.
  for(int k=0;k<12;k++){float a=k*TAU/12 + t*.28f;
    spoke(CX,CY,17+28*beatPulse(),185+28*drive,a,tint(hue(.48f+k*.043f),.10f+.18f*drive));}
  break;}
 case 13:{for(int k=0;k<24;k++){float a=k*TAU/24;
  for(int j=0;j<12;j++){float r=20+j*17+13*sinf(beatClock*.24f+j*.48f+k*.34f);
   dot(CX+r*cosf(a),CY+r*sinf(a),2,tint(hue(k*.03f+j*.02f),.3f+e*.4f));}}break;}
 case 14:{for(int j=0;j<10;j++){float x=CX,y=CY;
   for(int k=0;k<8;k++){float xx=x+42*cosf(j*.87f+t*.1f+k*.66f),yy=y+42*sinf(j*.87f+t*.1f+k*.66f);
    line(x,y,xx,yy,tint(hue(j*.05f+.54f),.3f+e*.5f),2);x=xx;y=yy;}}break;}
 case 15:{for(int i=0;i<150;i++){float a=frac(i*.618033f)*TAU;float rr=fmodf(i*13.f+t*(30+i%7*4),233.f);
    float x=CX+rr*cosf(a),y=CY+rr*sinf(a);dot(x,y,1+int(rr/90),tint(hue(i*.037f),.25f+rr/290.f));}break;}
 case 16:{for(int k=0;k<10;k++){float rr=35+k*18+16*sinf(t*.55f+k*.7f);
   circ(CX+12*sinf(t*.6f),CY+10*cosf(t*.7f),rr,tint(rgb(159,202,236),.19f+k*.04f),3);}break;}
 case 17:{for(int k=0;k<12;k++){float a=t*(k%2?-.22f:.29f)+TAU*k/12,r=40+k*13;
   circ(CX,CY,r,tint(hue(k*.09f),.13f),1);
   glow(CX+r*cosf(a),CY+r*sinf(a),3+(k%4),hue(k*.095f));}break;}
 case 18:{ // round whole-screen moire, no square tile.
   for(int i=0;i<68;i++){float a=i*TAU/68.f;
    for(int j=0;j<12;j++){float r=15+j*19+10*sinf(a*6+t*.36f+j*.2f);
      float rr=r+8*sinf(a*13-t*.55f+j*.6f);
      dot(CX+r*cosf(a),CY+rr*sinf(a),1+(j%5==0),tint(hue(i*.01f+j*.04f),.15f+.5f*e));}}
  break;}
 case 19:{for(int k=0;k<80;k++){float a=k*2.39996f+t*.023f,r=12+sqrtf(k)*20;
   glow(CX+r*cosf(a),CY+r*sinf(a),1+(k%5==0?2:0),tint(hue(k*.03f+t*.04f),.4f));}break;}
 case 20:{ // dance club duo with long-step choreography + lighting.
   for(int i=0;i<11;i++)spoke(CX,27,0,360,-PI_F/2+(i-5)*.095f+t*.12f,tint(hue(i*.09f),.13f),2);
   for(int i=0;i<19;i++)cv.fillRect(9+i*24,H-28-(int)(band(i+3)*100),16,15+(int)(band(i+3)*100),tint(hue(i*.05f),.22f));
   figure(150,242,1.f,beatClock+.2f,rgb(255,104,214));figure(318,242,1.f,beatClock+2.f,rgb(77,226,255));break;}
 case 21:{for(int j=0;j<3;j++){float x=126+j*108,angle=sinf(beatClock*.5f+j)*.3f; // genderless pirouettes
   for(int k=0;k<18;k++){float a=k*TAU/18,r=35+k%4*9;dot(x+r*cosf(a+angle+t*.3f),CY+r*sinf(a+angle+t*.3f),2,hue(j*.22f+k*.03f));}
   figure(x,280,.77f,beatClock+j*1.5f,rgb(220-j*40,90+j*50,255));}break;}
 case 22:{ // Robot Shuffle — V6 original mirror-grid disco ball, 8-beat partner crossing.
   // Laser rays behind the dancers, one coherent rotating sphere suspended at top.
   for(int k=0;k<11;k++){float a=t*.18f+k*TAU/11;
     spoke(CX,81,42,167,a,tint(hue(.55f+k*.08f),.11f+.20f*bass),2);}
   line(CX,0,CX,38,rgb(204,221,255),2);
   const int ballY=85,ballR=43;
   for(int rr=ballR;rr>=4;rr-=5)dot(CX,ballY,rr,tint(rgb(105+(ballR-rr)*2,153+(ballR-rr),226),.36f));
   float reflectRot=phase8()*TAU;
   for(int row=-5;row<=5;row++){
     int yy=row*8,halfwidth=(int)sqrtf(fmaxf(0,(float)(ballR*ballR-yy*yy)));
     for(int col=-6;col<=6;col++){
       int xx=col*11+int(reflectRot*7.f+row*3)%11-5;
       if(abs(xx)>halfwidth-2)continue;
       float shine=clampf(.44f+.48f*sinf(xx*.13f+reflectRot+row*.77f));
       uint16_t tile=tint(hue(.56f+row*.027f+col*.043f),.30f+.68f*shine);
       cv.fillRoundRect(CX+xx-5,ballY+yy-3,9,7,1,tile);
     }
   }
   circ(CX,ballY,ballR,rgb(180,221,255),2);dot(CX-13,ballY-13,4,rgb(253,255,255));
   for(int k=0;k<8;k++){float a=t*.33f+k*TAU/8,r=52+(k%2)*8;
     glow(CX+r*cosf(a),ballY+r*sinf(a),1,tint(rgb(245,255,255),.55f));}
   // V6 crossover path: ease over 8 beats, pass front/back smoothly.
   float q=phase8(),pass=.5f-.5f*cosf(q*TAU),fore=sinf(q*TAU);
   float xBlue=CX-96+192*pass,xPink=CX+96-192*pass;
   float yBlue=289+11*fore,yPink=289-11*fore;
   for(int j=0;j<20;j++)cv.fillRect(6+j*23,H-25-int(55*band(j+2)),15,8+int(55*band(j+2)),tint(hue(.55f+j*.055f),.20f+.30f*energy));
   if(fore<0){figure(xPink,yPink,.80f,beatClock+2,rgb(255,86,188),true);figure(xBlue,yBlue,.80f,beatClock,rgb(72,222,255),true);}
   else {figure(xBlue,yBlue,.80f,beatClock,rgb(72,222,255),true);figure(xPink,yPink,.80f,beatClock+2,rgb(255,86,188),true);}
   if(blueRobot.length())textC(blueRobot,(int)xBlue,(int)(yBlue-178),rgb(109,233,255),2);
   if(pinkRobot.length())textC(pinkRobot,(int)xPink,(int)(yPink-178),rgb(255,144,207),2);
  break;}
 }
}


// V17 fidelity-pass for the original collection, using the same spatial
// construction as the V6 browser. The microphone and beat-clock remain shared.
static void fidelitySilhouette(float x,float y,float scale,float phase,uint16_t c,bool robot=false){
 float sway=sinf(phase)*9.f,step=sinf(phase*1.05f),bounce=fmaxf(0,sinf(phase*2))*5;
 float X=x+sway,Y=y-bounce;
 int w=(int)(robot?35*scale:31*scale),h=(int)(robot?34*scale:30*scale);
 if(robot){
  cv.fillRoundRect(X-w/2,Y-144*scale,w,h,5,tint(c,.81f));
  cv.drawRoundRect(X-w/2,Y-144*scale,w,h,5,rgb(224,248,255));
  cv.fillRoundRect(X-24*scale,Y-100*scale,48*scale,57*scale,7,tint(c,.77f));
  cv.fillRect(X-18*scale,Y-87*scale,36*scale,6*scale,rgb(42,72,115));
  dot(X-8*scale,Y-130*scale,3,rgb(235,250,255));dot(X+8*scale,Y-130*scale,3,rgb(235,250,255));
 } else {
  dot(X,Y-132*scale,15*scale,c);
  cv.fillRoundRect(X-17*scale,Y-108*scale,34*scale,57*scale,10,tint(c,.83f));
 }
 float a=sinf(phase),b=sinf(phase+1.7f);
 line(X-17*scale,Y-90*scale,X-(35+11*a)*scale,Y-(76+15*b)*scale,c,7);
 line(X-(35+11*a)*scale,Y-(76+15*b)*scale,X-(52+15*a)*scale,Y-(48+27*b)*scale,c,5);
 line(X+17*scale,Y-90*scale,X+(36-10*a)*scale,Y-(75-12*b)*scale,c,7);
 line(X+(36-10*a)*scale,Y-(75-12*b)*scale,X+(52-14*a)*scale,Y-(50-29*b)*scale,c,5);
 float liftL=fmaxf(0,a),liftR=fmaxf(0,-a);
 line(X-10*scale,Y-53*scale,X+(-20+14*a)*scale,Y-(26+8*liftL)*scale,c,9);
 line(X+(-20+14*a)*scale,Y-(26+8*liftL)*scale,X+(-30+25*a)*scale,Y-16*liftL*scale,c,8);
 line(X+10*scale,Y-53*scale,X+(19-14*a)*scale,Y-(25+8*liftR)*scale,c,9);
 line(X+(19-14*a)*scale,Y-(25+8*liftR)*scale,X+(29-24*a)*scale,Y-16*liftR*scale,c,8);
 line(X+(-30+25*a)*scale-9,Y-16*liftL*scale,X+(-30+25*a)*scale+8,Y-16*liftL*scale,rgb(219,249,255),5);
 line(X+(29-24*a)*scale-8,Y-16*liftR*scale,X+(29-24*a)*scale+9,Y-16*liftR*scale,rgb(219,249,255),5);
}
static bool effectsClassicFidelity(uint8_t id){
 const float t=T(),b=bass,e=clampf(energy),p=beatClock;
 switch(id){
 case 1:{ // Prism: V6 58 columns with upwards-only segmented LEDs.
  black();const int count=58,base=415;
  for(int j=0;j<count;j++){
    float fi=j*31.f/(count-1),v=band((int)fi)*(1-frac(fi))+band(min(31,(int)fi+1))*frac(fi);
    float mod=.035f*sq(sinf(t*1.25f+j*.3f));
    int h=(int)(13+clampf(v*1.12f+mod)*357);
    int x=j*W/count,width=max(3,W/count-1);
    for(int k=0;k<h;k+=10){int y=base-k;
      uint16_t col=hue(frac((j*6.45f+t*5+k*.09f)/360.f));
      cv.fillRoundRect(x,y,width,8,2,col);
    }
    cv.fillRect(x,base-(int)(peak[(int)fi]*340),width,2,rgb(230,249,255));
  }
  line(0,425,W,425,rgb(48,91,211));return true;}
 case 2:{ // Psychedelic Bloom: nine continuously morphing V6 petal contours.
  black();for(int l=0;l<9;l++){
   float ra=24+l*20+e*30,oldx=0,oldy=0;
   uint16_t col=hue(frac((l*37.f+t*16.f)/360.f));
   for(int j=0;j<=170;j++){float a=TAU*j/170.f;
     float r=ra*(1+.20f*sinf(a*(5+l%5)+t*(1.2f+l*.13f))+.23f*b*sinf(a*13-t*2));
     float x=CX+cosf(a)*r,y=CY+sinf(a)*r;
     if(j)line(oldx,oldy,x,y,col,2);oldx=x;oldy=y;
   }
  }return true;}
 case 3:{ // V6 Vapor Scope: segmented sunset, perspective floor, five drifting waves.
   black();
   const int sy=151,sr=70;
   for(int yy=-sr;yy<=sr;yy+=3){
      int half=(int)sqrtf(fmaxf(0.f,(float)(sr*sr-yy*yy)));
      float f=(yy+sr)/(float)(2*sr);
      uint16_t c= f<.50f?blend(rgb(255,231,161),rgb(251,72,154),f*2.f)
                         :blend(rgb(251,72,154),rgb(165,51,229),(f-.5f)*2.f);
      cv.fillRect(CX-half,sy+yy,max(0,2*half),3,c);
   }
   for(int j=0;j<7;j++){
     float y=sy+10+j*j*1.95f;
     line(CX-100,y,CX+100,y,rgb(44,16,80),1+j/3);
   }
   for(int j=-9;j<=9;j++)line(CX,278,CX+j*72,468,tint(hue(.73f),.48f),1);
   for(int j=0;j<10;j++){
     float f=j/9.f,y=286+174*f*f,half=(y-286)*1.8f+10;
     line(CX-half,y,CX+half,y,tint(hue((315+j*5+t*2)/360.f),.60f));
   }
   for(int k=0;k<5;k++){
     float oldx=0,oldy=0;
     for(int j=0;j<=236;j++){
       float u=j/236.f,x=u*520-26,q=u*TAU;
       float env=.22f+b*.48f+band(min(31,(int)(u*31)))*.26f;
       float wav=sinf(q*(1.25f+k*.08f)+t*.36f+k*.6f);
       float y=230+(k-2)*22+31*wav+16*sinf(q*2.7f-t*.23f+k*.4f)*env
              +scopeY[min(159,(int)(u*159))]*22
              +7*sinf(q*5+t*.36f)*b;
       if(j){uint16_t c=hue(frac((176+k*35+t*5)/360.f));
         line(oldx,oldy,x,y,tint(c,.32f),k==2?5:3);line(oldx,oldy,x,y,c,k==2?3:2);}
       oldx=x;oldy=y;
     }
   }
   return true;}
 case 6:{ // Frequency Towers: radial rather than edge-aligned.
  black();for(int j=0;j<32;j++){
    float a=TAU*j/32,outer=55+band(j)*160;
    line(CX+cosf(a)*52,CY+sinf(a)*52,CX+cosf(a)*outer,CY+sinf(a)*outer,
      hue(frac((190+j*13+t*13)/360.f)),4);
  }circ(CX,CY,53,rgb(161,166,255),3);return true;}
 case 7:{ // 160 flowing radial rays (the native previously had 64).
  black();for(int j=0;j<160;j++){
    float a=TAU*j/160.f+t*.14f,outer=86+170*band(j*32/160),inner=80+12*sinf(j*.4f+t);
    line(CX+cosf(a)*inner,CY+sinf(a)*inner,CX+cosf(a)*outer,CY+sinf(a)*outer,
     hue(frac((j*3+t*15)/360.f)),2);
  }circ(CX,CY,68,rgb(82,126,243),3);
  textC("BASS",CX,CY-15,rgb(199,223,255),2);
  textC(String((int)(b*100))+"%",CX,CY+15,rgb(199,223,255),4);return true;}
 case 9:{ // Resonator: fine perturbed circular wavelets.
  black();for(int l=0;l<22;l++){
    float r=30+l*7.5f+band(l)*25;
    for(int j=0;j<130;j++){float a=TAU*j/130;
      float rr=r+4*sinf(a*8-t*1.5f+l*.32f)*(.15f+b);
      dot(CX+cosf(a)*rr,CY+sinf(a)*rr,1.4f,hue(frac((l*19+t*12)/360.f)));
    }
  }return true;}
 case 10:{ // Kaleidoscope Reactor: dense two-fold reflected particles.
  black();for(int j=0;j<12;j++){
    float a=TAU*j/12;
    for(int r=24;r<200;r+=8){float rr=r+15*sinf(r*.06f-t*1.4f)+band((r/8)%32)*32;
      float phi=a+.16f*sinf(r*.045f+t*.6f);
      uint16_t col=hue(frac((j*28+r*.4f+t*12)/360.f));
      dot(CX+cosf(phi)*rr,CY+sinf(phi)*rr,2,col);
      dot(CX+cosf(phi+TAU/24)*rr,CY+sinf(phi+TAU/24)*rr,2,col);
    }
  }return true;}
 case 11:{ // 410-point sunflower, same seed geometry as browser.
  black();for(int j=0;j<410;j++){
    float a=j*2.399963f+t*.2f,r=9.1f*sqrtf((float)j)*(1+.15f*e)+band((j*11)%32)*30;
    dot(CX+cosf(a)*r,CY+sinf(a)*r,j%9?1:2.4f,
       hue(frac((j*22.25f+t*13)/360.f)));
  }return true;}
 case 12:{ // 23 animated decagonal tunnel slices, still reacts to bass.
  black();for(int j=0;j<23;j++){
    float f=frac(j*.067f+t*.12f),r=(12+220*f*f)*(1+.13f*b);
    float oldx=0,oldy=0;uint16_t c=tint(hue(frac((235+f*140+t*13)/360.f)),.35f+f*.6f);
    for(int k=0;k<=10;k++){float a=TAU*k/10+t*.10f+j*.11f;
     float x=CX+cosf(a)*r,y=CY+sinf(a)*r;
     if(k)line(oldx,oldy,x,y,c,2);oldx=x;oldy=y;
    }
  }return true;}
 case 13:{ // Cymatics node geometry, no fixed dot-grid approximation.
  black();for(int j=0;j<145;j++){
    float a=TAU*j/145,th=a+t*.08f;
    for(int r=40;r<208;r+=5){
     float v=sinf(5*a+t*.7f)*cosf(r*.075f)+sinf(8*a-t*.3f)*b;
     if(fabsf(v)>.72f)dot(CX+cosf(th)*(r+v*10),CY+sinf(th)*(r+v*10),1.4f,
         hue(frac((202+v*52+t*8)/360.f)));
    }
  }return true;}
 case 16:{ // Liquid Chrome: 15 curving metallic wave ribbons.
  black();for(int j=0;j<15;j++){
    for(int x=-170;x<170;x+=6){
      float v=sinf(t+x*.026f+j*.54f),n=sinf(t+(x+6)*.026f+j*.54f);
      float y=(j-7)*22;
      float a=CY+y+v*(12+e*45)+band((x+170)*32/341)*35;
      float z=CY+y+n*(12+e*45)+band((x+176)*32/347)*35;
      uint16_t silver=rgb(132+(int)(v*35),151+(int)(v*33),184+(int)(v*20));
      line(CX+x,a,CX+x+6,z,silver,2);
    }
  }return true;}
 case 17:{ // Tilted musical orbits with luminous satellites.
  black();for(int j=0;j<13;j++){
    float tilt=j*.46f+t*.11f,r=46+j*11,sn=sinf(tilt),cs=cosf(tilt);
    for(int k=0;k<120;k+=2){float a=TAU*k/120,x=cosf(a)*r,y=sinf(a)*r*(.31f+.12f*band(j*2%32));
      dot(CX+x*cs-y*sn,CY+x*sn+y*cs,1.2f,hue(frac((j*30+t*15)/360.f)));
    }
    float a=t*(.3f+j*.03f)+j*1.1f,xx=cosf(a)*r,yy=sinf(a)*r*.37f;
    glow(CX+xx*cs-yy*sn,CY+xx*sn+yy*cs,2+band(j*2%32)*5,
      hue(frac((j*30+t*15)/360.f)));
  }return true;}
 case 18:{ // Quantum Moire: polar interference, NOT a square patch of dots.
  black();for(float r=7;r<=237;r+=5.5f){
    int points=max(24,(int)(r*1.6f));
    for(int k=0;k<points;k++){
      float a=TAU*k/points,xx=r*cosf(a),yy=r*sinf(a);
      float q1=hypotf(xx+45*sinf(t*.23f),yy),q2=hypotf(xx-45*cosf(t*.2f),yy);
      float v=sinf(q1*.22f+t*.35f)+sinf(q2*.21f-t*.24f);
      if(v>.23f){float rad=r+2.2f*sinf(a*9+t*.3f)*b;
        dot(CX+cosf(a)*rad,CY+sinf(a)*rad,1.1f+clampf(v)*.75f,
           tint(hue(frac((195+v*59+t*4)/360.f)),.35f+clampf(v)*.48f));
      }
    }
  }circ(CX,CY,223,tint(rgb(64,56,167),.36f));return true;}
 case 19:{ // Nebula: 340 drifting seeded particles.
  black();for(int j=0;j<340;j++){
    float a=j*2.399963f+t*(.05f+j%13*.005f)+e*.7f;
    float r=190*sqrtf(frac(j*.61803f));
    float scatter=14*sinf(j*.7f+t*.6f)+band(j%32)*40;
    dot(CX+cosf(a)*(r+scatter),CY+sinf(a)*(r+scatter),j%29?1.4f:3.f,
        tint(hue(frac((250+j*1.8f+t*9)/360.f)),.52f+band(j%32)*.45f));
  }return true;}
 case 20:{ // Warehouse duo: silhouette choreography with stage lights.
  black();for(int j=0;j<9;j++){
    float x=30+j*50+sinf(t*.24f+j)*20;
    line(x,0,CX+(x-CX)*.3f,348,tint(hue(frac((j*32+t*18)/360.f)),.25f),2);
  }
  for(int j=0;j<18;j++){
   float x=14+j*26,y=69+sinf(t*.7f+j)*7;
   glow(x,y,2+band(j%32)*5,hue(frac((j*17+t*7)/360.f)));
  }
  fidelitySilhouette(129,375,1.02f,p*PI_F,rgb(70,229,255));
  fidelitySilhouette(338,375,1.03f,p*PI_F+PI_F*.63f,rgb(252,100,205));
  return true;}
 case 21:{ // Ribbon Pirouette: paired performers surrounded by spiral ribbons.
  black();for(int j=0;j<2;j++){
    float base=j?330:138,phase=p*TAU*.2f+j*1.6f;
    for(int strand=0;strand<5;strand++){
      float oldx=0,oldy=0;
      for(int k=0;k<100;k++){
       float u=k/99.f,twist=phase-u*TAU*(2+strand*.29f);
       float x=base+sinf(twist)*u*(52+e*12),y=79+u*285+cosf(twist*1.45f+strand*.4f)*9;
       if(k)line(oldx,oldy,x,y,tint(hue(frac((160+j*157+strand*10+t*3)/360.f)),.52f),2);
       oldx=x;oldy=y;
      }
    }
    fidelitySilhouette(base+sinf(phase)*8,375,.85f,phase,j?rgb(255,88,215):rgb(72,241,223));
  }return true;}
 case 22:{ // V6 disco ball with full-body robot partners and original crossing.
  black();float rot=phase8()*TAU;
  for(int j=0;j<9;j++){float a=rot*.36f+j*TAU/9;
    line(CX+cosf(a)*48,90+sinf(a)*48,CX+cosf(a)*130,90+sinf(a)*130,
      tint(hue(frac((180+j*36+t*6)/360.f)),.2f+.2f*b),2);
  }
  line(CX,0,CX,47,rgb(200,226,255),2);
  dot(CX,90,43,rgb(53,84,129));
  for(int row=-5;row<=5;row++){int yy=row*8;
    int lim=(int)sqrtf(fmaxf(0.f,43.f*43-yy*yy));
    for(int k=-6;k<=6;k++){
       int xx=k*11+(int)(rot*7+row*3)%11-5;
       if(abs(xx)>lim-2)continue;
       float shine=.46f+.54f*fabsf(sinf(xx*.12f+rot*.65f+row*.8f));
       cv.fillRoundRect(CX+xx-5,90+yy-4,10,8,2,
         tint(hue(frac((194+row*11+xx*.9f+rot*14)/360.f)),.43f+.55f*shine));
    }
  }
  circ(CX,90,43,rgb(201,233,255),2);glow(CX-14,72,5,rgb(238,249,255));
  float phase=phase8()*TAU,ex=.5f-.5f*cosf(phase),front=sinf(phase);
  float xBlue=139+193*ex,xPink=329-193*ex;
  if(front<0){
    fidelitySilhouette(xPink,378-23*front,1.f,p*PI_F+PI_F,rgb(255,127,233),true);
    fidelitySilhouette(xBlue,378+23*front,1.f,p*PI_F,rgb(94,243,255),true);
  }else{
    fidelitySilhouette(xBlue,378+23*front,1.f,p*PI_F,rgb(94,243,255),true);
    fidelitySilhouette(xPink,378-23*front,1.f,p*PI_F+PI_F,rgb(255,127,233),true);
  }
  if(blueRobot.length())textC(blueRobot,(int)xBlue,190,rgb(94,243,255),2);
  if(pinkRobot.length())textC(pinkRobot,(int)xPink,190,rgb(255,127,233),2);
  return true;}
 default:return false;
 }
}

static void nameGlyphsOnRing(const String &name,float radius,float offset,uint16_t color){
 if(!name.length())return;
 cv.setTextDatum(middle_center);cv.setTextColor(color);
 int n=name.length();for(int j=0;j<n;j++){float a=offset+TAU*j/n-PI_F/2;
  glyph(name[j],CX+radius*cosf(a),CY+radius*sinf(a),color,4);
 }
}

// V6-style Fibonacci: many complete words orbit along the golden-angle spiral.
// Each word uses a tiny 7x9 glyph bitmap. Rotated stamps are rasterized straight
// into the PSRAM-backed RGB565 frame so neither fonts nor rotations allocate RAM.
static const uint8_t FIB_FONT[36][9] = {
  {0x1C,0x36,0x02,0x1E,0x3E,0x32,0x66,0x36,0x3E},
  {0x30,0x30,0x3C,0x3E,0x32,0x33,0x33,0x36,0x3E},
  {0x1E,0x3A,0x30,0x30,0x30,0x30,0x30,0x3A,0x1E},
  {0x06,0x06,0x1E,0x3E,0x26,0x66,0x66,0x36,0x3E},
  {0x1C,0x36,0x23,0x7F,0x7E,0x60,0x30,0x32,0x1E},
  {0x0E,0x18,0x3E,0x1C,0x18,0x18,0x18,0x18,0x18},
  {0x3E,0x36,0x26,0x66,0x26,0x3E,0x1E,0x06,0x1C},
  {0x30,0x30,0x3C,0x3E,0x36,0x36,0x36,0x36,0x36},
  {0x0C,0x0C,0x00,0x3C,0x0C,0x0C,0x0C,0x0C,0x3F},
  {0x0C,0x0C,0x1C,0x0C,0x0C,0x0C,0x0C,0x0C,0x3C},
  {0x30,0x30,0x32,0x36,0x3C,0x3C,0x3C,0x36,0x33},
  {0x78,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x0E},
  {0x7E,0x7F,0x6B,0x6B,0x6B,0x6B,0x6B,0x6B,0x6B},
  {0x3E,0x36,0x36,0x36,0x36,0x36,0x36,0x36,0x36},
  {0x1C,0x36,0x36,0x23,0x63,0x23,0x36,0x36,0x1C},
  {0x3E,0x36,0x33,0x33,0x32,0x3E,0x3C,0x30,0x30},
  {0x3E,0x36,0x66,0x66,0x26,0x3E,0x1E,0x06,0x06},
  {0x3F,0x39,0x38,0x30,0x30,0x30,0x30,0x30,0x30},
  {0x1C,0x36,0x30,0x3C,0x3E,0x0E,0x06,0x26,0x1C},
  {0x18,0x18,0x3E,0x18,0x18,0x18,0x18,0x18,0x0E},
  {0x36,0x36,0x36,0x36,0x36,0x36,0x36,0x36,0x3E},
  {0x63,0x36,0x36,0x36,0x36,0x1E,0x1C,0x1C,0x1C},
  {0x42,0x62,0x7A,0x7E,0x7E,0x7E,0x3E,0x36,0x36},
  {0x36,0x36,0x1C,0x1C,0x1C,0x1C,0x3E,0x36,0x77},
  {0x63,0x36,0x36,0x3E,0x1C,0x1C,0x1C,0x18,0x38},
  {0x3E,0x06,0x0E,0x0C,0x1C,0x18,0x38,0x30,0x3E},
  {0x1C,0x36,0x36,0x3E,0x3E,0x36,0x36,0x36,0x1C},
  {0x1C,0x2C,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x3F},
  {0x3C,0x26,0x06,0x06,0x0E,0x0C,0x18,0x30,0x3E},
  {0x3C,0x26,0x06,0x1C,0x06,0x06,0x02,0x26,0x3C},
  {0x0E,0x0E,0x1E,0x36,0x36,0x7E,0x3E,0x06,0x06},
  {0x3E,0x30,0x30,0x3C,0x26,0x06,0x06,0x26,0x3C},
  {0x1E,0x30,0x30,0x3C,0x36,0x32,0x32,0x36,0x1C},
  {0x3E,0x06,0x06,0x0C,0x0C,0x0C,0x18,0x18,0x18},
  {0x1C,0x36,0x36,0x1C,0x36,0x22,0x22,0x36,0x1C},
  {0x1C,0x36,0x26,0x26,0x36,0x1E,0x06,0x06,0x3C}
};
static inline const uint8_t *fibGlyph(char c){
 if(c>='A'&&c<='Z')c=(char)(c+('a'-'A'));
 int idx=(c>='a'&&c<='z')?c-'a':(c>='0'&&c<='9')?26+c-'0':-1;
 return idx<0?nullptr:FIB_FONT[idx];
}
static void fibStampWord(const String &word,float cx,float cy,float width,float angle,uint16_t color){
 if(!fb||word.length()==0)return;
 const int n=clampi(word.length(),1,12);
 const float pix=width/(n*8.f);
 const float ca=cosf(angle)*pix,sa=sinf(angle)*pix;
 const float xStart=-(n*8.f-1)*.5f,yStart=-4.f;
 const int thickness=clampi((int)(pix*.67f),1,2);
 for(int ci=0;ci<n;ci++){
  const uint8_t *rows=fibGlyph(word[ci]);if(!rows)continue;
  for(int py=0;py<9;py++){
   uint8_t mask=rows[py];
   for(int px=0;px<7;px++)if(mask & (1U<<(6-px))){
    const float xx=xStart+ci*8.f+px,yy=yStart+py;
    const int x=(int)roundf(cx+xx*ca-yy*sa),y=(int)roundf(cy+xx*sa+yy*ca);
    if(x<=1||y<=1||x>=W-3||y>=H-3)continue;
    fb[y*W+x]=color;
    if(thickness>=2){fb[y*W+x+1]=color;fb[(y+1)*W+x]=color;fb[(y+1)*W+x+1]=color;}
   }
  }
 }
}


// ------ V17: browser-composition typography engine ------
// The browser paints entire words, then scales/rotates them as a single piece.
// V16 used 7x9 dots; the 16-bit PSRAM framebuffer now receives a cached high-
// resolution FreeSansBold glyph mask. Build only when the editable word changes.
// Effects without editable typography continue using their original renderers.
static lgfx::LGFX_Sprite fidelityWord(&M5.Display);
static String fidelityCached;
static uint16_t *fidelityPixels=nullptr;
static int fidelityX=0,fidelityY=0,fidelityW=1,fidelityH=1;
static bool fidelityReady=false;
static void fidelityInit(){
 fidelityWord.setColorDepth(16);fidelityWord.setPsram(true);
 fidelityReady=bool(fidelityWord.createSprite(464,84));
 if(fidelityReady)fidelityPixels=(uint16_t*)fidelityWord.getBuffer();
 Serial.printf("V17 typography cache: %s (%d KB)\n",fidelityReady?"ready":"fallback",464*84*2/1024);
}
static void fidelityPrepare(const String &word){
 if(!fidelityReady||fidelityCached==word)return;
 fidelityCached=word;
 fidelityWord.fillScreen(0);
 fidelityWord.setTextDatum(middle_center);
 fidelityWord.setTextColor(0xffff);
 // All 31 effects can use these official M5GFX Latin fonts; no custom font files.
#ifdef STOPWATCH_HOST_TEST
 // Host-side visual regression: draw a small 7x9 equivalent for glyph masks.
 int xBase=232-(int)word.length()*8*2;
 for(int c=0;c<word.length();c++){
  const uint8_t *g=fibGlyph(word[c]);if(!g)continue;
  for(int y=0;y<9;y++)for(int x=0;x<7;x++)if(g[y]&(1U<<(6-x)))
   fidelityWord.fillRect(xBase+c*32+x*4,20+y*4,4,4,0xffff);
 }
#else
 fidelityWord.drawString(word,232,42,&lgfx::fonts::FreeSansBold24pt7b);
#endif
 int left=463,right=0,top=83,bottom=0;
 for(int y=0;y<84;y++)for(int x=0;x<464;x++)if(fidelityPixels[y*464+x]){
   if(x<left)left=x;if(x>right)right=x;if(y<top)top=y;if(y>bottom)bottom=y;
 }
 if(right<left){fidelityX=0;fidelityY=0;fidelityW=1;fidelityH=1;return;}
 fidelityX=left;fidelityY=top;fidelityW=right-left+1;fidelityH=bottom-top+1;
}
static inline void fidelityBlend(int x,int y,uint16_t c,uint8_t a){
 if((unsigned)x>=(unsigned)W||(unsigned)y>=(unsigned)H||a==0)return;
 uint16_t bg=fb[y*W+x];
 const uint32_t inv=255-a;
 const uint32_t r=((bg>>11)&31)*inv+((c>>11)&31)*a;
 const uint32_t g=((bg>>5)&63)*inv+((c>>5)&63)*a;
 const uint32_t b=(bg&31)*inv+(c&31)*a;
 fb[y*W+x]=(uint16_t)(((r/255)<<11)|((g/255)<<5)|(b/255));
}
static inline float fidelitySample(float u,float v){
 int x=(int)u,y=(int)v;
 if(x<0||y<0||x>=fidelityW||y>=fidelityH)return 0;
 const float du=u-x,dv=v-y;
 const int aX=fidelityX+x,aY=fidelityY+y;
 const uint16_t a=fidelityPixels[aY*464+aX];
 const uint16_t b=fidelityPixels[aY*464+min(463,aX+1)];
 const uint16_t c=fidelityPixels[min(83,aY+1)*464+aX];
 const uint16_t d=fidelityPixels[min(83,aY+1)*464+min(463,aX+1)];
 // Cached white text is 0xFFFF; use 6-bit green channel for alpha coverage.
 const float top=((a>>5)&63)*(1-du)+((b>>5)&63)*du;
 const float bot=((c>>5)&63)*(1-du)+((d>>5)&63)*du;
 return (top*(1-dv)+bot*dv)/63.f;
}
static void fidelityStamp(const String &word,float cx,float cy,float drawWidth,float angle,uint16_t color,float opacity=1.f,bool outline=false){
 if(!fb||drawWidth<3||opacity<=0)return;
 fidelityPrepare(word);
 if(!fidelityReady){fibStampWord(word,cx,cy,drawWidth,angle,color);return;}
 if(fidelityW<=1||fidelityH<=1)return;
 // The browser word sprites have generous margins. Retain comparable aspect.
 const float drawHeight=drawWidth*(fidelityH+10.f)/(fidelityW+25.f);
 const float co=cosf(angle),si=sinf(angle);
 const float boundX=(fabsf(co)*drawWidth+fabsf(si)*drawHeight)*.5f+3;
 const float boundY=(fabsf(si)*drawWidth+fabsf(co)*drawHeight)*.5f+3;
 const int x0=max(0,(int)floorf(cx-boundX)),x1=min(W-1,(int)ceilf(cx+boundX));
 const int y0=max(0,(int)floorf(cy-boundY)),y1=min(H-1,(int)ceilf(cy+boundY));
 const float du=fidelityW/drawWidth,dv=fidelityH/drawHeight;
 const float faded=clampf(opacity);
 for(int y=y0;y<=y1;y++)for(int x=x0;x<=x1;x++){
   const float ox=x-cx,oy=y-cy;
   const float sx=(ox*co+oy*si)*du+fidelityW*.5f;
   const float sy=(-ox*si+oy*co)*dv+fidelityH*.5f;
   if(sx<0||sy<0||sx>=fidelityW-1||sy>=fidelityH-1)continue;
   const float a=fidelitySample(sx,sy);
   if(a<.035f)continue;
   // Dim outer stroke and bright center, similar to V6 offscreen wordSprites.
   uint8_t strength=(uint8_t)(clampf(a*faded)*255.f);
   fidelityBlend(x,y,color,strength);
   if(a>.60f&&faded>.38f&&((x+y)&3)==0)
      fidelityBlend(x,y,0xffff,(uint8_t)(a*faded*(outline?27:46)));
 }
}
static inline void fidelityBigGlyph(char ch,int x,int y,uint16_t c){
 char str[2]={ch,0};
 cv.setTextDatum(middle_center);
 cv.setTextColor(tint(c,.22f));
 cv.drawString(str,x+1,y+2,&lgfx::fonts::DejaVu72);
 cv.setTextColor(c);
 cv.drawString(str,x,y,&lgfx::fonts::DejaVu72);
}
static void fidelityWavyRing(float r,float layer,float phase,uint16_t color){
 float lx=0,ly=0;
 for(int j=0;j<=160;j++){
   const float a=TAU*j/160.f;
   const float rr=r*(1+.10f*sinf(a*7+phase+layer*.4f)+.055f*sinf(a*13-phase*2.f));
   float x=CX+cosf(a)*rr,y=CY+sinf(a)*rr;
   if(j){line(lx,ly,x,y,tint(color,.23f),3);line(lx,ly,x,y,color,1);}
   lx=x;ly=y;
 }
}
static const uint16_t FID_PALETTE[8]={0x3fff,0xfb9b,0xff10,0xaff1,0xa47f,0xfed4,0xc53f,0x8fff};
static inline uint16_t fidelityCol(int k){return FID_PALETTE[(k%8+8)%8];}


static void textEffects(uint8_t id){
 black();const float t=T(),p=beatClock,rev=clampf(.24f+energy);
 const String &word=artName(effectID);const int n=clampi(word.length(),1,12);
 const float rhythm=beatPulse();
 const uint16_t cyan=rgb(86,250,255),pink=rgb(252,93,189);
 switch(id){
  case 23:{ // V6 Chromatic EQ: oversized independent letters, NOT numeric font 7.
   const float gap=fminf(79.f,410.f/n);
   const uint16_t col[5]={rgb(86,250,255),rgb(252,93,189),rgb(247,235,105),rgb(136,247,105),rgb(186,130,255)};
   for(int j=0;j<n;j++){
    int bi=clampi((int)((j+.5f)*32.f/n),0,29);
    float m=clampf((band(bi)+band(bi+1)+band(bi+2))/3.f);
    float osc=.5f+.5f*sinf(p*PI_F+j*1.38f);
    float xx=CX+(j-(n-1)*.5f)*gap;
    float yy=238-((m*.80f+osc*.20f)*112-50);
    fidelityBigGlyph(word[j],(int)xx,(int)yy,col[j%5]);
    line(xx,342,xx,326-m*59,tint(col[j%5],.34f),3);
   }
   break;}
  case 24:{ // V6 wave-shaped psychedelic halo, rather than disconnected dots.
   const float phase=p*TAU*.12f;
   for(int k=12;k>0;k--){
     float r=23+k*16+bass*13;
     fidelityWavyRing(r,k,phase,tint(hue(k*31.f/360.f+t*.010f),.54f));
   }
   float gap=fminf(72.f,358.f/n);
   for(int j=0;j<n;j++){
    float x=CX+(j-(n-1)*.5f)*gap;
    float y=CY+16*sinf(t*1.15f+j*.81f)+5*sinf(t*.56f+j*1.48f);
    // Large natural Latin glyphs; no tiny 7-segment digits.
    char ch[2]={word[j],0};cv.setTextDatum(middle_center);
    cv.setTextColor(tint(pink,.28f));cv.drawString(ch,(int)x+2,(int)y+2,&lgfx::fonts::FreeSansBold24pt7b);
    cv.setTextColor(j%2?rgb(255,189,255):rgb(255,242,254));
    cv.drawString(ch,(int)x,(int)y,&lgfx::fonts::FreeSansBold24pt7b);
   }
   break;}
  case 25:{ // V6 Kaleidoscope: 40 WHOLE words in ten rotating wedges.
   float rot=p*TAU*.07f;
   for(int slice=0;slice<10;slice++){
     const float angle=slice*TAU/10+rot;
     for(int k=0;k<4;k++){
       float r=83+k*46;
       float xx=CX+sinf(angle)*r,yy=CY-cosf(angle)*r;
       fidelityStamp(word,xx,yy,word.length()>8?72:77,
                     angle+(slice%2?PI_F:0),hue((slice*36+r*.25f+t*5)/360.f),.82f);
     }
   }
   circ(CX,CY,73,rgb(230,225,255),2);
   fidelityStamp(word,CX,CY,fminf(190,word.length()*31.f),0,rgb(232,211,255),1.f);
   break;}
  case 26:{ // V6 Round Resonator: orbiting WHOLE words, empty central opening.
   for(int k=0;k<15;k++){
     float r=28+k*14+band((k*2)%32)*16;
     circ(CX,CY,(int)r,tint(hue((180+k*17+t*5)/360.f),.42f+.16f*energy),k%4==0?2:1);
   }
   float rot=p*TAU*.065f;
   for(int k=0;k<6;k++){
     float a=k*TAU/6+rot;
     float rr=117+17*sinf(p*PI_F+k);
     fidelityStamp(word,CX+cosf(a)*rr,CY+sinf(a)*rr,
                   fminf(110.f,word.length()*21.f),a+PI_F*.5f,fidelityCol(k),.86f);
   }
   break;}
  case 27:{ // Pixel-geometry equivalent to V6 Fibonacci, but true font textures.
   const float golden=2.39996322972865f;
   const float travel=t*.092f+p*.002f;
   const float breath=1+.05f*bass+.027f*energy;
   for(int j=0;j<120;j++){
     float f=frac(j/120.f+travel*.38f),r=(14+217*sqrtf(f))*breath,a=j*golden+travel*.21f;
     dot(CX+cosf(a)*r,CY+sinf(a)*r,1,tint(hue(frac(j*.071f+t*.012f)),.24f+.43f*(1-f)));
   }
   for(int j=0;j<41;j++){
     float f=frac(j/41.f+travel),r=(16+218*sqrtf(f))*breath,a=j*golden+travel*.35f;
     float width=fminf(96,fmaxf(56,n*13.f))*(.40f+f*.50f);
     float pulse=.57f+.30f*rhythm+.25f*energy;
     float alpha=clampf(f*8.f)*clampf((1.f-f)*6.f)*pulse;
     fidelityStamp(word,CX+cosf(a)*r,CY+sinf(a)*r,width,a+PI_F*.5f,fidelityCol(j),alpha);
   }
   circ(CX,CY,25+13*bass,tint(hue(frac(.63f+t*.02f)),.4f),2);
   break;}
  case 28:{ // Hyperspace: V6 stacked, receding whole-word portals.
    const float rotation=phase16()*TAU;
    for(int j=16;j>=0;j--){
      float f=frac(j/17.f+t*.047f),r=12+220*f*f;
      float alpha=clampf(f*7)*clampf((1-f)*6)*(.55f+.34f*energy+.15f*rhythm);
      float a=rotation+f*.45f+(j%2)*.11f;
      fidelityStamp(word,CX+cosf(a)*r*.06f,CY+sinf(a)*r*.06f,
                    43+340*f*f*(1+.13f*bass),a,fidelityCol(j),alpha,true);
    }
    for(int j=0;j<10;j++){float a=j*TAU/10-rotation*.6f;
       glow(CX+cosf(a)*(72+12*bass),CY+sinf(a)*(72+12*bass),2,fidelityCol(j));}
    circ(CX,CY,54+11*bass,cyan,2);
    fidelityStamp(word,CX,CY,170,0,rgb(233,250,255),1.f);
    break;}
  case 29:{ // Aurora: smooth curtains carrying whole names (not individual dots).
    for(int layer=0;layer<15;layer++){
      float ly=0;for(int x=0;x<=W;x+=6){
       float y=90+layer*22 + sinf(x*.011f+t*.36f+layer*.3f)*(18+bass*25)
               +sinf(x*.032f-t*.19f+layer)*8;
       if(x)line(x-6,ly,x,y,tint(hue((135+layer*21+t*5)/360.f),.5f),2);
       ly=y;
      }
    }
    for(int j=0;j<8;j++){
      float yy=67+j*46,xx=CX+sinf(t*.25f+j*.85f)*55;
      fidelityStamp(word,xx,yy,fminf(205,word.length()*22.f),sinf(t*.14f+j)*.19f,
                    hue((145+j*37+t*5)/360.f),.74f);
    }
    break;}
  case 30:{ // Flowing halo: V6 deforming waves with orbiting typography.
    float phase=p*TAU*.12f,breath=1+.055f*bass;
    for(int layer=11;layer>0;layer--){
      float r=(22+layer*17)*breath;
      fidelityWavyRing(r,layer,phase,tint(hue((layer*31+t*4)/360.f),.56f));
    }
    for(int layer=0;layer<4;layer++){
      const int k=3+layer*2;float r=(22+k*17)*breath;
      const int groups=layer%2?3:2;
      for(int j=0;j<groups;j++){
        float ca=j*TAU/groups+t*(layer%2?-.095f:.13f)+layer*.47f;
        float rr=r*(1+.1f*sinf(ca*7+phase+k*.4f)+.055f*sinf(ca*13-phase*2));
        fidelityStamp(word,CX+cosf(ca)*rr,CY+sinf(ca)*rr,
                      fminf(145,word.length()*(17+layer*3)*.8f),ca+PI_F*.5f,fidelityCol(layer+j),.76f);
      }
    }
    break;}
  case 31:{ // Orbit portal: 23 flowing whole names + four anchoring orbit words.
    float rot=phase16()*TAU+.16f*sinf(p*.5f)*bass;
    for(int j=0;j<6;j++)circ(CX,CY,38+j*31+12*bass,tint(hue((215+j*25+t*3)/360.f),.22f+.12f*energy));
    for(int j=0;j<23;j++){
      float f=frac(j/23.f+t*.032f),r=27+205*sqrtf(f),a=rot+j*2.399963f*.75f+f*.55f;
      float alpha=clampf(f*8)*clampf((1-f)*5)*(.67f+.3f*bass+.14f*rhythm);
      fidelityStamp(word,CX+cosf(a)*r,CY+sinf(a)*r,35+67*f,a+PI_F*.5f,fidelityCol(j),alpha,true);
    }
    for(int j=0;j<4;j++){
      float a=rot+j*TAU/4,r=150+6*sinf(p*.2f+j);
      fidelityStamp(word,CX+cosf(a)*r,CY+sinf(a)*r,93,a+PI_F*.5f,fidelityCol(j+3),.75f+.15f*energy);
    }
    circ(CX,CY,22+10*bass,rgb(173,216,255),2);
    break;}
 }
}

// ------------------ Cinematic flagship renderers ------------------
// Asset: 65,536 palette indices + 256 RGB565 entries, stored in FLASH;
// expanded exactly once into PSRAM, reused every frame (no JPEG/PNG decoder).
static uint16_t *decodedIris=nullptr;
static void initArtwork(){
 decodedIris=(uint16_t*)heap_caps_malloc((size_t)EYE_ART_SIZE*EYE_ART_SIZE*2,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
 if(decodedIris){for(int i=0;i<EYE_ART_SIZE*EYE_ART_SIZE;i++){
   int index=pgm_read_byte(&EYE_IRIS_INDEX8[i]);decodedIris[i]=pgm_read_word(&EYE_PALETTE565[index]);}}

}
static void drawEye(){
 black();float t=T(),p=beatPulse();
 // Stationary layered bezel is cheap to draw over cached iris texture.
 for(int k=0;k<11;k++){float r=170+k*6;circ(CX,CY,r,tint(k%3?rgb(45,121,164):rgb(124,213,220),.24f+k*.035f),k%3==0?2:1);}
 for(int k=0;k<42;k++){float a=k*TAU/42;
  spoke(CX,CY,192,211,a,tint(k%6==0?rgb(187,249,255):rgb(56,120,156),.22f+k%4*.15f),k%6==0?3:1);}
 float gazeX=21*sinf(t*.45f)+11*sinf(t*.13f),gazeY=15*sinf(t*.36f+.6f);
 if((uint32_t)(millis()-lastArtTouchMs)<2800){gazeX=clampf((touchLastX-CX)*.19f,-34,34);gazeY=clampf((touchLastY-CY)*.16f,-28,28);}
 // Native pre-rendered 256x256 iris, with a separate aperture and independent lids.
 if(decodedIris)cv.pushImage(CX-EYE_ART_SIZE/2+(int)gazeX,CY-EYE_ART_SIZE/2+(int)gazeY,EYE_ART_SIZE,EYE_ART_SIZE,decodedIris);
 else for(int k=0;k<21;k++)circ(CX+gazeX,CY+gazeY,35+k*5,tint(hue(.49f+k*.013f),.3f));
 int pr=28+(int)(12.f*(1.f-bass)+5*sinf(t*.27f));
 // Twelve mechanical iris leaves, with motorized shutter motion around dark pupil.
 for(int k=0;k<12;k++){float a=TAU*k/12+t*.025f,rr=pr+13.f;
  spoke(CX+gazeX,CY+gazeY,rr,rr+15,a,tint(rgb(135,243,239),.22f+.34f*energy),3);
  float x=CX+gazeX+rr*cosf(a),y=CY+gazeY+rr*sinf(a);
  line(x,y,x+12*cosf(a+.6f),y+12*sinf(a+.6f),rgb(9,35,47),3);
 }
 dot(CX+gazeX,CY+gazeY,pr+6,rgb(4,31,41));dot(CX+gazeX,CY+gazeY,pr,rgb(0,1,4));
 dot(CX+gazeX-36,CY+gazeY-55,14,tint(rgb(255,255,255),.44f));dot(CX+gazeX+29,CY+gazeY+37,4,rgb(204,255,255));
 // Blink closes smoothly over just 120 ms, infrequent, independent of the beat.
 float u=fmodf(t+1.15f,6.3f),blink=expf(-sq((u-2.65f)/.085f));
 int lid=(int)(blink*166);for(int yy=0;yy<lid;yy++){
  int width=(int)(sqrtf(fmaxf(0,1.f-sq((yy+15.f)/230.f)))*216);
  line(CX-width,CY-190+yy,CX+width,CY-190+yy,rgb(4+yy/25,10+yy/19,23+yy/13));
  line(CX-width,CY+190-yy,CX+width,CY+190-yy,rgb(4+yy/25,10+yy/19,23+yy/13));
 }
 arc(CX,CY,215,-2.68f,-.48f,tint(rgb(79,224,249),.51f),3);
 arc(CX,CY,215,.48f,2.64f,tint(rgb(79,224,249),.51f),3);
}
// Reflective implicit surface. The expensive distance field runs at 178x178, not
// 466x466. RGB565 is written directly into the 16-bit PSRAM framebuffer.
// Static environment map was generated offline, then sampled with normals.
struct Pt2{float x,y,z;};
static Pt2 vertex4(int mask,float rot){
 float x=(mask&1)?1.f:-1.f,y=(mask&2)?1.f:-1.f,z=(mask&4)?1.f:-1.f,w=(mask&8)?1.f:-1.f;
 float a=rot*.57f,b=rot*.35f,c=rot*.21f;
 float X=x*cosf(a)-w*sinf(a), W4=x*sinf(a)+w*cosf(a);
 float Y=y*cosf(b)-z*sinf(b), Z=y*sinf(b)+z*cosf(b);
 float Z2=Z*cosf(c)-W4*sinf(c), W2=Z*sinf(c)+W4*cosf(c);
 float pr=2.95f/(3.5f-W2);
 float yaw=rot*.18f,px=(X*cosf(yaw)+Y*sinf(yaw))*pr,py=(Y*cosf(yaw)-X*sinf(yaw))*pr;
 return {CX+px*81.f,CY+py*81.f,Z2};
}
static void drawGeometry(){
 black();float t=T(),r=beatClock*.024f;stars(36,t*.1f,.18f);
 for(int k=0;k<12;k++){float rr=210.f/(1+k*.17f)+20*sinf(r+k);
   circ(CX,CY,rr,tint(rgb(57,129,157),.075f+.04f*energy));}
 Pt2 verts[16];for(int i=0;i<16;i++)verts[i]=vertex4(i,r);
 for(int pass=0;pass<2;pass++)for(int i=0;i<16;i++)for(int b=0;b<4;b++){
  int j=i^(1<<b);if(i>=j)continue;float dep=(verts[i].z+verts[j].z)*.5f;
  float lum=clampf(.47f+dep*.17f+energy*.15f);
  uint16_t co=tint(hue(.53f+b*.1f),lum);
  if(pass==0)line(verts[i].x,verts[i].y,verts[j].x,verts[j].y,tint(co,.15f),6);
  else line(verts[i].x,verts[i].y,verts[j].x,verts[j].y,co,dep>.2f?3:1);
 }
 for(int j=0;j<16;j++)glow(verts[j].x,verts[j].y,2,tint(rgb(212,246,255),.36f));
 for(int q=0;q<3;q++){float rr=29+q*30+19*sinf(r*.8f+q);
  arc(CX,CY,rr,r*.32f+q,r*.32f+q+PI_F*1.36f,tint(hue(.68f+q*.1f),.22f),2);}
}
static void drawCinematic(uint8_t id){
 float t=T(),p=beatPhase(),pulse=beatPulse(),e=clampf(energy+.15f*pulse);
 if(id==32){drawEye();return;}
 if(id==37){drawGeometry();return;}
 black();
 switch(id){

 case 40:{ // Personal Dance Aura, mic-driven, no IMU polling needed.
  for(int k=0;k<16;k++){float a=k*TAU/16+t*.14f,rr=46+e*132+27*sinf(t*.35f+k);
   spoke(CX,CY,45,rr,a,tint(hue(.58f+k*.061f),.12f+e*.4f),4);
   glow(CX+rr*cosf(a),CY+rr*sinf(a),3,hue(k*.055f+.58f));}
  for(int i=0;i<7;i++){float r=60+i*19+13*sinf(beatClock*.12f+i);
    arc(CX,CY,r,t*.2f+i*.45f,t*.2f+i*.45f+2.6f,tint(hue(i*.12f+.44f),.24f),2);}
  break;}




 }
}
static void renderEffect(){
 const uint8_t id=originalEffect(effectID);
 if(id<23){if(!effectsClassicFidelity(id))effectsClassic(id);}
 else if(id<32)textEffects(id);
 else drawCinematic(id);
 // Mask only corners of the square sprite; the real StopWatch is a circular OLED.
 // The hardware physically clips these pixels, so no costly software corner mask.
}

// ------------------ Low-clutter on-watch UI and controls ------------------
String draftBlue,draftPink;
static void labelSmall(const String& s,int x,int y,uint16_t co=0xffff){textC(s,x,y,co,2);}
static void panelBg(const String& headline){
 black();circ(CX,CY,R-3,rgb(42,92,126),2);textC(headline,CX,88,rgb(123,241,247),4);
}
static void uiButton(int x,int y,int w,int h,const String&s,bool active=false){
 cv.fillRoundRect(x,y,w,h,9,active?rgb(44,130,151):rgb(17,40,67));cv.drawRoundRect(x,y,w,h,9,active?rgb(183,255,255):rgb(80,134,163));
 textC(s,x+w/2,y+h/2,active?rgb(255,255,255):rgb(212,238,246),2);
}
static void uiBar(int y,int x0,int x1,float value,uint16_t color){
 cv.fillRoundRect(x0,y,x1-x0,12,6,rgb(26,41,60));cv.fillRoundRect(x0,y,(int)((x1-x0)*clampf(value)),12,6,color);
 dot(x0+(x1-x0)*clampf(value),y+6,8,rgb(239,250,255));
}
static void renderOverlay(){
 if(overlay==NONE)return;
 const uint16_t CYAN=rgb(88,229,245), PINK=rgb(255,113,207), GRAY=rgb(169,206,230);
 switch(overlay){
 case BRIGHTNESS:{panelBg("BRIGHTNESS");
  textC(String(candidateBrightness)+" %",CX,201,rgb(255,255,255),7);
  uiBar(254,80,W-80,candidateBrightness/100.f,CYAN);
  labelSmall("A - / B +",CX,310,GRAY);
  uiButton(102,343,115,54,"CANCEL");uiButton(242,343,125,54,"OK",true);
  break;}
 case GAIN:{panelBg("MIC SENSITIVITY");
  textC(String(gainPct[effectID])+" %",CX,166,rgb(255,255,255),7);
  labelSmall(gainIsManual(effectID)?"MANUAL - SAVED":"AUTO - LIVE",CX,216,gainIsManual(effectID)?PINK:CYAN);
  uiBar(258,77,W-77,(gainPct[effectID]-20)/230.f,CYAN);
  uiButton(79,313,176,56,gainIsManual(effectID)?"RETURN AUTO":"AUTO ACTIVE",gainIsManual(effectID));
  uiButton(267,313,112,56,"DONE",true);
  labelSmall("SWIPE FOR NAME ENTRY",CX,405,GRAY);break;}
 case RHYTHM:{panelBg("AUTO BEAT ENGINE");
  textC(String((int)roundf(bpm))+" BPM",CX,169,rgb(255,255,255),6);
  textC(String((int)(beatConfidence*100))+"% CONFIDENCE",CX,229,PINK,3);
  textC("NO TAP / MANUAL SYNC",CX,260,GRAY,2);
  uiButton(82,306,300,54,String("PROFILE: ")+(profile==0?"SHOW":profile==1?"BALANCED":"ENDURANCE"),true);
  uiButton(153,379,160,45,"DONE");break;}
 case LIBRARY:{
  black();textC("CLUB LIBRARY",CX,72,CYAN,4);
  uiButton(65,108,104,43,"FAVS",libraryPage==0);
  uiButton(180,108,104,43,"SCENES",libraryPage==1);
  uiButton(295,108,104,43,"POWER",libraryPage==2);
  if(libraryPage==0){
   textC(EFFECT_NAMES[effectID],CX,189,GRAY,3);
   uiButton(91,218,283,48,isFavorite(effectID)?"REMOVE FAVORITE":"ADD FAVORITE",true);
   uiButton(91,282,283,47,favoritesOnly?"FAVORITES ONLY":"ALL 33 EFFECTS",favoritesOnly);
   uiButton(93,350,130,49,"< FAV");uiButton(244,350,130,49,"FAV >");
  }else if(libraryPage==1){
    for(uint8_t i=0;i<4;i++){
      int y=170+i*59;textC(SCENE_LABELS[i],147,y+17,scenes[i].magic==SCENE_MAGIC?rgb(248,251,255):GRAY,2);
      uiButton(237,y,75,37,"LOAD",scenes[i].magic==SCENE_MAGIC);
      uiButton(318,y,83,37,"SAVE",true);
    }
    textC("HOLD SLOT TO CLEAR",CX,420,GRAY,2);
  }else{
    textC(String("BPM ")+String((int)bpm)+" · "+String((int)(beatConfidence*100))+"%",CX,180,GRAY,2);
    uiButton(94,217,280,49,profile==0?"STAGE / SHOW":profile==1?"BALANCED":"ENDURANCE",true);
    uiButton(94,281,280,49,adaptiveRender?"ADAPTIVE ON":"ADAPTIVE OFF",adaptiveRender);
    uiButton(94,346,280,49,String("REMOTE IDLE ")+String(remoteIdleSecs)+"s",true);
  }
  break;}
 case NAME:{panelBg(originalEffect(effectID)==22?"ROBOT NAMES":"YOUR WORD");
  if(originalEffect(effectID)==22){
   textC("BLUE: "+(draftBlue.length()?draftBlue:"(none)"),CX,131,robotField==0?CYAN:GRAY,2);
   textC("PINK: "+(draftPink.length()?draftPink:"(none)"),CX,165,robotField==1?PINK:GRAY,2);
  }else{textC(inputName.length()?inputName:"(none)",CX,159,PINK,4);}
  const char *rows[3]={"qwertyuiop","asdfghjkl","zxcvbnm"};
  for(int row=0;row<3;row++){
   String chars(rows[row]);int n=chars.length();int gap=3,kW=(W-84-(n-1)*gap)/n;
   int left=(W-(n*kW+(n-1)*gap))/2;
   for(int k=0;k<n;k++)uiButton(left+k*(kW+gap),209+row*47,kW,39,chars.substring(k,k+1));
  }
  uiButton(62,355,99,49,"DELETE");uiButton(170,355,102,49,"CLEAR");uiButton(282,355,115,49,"ENTER",true);
  labelSmall("SWIPE FOR GAIN",CX,433,GRAY);break;}
 default:break;
 }
}
static void applyRemoteSelection(){
 if(remoteSelected<Z_COUNT)radioSend(9,remoteSelected);
 else {
  // Command 12 requires the separately updated matching harness receiver.
  // Don't imply success on older WLED firmware: no ACK -> user sees UNAVAILABLE.
  radioSend(12,remoteSelected-Z_COUNT);
 }
 vibration(137,25);
}
static void renderRemote(){
 panelBg(remotePage==0?"HARNESS FX":remotePage==1?"PRESETS":remotePage==2?"EFFECT SETTINGS":remotePage==3?"PALETTES":"RADIO");
 uint16_t white=rgb(241,251,255),cyan=rgb(102,244,239),dim=rgb(143,184,205);
 if(remotePage==0){
   bool isZ=remoteSelected<Z_COUNT;int idx=isZ?remoteSelected:remoteSelected-Z_COUNT;
   textC(isZ?"CUSTOM Z":"BUILT-IN WLED",CX,147,dim,2);
   textC(String(remoteSelected+1)+" / "+String(Z_COUNT+WLED_COUNT),CX,180,cyan,3);
   String name=isZ?Z_NAMES[idx]:WLED_NAMES[clampi(idx,0,WLED_COUNT-1)];
   if(name.length()>19)name=name.substring(0,19);
   textC(name,CX,240,white,3);
   uiButton(63,290,89,60,"<");uiButton(156,290,155,60,"APPLY",true);uiButton(315,290,89,60,">");
 }else if(remotePage==1){
   textC("PRESET "+String(remotePreset),CX,199,white,5);
   uiButton(61,270,95,64,"-");uiButton(163,270,140,64,"RECALL",true);uiButton(310,270,96,64,"+");
 }else if(remotePage==2){
   labelSmall("BRIGHTNESS",CX,140,dim);uiBar(162,80,W-80,remoteBrightness/255.f,cyan);
   labelSmall("SPEED",CX,211,dim);uiBar(234,80,W-80,remoteSpeed/255.f,rgb(216,94,240));
   labelSmall("INTENSITY",CX,284,dim);uiBar(309,80,W-80,remoteIntensity/255.f,rgb(252,167,100));
   labelSmall("TAP BAR TO APPLY",CX,362,dim);
 }else if(remotePage==3){
   int id=clampi(selectedPalette,0,PAL_COUNT-1);
   textC(String(id)+" / "+String(PAL_COUNT-1),CX,166,cyan,3);
   textC(PAL_NAMES[id],CX,230,white,4);
   uiButton(63,290,88,60,"<");uiButton(155,290,154,60,"APPLY",true);uiButton(313,290,89,60,">");
   labelSmall("Requires protocol v2",CX,366,dim);
 }else{
   textC("CH "+String(rxChannel),CX,167,cyan,5);
   String mac=WiFi.macAddress();textC(mac.substring(0,14),CX,222,white,2);
   textC(mac.substring(14),CX,247,white,2);
   uiButton(118,296,104,53,"CH -");uiButton(240,296,104,53,"CH +");
 }
 // Bottom page navigation without big page fractions on the watch face.
 uiButton(95,386,88,45,"< PAGE");uiButton(280,386,93,45,"PAGE >");
 if(radioPending)textC("WAIT ACK",CX,365,rgb(252,182,76),2);
 else if(radioSuccess)textC("APPLIED",CX,365,rgb(117,255,171),2);
}
static void showTextEditor(){
 overlay=NAME;inputName=userText[effectID];draftBlue=blueRobot;draftPink=pinkRobot;robotField=0;vibration();
}
static void saveNameOverlay(){
 if(originalEffect(effectID)==22){blueRobot=draftBlue;pinkRobot=draftPink;}
 else if(isTextEffect(effectID)){userText[effectID]=inputName;}
 saveNames();overlay=NONE;vibration(180,65);
}
static void remoteTouch(int x,int y){
 if(y>=380){remotePage=(remotePage+(x<CX?REMOTE_PAGES-1:1))%REMOTE_PAGES;vibration();return;}
 if(remotePage==0&&y>=274&&y<360){
   if(x<155)remoteSelected=(remoteSelected+Z_COUNT+WLED_COUNT-1)%(Z_COUNT+WLED_COUNT);
   else if(x>309)remoteSelected=(remoteSelected+1)%(Z_COUNT+WLED_COUNT);
   else applyRemoteSelection();
 }else if(remotePage==1&&y>=260&&y<=349){
   if(x<160)remotePreset=clampi(remotePreset-1,1,250);
   else if(x>306)remotePreset=clampi(remotePreset+1,1,250);
   else radioSend(1,remotePreset);
 }else if(remotePage==2&&y>155&&y<325){
   uint8_t v=clampi((int)((x-80)*255.f/(W-160)),0,255);
   if(y<204){remoteBrightness=v;radioSend(2,v);}
   else if(y<272){remoteSpeed=v;radioSend(10,v);}
   else {remoteIntensity=v;radioSend(11,v);}
 }else if(remotePage==3&&y>=280&&y<367){
   if(x<155)selectedPalette=(selectedPalette+PAL_COUNT-1)%PAL_COUNT;
   else if(x>310)selectedPalette=(selectedPalette+1)%PAL_COUNT;
   else radioSend(13,selectedPalette); // requires harness-side update
 }else if(remotePage==4&&y>=292&&y<=357){
    rxChannel=clampi(rxChannel+(x<CX?-1:1),1,11);prefs.putUChar("radio",rxChannel);
    esp_now_deinit();WiFi.mode(WIFI_OFF);startRadio();
 }
 vibration(100,26);
}
static void editorTouch(int x,int y){
 if(overlay==LIBRARY){
   lastRemoteActivity=millis();
   if(y>=104&&y<160){libraryPage=clampi((x-55)/119,0,2);vibration();return;}
   if(libraryPage==0){
     if(y>=215&&y<279){favoriteBits^=1ULL<<effectID;if(!favoriteBits)favoritesOnly=false;storeLibrary();vibration(155,50);}
     else if(y>=279&&y<340){if(favoriteBits){favoritesOnly=!favoritesOnly;
       if(favoritesOnly&&!isFavorite(effectID)){effectID=nextFavorite(effectID,1);persistEffect();}}
       storeLibrary();vibration();}
     else if(y>=345&&y<413&&favoriteBits){effectID=nextFavorite(effectID,x<CX?-1:1);persistEffect();vibration();}
   }else if(libraryPage==1){
     if(y>=160&&y<412){uint8_t slot=clampi((y-170)/59,0,3);
      if(x>=313)saveScene(slot);else if(x>=228)loadScene(slot);}
   }else{
     if(y>=210&&y<276){profile=(profile+1)%3;prefs.putUChar("profile",profile);
       if(!remote)setCpuFrequencyMhz(profile==2?160:240);vibration();}
     else if(y>=279&&y<340){adaptiveRender=!adaptiveRender;prefs.putBool("adaptive",adaptiveRender);vibration();}
     else if(y>=342&&y<406){remoteIdleSecs=remoteIdleSecs==20?45:remoteIdleSecs==45?0:20;
       prefs.putUChar("idle",remoteIdleSecs);vibration();}
   }
   return;
 }
 if(overlay==BRIGHTNESS){
  if(y>=238&&y<290){candidateBrightness=clampi((x-80)*100/(W-160),8,100);M5.Display.setBrightness((uint8_t)(candidateBrightness*255/100));}
  else if(y>=333&&y<415){if(x<CX){M5.Display.setBrightness((uint8_t)(brightness*255/100));}
   else{brightness=candidateBrightness;prefs.putUChar("bright",brightness);}overlay=NONE;vibration(170,50);}
 }else if(overlay==GAIN){
   if(y>=241&&y<300){
     gainPct[effectID]=clampi(20+(x-77)*230/(W-154),20,250);
     micGain=gainPct[effectID]/100.f;setGainManual(effectID,true);
     // Persist only on explicit touch, never during automatic adjustment.
     prefs.putBytes("gains",gainPct,sizeof(gainPct));vibration(145,28);
   }else if(y>=308&&y<386){
     if(x<260){
       setGainManual(effectID,false);
       resetMicHistory(micHist[effectID]);
       vibration(190,65);
     }else {overlay=NONE;prefs.putBytes("gains",gainPct,sizeof(gainPct));}
   }
 }else if(overlay==RHYTHM){
   if(y>=300&&y<369){profile=(profile+1)%3;prefs.putUChar("profile",profile);
      setCpuFrequencyMhz(profile==2?160:240);vibration();}
   else if(y>=372)overlay=NONE;
 }else if(overlay==NAME){
   if(originalEffect(effectID)==22 && y>110&&y<196){robotField=y<150?0:1;return;}
   if(y>=209&&y<346){
     int row=(y-209)/47;String r=row==0?"qwertyuiop":row==1?"asdfghjkl":"zxcvbnm";
     int n=r.length(),gap=3,kw=(W-84-(n-1)*gap)/n,left=(W-(n*kw+(n-1)*gap))/2;
     int col=(x-left)/(kw+gap);
     if(col>=0&&col<n){String &dest=originalEffect(effectID)==22?(robotField==0?draftBlue:draftPink):inputName;
      if(dest.length()<12)dest+=r.substring(col,col+1);}
   }else if(y>=351&&y<418){
     String &dest=originalEffect(effectID)==22?(robotField==0?draftBlue:draftPink):inputName;
     if(x<163){if(dest.length())dest.remove(dest.length()-1);}
     else if(x<278){dest="";}else saveNameOverlay();
   }
 }
}
static void selectEffect(int d){
 if(remote){remoteSelected=(remoteSelected+Z_COUNT+WLED_COUNT+d)%(Z_COUNT+WLED_COUNT);}
 else {effectID=favoritesOnly&&favoriteBits?nextFavorite(effectID,d):((effectID+VIS_COUNT+d)%VIS_COUNT);persistEffect();}
 vibration(122,26);
}
static void controlButtons(){
 const uint32_t now=millis();
 if((M5.BtnA.wasPressed()||M5.BtnB.wasPressed())&&displayBlank){
  displayBlank=false;lastRemoteActivity=now;M5.Display.setBrightness((uint8_t)(brightness*255/100));
  wakeKeyConsumed=true; // first action only wakes the remote display
 }
 if(wakeKeyConsumed){if(!M5.BtnA.isPressed()&&!M5.BtnB.isPressed())wakeKeyConsumed=false;return;}
 if(remote&&(M5.BtnA.isPressed()||M5.BtnB.isPressed()))lastRemoteActivity=now;
 if(M5.BtnA.wasPressed()){keyADown=now;keyAHeld=false;}
 if(M5.BtnB.wasPressed()){keyBDown=now;keyBHeld=false;}
 if(M5.BtnA.isPressed() && M5.BtnB.isPressed() && !keyChord){
   // Chord has priority over independent button presses.
   keyChord=true;keyAHeld=keyBHeld=true;setRemote(!remote);
   if(remote && !radioReady)startRadio();
   if(!remote){if(radioReady){esp_now_deinit();radioReady=false;}WiFi.mode(WIFI_OFF);setCpuFrequencyMhz(profile==2?160:240);}
 }
 if(M5.BtnA.isPressed() && !keyChord && !keyAHeld && keyADown && now-keyADown>=730){
  overlay=BRIGHTNESS;candidateBrightness=brightness;keyAHeld=true;vibration(195,60);
 }
 if(M5.BtnB.isPressed() && !keyChord && !keyBHeld && keyBDown && now-keyBDown>=850){
  overlay=LIBRARY;libraryPage=0;keyBHeld=true;vibration(170,45);
 }
 if(M5.BtnA.wasReleased()){
   if(!keyChord && !keyAHeld){if(overlay==BRIGHTNESS){candidateBrightness=clampi(candidateBrightness-5,8,100);}
      else if(overlay==NONE)selectEffect(-1);}
   keyADown=0;
 }
 if(M5.BtnB.wasReleased()){
   if(!keyChord && !keyBHeld){if(overlay==BRIGHTNESS){candidateBrightness=clampi(candidateBrightness+5,8,100);}
      else if(overlay==NONE)selectEffect(+1);}
   keyBDown=0;
 }
 if(keyChord && !M5.BtnA.isPressed() && !M5.BtnB.isPressed())keyChord=false;
}
static void controlTouch(){
 auto touch=M5.Touch.getDetail(0);
 if(touch.wasPressed()){touchStartX=touchLastX=touch.x;touchStartY=touchLastY=touch.y;touchStart=millis();touchDown=true;}
 if(touch.isPressed()){touchLastX=touch.x;touchLastY=touch.y;}
 if(touch.wasReleased()||touch.wasClicked()){
   uint32_t held=millis()-touchStart;int x=touchLastX,y=touchLastY;
   int dx=x-touchStartX,dy=y-touchStartY;touchDown=false;
   if(overlay==LIBRARY && abs(dx)>58){libraryPage=(libraryPage+(dx<0?1:2))%3;vibration();return;}
   if(overlay==LIBRARY && libraryPage==1 && held>800 && y>=160 && y<411){
      uint8_t slot=clampi((y-170)/59,0,3);clearScene(slot);return;}
   if(overlay==NAME && abs(dx)>58){overlay=GAIN;vibration();return;}
   if(overlay==GAIN && abs(dx)>58){overlay=isTextEffect(effectID)||originalEffect(effectID)==22?NAME:NONE;vibration();return;}
   if(displayBlank){displayBlank=false;M5.Display.setBrightness((uint8_t)(brightness*255/100));lastRemoteActivity=millis();return;}
   if(overlay!=NONE){editorTouch(x,y);return;}
   if(remote){lastRemoteActivity=millis();remoteTouch(x,y);return;}
   if(held>600){overlay=GAIN;vibration();return;}
   if(abs(dx)>64){selectEffect(dx>0?-1:+1);return;}
   if(originalEffect(effectID)==22||isTextEffect(effectID)){showTextEditor();return;}
   // Tap unlabelled watch artwork for gain, except touch-specific visualizers.
   if(originalEffect(effectID)==32||originalEffect(effectID)==36||originalEffect(effectID)==41||originalEffect(effectID)==43){lastArtTouchMs=millis();if(originalEffect(effectID)==43)userSpin+=dx*.013f;return;}
   overlay=GAIN;vibration();
 }
}
static void saveFrameStats(){ // Diagnostic data via Serial only; no onscreen performance overlay.
 static uint32_t prev=0;uint32_t now=millis();if(now-prev<15000)return;prev=now;
 Serial.printf("Native31 V16 effect %u, BPM %.1f conf %.2f, freePSRAM %u, frame %lums, battery %d%%\n",
    effectID,bpm,beatConfidence,(unsigned)ESP.getFreePsram(),(unsigned long)(now-renderStamp),M5.Power.getBatteryLevel());
}
static uint8_t powerFrameCap(){
 uint8_t rate=profile==0?48:profile==1?35:24;
 if(remote||overlay!=NONE)return 15;
 if(!adaptiveRender)return rate;
 if(originalEffect(effectID)==2||originalEffect(effectID)==3||originalEffect(effectID)==27)return rate;
 // Lighter, ambient artwork is intentionally drawn less often; the FFT and
 // beat tracker are independent and retain their normal sampling cadence.
 static const uint8_t slow[]={10,11,13,16,17,18,19,20,21,25,26,28,29};
 static const uint8_t medium[]={1,4,6,7,8,9,12,22,24,30,31,32,37,40};
 uint8_t factor=100;
 for(uint8_t v:slow)if(originalEffect(effectID)==v){factor=62;break;}
 if(factor==100)for(uint8_t v:medium)if(originalEffect(effectID)==v){factor=82;break;}
 if(originalEffect(effectID)>=23 && originalEffect(effectID)<=31){
   // On the actual watch, the previous low-power heuristic made typography sluggish.
   // Preserve musical tempo; choose 30fps default, 24fps in endurance profile.
   return profile==2?24:30;
 }
 if(energy<.11f&&factor<100)factor=(uint8_t)(factor*0.84f);
 return clampi((rate*factor+50)/100,14,rate);
}
static void renderFrame(){
 if(overlay!=NONE)renderOverlay();else if(remote)renderRemote();else renderEffect();
 cv.pushSprite(0,0);
}
void setup(){
 Serial.begin(115200);auto cfg=M5.config();cfg.internal_mic=true;cfg.internal_spk=false;
 M5.begin(cfg);M5.Speaker.end();M5.Mic.begin();
 M5.Display.setRotation(0);W=M5.Display.width();H=M5.Display.height();CX=W/2;CY=H/2;R=clampi(min(W,H)/2-3,150,260);
 loadSettings();
 for(int i=0;i<NFFT;i++)fftWindow[i]=.5f-.5f*cosf(TAU*i/(NFFT-1));
 for(int k=0;k<NBANDS;k++){
   binBegin[k]=clampi((int)roundf(powf(255.f,(float)k/NBANDS)),1,254);
   binEnd[k]=clampi((int)roundf(powf(255.f,(float)(k+1)/NBANDS)),binBegin[k]+1,255);
 }
 M5.Display.setBrightness((uint8_t)(brightness*255/100));
 // V16.1 hardware hotfix: LGFX sprites default to internal heap unless PSRAM
 // is explicitly selected. A full RGB565 screen needs W*H*2 bytes (~438 KB),
 // which cannot fit safely in the ESP32-S3 internal heap.
 const size_t frameBytes=(size_t)W*(size_t)H*2;
 Serial.printf("Framebuffer requested=%u bytes; PSRAM total=%u free=%u largest=%u\n",
    (unsigned)frameBytes,(unsigned)ESP.getPsramSize(),(unsigned)ESP.getFreePsram(),
    (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM));
 cv.setColorDepth(16);
 cv.setPsram(true); // MUST come before createSprite(); unchanged renderer follows.
 if(ESP.getPsramSize()==0 || !cv.createSprite(W,H)){
   Serial.printf("PSRAM SPRITE ERROR: total=%u free=%u largest=%u request=%u\n",
      (unsigned)ESP.getPsramSize(),(unsigned)ESP.getFreePsram(),
      (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM),(unsigned)frameBytes);
   M5.Display.fillScreen(TFT_BLACK);M5.Display.setTextColor(TFT_RED);
   M5.Display.drawString("PSRAM SPRITE ERROR",22,H/2-20);
   if(ESP.getPsramSize()==0)M5.Display.drawString("PSRAM NOT DETECTED",22,H/2+10);
   else M5.Display.drawString("CHECK SERIAL LOG",22,H/2+10);
   while(true)delay(1000);
 }
 fb=(uint16_t*)cv.getBuffer();
 Serial.printf("Framebuffer OK; free PSRAM=%u bytes\n",(unsigned)ESP.getFreePsram());
 initArtwork();
 fidelityInit();
 M5.BtnA.setHoldThresh(730);M5.BtnB.setHoldThresh(850);
 M5.Power.setVibration(0);
 WiFi.mode(WIFI_OFF); // radio ONLY when harness remote enabled
 if(profile==2)setCpuFrequencyMhz(160);
 Serial.printf("StopWatch Native31 built for M5StopWatch; %dx%d, effects=%u, PSRAM=%u, static assets=%u bytes\n",
 W,H,VIS_COUNT,(unsigned)ESP.getPsramSize(),(unsigned)(EYE_ART_SIZE*EYE_ART_SIZE+512));
 vibration(170,65);
}
void loop(){
 M5.update();vibrationTick();controlButtons();controlTouch();radioTick();audioTick();beatTick();
 uint32_t now=millis();
 // Remote-only auto blank: the radio can still receive ACKs and A+B can wake.
 // No deep sleep during live audio or ESP-NOW; preserve responsiveness.
 if(remote && overlay==NONE && remoteIdleSecs>0 && !displayBlank && (now-lastRemoteActivity)>remoteIdleSecs*1000UL){
   displayBlank=true;M5.Display.setBrightness(0);
 }
 if(displayBlank){delay(12);return;}
 const int fps=powerFrameCap();
 if((uint32_t)(now-frameStamp)<(uint32_t)(1000/fps)){delay(1);return;}
 frameStamp=now;renderStamp=now;renderFrame();saveFrameStats();
}