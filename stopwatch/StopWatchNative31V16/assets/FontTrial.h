/*
 * StopWatch V16.6 three NEW text experiments (isolated to effects 1-5).
 * 1 FAST-A:    native font, compact 8-bit PALETTE sprite, glyph cache,
 *              LovyanGFX antialiased rotation into the existing framebuffer.
 * 2 DIRECT:    native fonts drawn at final size, upright, NO transform.
 *              This is the low-cost clarity/reference experiment.
 * 3 TILE CACHE: use native M5GFX AA once per quantized angle and width,
 *              save cropped coverage in PSRAM, composite only used pixels
 *              at runtime with no scaling and no rotate per hit.
 *
 * Baseline: V16.3 with the same five-effect geometry as the V16.5 lab.
 * No changes to NativeApp.cpp or effects 6-31.
 */
#pragma once
#ifndef STOPWATCH_FONT_TRIAL
#define STOPWATCH_FONT_TRIAL 1
#endif
#if STOPWATCH_FONT_TRIAL<1 || STOPWATCH_FONT_TRIAL>3
#error "STOPWATCH_FONT_TRIAL must be 1, 2 or 3"
#endif

static uint64_t trialTimeUs=0,trialCalls=0;
static uint32_t trialReportAt=0;
static uint32_t trialCacheHits=0,trialCacheMisses=0;
static void trialReport(uint32_t start){
 trialTimeUs+=(uint32_t)(micros()-start);trialCalls++;
 uint32_t now=millis();
 if(now-trialReportAt>=3000){
  Serial.printf("FONT LAB method=%d calls=%u avg_us=%u total_ms_3s=%u hit=%u miss=%u PSRAM_free=%u\n",
   STOPWATCH_FONT_TRIAL,(unsigned)trialCalls,
   (unsigned)(trialCalls?trialTimeUs/trialCalls:0),
   (unsigned)(trialTimeUs/1000),
   (unsigned)trialCacheHits,(unsigned)trialCacheMisses,
   (unsigned)ESP.getFreePsram());
  trialReportAt=now;trialTimeUs=0;trialCalls=0;trialCacheHits=0;trialCacheMisses=0;
 }
}
static inline void trialPaletteInk(lgfx::LGFX_Sprite &s,uint16_t c,float fade){
 uint16_t t=tint(c,clampf(fade));
 s.setPaletteColor(255,(uint8_t)(((t>>11)&31)*255/31),
                      (uint8_t)(((t>>5)&63)*255/63),
                      (uint8_t)((t&31)*255/31));
}

// Source artwork: the SAME native FreeSansBold24pt7b used by clean Trial A,
// but rasterized ONCE PER NAME/CHARACTER instead of for every word occurrence.
#if STOPWATCH_FONT_TRIAL==1 || STOPWATCH_FONT_TRIAL==3
static lgfx::LGFX_Sprite trialSource(&M5.Display);
static String trialSourceName="";
static int trialSourceWidth=0,trialSourceHeight=0;
static float trialSourceNatural=1.f;
static bool trialSourceReady=false;
static bool trialEnsureSource(const String &name){
 if(trialSourceReady&&name==trialSourceName)return true;
 trialSourceReady=false;trialSourceName="";
 trialSource.deleteSprite();
 trialSource.setColorDepth(8);
 trialSource.setPsram(true);
 trialSource.setFont(&lgfx::fonts::FreeSansBold24pt7b);
 trialSource.setTextSize(1.f);
 int natural=trialSource.textWidth(name);
 trialSourceWidth=clampi(natural+20,38,448);
 trialSourceHeight=70;
 if(!trialSource.createSprite(trialSourceWidth,trialSourceHeight))return false;
 trialSource.createPalette();
 trialSource.setPaletteColor(0,0,0,0);
 trialSource.setPaletteColor(255,255,255,255);
 trialSource.fillScreen(0);
 trialSource.setTextColor(255,0);
 trialSource.setFont(&lgfx::fonts::FreeSansBold24pt7b);
 trialSource.setTextSize(1.f);
 trialSource.setTextDatum(middle_center);
 trialSource.drawString(name,trialSourceWidth/2,trialSourceHeight/2);
 trialSource.setPivot((float)trialSourceWidth*.5f,(float)trialSourceHeight*.5f);
 trialSourceNatural=(float)max(1,natural);
 trialSourceName=name;trialSourceReady=true;
 trialCacheMisses++;
 return true;
}
#endif

