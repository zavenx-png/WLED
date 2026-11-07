/*
 * Club StopWatch — Native31 V16 · Favorites / Scenes / Adaptive Power (ESP32-S3 / M5Stack StopWatch C152)
 * Native rendering, 24 effects, 10-second microphone auto-calibration, A+B remote.
 * Hardware-targeted source; not a conversion of browser Canvas APIs.
 * M5Stack board package >=3.3.8, M5Unified>=0.2.19, M5GFX>=0.2.21.
 * Licensing: new original source; see README for validation limitations.
 */
#include <Arduino.h>
#include <M5Unified.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ctype.h>
#include <limits.h>
#include <esp_wifi.h>
#include <esp_now.h>
#include <Preferences.h>
#include <math.h>
#include <esp_heap_caps.h>
#include "assets/NativeTextures.h"
// Arduino defines sq(x) as a macro, which conflicts with the native float helper.
#ifdef sq
#undef sq
#endif

constexpr uint8_t VIS_COUNT=32, Z_COUNT=28, REMOTE_PAGES=6;
constexpr int NFFT=512, MIC_SAMPLE_RATE=16000, NBANDS=32;
constexpr float PI_F=3.14159265358979323846f, TAU=6.28318530717958647692f;
static const char *const EFFECT_NAMES[VIS_COUNT] = {
 "zaven Chromatic EQ",
 "Robot Shuffle",
 "Warehouse House Duo",
 "Matrix Phosphor",
 "zaven Psychedelic Halo",
 "zaven Electric Aurora",
 "zaven Flowing Halo",
 "Prism EQ",
 "Psychedelic Bloom",
 "Vapor Scope",
 "Frequency Towers",
 "Radial Spectrum",
 "Heartbeat Neon",
 "Round Resonator",
 "Kaleidoscope Reactor",
 "Fibonacci Sunflower",
 "Infinity Tunnel",
 "Cymatic Mandala",
 "Orbital Harmonics",
 "Nebula Particles",
 "zaven Kaleidoscope",
 "zaven Round Resonator",
 "zaven Fibonacci Spiral",
 "Warp Drive Loom",
 "Spiral Spindle",
 "Tidal Cable Knit",
 "Spike Ball Nova (variant)",
 "String Theory",
 "FluidBox Classic",
 "Gravity Lab",
 "AirDrop Art Capsule",
 "Ghost Companion"
};
static constexpr uint8_t ORIGINAL_EFFECT_ID[VIS_COUNT]={23,22,20,4,24,29,30,1,2,3,6,7,8,9,10,11,12,13,17,19,25,26,27,44,51,53,63,64,65,66,68,69};
static inline uint8_t originalEffect(uint8_t i);
// V12 presentation order -> V13, prior to the two V16.8 effect removals.
static constexpr uint8_t V12_INDEX_TO_V13[31]={9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,0,1,2,3,4,5,6,7,8,28,29,30};
// 0xff indicates one of the removed effects, or an old cinematic effect.
static inline uint8_t v13ToV18(uint8_t n){
 if(n==5||n==8||n>=28)return 0xff;
 return n<5?n:(n<8?n-1:n-2);
}
static inline uint8_t fromOldEffect(uint8_t n,bool legacyV12){
 if(legacyV12){if(n>=31)return 0xff;n=V12_INDEX_TO_V13[n];}
 return v13ToV18(n);
}
static inline uint8_t toV19Effect(uint8_t n,uint8_t savedRevision){
 if(savedRevision>=15)return n<24?n:0xff;
 if(savedRevision<14)n=fromOldEffect(n,savedRevision<13);
 if(n>=26||n==19||n==21)return 0xff;
 return n<19?n:(n<21?n-1:n-2);
}
// V17.1 removes the standalone Ribbon menu item (V17 index 22).
// Prior 24-entry Robot Shuffle (23) shifts to the new final slot (22).
// Preserve every retained effect's stored favorites, gains and scenes.
static inline uint8_t toV171Effect(uint8_t n,uint8_t savedRevision){
 if(savedRevision>=16)return n<23?n:0xff;
 if(savedRevision<15)n=toV19Effect(n,savedRevision);
 if(n>=24||n==22)return 0xff;
 return n<22?n:n-1;
}


// V17.8 exact bijective remap of V17.1-V17.7 menu positions.
static constexpr uint8_t V171_TO_V178[23]={0,4,20,21,22,5,6,7,8,9,3,10,11,12,13,14,15,16,17,18,19,2,1};
static inline uint8_t toV178Effect(uint8_t n,uint8_t savedRevision){
 if(savedRevision>=17)return n<23?n:0xff;
 if(savedRevision<16)n=toV171Effect(n,savedRevision);
 if(n>=VIS_COUNT)return 0xff;
 return V171_TO_V178[n];
}

// V17.14 menu migration: the first 23 remain identical. Three former
// festival slots 25/26/30 become menu slots 23/24/25. Seven are removed.
// Old V17.13 per-effect data is READ but never overwritten for rollback.
static inline uint8_t toV1714Index(uint8_t n,bool fromV1713){
 if(n<23)return n;
 if(!fromV1713)return 0xff;
 if(n==25)return 23; // Star Marionette (was 26)
 if(n==26)return 24; // Warp Drive Loom (was 27)
 if(n==30)return 25; // Cosmic Patchwork Moth (was 31)
 return 0xff;
}
// V17.17: menu migration from V17.16 and V17.15 (first 31 IDs shared).
// Removed effects are intentionally omitted; retain last unreviewed Nova.
static inline uint8_t toV1717Index(uint8_t n){
 if(n<23)return n;
 if(n==24)return 23; // Warp Drive Loom
 if(n==28)return 24; // Spiral Spindle
 if(n==30)return 25; // Tidal Cable Knit
 if(n==40)return 26; // Spike Ball Nova, pending feedback
 return 0xff;
}
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
 prefs.putBytes("man1717",&micManualMask,sizeof(micManualMask));
}
String userText[VIS_COUNT];String sharedName="",blueRobot="",pinkRobot="";
static const String DEFAULT_WORD="zaven";
enum Overlay : uint8_t {NONE,NAME,BRIGHTNESS,RHYTHM,BATTERY,AP_PASSWORD};
Overlay overlay=NONE;
String inputName="";uint8_t robotField=0;
bool nameRobotBlueEdited=false,nameRobotPinkEdited=false,nameDeleteConsumed=false;
int remoteTouchXSum=0,remoteTouchYSum=0,remoteTouchSamples=0;
String keyboardPopup="";int keyboardPopupX=0,keyboardPopupY=0;
uint32_t keyboardPopupUntil=0,keyboardPopupStarted=0,brightnessRepeatAt=0;
bool brightnessEntryHold=false,batteryEntryHold=false;
uint8_t candidateBrightness=52;float candidateBpm=125;
uint32_t batteryHudUntil=0,batteryKeyCooldown=0,batteryPowerPollAt=0;
int batteryHudPercent=0,batteryHudVoltage=0;bool batteryHudCharging=false;
// V17.7: one tiny in-RAM checkpoint. Estimate after every 5 percentage
// points consumed (100->95, 95->90, etc.) or 5 points below boot level.
// Sampling once a minute adds no extra rendering, Wi-Fi, or flash writes.
uint32_t batteryDrainLast=0,batteryCheckpointMs=0;
int batteryCheckpointPct=-1,batteryEstimatedMinutes=-1;
bool batteryPrevCharging=false;
// Persistent, compact NVS-backed favorites and complete per-scene parameters.
static constexpr uint64_t FAVORITE_DEFAULT=(1ULL<<22)|(1ULL<<7)|(1ULL<<16)|(1ULL<<1);
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
// V17.22 harness remote favorites are independent of the 31 local watch favorites.
static constexpr int REMOTE_FX_COUNT=Z_COUNT+WLED_COUNT;
static uint8_t remoteFavBits[(REMOTE_FX_COUNT+7)/8]={};
static uint16_t remoteFavScroll=0,remotePaletteScroll=0,remoteSettingsScroll=0;
static uint8_t remoteCustom[3]={128,128,16};
static bool remoteAwaitingApply=false;
// V17.26: Harness-AP station connection is a *separate* link from ESP-NOW.
// AP association may move the radio channel; ESP-NOW uses channel=0 (current).
// No AP password is ever sent over ESP-NOW or printed to serial.
static constexpr char HARNESS_AP_SSID[]="Harness-AP";
enum ApLinkState : uint8_t {AP_IDLE,AP_CONNECTING,AP_CONNECTED,AP_FAILED};
static ApLinkState apLink=AP_IDLE;
static String apPassword="wled1234",apDraft="";
static bool apAutoConnect=false,apWantConnected=false,apPasswordError=false;
static uint8_t apKeyboardMode=0; // 0 letters, 1 digits, 2 symbols
static bool apUpper=false,apWledChecked=false,apWledOnline=false;
static uint32_t apAttemptAt=0,apHttpAt=0,apLastRadioAckAt=0;
static uint8_t apFailReason=0;

static bool remoteIsFavorite(int i){return i>=0&&i<REMOTE_FX_COUNT&&(remoteFavBits[i/8]&(1u<<(i%8)));}
static void remoteToggleFavorite(int i){
 if(i<0||i>=REMOTE_FX_COUNT)return;
 remoteFavBits[i/8]^=(1u<<(i%8));
 prefs.putBytes("rfav1722",remoteFavBits,sizeof(remoteFavBits));
}
static int remoteFavCount(){int n=0;for(int i=0;i<REMOTE_FX_COUNT;i++)if(remoteIsFavorite(i))n++;return n;}
static int remoteFavIndexAt(int pos){for(int i=0;i<REMOTE_FX_COUNT;i++)if(remoteIsFavorite(i)&&pos--==0)return i;return -1;}
static String remoteEffectName(int i){return i<Z_COUNT?String(Z_NAMES[i]):String(WLED_NAMES[i-Z_COUNT]);}

static void storeLibrary(){prefs.putBytes("fav1717",&favoriteBits,sizeof(favoriteBits));prefs.putBool("favonly",favoritesOnly);}
static void vibration(uint8_t strength,int ms);
static inline int clampi(int v,int a,int b);
static void saveNames();
static void saveScene(uint8_t slot){
 if(slot>=4)return;SceneData v;v.magic=SCENE_MAGIC;v.effect=effectID;v.gain=gainPct[effectID];v.bright=brightness;v.profile=profile;
 v.palette=selectedPalette;v.harnessBrightness=remoteBrightness;v.speed=remoteSpeed;v.intensity=remoteIntensity;v.remoteFx=remoteSelected;
 v.adaptive=adaptiveRender?1:0;v.remoteIdle=remoteIdleSecs;
 sharedName.toCharArray(v.name,sizeof(v.name));blueRobot.toCharArray(v.blue,sizeof(v.blue));pinkRobot.toCharArray(v.pink,sizeof(v.pink));
 scenes[slot]=v;prefs.putBytes((String("sc1717")+String(slot)).c_str(),&v,sizeof(v));
 // Keep original SceneData binary layout compatible with V11-V15.
 prefs.putBool((String("scna1717")+String(slot)).c_str(),gainIsManual(effectID));
 vibration(170,72);
}
static void loadScene(uint8_t slot){
 if(slot>=4||scenes[slot].magic!=SCENE_MAGIC||scenes[slot].effect>=VIS_COUNT)return;
 const SceneData &v=scenes[slot];effectID=v.effect;brightness=clampi(v.bright,8,100);profile=v.profile%3;
 gainPct[effectID]=clampi(v.gain,20,250);micGain=gainPct[effectID]/100.f;
 sharedName=String(v.name);
 for(uint8_t i=0;i<VIS_COUNT;i++)if(ORIGINAL_EFFECT_ID[i]>=23 && ORIGINAL_EFFECT_ID[i]<=31)userText[i]=sharedName;
 blueRobot=String(v.blue);pinkRobot=String(v.pink);
 selectedPalette=clampi(v.palette,0,PAL_COUNT-1);remoteBrightness=v.harnessBrightness;remoteSpeed=v.speed;
 remoteIntensity=v.intensity;remoteSelected=clampi(v.remoteFx,0,Z_COUNT+WLED_COUNT-1);
 adaptiveRender=v.adaptive!=0;remoteIdleSecs=0;favoritesOnly=false;
 prefs.putUChar("eff1728",effectID);if(effectID<30)prefs.putUChar("eff1717",effectID);prefs.putUChar("bright",brightness);prefs.putUChar("profile",profile);
 prefs.putBool("adaptive",adaptiveRender);prefs.putUChar("idle",remoteIdleSecs);prefs.putBytes("gain1728",gainPct,sizeof(gainPct));
 storeLibrary();saveNames();M5.Display.setBrightness((uint8_t)(brightness*255/100));
 // No automatic transmissions from a recalled scene: avoid unexpected harness changes.
 if(!remote)setCpuFrequencyMhz(profile==2?160:240);
 overlay=NONE;vibration(180,85);
}
static void clearScene(uint8_t slot){if(slot>=4)return;scenes[slot]=SceneData();prefs.remove((String("sc1717")+String(slot)).c_str());vibration(140,70);}
static uint8_t nextFavorite(uint8_t start,int dir){
 for(int n=0;n<VIS_COUNT;n++){start=(start+VIS_COUNT+(dir<0?-1:1))%VIS_COUNT;if(isFavorite(start))return start;}return start;
}
static uint8_t powerFrameCap();

