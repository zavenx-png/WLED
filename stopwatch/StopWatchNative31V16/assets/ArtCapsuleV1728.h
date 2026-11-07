#pragma once
// Art Capsule V17.28 — native AirDrop receiver for M5Stack StopWatch C152.
// ESP32Drop (0BSD) must be installed at build time. Never run WiFi.begin()
// concurrently with AWDL: this receiver exclusively owns channel 6.
// Its receive service has no ad_end() API; leave the listening mode by reboot.
#include <LittleFS.h>
#include <ESP32AWDL.h>
#include <ESP32Drop.h>
#include <esp_heap_caps.h>
#include <cstring>

namespace ArtCapsule {
enum State : uint8_t { DORMANT, STARTING, LISTENING, RECEIVED, ERROR };
static State state=DORMANT;
static uint16_t *photo=nullptr;
static uint32_t lastFileAt=0,receivedBytes=0;
static char errorMessage[44]="";
static bool radioOwned=false,fsMounted=false,hasImage=false;
static constexpr int SIDE=466;
static constexpr size_t PIXELS=(size_t)SIDE*SIDE;
static constexpr size_t BYTES=PIXELS*2;
static constexpr uint32_t MAX_ARCHIVE=3UL*1024*1024;
static constexpr const char *IMAGE_PATH="/art_capsule.rgb565";

static void initialize(){
 if(fsMounted)return;
 fsMounted=LittleFS.begin(true);
 if(!fsMounted)return;
 File f=LittleFS.open(IMAGE_PATH,"r");
 if(!f)return;
 if((size_t)f.size()!=BYTES){f.close();return;}
 photo=(uint16_t*)heap_caps_malloc(BYTES,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
 if(photo){
   hasImage=(size_t)f.read((uint8_t*)photo,BYTES)==BYTES;
   if(!hasImage){heap_caps_free(photo);photo=nullptr;}
 }
 f.close();
}
static void store(){
 if(!fsMounted)initialize();
 if(!fsMounted||!hasImage||!photo)return;
 File f=LittleFS.open(IMAGE_PATH,"w");
 if(!f){Serial.println("ArtCapsule: flash write failed");return;}
 size_t n=f.write((const uint8_t*)photo,BYTES);
 f.close();
 if(n!=BYTES)Serial.println("ArtCapsule: incomplete photo persistence");
}
static void circleMask(){
 // Operate on PSRAM sprite pixels. Draw a true circular mask; never show
 // square corners on the physical OLED or in the stored round badge.
 for(int y=0;y<SIDE;++y)for(int x=0;x<SIDE;++x){
   const int dx=x-SIDE/2,dy=y-SIDE/2;
   if(dx*dx+dy*dy>(SIDE/2-5)*(SIDE/2-5))
     cv.drawPixel(x,y,TFT_BLACK);
 }
}
static bool onFile(void *,const AdFile *f){
 if(f->type==AD_FILE_APPLEDOUBLE)return true;
 if(f->type!=AD_FILE_JPEG && f->type!=AD_FILE_PNG){
   snprintf(errorMessage,sizeof(errorMessage),"Send JPEG or PNG");return false;
 }
 if(f->len==0 || f->len>MAX_ARCHIVE)return false;
 if(!photo)photo=(uint16_t*)heap_caps_malloc(BYTES,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
 if(!photo){snprintf(errorMessage,sizeof(errorMessage),"Photo RAM full");return false;}
 cv.fillScreen(TFT_BLACK);
 bool ok=f->type==AD_FILE_JPEG
   ? cv.drawJpg(f->data,f->len,0,0,0,0,0,0,0.0f,0.0f,middle_center)
   : cv.drawPng(f->data,f->len,0,0,0,0,0,0,0.0f,0.0f,middle_center);
 if(!ok){snprintf(errorMessage,sizeof(errorMessage),"Image decode failed");return false;}
 circleMask();
 memcpy(photo,cv.getBuffer(),BYTES);
 hasImage=true;lastFileAt=millis();receivedBytes=f->len;
 store();
 state=RECEIVED;
 Serial.printf("ArtCapsule: saved photo (%lu bytes)\n",(unsigned long)f->len);
 return true;
}
static bool begin(){
 if(radioOwned)return true;
 state=STARTING;
 M5.Mic.end();pendingMic=false;M5.Speaker.end();
 WiFi.disconnect(true,false);WiFi.mode(WIFI_OFF);
 if(awdl_begin()!=ESP_OK){
   snprintf(errorMessage,sizeof(errorMessage),"AWDL could not start");
   state=ERROR;M5.Mic.begin();return false;
 }
 radioOwned=true;
 ad_on_file(onFile,nullptr);
 if(ad_begin("ZAVEN ART CAPSULE",MAX_ARCHIVE)!=0){
   snprintf(errorMessage,sizeof(errorMessage),"AirDrop listener failed");
   state=ERROR;return false;
 }
 state=LISTENING;
 return true;
}
static void tick(){
 if(!radioOwned)return;
 ad_poll();
 char line[200];
 while(ad_diag_read_line(line,sizeof(line)))Serial.print(line);
}
static void render(){
 if(hasImage&&photo){
   memcpy(cv.getBuffer(),photo,BYTES);
 }else{
   cv.fillScreen(TFT_BLACK);
   float t=millis()*.001f;
   for(int i=0;i<72;i++){
     float a=i*6.2831853f/72+t*.14f;
     int x=CX+(int)(141*cosf(a)),y=CY+(int)(141*sinf(a));
     cv.fillCircle(x,y,1+i%3,rgb(22,70+i%4*18,95+i%5*22));
   }
   cv.fillCircle(CX,CY,114,rgb(8,21,35));
   cv.fillCircle(CX,CY,92,rgb(19,44,62));
   cv.fillCircle(CX,CY,83,rgb(2,9,17));
   cv.drawRoundRect(CX-36,CY-26,72,53,11,rgb(113,216,234));
   cv.fillCircle(CX+19,CY-13,7,rgb(113,216,234));
   cv.drawLine(CX-24,CY+14,CX-3,CY-9,rgb(113,216,234));
   cv.drawLine(CX-3,CY-9,CX+32,CY+22,rgb(113,216,234));
 }
 float t=millis()*.001f;
 uint16_t aqua=rgb(70+(int)(28*sinf(t*1.2f)),214,237);
 for(int i=0;i<4;++i)cv.drawCircle(CX,CY,R-7-i,aqua);
 if(radioOwned){
   if(state==ERROR)textC(errorMessage,CX,386,rgb(255,143,155),2);
   else if(state==RECEIVED && millis()-lastFileAt<4200)textC("PHOTO RECEIVED",CX,395,rgb(164,255,220),2);
   else textC("AIR DROP ON",CX,399,rgb(214,248,255),2);
 }else{
   textC(hasImage?"TAP TO RECEIVE MORE":"TAP TO ENABLE AIRDROP",CX,385,rgb(207,248,255),2);
 }
}
static bool busy(){
 if(!radioOwned)return false;
 AdStatus status;ad_stats_read(&status);return status.rx_active;
}
static void leaveEffect(){
 // ESP32Drop receiver has no listener teardown API. Reboot frees AWDL,
 // restores normal WiFi/ESP-NOW, and restores the user's selected effect.
 if(radioOwned){delay(80);ESP.restart();}
}
} // namespace ArtCapsule
