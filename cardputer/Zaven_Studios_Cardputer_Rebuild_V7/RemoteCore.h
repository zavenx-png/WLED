#pragma once
#include <M5Cardputer.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <Preferences.h>
#include <SPI.h>
#include <SD.h>
#include <ctype.h>
#include "JsonLite.h"

namespace ZV7 {
// All drawing targets the physical Cardputer ADV landscape display, 240 x 135.
// This app never modifies orientation at runtime, never uses rotation preferences,
// and does not allocate sprites with different geometries.
static const uint16_t BG=0x0862, PANEL=0x18C5, INK=0xFFFF, MUTED=0xA534;
static const uint16_t AQUA=0x061F, TEAL=0x07D7, HONEY=0xFE40, ERRORC=0xF98A;
static const char* CACHE="/ZAVEN_V7_CATALOG.BIN";
static const uint32_t CACHE_MAGIC=0x37565A43u; // CZV7
static const uint16_t CACHE_VERSION=7;
static const uint8_t BROADCAST[6]={255,255,255,255,255,255};
static const int MAX_FX=320, MAX_PAL=256, MAX_PRESETS=250;
static const uint32_t RADIO_VALID_MS=7000;

template <size_t N> static void copyText(char (&dest)[N],const String& value){size_t n=value.length();if(n>=N)n=N-1;memcpy(dest,value.c_str(),n);dest[n]=0;}
// The template above lives in this .h (not the .ino) to avoid Arduino automatic
// prototype injection, which caused the earlier V3 compilation failure.
static bool zName(const char* s){while(*s==' ')s++;return (s[0]=='Z'||s[0]=='z')&&(s[1]==' '||s[1]=='-'||s[1]=='_'||s[1]==':');}
static bool nonblank(const char* s){return s[0]!=0&&strcmp(s,"RSVD")!=0&&strcmp(s,"Reserved")!=0&&strcmp(s,"-")!=0;}
static bool matches(const char* hay,const String& query){if(query.length()==0)return true;String a=hay,b=query;a.toLowerCase();b.toLowerCase();return a.indexOf(b)>=0;}
static uint16_t rgb565(uint8_t r,uint8_t g,uint8_t b){return uint16_t(((r&248)<<8)|((g&252)<<3)|(b>>3));}

struct Preset {char name[44];uint8_t valid;uint8_t playlist;};
struct Palette {char name[40];uint16_t sample[5];uint8_t valid,dynamic,preview;};
struct Catalog {
 uint32_t magic=CACHE_MAGIC;
 uint16_t version=CACHE_VERSION,fxCount=0,palCount=0,presetCount=0,zCount=0;
 char fx[MAX_FX][62]={{0}};
 char meta[MAX_FX][118]={{0}};
 uint16_t zIds[MAX_FX]={0};
 Palette pal[MAX_PAL]={};
 Preset preset[MAX_PRESETS+1]={};
 uint8_t presetIds[MAX_PRESETS]={0};
 uint8_t quick[10]={0};
};
struct RadioState {
 bool live=false,validV2=false,powered=false;
 uint8_t channel=1,status=0,brightness=120,preset=0,palette=0,speed=128,intensity=128,mainseg=0;
 uint8_t custom[3]={0,0,0},options=0;
 uint16_t effect=0;
 int8_t rssi=0;
 uint32_t lastAck=0,received=0,sent=0;
};
struct Incoming {bool valid=false;uint8_t bytes[24]={0};int8_t rssi=0;};
static portMUX_TYPE ackMux=portMUX_INITIALIZER_UNLOCKED;
static Incoming incoming;
static RadioState radio;
static Preferences store;
static Catalog catalog; // No dynamic catalog allocation; also persists as a single SD binary.
static bool catalogReady=false,sdReady=false,radioReady=false,refreshing=false;
static char apSsid[40]="Harness-AP",apPassword[65]="wled1234";
static uint8_t favorite[(MAX_FX+7)/8]={0};
static uint32_t txSeq=1,lastPing=0,lastScan=0,lastDraw=0,noticeUntil=0;
static char notice[75]="";
static uint8_t currentChannel=1;
static uint16_t shownEffect=0,shownPalette=0;
static uint8_t brightnessTarget=120;
static bool uiDirty=true;
static String searchText;
static uint8_t baseColor[3]={0,0,0};

enum Page:uint8_t {HOME,MAIN_MENU,EFFECTS,PRESETS,PALETTES,SETTINGS,BRIGHTNESS,HELP,DIAGNOSTICS,NETWORK,SEARCH,CONFIRM};
static Page page=HOME,priorPage=HOME;
static int selection=0,firstRow=0,settingsRow=0,helpPage=0;
static String editor;
static uint8_t editorMode=0;
static uint32_t lastInteraction=0,holdAt=0,repeatAt=0;
static int holdDir=0;
static bool paletteFavorites=false;
static bool favoriteOnly=false,zOnly=false;
static uint32_t lastCatalogAt=0;

static void say(const String& s,uint32_t duration=2400){copyText(notice,s);noticeUntil=millis()+duration;uiDirty=true;Serial.println(s);}
static void open(Page p){page=p;selection=0;firstRow=0;uiDirty=true;lastInteraction=millis();}
static void home(){page=HOME;uiDirty=true;}
static bool isFavorite(int id){return id>=0&&id<MAX_FX&&(favorite[id/8]&(1U<<(id%8)))!=0;}
static void toggleFavorite(int id){if(id<0||id>=MAX_FX)return;favorite[id/8]^=(1U<<(id%8));store.putBytes("fav",favorite,sizeof(favorite));uiDirty=true;}
static uint32_t checksum(const uint8_t* data,size_t len){uint32_t hash=2166136261u;for(size_t i=0;i<len;i++){hash^=data[i];hash*=16777619u;}return hash;}
static bool saveCatalog(){if(!sdReady)return false;
 SD.remove("/ZAVEN_V7_TEMP.BIN");File f=SD.open("/ZAVEN_V7_TEMP.BIN",FILE_WRITE);if(!f)return false;
 size_t written=f.write((const uint8_t*)&catalog,sizeof(catalog));uint32_t hash=checksum((const uint8_t*)&catalog,sizeof(catalog));
 written+=f.write((const uint8_t*)&hash,4);f.close();
 if(written!=sizeof(catalog)+4){SD.remove("/ZAVEN_V7_TEMP.BIN");return false;}
 SD.remove(CACHE);if(!SD.rename("/ZAVEN_V7_TEMP.BIN",CACHE))return false;
 return true;
}
static bool loadCatalog(){if(!sdReady)return false;File f=SD.open(CACHE,FILE_READ);if(!f)return false;
 if(f.size()!=sizeof(catalog)+4){f.close();return false;}
 size_t read=f.read((uint8_t*)&catalog,sizeof(catalog));uint32_t expected=0;
 if(f.read((uint8_t*)&expected,4)!=4){f.close();return false;}f.close();
 if(read!=sizeof(catalog)||catalog.magic!=CACHE_MAGIC||catalog.version!=CACHE_VERSION || expected!=checksum((const uint8_t*)&catalog,sizeof(catalog)))return false;
 return catalog.fxCount>0&&catalog.fxCount<=MAX_FX;
}
static void initializeCatalog(){memset(&catalog,0,sizeof(catalog));catalog.magic=CACHE_MAGIC;catalog.version=CACHE_VERSION;}
static void finishCatalog(){catalog.zCount=0;catalog.palCount=0;catalog.presetCount=0;
 for(int i=0;i<catalog.fxCount;i++)if(nonblank(catalog.fx[i])&&zName(catalog.fx[i]))catalog.zIds[catalog.zCount++]=i;
 for(int i=0;i<MAX_PAL;i++)if(catalog.pal[i].valid)catalog.palCount++;
 for(int i=1;i<=MAX_PRESETS;i++)if(catalog.preset[i].valid){catalog.presetIds[catalog.presetCount++]=i;}
 for(int i=0;i<10;i++)catalog.quick[i]=i<catalog.presetCount?catalog.presetIds[i]:0;
}
static const char* effectName(int id){if(id>=0&&id<catalog.fxCount&&catalog.fx[id][0])return catalog.fx[id];return "NO CACHED EFFECT NAME";}
static const char* paletteName(int id){if(id>=0&&id<MAX_PAL&&catalog.pal[id].valid)return catalog.pal[id].name;return "WLED PALETTE";}
static const char* presetName(int id){if(id>=1&&id<=MAX_PRESETS&&catalog.preset[id].valid)return catalog.preset[id].name;return "WLED PRESET";}
static int effectNext(int from,int dir,bool onlyZ){if(!catalogReady||!catalog.fxCount)return -1;int id=from;
 if(id<0||id>=catalog.fxCount)id=dir>0?catalog.fxCount-1:0;
 for(int count=0;count<catalog.fxCount;count++){id=(id+dir+catalog.fxCount)%catalog.fxCount;if(nonblank(catalog.fx[id])&&(!onlyZ||zName(catalog.fx[id]))&&(!favoriteOnly||isFavorite(id)))return id;}return -1;}
static int paletteNext(int from,int dir){int id=from;for(int count=0;count<MAX_PAL;count++){id=(id+dir+MAX_PAL)%MAX_PAL;if(catalog.pal[id].valid)return id;}return -1;}

// ESP-NOW callback: no I/O, no String/SD/Preferences, no drawing.
static void onRadioMessage(const esp_now_recv_info_t* info,const uint8_t* data,int length){
 if(!info||!data||length!=24||memcmp(data,"ZHA2",4)!=0||data[4]!=2)return;
 portENTER_CRITICAL(&ackMux);
 memcpy(incoming.bytes,data,24);incoming.rssi=info->rx_ctrl?info->rx_ctrl->rssi:0;incoming.valid=true;
 portEXIT_CRITICAL(&ackMux);
}
static bool setChannel(uint8_t ch){if(ch<1||ch>11)return false;
 esp_wifi_set_promiscuous(true);esp_err_t r=esp_wifi_set_channel(ch,WIFI_SECOND_CHAN_NONE);esp_wifi_set_promiscuous(false);
 if(r!=ESP_OK)return false;currentChannel=ch;radio.channel=ch;return true;}
static void stopRadio(){if(radioReady){esp_now_deinit();radioReady=false;}radio.live=false;}
static bool startRadio(){
 WiFi.mode(WIFI_STA);WiFi.setSleep(false);
 if(esp_now_init()!=ESP_OK){radioReady=false;return false;}
 if(esp_now_register_recv_cb(onRadioMessage)!=ESP_OK){esp_now_deinit();return false;}
 esp_now_peer_info_t peer={};memcpy(peer.peer_addr,BROADCAST,6);peer.channel=0;peer.ifidx=WIFI_IF_STA;peer.encrypt=false;
 if(esp_now_add_peer(&peer)!=ESP_OK){esp_now_deinit();return false;}
 radioReady=true;return true;
}
static bool send(uint8_t op,uint16_t value=0,uint8_t arg=0){
 if(!radioReady)return false;
 uint32_t seq=txSeq++;uint8_t frame[16]={'Z','H','R','2',2,op,(uint8_t)value,(uint8_t)(value>>8),255,arg,0,0,
 (uint8_t)seq,(uint8_t)(seq>>8),(uint8_t)(seq>>16),(uint8_t)(seq>>24)};
 bool ok=esp_now_send(BROADCAST,frame,sizeof(frame))==ESP_OK;
 if(ok)radio.sent++;return ok;
}
static void consumeAck(){Incoming temp;
 portENTER_CRITICAL(&ackMux);temp=incoming;incoming.valid=false;portEXIT_CRITICAL(&ackMux);
 if(!temp.valid)return;
 const uint8_t* p=temp.bytes;
 radio.validV2=true;radio.live=true;radio.lastAck=millis();radio.received++;radio.status=p[6];radio.rssi=temp.rssi;
 radio.powered=(p[7]&1)!=0;radio.brightness=p[8];radio.preset=p[9];
 radio.effect=uint16_t(p[10])|(uint16_t(p[11])<<8);radio.palette=p[12];radio.speed=p[13];radio.intensity=p[14];radio.mainseg=p[15];
 radio.custom[0]=p[20];radio.custom[1]=p[21];radio.custom[2]=p[22];radio.options=p[23];
 if(!refreshing){shownEffect=radio.effect;shownPalette=radio.palette;if(page!=BRIGHTNESS || millis()-lastInteraction>850)brightnessTarget=radio.brightness;}
 if(currentChannel!=store.getUChar("radioch",1))store.putUChar("radioch",currentChannel);
 uiDirty=true;
}
static void tickRadio(){consumeAck();uint32_t now=millis();
 radio.live=radio.lastAck!=0 && now-radio.lastAck<RADIO_VALID_MS;
 if(refreshing)return;
 if(radio.live){if(now-lastPing>1900){send(8);lastPing=now;}}
 else if(now-lastScan>1300){if(lastScan!=0)setChannel((currentChannel%11)+1);send(8);lastScan=now;}
}
static void applyEffect(int fx){if(fx<0||fx>=catalog.fxCount)return;
 if(!send(6,fx)){say("ESP-NOW not initialized");return;}
 shownEffect=fx;say(String("EFFECT: ")+effectName(fx),1200);uiDirty=true;}
static void applyPreset(int id){if(id<1||id>250)return;
 if(send(1,id))say(String("PRESET: ")+presetName(id),1200);else say("ESP-NOW not initialized");}
static void applyPalette(int id){if(id<0||id>=MAX_PAL)return;
 if(send(7,id)){shownPalette=id;say(String("PALETTE: ")+paletteName(id),1200);}else say("ESP-NOW not initialized");}
static void changeBrightness(int delta){brightnessTarget=(uint8_t)constrain((int)brightnessTarget+delta,1,255);
 send(2,brightnessTarget);page=BRIGHTNESS;lastInteraction=millis();uiDirty=true;}

static bool httpGet(const String& path,String& result,int waitMs=3500){
 HTTPClient req;req.setConnectTimeout(900);req.setTimeout(waitMs);
 IPAddress gateway=WiFi.gatewayIP();String base="http://"+gateway.toString();
 if(gateway==(IPAddress(0,0,0,0)))base="http://4.3.2.1";
 if(!req.begin(base+path))return false;
 int status=req.GET();if(status!=200){Serial.printf("GET %s failed %d\n",path.c_str(),status);req.end();return false;}
 result=req.getString();req.end();return !result.isEmpty();
}
static void parseEffectNames(const String& text){Json::Span root=Json::root(text);if(!root.ok()||text[root.a]!='[')return;
 Json::Array items(text,root);while(items.advance()&&items.index<MAX_FX){copyText(catalog.fx[items.index],Json::str(text,items.val));catalog.fxCount=items.index+1;}
}
static void parsePalettes(const String& text){Json::Span root=Json::root(text);if(!root.ok()||text[root.a]!='[')return;
 Json::Array items(text,root);while(items.advance()&&items.index<MAX_PAL){String n=Json::str(text,items.val);
 if(n.length()){copyText(catalog.pal[items.index].name,n);catalog.pal[items.index].valid=1;}}
}
static void parsePresets(const String& text){Json::Span root=Json::root(text);if(!root.ok()||text[root.a]!='{')return;
 Json::Object object(text,root);while(object.advance()){
 int id=object.key.toInt();if(id<1||id>250||text[object.val.a]!='{')continue;
 Preset& p=catalog.preset[id];copyText(p.name,Json::text(text,object.val,"n","Preset "+String(id)));
 p.valid=1;p.playlist=Json::member(text,object.val,"playlist").ok()?1:0;
 }
}
static void parseEffectMetadata(const String& text){Json::Span root=Json::root(text);if(!root.ok()||text[root.a]!='[')return;
 Json::Array items(text,root);while(items.advance()&&items.index<MAX_FX){copyText(catalog.meta[items.index],Json::str(text,items.val));}
}
static void parseDeviceState(const String& text){Json::Span root=Json::root(text);if(!root.ok()||text[root.a]!='{')return;
 radio.brightness=constrain(Json::number(text,root,"bri",120),0,255);
 brightnessTarget=radio.brightness;int mainseg=Json::number(text,root,"mainseg",0);
 Json::Span segments=Json::member(text,root,"seg");if(!segments.ok()||text[segments.a]!='[')return;
 Json::Array items(text,segments);
 while(items.advance())if(text[items.val.a]=='{'&&Json::number(text,items.val,"id",items.index)==mainseg){
 shownEffect=Json::number(text,items.val,"fx",0);shownPalette=Json::number(text,items.val,"pal",0);return;}
}
static void parsePalettePreview(const String& body){
 Json::Span root=Json::root(body),colors=Json::member(body,root,"p");if(!colors.ok())return;
 Json::Object entries(body,colors);while(entries.advance()){
 int id=entries.key.toInt();if(id<0||id>=MAX_PAL||body[entries.val.a]!='[')continue;
 Palette& p=catalog.pal[id];p.valid=1;
 if(!p.name[0])copyText(p.name,"Custom "+String(id));
 uint8_t stop[40],R[40],G[40],B[40];int count=0;bool dynamic=false;
 Json::Array steps(body,entries.val);while(steps.advance()){
  if(body[steps.val.a]=='[' && count<40){Json::Array v(body,steps.val);int vals[4]={};int n=0;
   while(v.advance()&&n<4)vals[n++]=Json::num(body,v.val);
   if(n==4){stop[count]=constrain(vals[0],0,255);R[count]=constrain(vals[1],0,255);G[count]=constrain(vals[2],0,255);B[count]=constrain(vals[3],0,255);count++;}
  }else if(body[steps.val.a]=='"')dynamic=true;
 }
 p.dynamic=dynamic?1:0;
 if(!count){p.preview=0;continue;}
 for(int i=0;i<5;i++){int x=(255*i)/4,idx=0;while(idx+1<count&&stop[idx+1]<x)idx++;
  int next=min(idx+1,count-1),scale=next==idx||stop[next]==stop[idx]?0:(256*(x-stop[idx]))/(stop[next]-stop[idx]);
  int rr=constrain(R[idx]+((int)R[next]-R[idx])*scale/256,0,255);
  int gg=constrain(G[idx]+((int)G[next]-G[idx])*scale/256,0,255);
  int bb=constrain(B[idx]+((int)B[next]-B[idx])*scale/256,0,255);
  p.sample[i]=rgb565(rr,gg,bb);
 }
 p.preview=1;
 }
}
static void syncDisplay(const char* message,int percent){
 auto& d=M5Cardputer.Display;
 d.fillScreen(BG);d.setTextColor(INK,BG);d.setTextSize(2);d.setCursor(8,9);d.print("ZAVEN STUDIOS");
 d.setTextSize(1);d.setTextColor(AQUA,BG);d.setCursor(9,48);d.print("ONE-TIME WLED SYNC");
 d.setTextColor(INK,BG);d.setCursor(9,65);d.print(message);
 d.drawRoundRect(9,91,222,15,4,MUTED);d.fillRoundRect(12,94,constrain(percent,0,100)*216/100,9,3,AQUA);
 d.setCursor(10,116);d.setTextColor(MUTED,BG);d.print("ESP-NOW resumes after download");
}
static bool syncCatalog(){if(refreshing)return false;refreshing=true;stopRadio();
 syncDisplay("Joining Harness-AP...",3);
 WiFi.mode(WIFI_STA);WiFi.setSleep(false);WiFi.disconnect(false,false);
 WiFi.begin(apSsid,apPassword);
 uint32_t started=millis();while(WiFi.status()!=WL_CONNECTED && millis()-started<12500){delay(80);M5Cardputer.update();}
 if(WiFi.status()!=WL_CONNECTED){
   Serial.printf("Wi-Fi association failed status %d\n",(int)WiFi.status());
   syncDisplay("Could not join Harness-AP",0);delay(650);
   WiFi.disconnect(false,false);setChannel(currentChannel);startRadio();refreshing=false;
   say("WLED AP unavailable; ESP-NOW works independently",3200);return false;
 }
 uint8_t ch=WiFi.channel();
 if(ch>=1&&ch<=11){currentChannel=ch;radio.channel=ch;store.putUChar("radioch",ch);}
 String body;initializeCatalog();bool ok=false;
 syncDisplay("Reading effect list...",12);
 if(httpGet("/json/eff",body,5500)){parseEffectNames(body);ok=catalog.fxCount>0;}
 body="";
 if(ok){syncDisplay("Reading WLED palettes...",28);if(httpGet("/json/pal",body))parsePalettes(body);body="";
 syncDisplay("Reading saved presets...",39);if(httpGet("/presets.json",body,6000))parsePresets(body);body="";
 syncDisplay("Reading effect settings...",53);if(httpGet("/json/fxdata",body,7000))parseEffectMetadata(body);body="";
 syncDisplay("Reading active effect...",63);if(httpGet("/json/state",body))parseDeviceState(body);body="";
 // Download real RGB palette stops, one page at a time. WLED returns m=max page.
 int maxPage=0;for(int pageIndex=0;pageIndex<=maxPage && pageIndex<38;pageIndex++){
   syncDisplay("Caching palette colors...",64+pageIndex*32/max(1,maxPage+1));
   if(!httpGet("/json/palx?page="+String(pageIndex),body,2600))break;
   Json::Span r=Json::root(body);if(pageIndex==0)maxPage=constrain(Json::number(body,r,"m",0),0,37);
   parsePalettePreview(body);body="";
 }
 finishCatalog();catalogReady=true;bool persisted=saveCatalog();if(!persisted)Serial.println("Catalog retained in RAM only");ok=true;
 }
 WiFi.disconnect(false,false);WiFi.mode(WIFI_STA);WiFi.setSleep(false);
 setChannel(currentChannel);startRadio();refreshing=false;uiDirty=true;lastScan=0;
 say(ok?(sdReady?"WLED catalog downloaded (SD save attempted)":"WLED catalog ready in RAM, SD missing"):"Catalog sync failed",3800);
 return ok;
}

static void tiny(int x,int y,const String& text,uint16_t color=INK){
 auto& d=M5Cardputer.Display;d.setTextSize(1);d.setTextColor(color,BG);d.setCursor(x,y);d.print(text);
}
static void large(int x,int y,const String& text,uint16_t color=INK){
 auto& d=M5Cardputer.Display;d.setTextSize(2);d.setTextColor(color,BG);d.setCursor(x,y);d.print(text);
}
static void header(const String& subtitle){
 auto& d=M5Cardputer.Display;
 large(8,7,"ZAVEN STUDIOS");
 tiny(9,28,subtitle,AQUA);
 d.drawFastHLine(8,43,224,PANEL);
}
static void footer(){auto& d=M5Cardputer.Display;
 d.drawFastHLine(8,119,224,PANEL);
 if(noticeUntil>millis()&&notice[0]){
  tiny(9,125,String(notice).substring(0,37),HONEY);
 }else if(!sdReady)tiny(9,125,"microSD unavailable - no saved catalog",ERRORC);
 else if(!catalogReady)tiny(9,125,"K: one-time Wi-Fi catalog sync",HONEY);
 else tiny(9,125,"CATALOG SAVED    "+String(catalog.fxCount)+" EFFECTS",MUTED);
}
static void titleLines(const String& text,int y,uint16_t tint=INK){
 String t=text;if(t.length()<20){large(8,y,t,tint);return;}
 if(t.length()<40){int split=t.lastIndexOf(' ',19);if(split<9)split=19;
   large(8,y,t.substring(0,split),tint);large(8,y+23,t.substring(split+1),tint);return;}
 // Very long WLED names retain their prefix and wrap at 37 characters per row.
 tiny(9,y,t.substring(0,37),tint);tiny(9,y+14,t.substring(37,74),tint);
}
static void drawRow(int y,const String& text,bool active,bool larger=false){
 auto& d=M5Cardputer.Display;
 if(active)d.fillRoundRect(6,y-2,228,18,3,PANEL);
 d.setTextSize(larger?2:1);d.setTextColor(active?AQUA:INK,active?PANEL:BG);
 d.setCursor(11,y);d.print(text.substring(0,larger?19:36));
}
static const char* menuLabels[]={"ALL WLED EFFECTS","Z EFFECTS","FAVORITE EFFECTS","SAVED PRESETS","WLED PALETTES","EFFECT SETTINGS",
 "RADIO DIAGNOSTICS","SYNC WLED CATALOG","HARNESS-AP SETTINGS","KEYBOARD HELP"};
static const int MENU_COUNT=sizeof(menuLabels)/sizeof(menuLabels[0]);
static int listEffect(int rank){if(!catalogReady)return -1;
 for(int i=0;i<catalog.fxCount;i++){
  if(!nonblank(catalog.fx[i]))continue;
  if(zOnly&&!zName(catalog.fx[i]))continue;
  if(favoriteOnly&&!isFavorite(i))continue;
  if(!matches(catalog.fx[i],searchText))continue;
  if(rank--==0)return i;
 }return -1;
}
static int effectItems(){if(!catalogReady)return 0;int count=0;
 for(int i=0;i<catalog.fxCount;i++)if(nonblank(catalog.fx[i])&&(!zOnly||zName(catalog.fx[i]))&&(!favoriteOnly||isFavorite(i))&&matches(catalog.fx[i],searchText))count++;
 return count;
}
static int paletteItems(){return catalogReady?catalog.palCount:0;}
static int paletteAt(int rank){for(int i=0;i<MAX_PAL;i++)if(catalog.pal[i].valid&&rank--==0)return i;return -1;}
static int presetAt(int rank){return catalogReady&&rank>=0&&rank<catalog.presetCount?catalog.presetIds[rank]:-1;}
static void render(){auto& d=M5Cardputer.Display;
 d.fillScreen(BG);
 switch(page){
  case HOME:{
   header(radio.live?"ESP-NOW SYNC  |  CH "+String(currentChannel)+"  |  "+String(radio.rssi)+" dBm":"ESP-NOW SEARCHING  |  CH "+String(currentChannel));
   tiny(9,52,"CURRENT EFFECT",MUTED);
   titleLines(catalogReady?String(effectName(shownEffect)):"READY FOR WLED CATALOG",70);
   if(catalogReady&&shownEffect<catalog.fxCount)tiny(9,109,"EFFECT ID "+String(shownEffect),MUTED);
   footer();break;
  }
  case MAIN_MENU:{header("REMOTE MENU");int start=max(0,selection-3);for(int i=0;i<5&&i+start<MENU_COUNT;i++)drawRow(50+i*15,String(i+start+1)+". "+menuLabels[i+start],i+start==selection);
   footer();break;}
  case EFFECTS:{header(zOnly?"Z EFFECTS":favoriteOnly?"FAVORITES":"ALL EFFECTS");int count=effectItems();
   if(!count)tiny(9,67,"No cached matching effects",HONEY);
   else {int start=max(0,selection-2);for(int i=0;i<5&&i+start<count;i++){
    int id=listEffect(start+i);drawRow(51+14*i,String(id)+"  "+effectName(id),start+i==selection);
   }}footer();break;}
  case PRESETS:{header("WLED PRESETS  |  1-0 = FIRST TEN");if(!catalogReady||!catalog.presetCount)tiny(9,67,"No cached presets. Press K.",HONEY);
   else {int start=max(0,selection-2);for(int i=0;i<5&&i+start<catalog.presetCount;i++){
    int id=presetAt(start+i);drawRow(51+14*i,String(id)+"  "+presetName(id),start+i==selection);
   }}footer();break;}
  case PALETTES:{header("WLED COLOR PALETTES ONLY");int id=paletteAt(selection);
   if(id>=0){tiny(9,52,String(selection+1)+" OF "+String(paletteItems()),MUTED);
    String name=paletteName(id);if(name.length()<19)large(9,65,name);else tiny(9,70,name.substring(0,35),INK);
    const Palette& p=catalog.pal[id];
    if(p.preview){for(int i=0;i<5;i++){int x=26+46*i;d.fillCircle(x,102,10,p.sample[i]);d.drawCircle(x,102,11,INK);}}
    else tiny(9,99,p.dynamic?"DYNAMIC - colors change":"NO FIXED RGB PREVIEW",MUTED);
   }else tiny(9,66,"Press K to download palettes",HONEY);
   footer();break;}
  case SETTINGS:{header("CURRENT EFFECT SETTINGS");
   static const char* fields[]={"Speed","Intensity","Custom 1","Custom 2","Custom 3","Option 1","Option 2","Option 3"};
   uint8_t numbers[8]={radio.speed,radio.intensity,radio.custom[0],radio.custom[1],radio.custom[2],(uint8_t)(radio.options&1),(uint8_t)((radio.options>>1)&1),(uint8_t)((radio.options>>2)&1)};
   for(int i=0;i<4;i++){int idx=(settingsRow/4)*4+i;int y=52+i*17;
    if(idx>=8)break;
    String label=fields[idx];
    // Load effect-specific labels supplied by /json/fxdata where possible.
    if(catalogReady&&shownEffect<catalog.fxCount){String meta=catalog.meta[shownEffect];
     int sep=meta.indexOf(';');String axes=sep>=0?meta.substring(0,sep):meta;
     if(idx<5){int st=0;for(int k=0;k<idx;k++){int j=axes.indexOf(',',st);st=j<0?axes.length():j+1;}
      int en=axes.indexOf(',',st);if(en<0)en=axes.length();String custom=axes.substring(st,en);
      if(custom.length()&&custom!="!")label=custom.substring(0,16);
     }
    }
    if(idx==settingsRow)d.fillRoundRect(6,y-2,228,17,3,PANEL);
    tiny(10,y,label,idx==settingsRow?AQUA:INK);
    tiny(194,y,idx>=5?(numbers[idx]?"ON":"OFF"):String(numbers[idx]),INK);
   }footer();break;}
  case BRIGHTNESS:{header("HARNESS BRIGHTNESS");large(9,56,String((int)brightnessTarget*100/255)+"%");
   d.drawRoundRect(74,61,146,29,5,INK);d.fillRect(220,69,6,13,INK);
   int fill=(int)brightnessTarget*139/255;if(fill>0)d.fillRoundRect(78,65,fill,21,3,AQUA);
   tiny(9,103,"+ / -  HOLD FOR CONTINUOUS",MUTED);footer();break;}
  case HELP:{header("KEYBOARD MAP  |  TAB NEXT");
    if(helpPage%2==0){
      tiny(9,50,"1-0: First 10 saved WLED presets");tiny(9,62,", .: Previous / next ALL effects");
      tiny(9,74,"[ ]: Previous / next Z effects");tiny(9,86,"+ -: Brightness (hold supported)");
      tiny(9,98,"C: WLED palettes   P: Power");tiny(9,110,"ESC / DEL: Back to effects");
    }else{
      tiny(9,50,"; , . /: Up Left Down Right in menus");tiny(9,62,"Enter: Select / apply");
      tiny(9,74,"M: Menu   G: Effects   Q: Search");tiny(9,86,"E: Settings   B: Presets");
      tiny(9,98,"K: One-time Wi-Fi cache sync");tiny(9,110,"I: Radio status   X: BLACKOUT");
    }footer();break;}
  case DIAGNOSTICS:{header("ESP-NOW DIAGNOSTICS");
   tiny(9,50,"RADIO: "+String(radioReady?"INITIALIZED":"FAILED"));
   tiny(9,62,"HARNESS: "+String(radio.live?"ACK RECEIVED":"NO ACK YET"),radio.live?TEAL:HONEY);
   tiny(9,74,"CHANNEL: "+String(currentChannel)+"  RSSI: "+String(radio.rssi));
   tiny(9,86,"SENT: "+String(radio.sent)+"  ACKS: "+String(radio.received));
   tiny(9,98,"WLED V2: "+String(radio.validV2?"YES":"NOT VERIFIED"));
   tiny(9,110,"MICROSD: "+String(sdReady?"READY":"MISSING"));footer();break;}
  case NETWORK:{header("ONE-TIME WI-FI SYNC SETTINGS");
   tiny(9,52,"1  SSID: "+String(apSsid).substring(0,21));
   tiny(9,68,"2  PASSWORD: "+String("********"));
   tiny(9,90,"K  DOWNLOAD WLED CATALOG",AQUA);
   tiny(9,106,"Normal control uses ESP-NOW only",MUTED);footer();break;}
  case SEARCH:{header(editorMode==1?"EDIT HARNESS-AP NAME":editorMode==2?"EDIT HARNESS-AP PASSWORD":"SEARCH EFFECTS");
   tiny(9,55,"Type text, Enter to save",MUTED);
   String preview=(editorMode==2)?String("********"):editor;
   titleLines(preview.substring(max(0,(int)preview.length()-36)),72);
   footer();break;}
  case CONFIRM:{header("CONFIRM");large(9,59,"PRESS ENTER");tiny(9,91,"ESC / DEL cancels",MUTED);footer();break;}
 }
 lastDraw=millis();uiDirty=false;
}

static void chooseMenu(){switch(selection){
 case 0:zOnly=false;favoriteOnly=false;searchText="";open(EFFECTS);break;
 case 1:zOnly=true;favoriteOnly=false;searchText="";open(EFFECTS);break;
 case 2:zOnly=false;favoriteOnly=true;searchText="";open(EFFECTS);break;
 case 3:open(PRESETS);break;
 case 4:open(PALETTES);break;
 case 5:settingsRow=0;open(SETTINGS);break;
 case 6:open(DIAGNOSTICS);break;
 case 7:syncCatalog();home();break;
 case 8:open(NETWORK);break;
 case 9:helpPage=0;open(HELP);break;
 }}
static void adjustSetting(int dir){int idx=settingsRow;int value=0;
 if(idx==0)value=radio.speed;
 else if(idx==1)value=radio.intensity;
 else if(idx>=2&&idx<=4)value=radio.custom[idx-2];
 else if(idx>=5&&idx<=7)value=(radio.options>>(idx-5))&1;
 if(idx>=5)value=!value;else value=constrain(value+dir*6,0,idx==4?31:255);
 if(send(9,value,idx)){
  if(idx==0)radio.speed=value;
  else if(idx==1)radio.intensity=value;
  else if(idx>=2&&idx<=4)radio.custom[idx-2]=value;
  else if(value)radio.options|=(1U<<(idx-5));else radio.options&=~(1U<<(idx-5));
  uiDirty=true;
 }
}
static void navigate(char key){int dir=(key==';'||key==',')?-1:1;
 switch(page){
 case HOME:
  if(key==','||key=='.'){
   int fx=effectNext(shownEffect,dir,false);if(fx>=0&&fx<=255)applyEffect(fx);else say("Effect catalog missing or FX ID unsupported");
  }else if(key==';')open(PRESETS);else if(key=='/')open(SETTINGS);
  break;
 case MAIN_MENU:selection=(selection+MENU_COUNT+dir)%MENU_COUNT;uiDirty=true;break;
 case EFFECTS:{int count=effectItems();if(!count)break;
  selection=(selection+count+dir)%count;uiDirty=true;break;}
 case PRESETS:if(catalog.presetCount){selection=(selection+catalog.presetCount+dir)%catalog.presetCount;uiDirty=true;}break;
 case PALETTES:if(catalog.palCount){selection=(selection+catalog.palCount+dir)%catalog.palCount;uiDirty=true;}break;
 case SETTINGS:if(key==';'||key=='.'){settingsRow=(settingsRow+8+dir)%8;uiDirty=true;}else adjustSetting(dir);break;
 case HELP:helpPage=(helpPage+1)%2;uiDirty=true;break;
 default:break;
 }
}
static void accept(){switch(page){
 case HOME:settingsRow=0;open(SETTINGS);break;
 case MAIN_MENU:chooseMenu();break;
 case EFFECTS:{int id=listEffect(selection);if(id>=0){applyEffect(id);home();}break;}
 case PRESETS:{int id=presetAt(selection);if(id>0){applyPreset(id);home();}break;}
 case PALETTES:{int id=paletteAt(selection);if(id>=0){applyPalette(id);home();}break;}
 case SETTINGS:adjustSetting(1);break;
 case BRIGHTNESS:home();break;
 case DIAGNOSTICS:send(8);say("ESP-NOW ping sent");break;
 case HELP:helpPage=(helpPage+1)%2;uiDirty=true;break;
 case SEARCH:{
  if(editorMode==1){copyText(apSsid,editor);store.putString("apSSID",apSsid);open(NETWORK);say("SSID saved. Press K to sync.");}
  else if(editorMode==2){copyText(apPassword,editor);store.putString("apPass",apPassword);open(NETWORK);say("Password saved. Press K to sync.");}
  else {searchText=editor;zOnly=false;favoriteOnly=false;open(EFFECTS);}
  break;}
 default:home();break;
 }}
static void beginEditor(int mode){editorMode=mode;
 if(mode==1)editor=apSsid;else if(mode==2)editor=apPassword;else editor=searchText;
 open(SEARCH);
}
static void onKey(char raw){char ch=(char)tolower((unsigned char)raw);
 if(page==SEARCH){if(raw>=32&&raw<=126&&editor.length()<80){editor+=raw;uiDirty=true;}return;}
 if(page==NETWORK){if(ch=='1'){beginEditor(1);return;}if(ch=='2'){beginEditor(2);return;}if(ch=='k'){syncCatalog();home();return;}}
 if(ch=='p'){send(3);say("POWER TOGGLE SENT",1000);return;}
 if(ch=='x'){send(5);say("BLACKOUT SENT",1000);return;}
 if(ch=='k'){syncCatalog();home();return;}
 if(ch=='t'){send(8);say("PING SENT",1000);return;}
 if(ch=='m'){open(MAIN_MENU);return;}
 if(ch=='i'){open(DIAGNOSTICS);return;}
 if(ch=='w'){open(NETWORK);return;}
 if(ch=='g'){zOnly=false;favoriteOnly=false;searchText="";open(EFFECTS);return;}
 if(ch=='b'){open(PRESETS);return;}
 if(ch=='e'){settingsRow=0;open(SETTINGS);return;}
 if(ch=='c'){open(PALETTES);return;}
 if(ch=='q'){beginEditor(0);return;}
 if(ch=='f'){
  if(page==EFFECTS){int id=listEffect(selection);if(id>=0){toggleFavorite(id);say(isFavorite(id)?"Saved favorite":"Removed favorite");}return;}
  int id=shownEffect;if(id<MAX_FX){toggleFavorite(id);say(isFavorite(id)?"Saved favorite":"Removed favorite");}
  return;
 }
 if(ch>='1'&&ch<='9'){int id=catalog.quick[ch-'1'];if(id)applyPreset(id);else say("Cache presets first (K)");return;}
 if(ch=='0'){int id=catalog.quick[9];if(id)applyPreset(id);else say("Cache presets first (K)");return;}
 if(ch=='['||ch==']'){
  int next=effectNext(shownEffect,ch=='['?-1:1,true);if(next>=0&&next<=255)applyEffect(next);
  home();return;
 }
 // Bare arrow-position keys navigate with NO Fn. Main page comma/period
 // change all effects. Menus use semicolon/comma/period/slash for U/L/D/R.
 if(ch==';'||ch==':'||ch==','||ch=='<'||ch=='.'||ch=='>'||ch=='/'||ch=='?'){
  if(ch==':')ch=';';else if(ch=='<')ch=',';else if(ch=='>')ch='.';else if(ch=='?')ch='/';
  navigate(ch);return;
 }
}
static void keyboard(){
 const uint32_t now=millis();
 bool plus=M5Cardputer.Keyboard.isKeyPressed('=')||M5Cardputer.Keyboard.isKeyPressed('+');
 bool minus=M5Cardputer.Keyboard.isKeyPressed('-')||M5Cardputer.Keyboard.isKeyPressed('_');
 bool allow=page!=SEARCH && page!=NETWORK;
 int dir=allow&&plus!=minus?(plus?1:-1):0;
 if(dir){
  if(dir!=holdDir){holdDir=dir;holdAt=now;repeatAt=now;changeBrightness(6*dir);}
  else if(now-holdAt>=370 && now-repeatAt>=230){repeatAt=now;changeBrightness(6*dir);}
 }else holdDir=0;
 if(!M5Cardputer.Keyboard.isChange()||!M5Cardputer.Keyboard.isPressed())return;
 lastInteraction=now;
 auto keys=M5Cardputer.Keyboard.keysState();
 bool back=M5Cardputer.Keyboard.isKeyPressed('`')||M5Cardputer.Keyboard.isKeyPressed(KEY_BACKSPACE)||keys.del;
 if(back){if(page==SEARCH && (keys.del||M5Cardputer.Keyboard.isKeyPressed(KEY_BACKSPACE))){
  if(editor.length())editor.remove(editor.length()-1);uiDirty=true;return;}
  home();return;
 }
 if(keys.tab){if(page==HELP)helpPage=(helpPage+1)%2;else {helpPage=0;open(HELP);}uiDirty=true;return;}
 if(keys.enter){accept();return;}
 for(char ch:keys.word){
  if(page!=SEARCH&&(ch=='='||ch=='+'||ch=='-'||ch=='_'))continue;
  onKey(ch);break;
 }
}
static void start(){
 Serial.begin(115200);
 auto cfg=M5.config();
 M5Cardputer.begin(cfg,true);
 // M5Stack official Cardputer example: landscape 240x135 uses rotation 1.
 // Exactly ONE rotation call, once at boot. No orientation menus or migrations.
 M5Cardputer.Display.setRotation(1);
 M5Cardputer.Display.setBrightness(125);
 M5Cardputer.Display.fillScreen(BG);
 large(9,20,"ZAVEN STUDIOS");tiny(9,56,"STARTING ESP-NOW REMOTE...",MUTED);
 store.begin("zaven-v7",false);
 String ssid=store.getString("apSSID","");if(ssid.length())copyText(apSsid,ssid);
 String pass=store.getString("apPass","");
 if(!pass.length()){
  Preferences old; if(old.begin("zaven-v6",true)){pass=old.getString("p0pw","");old.end();}
  if(!pass.length()&&old.begin("festival2",true)){pass=old.getString("wledPass","");old.end();}
 }
 if(pass.length())copyText(apPassword,pass);
 if(store.getBytesLength("fav")==sizeof(favorite))store.getBytes("fav",favorite,sizeof(favorite));
 // Official Cardputer ADV microSD pin mapping. Missing SD cannot halt controls.
 SPI.begin(40,39,14,12);sdReady=SD.begin(12,SPI,10000000);
 initializeCatalog();catalogReady=loadCatalog();
 currentChannel=constrain((int)store.getUChar("radioch",1),1,11);
 WiFi.mode(WIFI_STA);WiFi.setSleep(false);WiFi.setAutoReconnect(false);
 if(!setChannel(currentChannel))Serial.println("Initial channel selection failed");
 if(!startRadio())say("ESP-NOW init failed - check radio",4500);
 send(8);lastPing=millis();
 page=HOME;uiDirty=true;lastInteraction=millis();
 Serial.printf("Zaven V7 boot: display 240x135 rotation1; SD %s catalog %s; CH %u\n",sdReady?"OK":"MISSING",catalogReady?"READY":"EMPTY",currentChannel);
}
static void step(){
 M5Cardputer.update();
 keyboard();tickRadio();
 uint32_t now=millis();
 if(page==BRIGHTNESS && now-lastInteraction>=3000)home();
 if(noticeUntil&&now>=noticeUntil){noticeUntil=0;notice[0]=0;uiDirty=true;}
 // Home refreshes connection status periodically; rest redraws only on change.
 if(uiDirty||(page==HOME&&now-lastDraw>=800)||(page==DIAGNOSTICS&&now-lastDraw>=900))render();
 delay(7);
}
} // namespace ZV7