uint32_t buzzUntil=0;bool buzzEnabled=true;
uint32_t keyADown=0,keyBDown=0;bool keyChord=false,keyAHeld=false,keyBHeld=false;
int touchStartX=0,touchStartY=0,touchLastX=0,touchLastY=0;
int32_t nameTouchXSum=0,nameTouchYSum=0;
uint8_t nameTouchSamples=0;
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
static inline const String& artName(uint8_t i){(void)i;return sharedName.length()?sharedName:DEFAULT_WORD;}
static inline void glyph(char ch,int x,int y,uint16_t c,int size=4){char b[2]={ch,0};cv.setTextDatum(middle_center);cv.setTextColor(c);cv.drawString(b,x,y,size);}
static inline void vibration(uint8_t strength=145,int ms=38){if(remote || !buzzEnabled)return;M5.Power.setVibration(strength);buzzUntil=millis()+ms;}
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
 MIC_SPECTRUM, MIC_ENERGY, MIC_ENERGY, MIC_WAVE, MIC_BASS, MIC_ENERGY, MIC_ENERGY, MIC_SPECTRUM, MIC_ENERGY, MIC_ENERGY, MIC_SPECTRUM, MIC_SPECTRUM, MIC_WAVE, MIC_ENERGY, MIC_ENERGY, MIC_ENERGY, MIC_BASS, MIC_BASS, MIC_SPECTRUM, MIC_ENERGY, MIC_ENERGY, MIC_SPECTRUM, MIC_BASS, MIC_FIXED, MIC_BASS, MIC_ENERGY, MIC_SPECTRUM, MIC_ENERGY, MIC_FIXED, MIC_ENERGY, MIC_FIXED, MIC_FIXED
};
static_assert(sizeof(AUTO_MIC_TYPES)/sizeof(AUTO_MIC_TYPES[0])==VIS_COUNT,"auto mic profile count");
static constexpr float AUTO_MIC_TARGET=.80f;
// V16.9: peak-window AGC uses a full 10-second history of sound levels.
// Use the ACTUAL ungained control inputs
// that drive each effect (FFT bins/bass/energy/wave) rather than an arbitrary
// hard minimum. About 85% of the 150-ms music peaks is aimed at 80% travel.
// This never changes any effect's drawing code or the automatic beat engine.
static constexpr int AUTO_BUCKETS=40;
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
 if(remote||effectID>=VIS_COUNT)return;
 const uint32_t now=millis();
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
 if(now-h.bucketAt>=250){
   h.peaks[h.next]=h.bucketMax;
   h.next=(h.next+1)%AUTO_BUCKETS;
   if(h.count<AUTO_BUCKETS)h.count++;
   h.bucketMax=base;
   h.bucketAt=now;
 }
 if(h.count<AUTO_BUCKETS||now-h.lastAdjust<750)return;
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
  if(remote || originalEffect(effectID)>=68 || (originalEffect(effectID)>=41 && originalEffect(effectID)<=48) || !M5.Mic.isEnabled())return;
 if(pendingMic){if(!M5.Mic.isRecording()){pendingMic=false;processMic();}}
 else pendingMic=M5.Mic.record(pcm,NFFT,MIC_SAMPLE_RATE);
}
static void beatTick(){static uint32_t last=0;uint32_t now=millis();if(!last){last=now;return;}uint32_t d=now-last;last=now;
 beatClock+=clampf(d*.001f,0,.05f)*bpm/60.f;
 if(now-micStamp>2500){beatConfidence=smooth(beatConfidence,0,.009f);
  // Continue animating in silence; reacquire tempo without sudden jumps when music returns.
  if(now-beatStamp>12000){bpm=smooth(bpm,125.f,.0013f);onsetNum=0;}
 }
}
static void saveNames(){
 prefs.putString("globalname",sharedName);
 for(int i=0;i<VIS_COUNT;i++)if(isTextEffect(i)){
   userText[i]=sharedName;
   prefs.putString((String("t1717")+String(i)).c_str(),sharedName);
 }
 prefs.putString("bluerobot",blueRobot);prefs.putString("pinkrobot",pinkRobot);
}
static void loadSettings(){
 prefs.begin("clubnative31",false);
 apPassword=prefs.getString("hap_pw","wled1234");
 apAutoConnect=prefs.getBool("hap_auto",false);

 // V17.14 uses separate index-based NVS keys so V17.13 rollback remains safe.
 const bool hasV1717=prefs.getUChar("s1717",0)==22;
 const bool hasV1716=prefs.getUChar("s1716",0)==21;
 const bool hasV1715=prefs.getUChar("s1715",0)==20;
 const bool hasV1714=prefs.getUChar("s1714",0)==19;
 const bool hasV1712=prefs.getUChar("s1712",0)==18;
 const bool hasV178=prefs.getUChar("s178",0)==17;
 const bool hadSaved=hasV1717||hasV1716||hasV1715||hasV1714||hasV1712||hasV178||
       prefs.isKey("effect")||prefs.isKey("favs")||prefs.isKey("gains")||prefs.isKey("scn0");
 const uint8_t sortVersion=(hasV1717||hasV1716||hasV1715||hasV1714||hasV1712||hasV178)?17:prefs.getUChar("sortrev",12);
 const bool legacyV12=hadSaved&&sortVersion<13;
 const bool remapToV178=hadSaved&&!hasV1717&&!hasV1716&&!hasV1715&&!hasV1714&&!hasV1712&&!hasV178&&sortVersion<17;
 // For an old 33-slot menu, only index 25->23, 26->24 and 30->25
 // survive; seven discarded String modes are deliberately not migrated.
 const auto mapSlot=[&](uint8_t slot)->uint8_t{
  if(hasV1717)return slot<VIS_COUNT?slot:0xff;
  if(hasV1716)return toV1717Index(slot);
  if(hasV1715)return slot<31?toV1717Index(slot):0xff;
  if(hasV1714)return slot<26?toV1717Index(slot):0xff;
  if(remapToV178)slot=toV178Effect(slot,sortVersion);
  return toV1717Index(toV1714Index(slot,hasV1712));
 };
 const int sourceCount=prefs.getUChar("s1728",0)==1?32:(hasV1717?30:(hasV1716?41:(hasV1715?31:(hasV1714?26:(hasV1712?33:(remapToV178?31:23))))));
 const char *effectKey=hasV1717?"eff1717":(hasV1716?"eff1716":(hasV1715?"eff1715":(hasV1714?"eff1714":(hasV1712?"eff1712":(hasV178?"eff178":"effect")))));
 const char *favoriteKey=hasV1717?"fav1717":(hasV1716?"fav1716":(hasV1715?"fav1715":(hasV1714?"fav1714":(hasV1712?"fav1712":(hasV178?"fav178":"favs")))));
 const char *manualKey=hasV1717?"man1717":(hasV1716?"man1716":(hasV1715?"man1715":(hasV1714?"man1714":(hasV1712?"man1712":(hasV178?"man178":"micmodes")))));
 const char *gainKey=prefs.getUChar("s1728",0)==1?"gain1728":hasV1717?"gain1717":(hasV1716?"gain1716":(hasV1715?"gain1715":(hasV1714?"gain1714":(hasV1712?"gain1712":(hasV178?"gain178":"gains")))));
 const char *sceneStem=hasV1717?"sc1717":(hasV1716?"sc1716":(hasV1715?"sc1715":(hasV1714?"sc1714":(hasV1712?"sc1712":(hasV178?"sc178":"scn")))));
 const char *textStem=hasV1717?"t1717":(hasV1716?"t1716":(hasV1715?"t1715":(hasV1714?"t1714":(hasV1712?"t1712":(hasV178?"t178":"t")))));

 const uint8_t selected=prefs.getUChar("s1728",0)==1 ? prefs.getUChar("eff1728",0) : mapSlot(prefs.getUChar(effectKey,0));
 effectID=selected<VIS_COUNT?selected:0;
 brightness=clampi(prefs.getUChar("bright",52),8,100);
 profile=prefs.getUChar("profile",1)%3;

 uint64_t oldFavorites=FAVORITE_DEFAULT;
 if(prefs.getBytes(favoriteKey,&oldFavorites,sizeof(oldFavorites))!=sizeof(oldFavorites)){
  oldFavorites=remapToV178
    ? ((1ULL<<4)|(1ULL<<9)|(1ULL<<19)|(1ULL<<27)|(1ULL<<28)|(1ULL<<29)|(1ULL<<30))
    : FAVORITE_DEFAULT;
  if(legacyV12)oldFavorites=(1ULL<<0)|(1ULL<<10)|(1ULL<<18)|(1ULL<<23)|(1ULL<<28)|(1ULL<<29)|(1ULL<<30);
 }
 favoriteBits=0;
 for(int i=0;i<sourceCount;i++)if(oldFavorites&(1ULL<<i)){
  const uint8_t dest=mapSlot((uint8_t)i);
  if(dest<VIS_COUNT)favoriteBits|=1ULL<<dest;
 }
 favoriteBits&=((1ULL<<VIS_COUNT)-1ULL);
 favoritesOnly=false;

 // Match current auto mic behavior. Retain stored compatibility, but all
 // auto/manual controls continue in their prior automatically calibrated mode.
 uint64_t storedManual=0;
 if(prefs.getBytes(manualKey,&storedManual,sizeof(storedManual))!=sizeof(storedManual))storedManual=0;
 micManualMask=0;

 adaptiveRender=prefs.getBool("adaptive",true);
 remoteIdleSecs=0; // Retain Preferences schema, but disable automatic screen off.
 if(prefs.getBytesLength("rfav1722")==sizeof(remoteFavBits))prefs.getBytes("rfav1722",remoteFavBits,sizeof(remoteFavBits));
 for(uint8_t j=0;j<4;j++){
  SceneData v;
  const size_t n=prefs.getBytes((String(sceneStem)+String(j)).c_str(),&v,sizeof(v));
  if(n!=sizeof(v)||v.magic!=SCENE_MAGIC)continue;
  const uint8_t dest=mapSlot(v.effect);
  if(dest>=VIS_COUNT)continue; // Ignore scenes bound to deleted effects.
  v.effect=dest;scenes[j]=v;
 }
 rxChannel=clampi(prefs.getUChar("radio",1),1,11);

 // Important: older releases stored up to 41 bytes. Never read them into a 27-byte
 // destination. The full legacy buffer is temporary stack storage only.
 uint8_t oldGain[41]={};
 const size_t nGain=prefs.getBytesLength(gainKey);
 if(nGain>0&&nGain<=sizeof(oldGain))prefs.getBytes(gainKey,oldGain,nGain);
 for(uint8_t i=0;i<VIS_COUNT;i++)gainPct[i]=135;
 for(int i=0;i<sourceCount&&i<(int)sizeof(oldGain);i++){
  const uint8_t dest=mapSlot((uint8_t)i);
  if(dest>=VIS_COUNT)continue;
  if(oldGain[i]>=20&&oldGain[i]<=250)gainPct[dest]=oldGain[i];
 }

 for(int i=0;i<sourceCount;i++){
  const uint8_t dest=mapSlot((uint8_t)i);
  if(dest>=VIS_COUNT||!isTextEffect(dest))continue;
  userText[dest]=prefs.getString((String(textStem)+String(i)).c_str(),"");
 }
 if(prefs.isKey("globalname"))sharedName=prefs.getString("globalname","");
 else{
  if(isTextEffect(effectID)&&userText[effectID].length())sharedName=userText[effectID];
  else for(uint8_t i=0;i<VIS_COUNT;i++)if(isTextEffect(i)&&userText[i].length()){
   sharedName=userText[i];break;
  }
  prefs.putString("globalname",sharedName);
 }
 for(uint8_t i=0;i<VIS_COUNT;i++)if(isTextEffect(i))userText[i]=sharedName;
 blueRobot=prefs.getString("bluerobot","");
 pinkRobot=prefs.getString("pinkrobot","");
 micGain=gainPct[effectID]/100.f;

 // V17.17: remap all retained V17.16 slots into independent keys; preserve older NVS for rollback.
 // On upgrade write new ordering ONCE and mark completion LAST.
 // These writes do not touch V17.13's effect/gain/favorite/scene keys.
 if(!hasV1717){
  prefs.putUChar("eff1717",effectID);
  prefs.putBytes("fav1717",&favoriteBits,sizeof(favoriteBits));
  prefs.putBytes("man1717",&micManualMask,sizeof(micManualMask));
  prefs.putBytes("gain1728",gainPct,sizeof(gainPct));
  for(uint8_t i=0;i<VIS_COUNT;i++)if(isTextEffect(i))
   prefs.putString((String("t1717")+String(i)).c_str(),userText[i]);
  for(uint8_t j=0;j<4;j++)if(scenes[j].magic==SCENE_MAGIC)
   prefs.putBytes((String("sc1717")+String(j)).c_str(),&scenes[j],sizeof(SceneData));
  prefs.putUChar("s1717",22);
 }
 // Always boot into 01 Chromatic EQ, unchanged from V17.8 onward.
 effectID=0;
 micGain=gainPct[effectID]/100.f;
}
static void persistEffect(){prefs.putUChar("eff1728",effectID);if(effectID<30)prefs.putUChar("eff1717",effectID);micGain=gainPct[effectID]/100.f;}
static void radioSend(uint8_t cmd,uint8_t value){
 if(!radioReady)return;
 uint32_t seq=++remoteSeq;uint8_t payload[12]={'Z','H','R','1',1,cmd,value,0,
   (uint8_t)seq,(uint8_t)(seq>>8),(uint8_t)(seq>>16),(uint8_t)(seq>>24)};
 radioPending=(esp_now_send(BROADCAST,payload,sizeof(payload))==ESP_OK);remoteLast=millis();radioSuccess=false;remoteAwaitingApply=true;
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
 if(a.ready&&a.seq==remoteSeq){radioPending=false;radioSuccess=a.status==0;remoteAwaitingApply=false;
  if(a.status==0)apLastRadioAckAt=millis(); /* No harness haptics on ACK. */}}
 if(radioPending&&millis()-remoteLast>1500)radioPending=false;}