#if STOPWATCH_FONT_TRIAL==1
static void trialWord(const String &s,float x,float y,float width,float angle,uint16_t ink,float fade){
 uint32_t ts=micros();
 if(s.length()==0||width<4||fade<=.01f||!trialEnsureSource(s)){trialReport(ts);return;}
 trialPaletteInk(trialSource,ink,fade);
 float zoom=width/trialSourceNatural;
 trialSource.pushRotateZoomWithAA(&cv,x,y,angle*57.2957795f,zoom,zoom,0);
 trialCacheHits++;
 trialReport(ts);
}
static void trialChar(char ch,float x,float y,float scale,uint16_t ink,float angle){
 char s[2]={ch,0};trialWord(String(s),x,y,scale*75.f,angle,ink,1.f);
}
#endif

// Trial 2 deliberately avoids ALL rotation and resampling. The native
// FreeSansBold family is drawn directly at its final point size into a small
// indexed sprite, and copied to the full-screen framebuffer transparently.
// Rotating words remain UPRIGHT while their orbital positions still animate.
#if STOPWATCH_FONT_TRIAL==2
static lgfx::LGFX_Sprite trialFlat(&M5.Display);
static String trialFlatName="";
static uint16_t trialFlatBucket=0;
static int trialFlatWidth=0,trialFlatHeight=0;
static bool trialFlatReady=false;
static bool trialEnsureFlat(const String&s,float target){
 uint16_t bucket=(uint16_t)clampi((int)lroundf(target/6.f)*6,24,450);
 if(trialFlatReady&&trialFlatName==s&&bucket==trialFlatBucket)return true;
 trialFlatReady=false;trialFlat.deleteSprite();
 trialFlat.setColorDepth(8);trialFlat.setPsram(true);
 // Font chosen from the library's real sizes, NOT an image scaled in software.
 if(bucket<90)trialFlat.setFont(&lgfx::fonts::FreeSansBold12pt7b);
 else if(bucket<175)trialFlat.setFont(&lgfx::fonts::FreeSansBold18pt7b);
 else trialFlat.setFont(&lgfx::fonts::FreeSansBold24pt7b);
 // V16.8: enlarge selected words by drawing native font at the final pixel size.
 // The baseline D buckets below 270 remain pixel-identical.
 const float directZoom=bucket>=270?clampf(bucket/200.f,1.f,1.95f):1.f;
 trialFlat.setTextSize(directZoom);
 int native=trialFlat.textWidth(s);
 // Use no resampling: choose font and preserve its true pixel dimensions.
 trialFlatWidth=clampi(native+16,26,456);
 trialFlatHeight=bucket<90?42:bucket<175?54:clampi((int)lroundf(70*directZoom)+8,70,160);
 if(!trialFlat.createSprite(trialFlatWidth,trialFlatHeight))return false;
 trialFlat.createPalette();trialFlat.setPaletteColor(0,0,0,0);
 trialFlat.setPaletteColor(255,255,255,255);
 trialFlat.fillScreen(0);trialFlat.setTextColor(255,0);
 if(bucket<90)trialFlat.setFont(&lgfx::fonts::FreeSansBold12pt7b);
 else if(bucket<175)trialFlat.setFont(&lgfx::fonts::FreeSansBold18pt7b);
 else trialFlat.setFont(&lgfx::fonts::FreeSansBold24pt7b);
 trialFlat.setTextSize(directZoom);
 trialFlat.setTextDatum(middle_center);
 trialFlat.drawString(s,trialFlatWidth/2,trialFlatHeight/2);
 trialFlatName=s;trialFlatBucket=bucket;trialFlatReady=true;trialCacheMisses++;
 return true;
}
static void trialWord(const String &s,float x,float y,float width,float angle,uint16_t ink,float fade){
 uint32_t ts=micros();(void)angle;
 if(s.length()==0||width<4||fade<.01f||!trialEnsureFlat(s,width)){trialReport(ts);return;}
 trialPaletteInk(trialFlat,ink,fade);
 trialFlat.pushSprite(&cv,(int)lroundf(x-trialFlatWidth*.5f),
                          (int)lroundf(y-trialFlatHeight*.5f),0);
 trialCacheHits++;trialReport(ts);
}
static void trialChar(char ch,float x,float y,float scale,uint16_t ink,float angle){
 char s[2]={ch,0};trialWord(String(s),x,y,scale*75.f,angle,ink,1.f);
}
/* V17: lightweight selective edge smoothing ONLY for Chromatic EQ.
 * Native D glyph pixels are cached once as 8-bit coverage. A 3x3 edge
 * kernel creates translucent border pixels at final size: no scaling,
 * no rotation, no full-frame AA, and no rendering change to other effects.
 */