// ESP-NOW and the AP must use the same physical 2.4GHz channel. A broadcast
// peer with channel 0 follows the Wi-Fi station channel after AP association.
static void initEspNowOnCurrentChannel(){
 if(radioReady)return;
 radioReady=esp_now_init()==ESP_OK;
 if(radioReady){
  esp_now_register_recv_cb(radioCallback);
  esp_now_peer_info_t peer={};memcpy(peer.peer_addr,BROADCAST,6);
  peer.channel=0;peer.encrypt=false;
  if(esp_now_add_peer(&peer)!=ESP_OK){esp_now_deinit();radioReady=false;}
 }
}
static void startRadio(){
 WiFi.mode(WIFI_STA);WiFi.disconnect(false,false);WiFi.setSleep(false);
 esp_wifi_set_promiscuous(true);
 esp_err_t channelStatus=esp_wifi_set_channel(rxChannel,WIFI_SECOND_CHAN_NONE);
 esp_wifi_set_promiscuous(false);if(channelStatus!=ESP_OK){radioReady=false;return;}
 initEspNowOnCurrentChannel();
}
static bool harnessApAssociated(){
 return WiFi.status()==WL_CONNECTED&&WiFi.SSID()==HARNESS_AP_SSID;
}
static void harnessApStartConnect(){
 if(!remote)return;
 apWantConnected=true;apLink=AP_CONNECTING;apFailReason=0;
 apWledChecked=false;apWledOnline=false;apHttpAt=0;
 apAttemptAt=millis();
 WiFi.mode(WIFI_STA);WiFi.setSleep(false);
 // Nonblocking association: do not interrupt on-watch UI rendering.
 WiFi.begin(HARNESS_AP_SSID,apPassword.c_str());
}
static void harnessApDisconnect(bool rememberPreference){
 apWantConnected=false;apLink=AP_IDLE;apWledChecked=false;apWledOnline=false;
 if(rememberPreference){apAutoConnect=false;prefs.putBool("hap_auto",false);}
 WiFi.disconnect(false,false); // Keep the station interface for ESP-NOW.
 if(radioReady){esp_now_deinit();radioReady=false;}
 rxChannel=clampi(prefs.getUChar("radio",1),1,11);
 if(remote)startRadio();
}
static void harnessApTick(){
 if(!remote)return;
 const uint32_t now=millis();
 const bool associated=harnessApAssociated();
 if(associated){
  if(!apWantConnected){WiFi.disconnect(false,false);return;}
  if(apLink!=AP_CONNECTED){
   apLink=AP_CONNECTED;apWledChecked=false;apWledOnline=false;apHttpAt=0;
   rxChannel=(uint8_t)clampi(WiFi.channel(),1,13); // AP dictates the real channel.
   initEspNowOnCurrentChannel();
  }
  // Verify WLED itself, rather than assuming an associated AP is a working
  // controller. Keep probes only on Page 6, spaced out, with short timeouts.
  if(remotePage==5 && (!apWledChecked||now-apHttpAt>12000UL)){
   apHttpAt=now;apWledChecked=true;
   String gateway=WiFi.gatewayIP().toString();
   if(gateway=="0.0.0.0")gateway="4.3.2.1";
   WiFiClient client;HTTPClient http;
   http.setConnectTimeout(350);http.setTimeout(600);
   bool ok=false;
   if(http.begin(client,String("http://")+gateway+"/json/info")){
    ok=http.GET()==HTTP_CODE_OK;
    http.end();
   }
   apWledOnline=ok;
  }
  return;
 }
 if(apLink==AP_CONNECTED){
  // A lost AP link should not silently report CONNECTED. Return ESP-NOW
  // to the manual channel; do not restart Wi-Fi while a connection is active.
  apLink=AP_FAILED;apFailReason=3;apWledChecked=false;apWledOnline=false;
  if(radioReady){esp_now_deinit();radioReady=false;}
  rxChannel=clampi(prefs.getUChar("radio",1),1,11);startRadio();
 }
 if(apLink==AP_CONNECTING){
  const wl_status_t status=WiFi.status();
  if(status==WL_NO_SSID_AVAIL || status==WL_CONNECT_FAILED || now-apAttemptAt>14000UL){
   apFailReason=status==WL_NO_SSID_AVAIL?1:status==WL_CONNECT_FAILED?2:3;
   apLink=AP_FAILED;WiFi.disconnect(false,false);
   if(radioReady){esp_now_deinit();radioReady=false;}
   rxChannel=clampi(prefs.getUChar("radio",1),1,11);startRadio();
  }
 }
}
static void setRemote(bool on){if(remote==on)return;remote=on;overlay=NONE;remotePage=0;remoteSettingsScroll=0;displayBlank=false;lastRemoteActivity=millis();
 // Remote-only safeguard: restore the original visualizer's default native text
 // state before its next frame. The 31 effect renderers and their frame pacing
 // are intentionally unchanged from V17.21.1.
 if(!remote){cv.setFont(&lgfx::fonts::Font0);cv.setTextSize(1.0f);cv.setTextDatum(top_left);frameStamp=0;}
 M5.Display.setBrightness((uint8_t)(brightness*255/100));radioSuccess=false;radioPending=false;
 if(remote){M5.Mic.end();M5.Power.setVibration(0);buzzUntil=0;}
 else{
  // Returning to the normal effects always shuts down AP Wi-Fi; never keep
  // a radio association running in visualizer mode.
  apWantConnected=false;apLink=AP_IDLE;apWledChecked=false;apWledOnline=false;
  rxChannel=clampi(prefs.getUChar("radio",1),1,11);
  M5.Mic.begin();pendingMic=false;
 }
 // No vibration when entering or leaving harness mode.
}

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
 line(shoulderX,sy,hx,hip,c,robot?4:3);line(shoulderX-21*scale,sy,shoulderX+21*scale,sy,c,robot?3:2);
 float elbowsY=sy+28*scale, handsY=sy+33*scale+28*scale*sinf(TAU*t/4+.8f);
 line(shoulderX-21*scale,sy,shoulderX-39*scale-17*scale*step,elbowsY,c,robot?3:2);
 line(shoulderX-39*scale-17*scale*step,elbowsY,shoulderX-43*scale-23*scale*step,handsY,c,robot?3:2);
 line(shoulderX+21*scale,sy,shoulderX+39*scale+17*scale*step,elbowsY,c,robot?3:2);
 line(shoulderX+39*scale+17*scale*step,elbowsY,shoulderX+43*scale+18*scale*step,sy+2*scale-20*scale*sinf(TAU*t/4),c,robot?3:2);
 float footBase=hip+61*scale;
 line(hx,hip,hx-22*scale-19*scale*step,hip+35*scale-13*scale*fmaxf(0,step),c,robot?4:3);
 line(hx-22*scale-19*scale*step,hip+35*scale-13*scale*fmaxf(0,step),hx-32*scale-20*scale*step,footBase-20*scale*fmaxf(0,step),c,3);
 line(hx,hip,hx+22*scale+19*scale*step,hip+35*scale+13*scale*fminf(0,step),c,robot?4:3);
 line(hx+22*scale+19*scale*step,hip+35*scale+13*scale*fminf(0,step),hx+32*scale+20*scale*step,footBase+20*scale*fminf(0,step),c,3);
}
static void eqBars(bool mirrored=false){
 // V16.11: one continuous set of bars built UPWARD from each column's
 // true circular-bottom point; no separate reflection or horizontal seam.
 const int count=29,span=W-12,slot=span/count,oldGround=CY+143;
 for(int i=0;i<count;i++){
   float v=bins[clampi((int)(i*NBANDS/(float)count),0,NBANDS-1)];
   int originalH=18+(int)(clampf(v*.9f+beatPulse()*.05f)*300);
   int x=6+i*slot;uint16_t co=hue(i*.023f+.43f);
   if(mirrored){
     for(int yy=0;yy<originalH;yy+=10){
       uint16_t col=yy>originalH*.8f?rgb(253,112,174):yy>originalH*.46f?hue(.45f+i*.015f):co;
       cv.fillRoundRect(x,CY-yy-8,slot-2,8,2,tint(col,.35f+.55f*(float)yy/fmaxf(originalH,1)));
       cv.fillRoundRect(x,CY+yy,slot-2,7,2,tint(col,.46f));
     }
     continue;
   }
   float dx=x+slot*.5f-CX;
   int bottom=CY+(int)sqrtf(fmaxf(0.f,(float)(R-3)*(R-3)-dx*dx));
   // Preserve top-bar position from the old rendering wherever the circular
   // bezel allows it, filling the whole height using ONE identical bar style.
   int fullH=clampi(originalH+bottom-oldGround,8,H);
   for(int yy=0;yy<fullH;yy+=10){
     float q=(float)yy/fmaxf((float)fullH,1.f);
     uint16_t col=yy>fullH*.8f?rgb(253,112,174):yy>fullH*.46f?hue(.45f+i*.015f):co;
     int y=bottom-yy-8;
     cv.fillRoundRect(x,y,slot-2,8,2,tint(col,.35f+.55f*q));
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
   // No orbiting white dots around the disco ball.
   // V6 crossover path: ease over 8 beats, pass front/back smoothly.
   float q=phase8(),pass=.5f-.5f*cosf(q*TAU),fore=sinf(q*TAU);
   float xBlue=CX-96+192*pass,xPink=CX+96-192*pass;
   float yBlue=289+11*fore,yPink=289-11*fore;
   for(int j=0;j<20;j++)cv.fillRect(6+j*23,H-25-int(55*band(j+2)),15,8+int(55*band(j+2)),tint(hue(.55f+j*.055f),.20f+.30f*energy));
   if(fore<0){figure(xPink,yPink,.96f,beatClock+2,rgb(255,86,188),true);figure(xBlue,yBlue,.96f,beatClock,rgb(72,222,255),true);}
   else {figure(xBlue,yBlue,.96f,beatClock,rgb(72,222,255),true);figure(xPink,yPink,.96f,beatClock+2,rgb(255,86,188),true);}
   // V17.6: revert Robot Shuffle names to the exact V17.4 font size.
   if(blueRobot.length())textC(blueRobot,(int)xBlue,(int)(yBlue-116),rgb(109,233,255),4);
   if(pinkRobot.length())textC(pinkRobot,(int)xPink,(int)(yPink-116),rgb(255,144,207),4);
  break;}
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

#include "assets/BrowserFiveRenderer.h"
static void textEffects(uint8_t id){
 if(id>=23&&id<=27&&renderBrowserFive(id))return; // V16.2 effects 1-5 only

 black();float t=T(),q=beatClock,norm=clampf(.24f+energy);
 const String &word=artName(effectID);uint16_t purple=rgb(209,72,255),cyan=rgb(69,231,246),pink=rgb(254,83,173);
 int wid=word.length();
 switch(id){
 case 23:{ // Each character independently responds to its own EQ band.
  int n=clampi(wid,1,12), spacing=clampi(390/n,30,76),begin=CX-(n-1)*spacing/2;
  for(int i=0;i<n;i++){float a=band(clampi(3+i*NBANDS/n,0,31));float y=CY-(a+.10f*beatPulse())*168+16;
   glyph(word[i],begin+i*spacing,y,hue(i/(float)n),7);
  }break;}
 case 24:{ // Original halo, only backdrop sound-reactive. Text waves independently.
  for(int j=0;j<24;j++){float a=j*TAU/24+t*.19f,r=91+72*sinf(j*.56f+beatClock*.11f)+36*energy;
    glow(CX+r*cosf(a),CY+r*sinf(a),3,hue(j*.043f+t*.022f));}
  for(int j=0;j<10;j++)circ(CX,CY,42+j*17,tint(hue(j*.06f+t*.036f),.13f+.13f*norm));
  for(int j=0;j<wid;j++){float x=CX-(wid-1)*17+j*34.f;
    float y=CY+11*sinf(t*.88f+j*.55f);glyph(word[j],(int)x,(int)y,pink,4);}
  break;}
 case 25:{ // Kaleidoscope: label integrated into wedge pattern.
  for(int k=0;k<12;k++){float a=k*TAU/12+t*.09f;
    spoke(CX,CY,29,223,a,tint(hue(k*.083f),.24f),3);
    for(int m=0;m<4;m++){float r=58+m*39+7*sinf(beatClock*.15f+k);
     float x=CX+r*cosf(a+.14f*sinf(m+t*.14f)),y=CY+r*sinf(a+.14f*sinf(m+t*.14f));
     glow(x,y,2+(m%3),hue(k*.075f+m*.05f));}}
  nameGlyphsOnRing(word,141,t*.13f+norm*.04f,cyan);break;}
 case 26:{ // Round Resonator: letters on rings only, no stationary central text.
  for(int j=0;j<12;j++){float r=26+j*17+8*sinf(q*.25f+j*.4f);
    circ(CX,CY,r,tint(hue(.52f+j*.045f),.25f+.28f*norm),2);}
  nameGlyphsOnRing(word,141,t*.19f,pink);nameGlyphsOnRing(word,89,-t*.12f,cyan);break;}
 case 27:{ // Restored V6 composition: 41 *complete*, individually rotated words.
  const float golden=2.39996322972865f;
  const float travel=t*.092f + beatClock*.002f; // smooth movement, sound modulation; never stalls in silence
  const float breath=1+.05f*bass+.027f*energy;
  for(int i=0;i<120;i++){
   float f=frac(i/120.f+travel*.38f),r=(14+217*sqrtf(f))*breath,a=i*golden+travel*.21f;
   cv.drawPixel(CX+(int)(cosf(a)*r),CY+(int)(sinf(a)*r),tint(hue(frac(i*.071f+t*.012f)),.24f+.43f*(1-f)));
  }
  for(int i=0;i<41;i++){
   const float f=frac(i/41.f+travel),r=(16+218*sqrtf(f))*breath,a=i*golden+travel*.35f;
   const float wordWidth=fminf(96,fmaxf(56,wid*13.f))*(.40f+f*.50f);
   const float pulse=.55f+.25f*beatPulse()+.17f*energy;
   const float fade=clampf(f*8)*clampf((1-f)*6)*pulse;
   // Most luminous near middle radius; dimly fade in/out as words circulate.
   fibStampWord(word,CX+cosf(a)*r,CY+sinf(a)*r,wordWidth,a+PI_F*.5f,tint(hue(frac(i*.137f+.78f)),fade));
  }
  circ(CX,CY,25+13*bass,tint(hue(.63f),.32f+.24f*energy),2);
  break;}
 case 28:case 31:{ // Portal & orbit: 16-beat BPM-locked turn, name either center or orbit.
  float rot=phase16()*TAU;
  for(int j=0;j<23;j++){float r=26+j*8+8*sinf(q*.06f+j*.2f);
    arc(CX,CY,r,rot+j*.4f,rot+j*.4f+PI_F*.85f,tint(hue(j*.045f+.59f),.25f+.48f*norm),2);}
  if(id==28){glow(CX,CY,36,cyan);glassText(word,CX,CY,pink,clampi(5-(wid>8),3,5));}
  else nameGlyphsOnRing(word,158,rot,pink);
  break;}
 case 29:{ // Aurora: each letter is attached to a flowing curtain (no center name).
  for(int i=0;i<24;i++){float x=15+i*19,yy=100+45*sinf(i*.23f+t*.24f);
   line(x,yy,x+14,yy+210,tint(hue(.43f+i*.024f),.19f+.25f*norm),5);}
  for(int i=0;i<wid;i++){float x=70+i*(W-140)/(float)clampi(wid-1,1,12);
    float yy=170+48*sinf(t*.28f+i*.4f);glyph(word[i],x,yy,cyan,4);}
  break;}
 case 30:{ // Text flows *with* the waves; original halo at 24 remains.
  for(int k=0;k<12;k++){float oldx=0,oldy=CY;
    for(int i=1;i<75;i++){float x=i*W/75.f,y=CY+45*sinf(x*.021f+t*.28f+k*.3f)+7*k;
      line(oldx,oldy,x,y,tint(hue(k*.07f+.59f),.23f+.35f*norm),2);oldx=x;oldy=y;}}
  for(int i=0;i<wid;i++){float x=CX-(wid-1)*18+i*36.f;
    glyph(word[i],x,CY+45*sinf(x*.021f+t*.28f),pink,4);}
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
#include "assets/FidelityClassic167.h"
#include "assets/FidelityText167.h"
#include "assets/StringFestivalV1714.h"
#include "assets/StringFestivalV1715.h"
#include "assets/ImportedAudioFXV1716.h"
#include "assets/StringTheoryV1719.h"
#include "assets/GravityLabV1720.h"
#include "assets/ArtCapsuleV1728.h"
#include "assets/GhostCompanionV1728.h"
static void renderEffect(){
 const uint8_t id=originalEffect(effectID);
 if(id==64){StringTheory::render();return;}
 if(id==65||id==66){GravityLab::render(id==65);return;}
 if(id==68){ArtCapsule::render();return;}
 if(id==69){GhostCompanion::render();return;}
 if(renderStringFestivalV1714(id))return; // original three, completely unchanged
 if(renderStringFestivalV1715(id))return; // five unchanged textile scenes
 if(renderImportedAudioV1716(id))return; // 10 imported/adapted audio faces, IDs 54..63
 if(fidelityClassic167(id)||fidelityText167(id))return;
 if(id<23)effectsClassic(id);
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
// Full-width one-screen QWERTY; exact same hit geometry used for rendering and touch.
// One letter per physical contact, picked by the CENTER of the touch blob.
// The closest physical key wins even if a finger overlaps 2-3 key hitboxes.
// V17.3: larger, roomy one-screen keyboard. These SAME shared constants
// drive the drawing AND the nearest-center touch hit testing.
static constexpr int QW_LEFT[3]={14,36,80};
static constexpr int QW_TOP[3]={155,224,293};
static constexpr int QW_PITCH=44,QW_WIDTH=41,QW_HEIGHT=61;
static constexpr int QW_DEL_X=138,QW_DEL_Y=382,QW_DEL_W=190,QW_DEL_H=52;
static inline bool deleteHit(int x,int y){
 // Generous invisible hit target extends beyond the button's visual border
 // (especially helpful when touching the lower curved screen edge).
 return x>=103&&x<=363&&y>=366&&y<=446;
}
static bool qwertyHit(int x,int y,char &out,int &cx,int &cy){
 static const char *const rows[3]={"qwertyuiop","asdfghjkl","zxcvbnm"};
 if(x<8||x>458||y<146||y>362)return false;
 long best=2147483647L;char candidate=0;int bx=0,by=0;
 for(int row=0;row<3;row++){
  const int yy=QW_TOP[row]+QW_HEIGHT/2;
  if(abs(y-yy)>37)continue;
  for(int k=0;k<(int)strlen(rows[row]);k++){
   const int xx=QW_LEFT[row]+k*QW_PITCH+QW_WIDTH/2;
   const long dx=x-xx,dy=y-yy;
   const long d=dx*dx+dy*dy;
   if(d<best){best=d;candidate=rows[row][k];bx=xx;by=yy;}
  }
 }
 if(!candidate||best>2200)return false;
 out=candidate;cx=bx;cy=by;return true;
}
// Password editor uses the watch's existing QWERTY geometry, with an
// uppercase switch and separate digit/symbol rows for WPA2 passwords.
static char harnessApKey(int row,int key){
 static const char *const alpha[3]={"qwertyuiop","asdfghjkl","zxcvbnm"};
 static const char *const digits[3]={"1234567890","@#$%&*-_=","+?./:;!"};
 static const char *const symbols[3]={"!@#$%^&*()","_-+=[]{}:",".,/\\?;~"};
 if(row<0||row>=3)return 0;
 const char *str=(apKeyboardMode==0?alpha:apKeyboardMode==1?digits:symbols)[row];
 if(key<0||key>=(int)strlen(str))return 0;
 const char c=str[key];return apKeyboardMode==0&&apUpper?(char)toupper((unsigned char)c):c;
}
static bool harnessApKeyHit(int x,int y,char &out){
 if(x<8||x>458||y<144||y>365)return false;
 long best=LONG_MAX;char result=0;
 for(int row=0;row<3;row++){
  const int yy=QW_TOP[row]+QW_HEIGHT/2;
  if(abs(y-yy)>38)continue;
  for(int k=0;harnessApKey(row,k);k++){
   const int xx=QW_LEFT[row]+k*QW_PITCH+QW_WIDTH/2;
   const long dx=x-xx,dy=y-yy,d=dx*dx+dy*dy;
   if(d<best){best=d;result=harnessApKey(row,k);}
  }
 }
 if(!result||best>2400)return false;
 out=result;return true;
}
static void openHarnessApPasswordEditor(){
 if(!remote)return;
 apDraft="";apUpper=false;apKeyboardMode=0;apPasswordError=false;
 keyboardPopup="";overlay=AP_PASSWORD;M5.Power.setVibration(0);
}
static void saveHarnessApPassword(){
 // WPA2 allows 8-63 characters; an empty password can connect to an open AP.
 if(apDraft.length() && apDraft.length()<8){apPasswordError=true;return;}
 apPassword=apDraft;prefs.putString("hap_pw",apPassword);
 overlay=NONE;apPasswordError=false;keyboardPopup="";
 if(apWantConnected){
  WiFi.disconnect(false,false);harnessApStartConnect();
 }
}
static void harnessApPasswordTouch(int x,int y){
 if(y>=376&&y<431){
  if(x>=107&&x<184){if(apKeyboardMode==0)apUpper=!apUpper;else apKeyboardMode=(apKeyboardMode==1?2:1);return;}
  if(x>=185&&x<276){apKeyboardMode=(apKeyboardMode+1)%3;return;}
  if(x>=277&&x<359){if(apDraft.length())apDraft.remove(apDraft.length()-1);apPasswordError=false;return;}
 }
 char c=0;
 if(harnessApKeyHit(x,y,c)&&apDraft.length()<63){apDraft+=c;apPasswordError=false;}
}
// Native vector-font drawing, no resampled bitmap scaling. Fits prominent UI
// labels to the available width while retaining the selected direct-native style.
static void drawLargeNativeLabel(const String &word,int x,int y,int maxWidth,
                                 float maxSize,uint16_t ink){
 if(!word.length())return;
 cv.setFont(&lgfx::fonts::FreeSansBold24pt7b);
 cv.setTextSize(1.f);
 float nativeW=fmaxf(1.f,(float)cv.textWidth(word));
 float scale=clampf((float)maxWidth/nativeW,.85f,maxSize);
 cv.setTextSize(scale);
 cv.setTextDatum(middle_center);
 cv.setTextColor(ink);
 cv.drawString(word,x,y);
 cv.setTextSize(1.f);
 cv.setFont(&lgfx::fonts::Font0);
}
static void keyboardPop(char letter,int x,int y){
 keyboardPopup=String(letter);
 keyboardPopupX=x;keyboardPopupY=y;
 keyboardPopupStarted=millis();
 keyboardPopupUntil=keyboardPopupStarted+455; // Visible for >10 frames at 30 fps.
}
// V16.12: smaller, clean native font tiers with NO fractional bitmap zoom.
// Heading: 18pt instead of 24pt. Both button labels: matching 12pt bold.
static void crispBrightnessText(const String &word,int x,int y,uint16_t ink,bool buttons=false){
 if(buttons)cv.setFont(&lgfx::fonts::FreeSansBold12pt7b);
 else cv.setFont(&lgfx::fonts::FreeSansBold18pt7b);
 cv.setTextSize(1.f);cv.setTextDatum(middle_center);cv.setTextColor(ink);
 cv.drawString(word,x,y);
 cv.setFont(&lgfx::fonts::Font0);
}
static void refreshBatteryHud(){
 batteryHudPercent=clampi(M5.Power.getBatteryLevel(),0,100);
 batteryHudVoltage=M5.Power.getBatteryVoltage();
 batteryHudCharging=(int)M5.Power.isCharging()==1;
}
static void sampleBatteryDrain(bool force=false){
 uint32_t now=millis();
 // PMIC read only once per minute during normal use. The HUD may request
 // one immediate reading when opened; zero persistent/NVS writes.
 if(!force&&batteryDrainLast&&now-batteryDrainLast<60000UL)return;
 batteryDrainLast=now;
 refreshBatteryHud();
 const int current=batteryHudPercent;
 if(batteryHudCharging||current<=0){
  batteryCheckpointPct=-1;batteryCheckpointMs=0;
  batteryEstimatedMinutes=-1;batteryPrevCharging=batteryHudCharging;
  return;
 }
 if(batteryPrevCharging||batteryCheckpointPct<0){
  // Capture 100% too: the first 100->95% interval must be measurable.
  batteryCheckpointPct=current;batteryCheckpointMs=now;
  batteryEstimatedMinutes=-1;batteryPrevCharging=false;
  return;
 }
 // Battery percentages can jitter by 1 point. Ignore minor increases,
 // and discard stale estimates after a larger increase/reconnection.
 if(current>batteryCheckpointPct){
  if(current>batteryCheckpointPct+1){
   batteryCheckpointPct=current;batteryCheckpointMs=now;
   batteryEstimatedMinutes=-1;
  }
  return;
 }
 const int drop=batteryCheckpointPct-current;
 if(drop<5)return;
 const uint32_t elapsed=now-batteryCheckpointMs; // wrap-safe millis difference
 if(elapsed>=60000UL&&elapsed<24UL*3600000UL){
  // Scale the measured 5-point interval to the current remaining percent.
  // Uses actual point change when a minute poll skipped an exact boundary.
  const float minutes=(float)elapsed*(float)current/
                       ((float)drop*60000.f);
  batteryEstimatedMinutes=clampi((int)lroundf(minutes),1,72*60);
 }
 // Rebase every 5 points; no 15-minute gate, history buffer or extra sensor.
 batteryCheckpointPct=current;batteryCheckpointMs=now;
}
static void openBatteryHud(bool haptic=true){
 sampleBatteryDrain(true);batteryHudUntil=millis()+4300;overlay=BATTERY;
 if(haptic)vibration(124,28);
}
static void renderBatteryHud(){
 // V17.6: user-approved minimal battery mock. No rings, horizontal meter,
 // voltage reading or graphic decorations; use the round display's real estate.
 black();
 cv.setTextDatum(middle_center);
 const uint16_t accent=batteryHudPercent>=55?rgb(0,231,225):
                       batteryHudPercent>=25?rgb(246,199,96):rgb(255,91,139);
 const uint16_t vividPink=rgb(249,64,225);
 // Heading kept inside the safe upper portion of the round screen.
 cv.setFont(&lgfx::fonts::FreeSansBold18pt7b);
 cv.setTextSize(1.12f);cv.setTextColor(accent);
 cv.drawString("BATTERY",CX,89);
 // Exactly 30% smaller than V17.5's 1.48x percentage.
 cv.setFont(&lgfx::fonts::FreeSansBold24pt7b);
 cv.setTextSize(1.036f);cv.setTextColor(rgb(252,253,255));
 cv.drawString(String(batteryHudPercent)+"%",CX,180);
 // Use a native 18pt font, ~25% smaller than V17.5's 24pt status.
 // Avoid downsampling a larger raster font: this keeps edges clean.
 cv.setFont(&lgfx::fonts::FreeSansBold18pt7b);
 cv.setTextSize(1.f);cv.setTextColor(vividPink);
 cv.drawString(batteryHudCharging?"CHARGING":"REMAINING",CX,262);
 // ETA recalculates on each 5-percentage-point discharge interval.
 String eta;
 if(batteryHudCharging)eta="NO ETA";
 else if(batteryEstimatedMinutes>=0){
  const int hh=batteryEstimatedMinutes/60,mm=batteryEstimatedMinutes%60;
  eta=String(hh)+"h "+String(mm)+"m";
 }else eta="ESTIMATING";
 cv.setFont(&lgfx::fonts::FreeSansBold24pt7b);
 cv.setTextSize(1.f);
 const int safeWidth=W-104;
 float nativeWidth=fmaxf(1.f,(float)cv.textWidth(eta));
 float scale=clampf((float)safeWidth/nativeWidth,.72f,1.22f);
 cv.setTextSize(scale);cv.setTextColor(rgb(252,253,255));
 cv.drawString(eta,CX,328);
 cv.setTextSize(1.f);
 // Uptime since this boot, computed from the existing system clock,
 // with no extra timer interrupts, sampling, or flash writes.
 const uint32_t uptimeMins=millis()/60000UL;
 const String uptime=String("POWERED ON  ")+String(uptimeMins/60UL)+
                     "H "+String(uptimeMins%60UL)+"M";
 cv.setFont(&lgfx::fonts::FreeSansBold12pt7b);
 cv.setTextColor(rgb(136,204,220));
 cv.drawString(uptime,CX,384);
 cv.setFont(&lgfx::fonts::Font0);
 textC("A/B TO CLOSE",CX,423,rgb(104,145,165),2);
}
static void renderOverlay(){
 if(overlay==NONE)return;
 const uint16_t CYAN=rgb(88,229,245), PINK=rgb(255,113,207), GRAY=rgb(169,206,230);
 switch(overlay){
 case BATTERY:{renderBatteryHud();break;}
 case BRIGHTNESS:{
  black();circ(CX,CY,R-3,rgb(42,92,126),2);
  // User-approved 147x82 button geometry and long-hold behavior are unchanged.
  crispBrightnessText("BRIGHTNESS",CX,87,rgb(105,239,253));
  crispBrightnessText(String(candidateBrightness)+"%",CX,181,rgb(253,253,255));
  uiBar(255,65,W-65,candidateBrightness/100.f,CYAN);
  cv.fillRoundRect(77,309,147,82,12,rgb(19,45,73));
  cv.drawRoundRect(77,309,147,82,12,rgb(98,154,184));
  cv.fillRoundRect(241,309,147,82,12,rgb(44,130,151));
  cv.drawRoundRect(241,309,147,82,12,rgb(183,255,255));
  crispBrightnessText("CANCEL",150,350,rgb(244,251,255),true);
  crispBrightnessText("OK",314,350,rgb(255,255,255),true);
  break;
 }
 case RHYTHM:{panelBg("AUTO BEAT ENGINE");
  textC(String((int)roundf(bpm))+" BPM",CX,169,rgb(255,255,255),6);
  textC(String((int)(beatConfidence*100))+"% CONFIDENCE",CX,229,PINK,3);
  textC("NO TAP / MANUAL SYNC",CX,260,GRAY,2);
  uiButton(82,306,300,54,String("PROFILE: ")+(profile==0?"SHOW":profile==1?"BALANCED":"ENDURANCE"),true);
  uiButton(153,379,160,45,"DONE");break;}
 case AP_PASSWORD:{
  black();circ(CX,CY,R-3,rgb(42,92,126),2);
  textC("HARNESS-AP KEY",CX,49,CYAN,3);
  textC("A CANCEL     B SAVE",CX,89,GRAY,2);
  textC(apPasswordError?"8+ CHARS OR EMPTY":String("PASSWORD  ")+String(apDraft.length())+" CHARS",CX,122,
        apPasswordError?rgb(255,130,120):GRAY,2);
  for(int row=0;row<3;row++)for(int k=0;harnessApKey(row,k);k++){
   int x=QW_LEFT[row]+k*QW_PITCH,y=QW_TOP[row];
   cv.fillRoundRect(x,y,QW_WIDTH,QW_HEIGHT,8,rgb(27,55,87));
   cv.drawRoundRect(x,y,QW_WIDTH,QW_HEIGHT,8,rgb(116,171,211));
   textC(String(harnessApKey(row,k)),x+QW_WIDTH/2,y+QW_HEIGHT/2,rgb(246,250,255),3);
  }
  const int xs[3]={111,191,279},ws[3]={69,80,70};
  const String names[3]={apKeyboardMode==0?(apUpper?"LOWER":"SHIFT"):(apKeyboardMode==1?"SYM":"123"),
      apKeyboardMode==0?"123":apKeyboardMode==1?"SYM":"ABC","DEL"};
  for(int k=0;k<3;k++){
   cv.fillRoundRect(xs[k],378,ws[k],48,8,rgb(32,97,125));
   cv.drawRoundRect(xs[k],378,ws[k],48,8,rgb(166,225,246));
   textC(names[k],xs[k]+ws[k]/2,402,rgb(246,250,255),2);
  }
  break;
 }
 case NAME:{
  black();circ(CX,CY,R-3,rgb(42,92,126),2);
  const bool robotName=originalEffect(effectID)==22;
  if(robotName){
   uiButton(112,40,112,43,"BLUE",robotField==0);
   uiButton(241,40,112,43,"PINK",robotField==1);
   const String& chosen=robotField==0?draftBlue:draftPink;
   if(chosen.length())
    drawLargeNativeLabel(chosen,CX,109,406,1.575f,robotField==0?CYAN:PINK);
  }else{
   if(inputName.length())drawLargeNativeLabel(inputName,CX,99,390,1.575f,PINK);
   textC("A CANCEL    B SAVE",CX,42,GRAY,2);
  }
  // All 26 keys are visible at once, 41x61 instead of V17.2's 39x53.
  static const char *const kRows[3]={"qwertyuiop","asdfghjkl","zxcvbnm"};
  uint32_t now=millis();
  bool popActive=keyboardPopup.length()&&(touchDown||(int32_t)(keyboardPopupUntil-now)>0);
  for(int row=0;row<3;row++)for(int k=0;k<(int)strlen(kRows[row]);k++){
   int x=QW_LEFT[row]+k*QW_PITCH,y=QW_TOP[row];
   bool active=popActive&&keyboardPopupX==x+QW_WIDTH/2&&keyboardPopupY==y+QW_HEIGHT/2;
   cv.fillRoundRect(x,y,QW_WIDTH,QW_HEIGHT,9,active?rgb(93,172,209):rgb(27,55,87));
   cv.drawRoundRect(x,y,QW_WIDTH,QW_HEIGHT,9,active?rgb(224,253,255):rgb(116,171,211));
   textC(String(kRows[row][k]),x+QW_WIDTH/2,y+QW_HEIGHT/2,rgb(246,250,255),4);
  }
  // Only DELETE remains below the keyboard. Large visible button and
  // deliberately wider invisible hit target for smaller fingers.
  cv.fillRoundRect(QW_DEL_X,QW_DEL_Y,QW_DEL_W,QW_DEL_H,13,rgb(33,94,122));
  cv.drawRoundRect(QW_DEL_X,QW_DEL_Y,QW_DEL_W,QW_DEL_H,13,rgb(164,229,249));
  crispBrightnessText("DEL",CX,QW_DEL_Y+QW_DEL_H/2,rgb(251,254,255));
  if(popActive){
   const uint32_t elapsed=now-keyboardPopupStarted;
   float grow=clampf(elapsed/70.f);
   int bw=60+(int)(16*grow),bh=66+(int)(14*grow);
   int bx=clampi(keyboardPopupX-bw/2,11,W-bw-11);
   int by=clampi(keyboardPopupY-112,41,255);
   cv.fillRoundRect(bx-3,by-2,bw+6,bh+6,13,rgb(51,77,99));
   cv.fillRoundRect(bx,by,bw,bh,12,rgb(248,251,255));
   cv.drawRoundRect(bx,by,bw,bh,12,rgb(107,178,221));
   cv.fillTriangle(keyboardPopupX-10,by+bh-1,keyboardPopupX+10,by+bh-1,
                   keyboardPopupX,by+bh+13,rgb(248,251,255));
   drawLargeNativeLabel(keyboardPopup,bx+bw/2,by+bh/2,bw-12,1.60f,rgb(17,38,62));
  }
  break;
 }
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
 // Harness remote is silent: no haptic confirmation.
}
// V17.25: all remote layout and touch rectangles share these constants.
// Every corner of each rectangle is inside the 466px circular panel; the
// visualizer renderers (31) are not modified.
static constexpr int RNAV_LX=120,RNAV_RX=248,RNAV_Y=388,RNAV_W=98,RNAV_H=40;
static constexpr int RPREV_X=78,RAPPLY_X=169,RNEXT_X=299;
static constexpr int RROW_Y=258,RROW_H=60,RPREV_W=89,RAPPLY_W=128;
static constexpr int RFAV_X=116,RFAV_Y=327,RFAV_W=234,RFAV_H=52;
static constexpr int RFROW_X=75,RFROW_Y=144,RFROW_W=316,RFROW_H=52,RFROW_GAP=58;
static constexpr int RPAL_X=78,RPAL_Y=146,RPAL_W=310,RPAL_H=52,RPAL_GAP=58;
static constexpr int RRADIO_LEFT=112,RRADIO_RIGHT=241,RRADIO_Y=211,RRADIO_W=113,RRADIO_H=57;
static constexpr int RSET_TOP=130,RSET_BOTTOM=374;
static constexpr int RBAR_X0=99,RBAR_X1=367,RTOUCH_PAD=8;
static bool remotePointIn(int x,int y,int bx,int by,int bw,int bh){
 return x>=bx&&x<bx+bw&&y>=by&&y<by+bh;
}
struct RemoteHit {int x,y,w,h,id;};
// One physical gesture is one action. For wide finger contact or two close
// contacts, choose the hit whose *center* is closest to the touch centroid.
// Hit padding also accepts drifts off a button's rounded visible corner.
static int remoteNearestHit(int x,int y,const RemoteHit *items,int count){
 int selected=-999;int64_t best=INT64_MAX;
 for(int i=0;i<count;i++){
  const RemoteHit &h=items[i];
  if(!remotePointIn(x,y,h.x-RTOUCH_PAD,h.y-RTOUCH_PAD,h.w+2*RTOUCH_PAD,h.h+2*RTOUCH_PAD))continue;
  const int64_t dx=x-(h.x+h.w/2),dy=y-(h.y+h.h/2);
  const int64_t dist=dx*dx+dy*dy;
  if(dist<best){best=dist;selected=h.id;}
 }
 return selected;
}
static void remoteButton(int x,int y,int w,int h,const String &label,bool active=false){
 // Immediate visual response while pressed; action still commits once on release.
 const bool pressed=remote&&touchDown&&overlay==NONE&&
   remotePointIn(touchLastX,touchLastY,x,y,w,h);
 cv.fillRoundRect(x,y,w,h,12,pressed?rgb(68,166,182):(active?rgb(44,130,151):rgb(17,40,67)));
 cv.drawRoundRect(x,y,w,h,12,pressed?rgb(255,255,255):active?rgb(183,255,255):rgb(80,134,163));
 cv.setFont(&lgfx::fonts::FreeSansBold12pt7b);cv.setTextSize(0.94f);
 cv.setTextDatum(middle_center);cv.setTextColor(active?rgb(255,255,255):rgb(222,244,250));
 cv.drawString(label,x+w/2,y+h/2);
 cv.setFont(&lgfx::fonts::Font0);cv.setTextSize(1.f);
}
static void remotePanelBg(const String &headline){
 black();circ(CX,CY,R-3,rgb(42,92,126),2);
 // Lift the heading 17px, leaving a large gap to the circular outline.
 textC(headline,CX,71,rgb(123,241,247),4);
}
static void remoteNav(){
 remoteButton(RNAV_LX,RNAV_Y,RNAV_W,RNAV_H,"< PAGE");
 remoteButton(RNAV_RX,RNAV_Y,RNAV_W,RNAV_H,"PAGE >");
 textC(String(remotePage+1)+"/"+String(REMOTE_PAGES),CX,RNAV_Y+RNAV_H/2,rgb(145,199,213),2);
}
// Font changes are limited to harness pages and RESET font state every call.
// Sizes relative to V17.24: 75% for brightness/category, 80% for FX name/ID
// and other primary captions. 12pt native font is used only for width fallback.
static void remoteLabel(const String &value,int x,int y,int maxWidth,uint16_t color,float scale=.80f){
 if(value.isEmpty())return;
 cv.setFont(&lgfx::fonts::FreeSansBold18pt7b);cv.setTextSize(scale);
 String label=value;
 if(cv.textWidth(label)>maxWidth){
  cv.setFont(&lgfx::fonts::FreeSansBold12pt7b);cv.setTextSize(1.f);
  while(label.length()>2&&cv.textWidth(label)>maxWidth)label.remove(label.length()-1);
  if(label.length()<value.length()&&label.length()>2){label.remove(label.length()-1);label+="~";}
 }
 cv.setTextDatum(middle_center);cv.setTextColor(color);cv.drawString(label,x,y);
 cv.setFont(&lgfx::fonts::Font0);cv.setTextSize(1.f);
}
static void remoteCaption(const String &value,int x,int y,int maxWidth,uint16_t color){
 if(value.isEmpty())return;
 cv.setFont(&lgfx::fonts::FreeSansBold12pt7b);cv.setTextSize(.86f);
 String label=value;
 while(label.length()>2&&cv.textWidth(label)>maxWidth)label.remove(label.length()-1);
 cv.setTextDatum(middle_center);cv.setTextColor(color);cv.drawString(label,x,y);
 cv.setFont(&lgfx::fonts::Font0);cv.setTextSize(1.f);
}
static int remoteBrightnessBarY(){return 145;}
static void remoteBrightnessControl(int y){
 const uint16_t cyan=rgb(91,239,232);
 remoteLabel("BRIGHTNESS",151,y,233,rgb(235,249,255),.75f);
 remoteLabel(String((remoteBrightness*100+127)/255)+"%",346,y,76,cyan,.75f);
 uiBar(y+23,RBAR_X0,RBAR_X1,remoteBrightness/255.f,cyan);
}
static uint16_t remotePaletteColor(int pal,int slot){
 // Representative WLED palette colors; actual LED colors still come from harness palette.
 static const uint8_t sample[18][9]={
  {46,227,221,255,120,79,109,79,255}, {255,58,139,35,255,215,245,202,90},
  {255,71,64,244,176,47,0,131,250},{255,110,35,221,47,206,64,217,246},
  {255,140,33,248,57,95,51,199,211},{255,224,89,111,56,237,52,211,179},
  {252,41,143,242,196,46,54,183,250},{245,244,255,99,169,247,25,78,175},
  {252,177,35,251,57,33,123,12,21},{43,139,255,15,218,234,8,46,147},
  {23,82,37,58,181,48,197,225,72},{251,63,64,250,205,47,40,222,209},
  {252,52,63,46,232,242,245,221,66},{252,177,96,241,78,103,53,29,90},
  {96,150,245,180,114,236,47,222,204},{168,224,250,55,174,237,22,53,125},
  {249,51,61,52,112,255,175,35,141},{255,233,51,255,124,5,117,28,20}
 };
 const uint8_t *c=sample[pal%18]+3*slot;
 return rgb(c[0],c[1],c[2]);
}
// This is a local compatibility map, not a substitute for live /json/fxdata.
// Extended metadata commands still require corresponding harness receiver support.
struct RemoteFxParam { const char *name;uint8_t cmd;uint8_t value; };
static int remoteFxParams(RemoteFxParam out[5]){
 if(remoteSelected<Z_COUNT){out[0]={"Speed",10,remoteSpeed};out[1]={"Intensity",11,remoteIntensity};return 2;}
 int id=remoteSelected-Z_COUNT;
 if(id==0)return 0; // Solid has brightness only.
 if(id==1||id==2||id==3||id==9){out[0]={"Speed",10,remoteSpeed};return 1;}
 out[0]={"Speed",10,remoteSpeed};out[1]={"Intensity",11,remoteIntensity};
 if(id==66||id==38||id==97||id==161){out[2]={"Custom 1",14,remoteCustom[0]};return 3;}
 return 2;
}
// FX settings evenly fill the available vertical region: 2, 3, and 4
// controls get increasingly compact cells, while a single control is centered.
// Rendering and hit testing use exactly the SAME computed row coordinates.
static int remoteSettingsLabelY(int row,int total){
 if(total<1)total=1;
 const int cell=(RSET_BOTTOM-RSET_TOP)/total;
 return RSET_TOP+row*cell+cell/2-10;
}
static int remoteSettingsBarY(int row,int total){
 const int cell=(RSET_BOTTOM-RSET_TOP)/(total>0?total:1);
 return remoteSettingsLabelY(row,total)+clampi(cell/3,15,23);
}
static bool remoteStartOnSlider(int y){
 if(remotePage==0)return y>=remoteBrightnessBarY()-12&&y<=remoteBrightnessBarY()+25;
 if(remotePage!=2)return false;
 RemoteFxParam p[5];const int n=remoteFxParams(p),total=n+1;
 for(int row=0;row<total;row++){
  int bar=remoteSettingsBarY(row,total);
  if(y>=bar-14&&y<=bar+27)return true;
 }
 return false;
}
static void remoteChangePage(int step){
 remotePage=(uint8_t)((remotePage+REMOTE_PAGES+step)%REMOTE_PAGES);
 // Retain the selected effect and favorites, but do not retain old scroll
 // offsets that could conceal the start of a newly opened page.
 if(remotePage==1)remoteFavScroll=0;
 if(remotePage==3)remotePaletteScroll=0;
}
static void renderRemote(){
 const uint16_t white=rgb(238,252,255),dim=rgb(156,196,214),cyan=rgb(90,238,234);
 const char *heads[]={"HARNESS FX","FAVORITES","FX SETTINGS","PALETTES","RADIO","HARNESS-AP"};
 remotePanelBg(heads[remotePage]);
 if(remotePage==0){
  remoteBrightnessControl(122);
  const bool custom=remoteSelected<Z_COUNT;
  remoteLabel(custom?"CUSTOM Z":"BUILT-IN WLED",CX,175,315,dim,.75f);
  remoteLabel(String(remoteSelected+1)+" / "+String(REMOTE_FX_COUNT),CX,204,315,cyan,.80f);
  remoteLabel(remoteEffectName(remoteSelected),CX,233,330,white,.80f);
  remoteButton(RPREV_X,RROW_Y,RPREV_W,RROW_H,"<");
  remoteButton(RAPPLY_X,RROW_Y,RAPPLY_W,RROW_H,"APPLY",true);
  remoteButton(RNEXT_X,RROW_Y,RPREV_W,RROW_H,">");
  remoteButton(RFAV_X,RFAV_Y,RFAV_W,RFAV_H,
   remoteIsFavorite(remoteSelected)?"REMOVE STAR":"ADD FAVORITE",remoteIsFavorite(remoteSelected));
 }else if(remotePage==1){
  remoteLabel(String(remoteFavCount())+" SAVED EFFECTS",CX,122,319,dim,.80f);
  const int total=remoteFavCount();
  if(remoteFavScroll>(uint16_t)(total>4?total-4:0))remoteFavScroll=(uint16_t)(total>4?total-4:0);
  if(total==0)remoteLabel("STAR FX ON PAGE 1",CX,236,319,dim,.80f);
  for(int row=0;row<4&&row+remoteFavScroll<total;row++){
   const int id=remoteFavIndexAt(row+remoteFavScroll),y=RFROW_Y+row*RFROW_GAP;
   if(id<0)continue;
   const bool pressed=touchDown&&remotePointIn(touchLastX,touchLastY,RFROW_X,y,RFROW_W,RFROW_H);
   cv.fillRoundRect(RFROW_X,y,RFROW_W,RFROW_H,11,pressed?rgb(49,136,160):id==remoteSelected?rgb(24,112,133):rgb(18,46,69));
   cv.drawRoundRect(RFROW_X,y,RFROW_W,RFROW_H,11,pressed?white:id==remoteSelected?cyan:rgb(66,118,145));
   remoteLabel(remoteEffectName(id),217,y+RFROW_H/2,254,white,.79f);
   textC("*",367,y+RFROW_H/2,rgb(255,209,108),3);
  }
  if(total>4)remoteCaption("SWIPE UP FOR MORE",CX,379,220,dim);
 }else if(remotePage==2){
  RemoteFxParam params[5];const int n=remoteFxParams(params),total=n+1;
  for(int row=0;row<total;row++){
   const int ly=remoteSettingsLabelY(row,total),by=remoteSettingsBarY(row,total);
   const bool bright=row==0;
   const char *name=bright?"BRIGHTNESS":params[row-1].name;
   const uint8_t value=bright?remoteBrightness:params[row-1].value;
   const uint16_t ink=bright?cyan:rgb(180,116,236);
   remoteLabel(String(name),163,ly,178,white,bright?.75f:.80f);
   remoteLabel(bright?String((value*100+127)/255)+"%":String((int)value),334,ly,102,cyan,.80f);
   uiBar(by,RBAR_X0,RBAR_X1,value/255.f,ink);
  }
 }else if(remotePage==3){
  remoteCaption("TAP TO APPLY",CX,120,258,dim);
  for(int row=0;row<4;row++){
   const int p=remotePaletteScroll+row;if(p>=PAL_COUNT)break;
   const int y=RPAL_Y+row*RPAL_GAP;
   const bool pressed=touchDown&&remotePointIn(touchLastX,touchLastY,RPAL_X,y,RPAL_W,RPAL_H);
   cv.fillRoundRect(RPAL_X,y,RPAL_W,RPAL_H,11,pressed?rgb(49,136,160):p==selectedPalette?rgb(20,104,118):rgb(17,43,66));
   cv.drawRoundRect(RPAL_X,y,RPAL_W,RPAL_H,11,pressed?white:p==selectedPalette?cyan:rgb(65,109,134));
   remoteLabel(String(PAL_NAMES[p]),171,y+RPAL_H/2,184,white,.80f);
   for(int j=0;j<3;j++){
    int px=291+j*31;cv.fillCircle(px,y+RPAL_H/2,11,remotePaletteColor(p,j));
    cv.drawCircle(px,y+RPAL_H/2,11,rgb(182,199,213));
   }
  }
 }else if(remotePage==4){
  remoteLabel("ESP-NOW CHANNEL",CX,129,320,dim,.80f);
  drawLargeNativeLabel(String(rxChannel),CX,185,188,.90f,cyan);
  remoteButton(RRADIO_LEFT,RRADIO_Y,RRADIO_W,RRADIO_H,harnessApAssociated()?"AP SET":"CH -");
  remoteButton(RRADIO_RIGHT,RRADIO_Y,RRADIO_W,RRADIO_H,harnessApAssociated()?"AUTO":"CH +");
  String mac=WiFi.macAddress();
  remoteLabel(mac.substring(0,9),CX,301,336,white,.80f);
  remoteLabel(mac.substring(9),CX,330,336,white,.80f);
  remoteCaption(harnessApAssociated()?"CHANNEL SET BY HARNESS-AP":"PAIR IN WLED REMOTES",CX,365,343,dim);
 }else if(remotePage==5){
  remoteLabel("SSID: HARNESS-AP",CX,121,346,white,.79f);
  const bool connected=harnessApAssociated();
  String status=connected?"CONNECTED":apLink==AP_CONNECTING?"CONNECTING":apLink==AP_FAILED?"FAILED":"NOT CONNECTED";
  remoteLabel("WI-FI: "+status,CX,155,346,connected?rgb(111,255,175):apLink==AP_FAILED?rgb(255,154,113):cyan,.79f);
  String wled=connected?(apWledChecked?(apWledOnline?"WLED: ONLINE":"WLED: NO REPLY"):"WLED: CHECKING"):"WLED: NOT CHECKED";
  remoteLabel(wled,CX,189,340,apWledOnline?rgb(111,255,175):dim,.75f);
  remoteCaption(connected?String("IP: ")+WiFi.localIP().toString():
    apLink==AP_FAILED?(apFailReason==1?"AP NOT FOUND":apFailReason==2?"CHECK PASSWORD":"AP TIMED OUT / LOST"):
    "Wi-Fi + ESP-NOW are separate links",CX,226,346,dim);
  if(connected)remoteCaption(String("CH ")+String(WiFi.channel())+"   "+String(WiFi.RSSI())+" dBm",CX,250,338,cyan);
  else remoteCaption("WLED AP: NO INTERNET NEEDED",CX,250,340,dim);
  String ack=apLastRadioAckAt?String("ESP-NOW ACK ")+String((millis()-apLastRadioAckAt)/1000)+"s ago":
    radioReady?"ESP-NOW READY / NO ACK":"ESP-NOW NOT READY";
  remoteCaption(ack,CX,278,345,dim);
  remoteButton(92,308,136,54,connected?"DISCONNECT":apLink==AP_CONNECTING?"CANCEL":apLink==AP_FAILED?"RETRY":"CONNECT",connected);
  remoteButton(239,308,136,54,"PASSWORD");
 }
 if(radioPending)textC("SENDING",CX,100,rgb(255,188,96),2);
 else if(radioSuccess)textC("APPLIED",CX,100,rgb(107,255,181),2);
 remoteNav();
}
static void showTextEditor(){
 // Always start with a BLANK name entry; no previous name is prefilled.
 overlay=NAME;inputName="";draftBlue="";draftPink="";robotField=0;
 nameRobotBlueEdited=false;nameRobotPinkEdited=false;
 keyboardPopup="";keyboardPopupUntil=0;keyboardPopupStarted=0;vibration();
}
static void saveNameOverlay(){
 if(originalEffect(effectID)==22){
  // Unedited partner name is preserved; pressing Save with an empty field
  // never silently wipes an existing robot name.
  if(nameRobotBlueEdited&&draftBlue.length())blueRobot=draftBlue;
  if(nameRobotPinkEdited&&draftPink.length())pinkRobot=draftPink;
 }else if(isTextEffect(effectID)||originalEffect(effectID)==44){
  // Empty initial editor is a request to type, not to erase all prior names.
  if(inputName.length())sharedName=inputName;
 }
 saveNames();overlay=NONE;vibration(180,65);
}
static void remoteTouch(int x,int y,int dx,int dy){
 // On every page, swipe LEFT -> next page; RIGHT -> previous page.
 // Horizontal slider drags are kept as slider adjustments, not page swipes.
 // A strong swipe must dominate vertical movement to avoid list scrolling.
 const bool onSlider=remoteStartOnSlider(touchStartY);
 if(abs(dx)>=66&&abs(dx)>abs(dy)*1.30f&&(!onSlider||abs(dx)>=145)){
  remoteChangePage(dx<0?1:-1);return;
 }
 const RemoteHit navigation[]={
  {RNAV_LX,RNAV_Y,RNAV_W,RNAV_H,-1},
  {RNAV_RX,RNAV_Y,RNAV_W,RNAV_H,1}
 };
 const int pageStep=remoteNearestHit(x,y,navigation,2);
 if(pageStep!=-999){remoteChangePage(pageStep);return;}
 if(remotePage==0){
  if(y>=remoteBrightnessBarY()-13&&y<remoteBrightnessBarY()+28&&x>=RBAR_X0-12&&x<=RBAR_X1+12){
   remoteBrightness=(uint8_t)clampi((x-RBAR_X0)*255/(RBAR_X1-RBAR_X0),0,255);radioSend(2,remoteBrightness);
   return;
  }
  const RemoteHit buttons[]={
   {RPREV_X,RROW_Y,RPREV_W,RROW_H,1},
   {RAPPLY_X,RROW_Y,RAPPLY_W,RROW_H,2},
   {RNEXT_X,RROW_Y,RPREV_W,RROW_H,3},
   {RFAV_X,RFAV_Y,RFAV_W,RFAV_H,4}
  };
  switch(remoteNearestHit(x,y,buttons,4)){
   case 1:remoteSelected=(remoteSelected+REMOTE_FX_COUNT-1)%REMOTE_FX_COUNT;break;
   case 2:applyRemoteSelection();break;
   case 3:remoteSelected=(remoteSelected+1)%REMOTE_FX_COUNT;break;
   case 4:remoteToggleFavorite(remoteSelected);break;
  }
 }else if(remotePage==1){
  const int count=remoteFavCount();
  if(abs(dy)>27 && abs(dy)>abs(dx)){
   remoteFavScroll=clampi((int)remoteFavScroll+(dy<0?1:-1),0,count>4?count-4:0);return;
  }
  RemoteHit rows[4];int n=0;
  for(int row=0;row<4;row++){
   int id=remoteFavIndexAt(remoteFavScroll+row);if(id<0)break;
   rows[n++]={RFROW_X,RFROW_Y+row*RFROW_GAP,RFROW_W,RFROW_H,id};
  }
  if(n){int id=remoteNearestHit(x,y,rows,n);if(id>=0){remoteSelected=id;applyRemoteSelection();}}
 }else if(remotePage==2){
  RemoteFxParam params[5];const int n=remoteFxParams(params),total=n+1;
  // The full width of each control row, including its label, value and bar,
  // is selectable. A touch adjusts the corresponding control only once.
  RemoteHit rows[6];
  for(int row=0;row<total;row++){
   const int ly=remoteSettingsLabelY(row,total),by=remoteSettingsBarY(row,total);
   int upper=ly-18,lower=by+19;
   rows[row]={RBAR_X0-11,upper,RBAR_X1-RBAR_X0+22,lower-upper,row};
  }
  const int row=remoteNearestHit(x,y,rows,total);
  if(row>=0){
   const uint8_t value=(uint8_t)clampi((x-RBAR_X0)*255/(RBAR_X1-RBAR_X0),0,255);
   if(row==0){remoteBrightness=value;radioSend(2,remoteBrightness);}
   else{
    const uint8_t command=params[row-1].cmd;
    if(command==10)remoteSpeed=value;
    else if(command==11)remoteIntensity=value;
    else if(command>=14&&command<=16)remoteCustom[command-14]=value;
    radioSend(command,value);
   }
  }
 }else if(remotePage==3){
  if(abs(dy)>27&&abs(dy)>abs(dx)){
   remotePaletteScroll=clampi((int)remotePaletteScroll+(dy<0?2:-2),0,PAL_COUNT>4?PAL_COUNT-4:0);return;
  }
  RemoteHit rows[4];int n=0;
  for(int row=0;row<4;row++){
   int id=remotePaletteScroll+row;if(id>=PAL_COUNT)break;
   rows[n++]={RPAL_X,RPAL_Y+row*RPAL_GAP,RPAL_W,RPAL_H,id};
  }
  if(n){int id=remoteNearestHit(x,y,rows,n);if(id>=0){selectedPalette=(uint8_t)id;radioSend(13,selectedPalette);}}
 }else if(remotePage==4){
  const RemoteHit channels[]={
   {RRADIO_LEFT,RRADIO_Y,RRADIO_W,RRADIO_H,-1},
   {RRADIO_RIGHT,RRADIO_Y,RRADIO_W,RRADIO_H,1}
  };
  const int step=remoteNearestHit(x,y,channels,2);
  if(step!=-999&&!harnessApAssociated()){
   rxChannel=clampi(rxChannel+step,1,11);prefs.putUChar("radio",rxChannel);
   if(radioReady){esp_now_deinit();radioReady=false;}WiFi.mode(WIFI_OFF);startRadio();
  }
 }else if(remotePage==5){
  const RemoteHit controls[]={
   {92,308,136,54,1},{239,308,136,54,2}
  };
  switch(remoteNearestHit(x,y,controls,2)){
   case 1:
    if(harnessApAssociated()||apLink==AP_CONNECTING)harnessApDisconnect(true);
    else{apAutoConnect=true;prefs.putBool("hap_auto",true);harnessApStartConnect();}
    break;
   case 2:openHarnessApPasswordEditor();break;
  }
 }
}
static void editorTouch(int x,int y){
 if(overlay==BRIGHTNESS){
  if(y>=233&&y<287){
    candidateBrightness=clampi((x-65)*100/(W-130),8,100);
    M5.Display.setBrightness((uint8_t)(candidateBrightness*255/100));
  }else if(y>=304&&y<399){
    if(x<CX)M5.Display.setBrightness((uint8_t)(brightness*255/100));
    else{brightness=candidateBrightness;prefs.putUChar("bright",brightness);}
    overlay=NONE;vibration(170,50);
  }
 }else if(overlay==RHYTHM){
   if(y>=300&&y<369){profile=(profile+1)%3;prefs.putUChar("profile",profile);
      setCpuFrequencyMhz(profile==2?160:240);vibration();}
   else if(y>=372)overlay=NONE;
 }else if(overlay==AP_PASSWORD){
   harnessApPasswordTouch(x,y);
 }else if(overlay==NAME){
   if(originalEffect(effectID)==22&&y>=40&&y<87){
     if(x>=112&&x<=353){robotField=x<CX?0:1;keyboardPopup="";vibration(110,24);}
     return;
   }
   // DELETE takes priority over neighboring keys and uses a much larger
   // invisible touch target than the visible DEL button.
   if(deleteHit(x,y)){
     String &dest=originalEffect(effectID)==22?(robotField==0?draftBlue:draftPink):inputName;
     if(dest.length()){
       dest.remove(dest.length()-1);
       if(originalEffect(effectID)==22){
         if(robotField==0)nameRobotBlueEdited=true;else nameRobotPinkEdited=true;
       }
     }
     keyboardPopup="";vibration(150,35);
     return;
   }
   char typed=0;int px=0,py=0;
   if(qwertyHit(x,y,typed,px,py)){
     String &dest=originalEffect(effectID)==22?(robotField==0?draftBlue:draftPink):inputName;
     if(dest.length()<12){
       dest+=typed;
       if(originalEffect(effectID)==22){
         if(robotField==0)nameRobotBlueEdited=true;else nameRobotPinkEdited=true;
       }
       keyboardPop(typed,px,py);vibration(105,20);
     }
   }
 }
}
static void selectEffect(int d){
 // V17.19: ONLY physical A/B buttons navigate effects. Touch is reserved for
 // creative interaction and editor/remote controls; no horizontal swipe nav.
 if(remote){remoteSelected=(remoteSelected+Z_COUNT+WLED_COUNT+d)%(Z_COUNT+WLED_COUNT);}
 else {
  if(originalEffect(effectID)==64)StringTheory::dismissUI();
  if(originalEffect(effectID)==65||originalEffect(effectID)==66)GravityLab::dismissUI();
  const uint8_t departing=originalEffect(effectID);
  if(departing==69 && GhostCompanion::busy())return;
  effectID=favoritesOnly&&favoriteBits?nextFavorite(effectID,d):((effectID+VIS_COUNT+d)%VIS_COUNT);
  persistEffect();
  if(departing==68)ArtCapsule::leaveEffect();
  if(departing==69)GhostCompanion::deactivate();
 }
}
static void controlButtons(){
 const uint32_t now=millis();
 if(remote&&(M5.BtnA.isPressed()||M5.BtnB.isPressed()))lastRemoteActivity=now;
 if(M5.BtnA.wasPressed()){keyADown=now;keyAHeld=false;brightnessRepeatAt=now;}
 if(M5.BtnB.wasPressed()){keyBDown=now;keyBHeld=false;brightnessRepeatAt=now;}

 // Battery HUD owns both buttons: pressing either returns to the effect.
 // Ignore the *release* of the B hold which originally opened this overlay.
 if(overlay==BATTERY){
   if(batteryEntryHold){
     if(M5.BtnB.wasReleased()||!M5.BtnB.isPressed()){
       batteryEntryHold=false;keyBDown=0;
     }
     return;
   }
   if(M5.BtnA.wasPressed()||M5.BtnB.wasPressed()){
     overlay=NONE;batteryHudUntil=0;
     // Consume this button's eventual release; don't change effects.
     keyAHeld=true;keyBHeld=true;keyADown=0;keyBDown=0;
     vibration(112,27);
   }
   return;
 }
 // AP password editor: physical A cancels, B saves. Never touches
 // the visualizer's shared names and never vibrates in harness mode.
 if(overlay==AP_PASSWORD){
  if(M5.BtnA.wasReleased()){overlay=NONE;apDraft="";apPasswordError=false;keyADown=0;}
  if(M5.BtnB.wasReleased()){saveHarnessApPassword();keyBDown=0;}
  return;
 }
 // Text editor: physical A discards all edits, physical B saves.
 if(overlay==NAME){
  if(M5.BtnA.wasReleased()){overlay=NONE;inputName="";keyboardPopup="";vibration(170,45);}
  else if(M5.BtnB.wasReleased()){keyboardPopup="";saveNameOverlay();}
  if(M5.BtnA.wasReleased())keyADown=0;
  if(M5.BtnB.wasReleased())keyBDown=0;
  return;
 }
 if(overlay==BRIGHTNESS){
  // Ignore the remainder of the original long A press used to open this page.
  if(brightnessEntryHold&&!M5.BtnA.isPressed())brightnessEntryHold=false;
  if(!brightnessEntryHold && M5.BtnA.isPressed()&&!M5.BtnB.isPressed()
     &&keyADown&&now-keyADown>=320&&now-brightnessRepeatAt>=90){
    candidateBrightness=clampi(candidateBrightness-2,8,100);keyAHeld=true;
    brightnessRepeatAt=now;M5.Display.setBrightness((uint8_t)(candidateBrightness*255/100));
  }
  if(!brightnessEntryHold && M5.BtnB.isPressed()&&!M5.BtnA.isPressed()
     &&keyBDown&&now-keyBDown>=320&&now-brightnessRepeatAt>=90){
    candidateBrightness=clampi(candidateBrightness+2,8,100);keyBHeld=true;
    brightnessRepeatAt=now;M5.Display.setBrightness((uint8_t)(candidateBrightness*255/100));
  }
  if(M5.BtnA.wasReleased()){
    if(!keyAHeld){candidateBrightness=clampi(candidateBrightness-5,8,100);
      M5.Display.setBrightness((uint8_t)(candidateBrightness*255/100));}keyADown=0;
  }
  if(M5.BtnB.wasReleased()){
    if(!keyBHeld){candidateBrightness=clampi(candidateBrightness+5,8,100);
      M5.Display.setBrightness((uint8_t)(candidateBrightness*255/100));}keyBDown=0;
  }
  return;
 }
 if(M5.BtnA.isPressed()&&M5.BtnB.isPressed()&&!keyChord){
  if(!remote && originalEffect(effectID)==68 && ArtCapsule::radioOwned){ESP.restart();return;}
  if(!remote && originalEffect(effectID)==69){if(GhostCompanion::busy())return;GhostCompanion::deactivate();}
  keyChord=true;keyAHeld=keyBHeld=true;setRemote(!remote);
  if(remote&&!radioReady){startRadio();if(apAutoConnect)harnessApStartConnect();}
  if(!remote){if(radioReady){esp_now_deinit();radioReady=false;}
    WiFi.mode(WIFI_OFF);setCpuFrequencyMhz(profile==2?160:240);}
 }
 // In harness remote both physical buttons adjust HARNESS brightness, not watch FX.
 if(remote && overlay==NONE){
  if(!keyChord&&M5.BtnA.isPressed()&&!M5.BtnB.isPressed()&&keyADown&&now-keyADown>=390&&now-brightnessRepeatAt>=130){
    remoteBrightness=(uint8_t)clampi((int)remoteBrightness-4,0,255);radioSend(2,remoteBrightness);brightnessRepeatAt=now;keyAHeld=true;
  }
  if(!keyChord&&M5.BtnB.isPressed()&&!M5.BtnA.isPressed()&&keyBDown&&now-keyBDown>=390&&now-brightnessRepeatAt>=130){
    remoteBrightness=(uint8_t)clampi((int)remoteBrightness+4,0,255);radioSend(2,remoteBrightness);brightnessRepeatAt=now;keyBHeld=true;
  }
  if(M5.BtnA.wasReleased()){if(!keyChord&&!keyAHeld){remoteBrightness=(uint8_t)clampi((int)remoteBrightness-13,0,255);radioSend(2,remoteBrightness);}keyADown=0;}
  if(M5.BtnB.wasReleased()){if(!keyChord&&!keyBHeld){remoteBrightness=(uint8_t)clampi((int)remoteBrightness+13,0,255);radioSend(2,remoteBrightness);}keyBDown=0;}
  if(keyChord&&!M5.BtnA.isPressed()&&!M5.BtnB.isPressed())keyChord=false;
  return;
 }
 if(M5.BtnA.isPressed()&&!keyChord&&!keyAHeld&&keyADown&&now-keyADown>=730&&overlay==NONE){
  overlay=BRIGHTNESS;candidateBrightness=brightness;brightnessEntryHold=true;
  brightnessRepeatAt=now;keyAHeld=true;vibration(195,60);
 }
 if(M5.BtnB.isPressed()&&!keyChord&&!keyBHeld&&keyBDown&&now-keyBDown>=850&&overlay==NONE){
  // V17.1: B long press opens battery, never the removed Club Library.
  openBatteryHud();batteryEntryHold=true;keyBHeld=true;
 }
 if(M5.BtnA.wasReleased()){
  if(!keyChord&&!keyAHeld&&overlay==NONE)selectEffect(-1);keyADown=0;
 }
 if(M5.BtnB.wasReleased()){
  if(!keyChord&&!keyBHeld&&overlay==NONE)selectEffect(+1);keyBDown=0;
 }
 if(keyChord&&!M5.BtnA.isPressed()&&!M5.BtnB.isPressed())keyChord=false;
}
static void controlTouch(){
 auto touch=M5.Touch.getDetail(0);
 if(touch.wasPressed()){
   touchStartX=touchLastX=touch.x;
   touchStartY=touchLastY=touch.y;
   touchStart=millis();
   touchDown=true;
   if(overlay==NONE&&!remote&&originalEffect(effectID)==64)StringTheory::touchStart(touch.x,touch.y);
   if(overlay==NONE&&!remote&&(originalEffect(effectID)==65||originalEffect(effectID)==66))GravityLab::touchStart(touch.x,touch.y,originalEffect(effectID)==65);
   nameTouchXSum=touch.x;nameTouchYSum=touch.y;nameTouchSamples=1;
   remoteTouchXSum=touch.x;remoteTouchYSum=touch.y;remoteTouchSamples=1;
   nameDeleteConsumed=false;
   if(overlay==NAME){
     // V17.4: DELETE is an immediate ON-PRESS action. Do not wait for the
     // finger to lift or use its averaged coordinates, which caused misses.
     if(deleteHit(touch.x,touch.y)){
       editorTouch(touch.x,touch.y);
       nameDeleteConsumed=true;
     }else{
       char ch=0;int xx=0,yy=0;
       if(qwertyHit(touch.x,touch.y,ch,xx,yy))keyboardPop(ch,xx,yy);
       else keyboardPopup="";
     }
   }
 }
 if(touch.isPressed()&&touchDown){
   touchLastX=touch.x;touchLastY=touch.y;
   if(remote&&overlay==NONE&&remoteTouchSamples<40){
     const int count=clampi((int)M5.Touch.getCount(),1,5);
     int midX=0,midY=0,active=0;
     for(int i=0;i<count;i++){
       const auto &contact=M5.Touch.getDetail(i);
       if(contact.isPressed()) {midX+=contact.x;midY+=contact.y;active++;}
     }
     if(active){remoteTouchXSum+=midX/active;remoteTouchYSum+=midY/active;remoteTouchSamples++;}
   }
   if(overlay==NONE&&!remote&&originalEffect(effectID)==64)StringTheory::touchMove(touch.x,touch.y);
   if(overlay==NONE&&!remote&&(originalEffect(effectID)==65||originalEffect(effectID)==66))GravityLab::touchMove(touch.x,touch.y,originalEffect(effectID)==65);
   if((overlay==NAME||overlay==AP_PASSWORD) && !nameDeleteConsumed){
     // Compute ONE mean contact center across this entire physical press.
     // Ignore simultaneous / adjacent key edges, not the touch centroid.
     if(nameTouchSamples<28){
       nameTouchXSum+=touch.x;nameTouchYSum+=touch.y;nameTouchSamples++;
     }
     char ch=0;int xx=0,yy=0;
     int avgX=nameTouchXSum/nameTouchSamples,avgY=nameTouchYSum/nameTouchSamples;
     if(qwertyHit(avgX,avgY,ch,xx,yy)&&
        (keyboardPopup.length()!=1||keyboardPopup[0]!=ch))keyboardPop(ch,xx,yy);
   }
 }
 if(touch.wasReleased()||touch.wasClicked()){
   // A physical contact can report both wasClicked and wasReleased across
   // frames. Commit exactly once, then discard any duplicate event.
   if(!touchDown)return;
   if(overlay==NAME&&nameDeleteConsumed){
     // Consume the matching release; never delete twice or add a letter.
     touchDown=false;nameTouchSamples=0;nameDeleteConsumed=false;
     keyboardPopup="";return;
   }
   uint32_t held=millis()-touchStart;
   int x=touchLastX,y=touchLastY;
   if((overlay==NAME||overlay==AP_PASSWORD)&&nameTouchSamples>0){
     x=nameTouchXSum/nameTouchSamples;
     y=nameTouchYSum/nameTouchSamples;
   }
   int dx=x-touchStartX;
   const int dy=y-touchStartY;
   if(remote&&overlay==NONE&&remoteTouchSamples>0&&abs(dx)<26&&abs(dy)<26){
     // Tap centroid wins when a broad press overlaps adjacent remote buttons.
     x=remoteTouchXSum/remoteTouchSamples;
     y=remoteTouchYSum/remoteTouchSamples;
   }
   touchDown=false;nameTouchSamples=0;remoteTouchSamples=0;
   if((overlay==NAME||overlay==AP_PASSWORD)&&abs(dx)>60){keyboardPopup="";return;}
   if(overlay==BATTERY){overlay=NONE;batteryHudUntil=0;vibration(105,20);return;}
   if(overlay!=NONE){editorTouch(x,y);return;}
   if(remote){lastRemoteActivity=millis();remoteTouch(x,y,dx,y-touchStartY);return;}
   if(originalEffect(effectID)==64){
     if(StringTheory::touchEnd(x,y,held,dx,y-touchStartY))return;
   }
   if(originalEffect(effectID)==65||originalEffect(effectID)==66){
     if(GravityLab::touchEnd(x,y,held,dx,y-touchStartY,originalEffect(effectID)==65))return;
   }
   if(originalEffect(effectID)==68){if(!ArtCapsule::radioOwned)ArtCapsule::begin();return;}
   if(originalEffect(effectID)==69){GhostCompanion::tap(x,y);return;}
   // No swipe changes effects in V17.19. A and B own effect navigation.
   // Avoid opening the text editor accidentally after a horizontal gesture.
   if(abs(dx)>60 && abs(dx)>abs(y-touchStartY)*1.2f)return;
   if(originalEffect(effectID)==22||isTextEffect(effectID)||originalEffect(effectID)==44){showTextEditor();return;}
   // No manual microphone screen: all audio is calibrated automatically.
   if(originalEffect(effectID)==32||originalEffect(effectID)==36||
      originalEffect(effectID)==41||originalEffect(effectID)==43){
     lastArtTouchMs=millis();
     if(originalEffect(effectID)==43)userSpin+=dx*.013f;
   }
 }
}
static void saveFrameStats(){ // Diagnostic data via Serial only; no onscreen performance overlay.
 static uint32_t prev=0;uint32_t now=millis();if(now-prev<15000)return;prev=now;
 Serial.printf("Native31 V16 effect %u, BPM %.1f conf %.2f, freePSRAM %u, frame %lums, battery %ld%%\n",
    effectID,bpm,beatConfidence,(unsigned)ESP.getFreePsram(),(unsigned long)(now-renderStamp),batteryHudPercent);
}
static uint8_t powerFrameCap(){
 uint8_t rate=profile==0?48:profile==1?35:24;
 if(overlay==NAME)return profile==2?24:32; // smoother, visibly held key popovers
 if(remote||overlay!=NONE)return 15;
 if(originalEffect(effectID)==64)return profile==2?20:(profile==1?28:34); // improved gesture response
 if(originalEffect(effectID)==65)return profile==2?18:(profile==1?26:32); // classic FluidBox
 if(originalEffect(effectID)==66)return profile==2?18:(profile==1?27:33); // deluxe Gravity Lab
 if(originalEffect(effectID)==68)return 12; // image stays steady; perimeter moves
 if(originalEffect(effectID)==69)return profile==2?24:(profile==1?35:44); // bobbing ghost
 if(originalEffect(effectID)==51||originalEffect(effectID)==53)return profile==2?18:26; // smoother/faster surviving yarn effects
 if(originalEffect(effectID)>=49)return profile==2?14:20; // limit imported Nova for power
 if(!adaptiveRender)return rate;
 if(originalEffect(effectID)==2||originalEffect(effectID)==3||originalEffect(effectID)==27)return rate;
 // Lighter, ambient artwork is intentionally drawn less often; the FFT and
 // beat tracker are independent and retain their normal sampling cadence.
 static const uint8_t slow[]={10,11,13,16,17,18,19,20,21,25,26,28,29};
 static const uint8_t medium[]={1,4,6,7,8,9,12,22,24,30,31,32,37,40};
 uint8_t factor=100;
 for(uint8_t v:slow)if(originalEffect(effectID)==v){factor=62;break;}
 if(factor==100)for(uint8_t v:medium)if(originalEffect(effectID)==v){factor=82;break;}
 if(energy<.11f&&factor<100)factor=(uint8_t)(factor*0.84f);
 return clampi((rate*factor+50)/100,14,rate);
}
// Physical 90-second motionlessness shutoff, independent of animation.
namespace MotionPower {
 static uint32_t lastMove=0,lastPoll=0,lastSensorAt=0;
 static float ax=0,ay=0,az=0;
 static bool calibrated=false,off=false;
 static void begin(){lastMove=lastSensorAt=millis();}
 static void tick(){
  uint32_t now=millis();if(now-lastPoll<50)return;lastPoll=now;
  if(M5.BtnA.isPressed()||M5.BtnB.isPressed()||touchDown
     ||(originalEffect(effectID)==68&&ArtCapsule::busy())
     ||(originalEffect(effectID)==69&&GhostCompanion::busy()))lastMove=now;
  if(!M5.Imu.isEnabled()){lastMove=now;return;}
  if(!M5.Imu.update()){
    if(now-lastSensorAt>1800UL)lastMove=now; // sensor failure: never shut down on stale data
    return;
  }
  lastSensorAt=now;
  auto d=M5.Imu.getImuData();
  float a=d.accel.x,b=d.accel.y,c=d.accel.z;
  if(!isfinite(a)||!isfinite(b)||!isfinite(c)){lastMove=now;return;}
  if(!calibrated){ax=a;ay=b;az=c;calibrated=true;lastMove=now;return;}
  float delta=fabsf(a-ax)+fabsf(b-ay)+fabsf(c-az);
  ax=ax*.88f+a*.12f;ay=ay*.88f+b*.12f;az=az*.88f+c*.12f;
  float spin=fabsf(d.gyro.x)+fabsf(d.gyro.y)+fabsf(d.gyro.z);
  if(delta>.039f||spin>1.8f)lastMove=now;
  if(!off && (uint32_t)(now-lastMove)>=90000UL){
   off=true;Serial.println("Powering down after 90s motionless");
   M5.Power.setVibration(0);M5.Mic.end();M5.Speaker.end();
   M5.Display.fillScreen(TFT_BLACK);M5.Power.powerOff();
   while(true)delay(1000);
  }
 }
}
static void renderFrame(){
 if(overlay!=NONE)renderOverlay();else if(remote)renderRemote();else renderEffect();
 cv.pushSprite(0,0);
}
void setup(){
 Serial.begin(115200);auto cfg=M5.config();cfg.internal_mic=true;cfg.internal_imu=true;cfg.internal_spk=true;
 M5.begin(cfg);
 // V17.27: Switch OFF the green power LED while the StopWatch is running.
 // The LED is controlled by M5PM1 (I2C addr 0x6E), not an ESP32 GPIO.
 // M5PM1 PWR_CFG register 0x06, bit 4 = LED_EN default output level.
 // Clear ONLY bit 4 (same operation as M5PM1::setLedEnLevel(false)).
 // Preserve all PMIC power-rail and charging bits and all application behavior.
 bool greenPowerLedOff=false;
 for(uint8_t attempt=0;attempt<3 && !greenPowerLedOff;attempt++){
   greenPowerLedOff=M5.In_I2C.bitOff(0x6E,0x06,0x10,100000);
   if(!greenPowerLedOff)delay(15);
 }
 Serial.printf("Green power indicator: %s\n",greenPowerLedOff?"OFF":"PMIC I2C write failed");
 M5.Speaker.end();M5.Mic.begin();
 M5.Display.setRotation(0);W=M5.Display.width();H=M5.Display.height();CX=W/2;CY=H/2;R=clampi(min(W,H)/2-3,150,260);
 loadSettings();
 for(int i=0;i<NFFT;i++)fftWindow[i]=.5f-.5f*cosf(TAU*i/(NFFT-1));
 for(int k=0;k<NBANDS;k++){
   binBegin[k]=clampi((int)roundf(powf(255.f,(float)k/NBANDS)),1,254);
   binEnd[k]=clampi((int)roundf(powf(255.f,(float)(k+1)/NBANDS)),binBegin[k]+1,255);
 }
 M5.Display.setBrightness((uint8_t)(brightness*255/100));
 // Hotfix: the 16-bit full-screen sprite exceeds the ESP32-S3 internal heap.
 // LovyanGFX must be explicitly told to allocate the sprite from OPI PSRAM.
 const size_t frameBytes=(size_t)W*(size_t)H*2;
 Serial.printf("Frame request=%u, PSRAM total=%u, free=%u, largest=%u bytes\n",
    (unsigned)frameBytes,(unsigned)ESP.getPsramSize(),(unsigned)ESP.getFreePsram(),
    (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM));
 cv.setColorDepth(16);
 cv.setPsram(true); // Must run BEFORE createSprite().
 if(ESP.getPsramSize()==0 || !cv.createSprite(W,H)){
   Serial.printf("PSRAM SPRITE ERROR: total=%u free=%u largest=%u need=%u\n",
       (unsigned)ESP.getPsramSize(),(unsigned)ESP.getFreePsram(),
       (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM),(unsigned)frameBytes);
   M5.Display.fillScreen(TFT_BLACK);M5.Display.setTextColor(TFT_RED);
   M5.Display.drawString("PSRAM SPRITE ERROR",22,H/2-20);
   M5.Display.drawString(ESP.getPsramSize()==0?"PSRAM NOT DETECTED":"CHECK SERIAL LOG",22,H/2+10);
   while(true)delay(1000);
 }
 fb=(uint16_t*)cv.getBuffer();
 Serial.printf("Framebuffer OK. Free PSRAM=%u bytes\n",(unsigned)ESP.getFreePsram());
 initArtwork();
 ArtCapsule::initialize();GhostCompanion::load();
 if(prefs.getUChar("s1728",0)!=1){
   // One-time migration: retain all legacy per-effect sensitivity gains.
   prefs.putBytes("gain1728",gainPct,sizeof(gainPct));
   prefs.putUChar("eff1728",effectID);
   prefs.putUChar("s1728",1);
 }
 // V17.1: boot directly into the final effect; do not cover it with the battery HUD.
 sampleBatteryDrain(true);
 M5.BtnA.setHoldThresh(730);M5.BtnB.setHoldThresh(850);
 M5.Power.setVibration(0);
 MotionPower::begin();
 WiFi.mode(WIFI_OFF); // radio ONLY when harness remote enabled
 if(profile==2)setCpuFrequencyMhz(160);
 Serial.printf("StopWatch Native31 built for M5StopWatch; %dx%d, effects=%u, PSRAM=%u, static assets=%u bytes\n",
 W,H,VIS_COUNT,(unsigned)ESP.getPsramSize(),(unsigned)(EYE_ART_SIZE*EYE_ART_SIZE+512));
 vibration(170,65);
}
void loop(){
 M5.update();vibrationTick();
 sampleBatteryDrain();
 // M5PM1 sends 2 for a short key press when it can be caught without a
 // PMIC reset; the same HUD also appears after the reset/boot path.
 uint32_t powerNow=millis();
 if(powerNow-batteryPowerPollAt>=120){
  batteryPowerPollAt=powerNow;
  uint8_t state=M5.Power.getKeyState();
  if((state&2) && powerNow>1500 && powerNow-batteryKeyCooldown>550){
   batteryKeyCooldown=powerNow;openBatteryHud();
  }
 }
 if(overlay==BATTERY && (int32_t)(powerNow-batteryHudUntil)>=0)overlay=NONE;
 // Battery state and ETA are read by the 60-second sampler, not per frame.
 controlButtons();controlTouch();MotionPower::tick();radioTick();harnessApTick();audioTick();beatTick();
 if(!remote && overlay==NONE && (originalEffect(effectID)==65||originalEffect(effectID)==66))
    GravityLab::tick(millis(),originalEffect(effectID)==65);
 if(!remote && overlay==NONE && originalEffect(effectID)==68)ArtCapsule::tick();
 if(!remote && overlay==NONE && originalEffect(effectID)==69)GhostCompanion::tick();
 uint32_t now=millis();
 // V17.24: always-on display in both visualizer and harness modes.
 // Removed the remote idle blanker entirely; no automatic brightness-to-zero.
 const int fps=powerFrameCap();
 if((uint32_t)(now-frameStamp)<(uint32_t)(1000/fps)){delay(1);return;}
 frameStamp=now;renderStamp=now;renderFrame();saveFrameStats();
}