struct V17SoftGlyph {
 char ch=0; uint16_t bucket=0;
 uint16_t w=0,h=0; uint8_t* coverage=nullptr;
};
static V17SoftGlyph v17SoftGlyphs[36]={};
static uint8_t v17GlyphReplace=0;
static void trialCharSoft(char ch,float x,float y,float scale,uint16_t color,float angle){
 (void)angle;
 const float target=scale*75.f;
 const uint16_t bucket=(uint16_t)clampi((int)lroundf(target/6.f)*6,24,450);
 V17SoftGlyph* g=nullptr;
 for(auto &it:v17SoftGlyphs)if(it.coverage&&it.ch==ch&&it.bucket==bucket){g=&it;break;}
 if(!g){
  char word[2]={ch,0};
  if(!trialEnsureFlat(String(word),target))return;
  const uint8_t* src=(const uint8_t*)trialFlat.getBuffer();
  if(!src)return;
  g=&v17SoftGlyphs[v17GlyphReplace++%36];
  if(g->coverage){heap_caps_free(g->coverage);g->coverage=nullptr;}
  const int ww=trialFlatWidth,hh=trialFlatHeight;
  uint8_t *alpha=(uint8_t*)heap_caps_malloc((size_t)ww*hh,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
  if(!alpha)return;
  for(int yy=0;yy<hh;yy++)for(int xx=0;xx<ww;xx++){
   // A sharp, not smeared edge: keep most weight in the source pixel.
   const int p=yy*ww+xx;
   int v=src[p]?184:0;
   if(xx>0) v+=src[p-1]?13:0;
   if(xx+1<ww) v+=src[p+1]?13:0;
   if(yy>0) v+=src[p-ww]?13:0;
   if(yy+1<hh) v+=src[p+ww]?13:0;
   if(xx>0&&yy>0)v+=src[p-ww-1]?4:0;
   if(xx+1<ww&&yy>0)v+=src[p-ww+1]?4:0;
   if(xx>0&&yy+1<hh)v+=src[p+ww-1]?4:0;
   if(xx+1<ww&&yy+1<hh)v+=src[p+ww+1]?4:0;
   alpha[p]=(uint8_t)clampi(v,0,255);
  }
  g->ch=ch;g->bucket=bucket;g->w=ww;g->h=hh;g->coverage=alpha;
 }
 const int left=(int)lroundf(x-g->w*.5f),top=(int)lroundf(y-g->h*.5f);
 for(int j=0;j<g->h;j++){
  int destY=top+j;if((unsigned)destY>=(unsigned)H)continue;
  const int srcRow=j*g->w,destRow=destY*W;
  for(int i=0;i<g->w;i++){
   int destX=left+i;if((unsigned)destX>=(unsigned)W)continue;
   uint8_t a=g->coverage[srcRow+i];
   if(a>=247)fb[destRow+destX]=color;
   else if(a>11)fb[destRow+destX]=b5Mix(fb[destRow+destX],color,a);
  }
 }
}

#endif

// Trial 3: native font raster is the original clean A. For each distinct
// angle + size the library's AA transform is applied ONCE to a blank temp
// sprite. A tightly cropped 8-bit coverage tile is saved in PSRAM. Future
// frames place it through a plain, clipped RGB565 blend; no font draw,
// rotation, zoom, AA or float math in the hot inner pixel loop.
#if STOPWATCH_FONT_TRIAL==3
static constexpr int TA_ANGLES=48; // 7.5 degrees per bucket
static constexpr int TA_CACHE=160; // LRU entries across five size buckets
static constexpr int TA_SIZES=5;
static const uint16_t TA_WIDTHS[TA_SIZES]={72,90,108,144,210};
struct TATile{
 uint8_t *data=nullptr;
 int16_t x=0,y=0,w=0,h=0;
 uint8_t angle=0,size=0;
 uint32_t age=0;
};
static TATile ta[TA_CACHE];
static String taName="";
static lgfx::LGFX_Sprite taTemp(&M5.Display);
static bool taTempReady=false;
static uint32_t taAge=1,taBuildStamp=0;
static uint8_t taBuildsThisFrame=0;
static void taClear(){
 for(int i=0;i<TA_CACHE;i++){if(ta[i].data)heap_caps_free(ta[i].data);ta[i]=TATile{};}
}
static bool taSetup(const String &s){
 if(taName!=s){taClear();taName=s;trialCacheMisses++;}
 if(!trialEnsureSource(s))return false;
 if(!taTempReady){
  taTemp.setColorDepth(16);taTemp.setPsram(true);
  if(!taTemp.createSprite(260,260))return false;
  taTempReady=true;
 }
 return true;
}
static int taNearestSize(float width){
 int best=0;float d=fabsf(width-TA_WIDTHS[0]);
 for(int i=1;i<TA_SIZES;i++)if(fabsf(width-TA_WIDTHS[i])<d){best=i;d=fabsf(width-TA_WIDTHS[i]);}
 return best;
}
static TATile*taFind(int size,int ang){
 TATile*old=nullptr;
 for(int i=0;i<TA_CACHE;i++)if(ta[i].data&&ta[i].size==size&&ta[i].angle==ang){old=&ta[i];break;}
 if(old){old->age=++taAge;trialCacheHits++;}
 return old;
}
static TATile*taMake(int size,int ang){
 if(taBuildStamp!=frameStamp){taBuildStamp=frameStamp;taBuildsThisFrame=0;}
 // Limit expensive atlas generation to two misses per video frame.
 if(taBuildsThisFrame>=2)return nullptr;
 taBuildsThisFrame++;trialCacheMisses++;
 int slot=-1;uint32_t oldest=0xffffffffUL;
 for(int i=0;i<TA_CACHE;i++){
  if(!ta[i].data){slot=i;break;}
  if(ta[i].age<oldest){oldest=ta[i].age;slot=i;}
 }
 if(slot<0)return nullptr;
 if(ta[slot].data)heap_caps_free(ta[slot].data);
 ta[slot]=TATile{};
 // Transparent black background; native clean FreeSansBold AA sampling.
 taTemp.fillScreen(TFT_BLACK);
 trialSource.setPaletteColor(255,255,255,255);
 float width=TA_WIDTHS[size],zoom=width/trialSourceNatural;
 trialSource.pushRotateZoomWithAA(&taTemp,130.f,130.f,ang*(360.f/TA_ANGLES),zoom,zoom,0);
 const uint16_t *pix=(const uint16_t*)taTemp.getBuffer();
 if(!pix)return nullptr;
 int left=260,top=260,right=-1,bottom=-1;
 for(int y=0;y<260;y++)for(int x=0;x<260;x++){
  if(pix[y*260+x]){left=min(left,x);top=min(top,y);right=max(right,x);bottom=max(bottom,y);}
 }
 if(right<left||bottom<top)return nullptr;
 // Store only the nonblack bounding box to maximize PSRAM cache capacity.
 int w=right-left+1,h=bottom-top+1;
 uint8_t *coverage=(uint8_t*)heap_caps_malloc((size_t)w*h,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
 if(!coverage)return nullptr;
 for(int yy=0;yy<h;yy++)for(int xx=0;xx<w;xx++){
  uint16_t p=pix[(yy+top)*260+xx+left];
  coverage[yy*w+xx]=(uint8_t)((((p>>11)&31)*255+15)/31);
 }
 ta[slot].data=coverage;ta[slot].x=left-130;ta[slot].y=top-130;
 ta[slot].w=w;ta[slot].h=h;ta[slot].angle=ang;
 ta[slot].size=size;ta[slot].age=++taAge;
 return &ta[slot];
}
static void taBlit(const TATile &q,float xx,float yy,uint16_t color,float fade){
 int ox=(int)lroundf(xx)+q.x,oy=(int)lroundf(yy)+q.y;
 int lx=max(0,-ox),ly=max(0,-oy),rx=min((int)q.w,W-ox),ry=min((int)q.h,H-oy);
 if(rx<=lx||ry<=ly)return;
 for(int y=ly;y<ry;y++){
  int pos=(y+oy)*W+ox+lx,at=y*q.w+lx;
  for(int x=lx;x<rx;x++,pos++,at++){
   int a=(int)(q.data[at]*fade);
   if(a<7)continue;
   fb[pos]=b5Mix(fb[pos],color,(uint8_t)min(255,a));
  }
 }
}
static void trialWord(const String &s,float x,float y,float width,float angle,uint16_t ink,float fade){
 uint32_t ts=micros();
 if(s.length()==0||width<4||fade<.01f||!taSetup(s)){trialReport(ts);return;}
 int size=taNearestSize(width);
 int bin=(int)lroundf(angle*(TA_ANGLES/TAU));
 bin=(bin%TA_ANGLES+TA_ANGLES)%TA_ANGLES;
 TATile *tile=taFind(size,bin);
 if(!tile)tile=taMake(size,bin);
 if(tile)taBlit(*tile,x,y,ink,fade);
 else{
   // Only a cold-cache fallback; provides visible text during atlas warmup.
   trialPaletteInk(trialSource,ink,fade);
   float z=width/trialSourceNatural;
   trialSource.pushRotateZoom(&cv,x,y,angle*57.2957795f,z,z,0);
 }
 trialReport(ts);
}
static void trialChar(char ch,float x,float y,float scale,uint16_t ink,float angle){
 // Individual letters (#1 and #2) do not benefit from the shared whole-word
 // rotation atlas; render them from the small native glyph source directly.
 uint32_t ts=micros();
 char s[2]={ch,0};
 if(trialEnsureSource(String(s))){
  trialPaletteInk(trialSource,ink,1.f);
  float z=(scale*75.f)/trialSourceNatural;
  trialSource.pushRotateZoomWithAA(&cv,x,y,angle*57.2957795f,z,z,0);
 }
 trialReport(ts);
}
#endif
