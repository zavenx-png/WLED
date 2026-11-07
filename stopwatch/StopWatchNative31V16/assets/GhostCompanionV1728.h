#pragma once
// Ghost Companion V17.28. Procedural, independently animated; no copied KK assets.
// Touch: configure Wi-Fi/service once, then tap to record a voice turn.
// TLS uses a user-provided trusted CA certificate; credentials stay in NVS.
#include <WebServer.h>
#include <DNSServer.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <esp_heap_caps.h>
#include <cstring>

namespace GhostCompanion {
enum Phase : uint8_t { IDLE, PORTAL, CONNECTING, READY, RECORDING, REQUESTING, PLAYBACK, FAILURE };
static volatile Phase phase=IDLE;
static WebServer server(80);
static DNSServer dns;
static String ssid,pass,apiUrl,auth,caPem,apPass,status="Tap to connect Wi-Fi";
static uint32_t joinedAt=0,recordAt=0,blinkAt=0,lookAt=0;
static float eyesX=0.f,eyesY=0.f,tiltX=0.f,tiltY=0.f;
static int16_t *recording=nullptr,*reply=nullptr;
static size_t replySamples=0;
static constexpr int SAMPLE_RATE=16000,RECORD_SECS=3,REC_SAMPLES=SAMPLE_RATE*RECORD_SECS;
static constexpr size_t MAX_REPLY_BYTES=256*1024;
static TaskHandle_t httpTaskHandle=nullptr;
static bool recordingInProgress=false;
static float motion=0.f;
static String htmlEscape(const String& s){
 String o; for(size_t i=0;i<s.length();i++){char c=s[i];
   if(c=='&')o+="&amp;";else if(c=='<')o+="&lt;";else if(c=='>')o+="&gt;";
   else if(c=='"')o+="&quot;";else if(c=='\'')o+="&#39;";else o+=c;
 } return o;
}
static void load(){
 ssid=prefs.getString("ghost_ssid","");
 pass=prefs.getString("ghost_wifi","");
 apiUrl=prefs.getString("ghost_url","");
 auth=prefs.getString("ghost_tok","");
 caPem=prefs.getString("ghost_ca","");
}
static String portalPage(){
 String p=String(F("<!doctype html><html><head><meta name='viewport' content='width=device-width, initial-scale=1'><title>Ghost Companion</title>"
 "<style>body{font:17px system-ui;background:#101522;color:#edf8ff;max-width:600px;margin:40px auto;padding:16px}"
 "input,textarea{box-sizing:border-box;width:100%;padding:12px;margin:6px 0 15px;background:#273145;color:#fff;border:1px solid #7298af;border-radius:9px}"
 "button{padding:15px;background:#8de9e8;border:0;border-radius:10px;font-weight:bold;width:100%}label{display:block}</style></head>"
 "<body><h2>Ghost Companion</h2><p>Set the watch's 2.4 GHz Wi-Fi and your HTTPS AI voice gateway. API provider keys remain on your gateway, not on the watch.</p><form method='post' action='/save'>"));
 p+="<label>Wi-Fi name<input name='ssid' maxlength='64' value='"+htmlEscape(ssid)+"'></label>";
 p+="<label>Wi-Fi password<input name='wifi' type='password' maxlength='64' value='"+htmlEscape(pass)+"'></label>";
 p+="<label>Voice gateway URL (https://example.com/voice)<input name='url' maxlength='180' value='"+htmlEscape(apiUrl)+"'></label>";
 p+="<label>Gateway device token<input name='tok' type='password' maxlength='160' value='"+htmlEscape(auth)+"'></label>";
 p+="<label>Trusted server root CA (PEM, required for verified HTTPS)<textarea name='ca' rows='6'>"+htmlEscape(caPem)+"</textarea></label>";
 p+="<button>Save and connect</button></form><p>The setup hotspot closes after saving. Reopen it by tapping the Wi-Fi icon when disconnected.</p></body></html>";
 return p;
}
static void stopPortal(){
 dns.stop();server.stop();WiFi.softAPdisconnect(true);
}
static void connect(){
 if(ssid.isEmpty()){phase=IDLE;status="Tap to set up Wi-Fi";return;}
 WiFi.mode(WIFI_STA);WiFi.setSleep(false);
 WiFi.begin(ssid.c_str(),pass.c_str());
 joinedAt=millis();phase=CONNECTING;status="Connecting to Wi-Fi";
}
static void startPortal(){
 if(phase==PORTAL)return;
 if(phase==RECORDING||phase==REQUESTING||phase==PLAYBACK)return;
 WiFi.disconnect(true,false);
 uint32_t r0=esp_random(),r1=esp_random();
 char code[25];snprintf(code,sizeof(code),"Ghost-%08lX%08lX",(unsigned long)r0,(unsigned long)r1);
 apPass=code;joinedAt=0;
 WiFi.mode(WIFI_AP);
 if(!WiFi.softAP("ZAVEN-GHOST",apPass.c_str())){phase=FAILURE;status="Hotspot failed";return;}
 server.on("/",HTTP_GET,[](){server.send(200,"text/html; charset=utf-8",portalPage());});
 server.on("/save",HTTP_POST,[](){
   String ns=server.arg("ssid"),np=server.arg("wifi"),nu=server.arg("url"),
          nt=server.arg("tok"),nc=server.arg("ca");
   if(ns.length()>64||np.length()>64||nu.length()>180||nt.length()>160||nc.length()>4096){
     server.send(400,"text/plain","Field too long");return;
   }
   if(!nu.startsWith("https://") || nc.indexOf("-----BEGIN CERTIFICATE-----")<0){
     server.send(400,"text/plain","HTTPS gateway and trusted root CA PEM are required");return;
   }
   ssid=ns;pass=np;apiUrl=nu;auth=nt;caPem=nc;
   prefs.putString("ghost_ssid",ssid);prefs.putString("ghost_wifi",pass);
   prefs.putString("ghost_url",apiUrl);prefs.putString("ghost_tok",auth);
   prefs.putString("ghost_ca",caPem);
   server.send(200,"text/html","<h2>Saved</h2><p>Return to your StopWatch. The hotspot will close.</p>");
   joinedAt=millis(); // delayed close, so response gets to the browser
 });
 server.onNotFound([](){server.sendHeader("Location","http://192.168.4.1/");server.send(302,"text/plain","");});
 server.begin();
 dns.start(53,"*",WiFi.softAPIP());
 phase=PORTAL;status="Connect phone to ZAVEN-GHOST";
}
static void httpWorker(void*){
 const size_t byteCount=44+REC_SAMPLES*sizeof(int16_t);
 uint8_t *wav=(uint8_t*)heap_caps_malloc(byteCount,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
 if(!wav){status="Voice RAM full";phase=FAILURE;httpTaskHandle=nullptr;vTaskDelete(nullptr);return;}
 memset(wav,0,44);
 memcpy(wav,"RIFF",4);uint32_t v=(uint32_t)byteCount-8;memcpy(wav+4,&v,4);
 memcpy(wav+8,"WAVEfmt ",8);v=16;memcpy(wav+16,&v,4);
 uint16_t w=1;memcpy(wav+20,&w,2);w=1;memcpy(wav+22,&w,2);
 v=SAMPLE_RATE;memcpy(wav+24,&v,4);v=SAMPLE_RATE*2;memcpy(wav+28,&v,4);
 w=2;memcpy(wav+32,&w,2);w=16;memcpy(wav+34,&w,2);
 memcpy(wav+36,"data",4);v=REC_SAMPLES*2;memcpy(wav+40,&v,4);
 memcpy(wav+44,recording,REC_SAMPLES*2);
 WiFiClientSecure tls;
 tls.setCACert(caPem.c_str());
 tls.setTimeout(15000);
 HTTPClient http;
 bool ok=false;
 if(http.begin(tls,apiUrl)){
   http.setTimeout(20000);
   http.addHeader("Content-Type","audio/wav");
   http.addHeader("X-Device-Token",auth);
   int code=http.POST(wav,byteCount);
   if(code==200){
     int n=http.getSize();
     if(n>0&&n<=(int)MAX_REPLY_BYTES&&((n&1)==0)){
       if(reply){heap_caps_free(reply);reply=nullptr;}
       reply=(int16_t*)heap_caps_malloc(n,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
       if(reply){
         size_t have=http.getStream().readBytes((uint8_t*)reply,n);
         if(have==(size_t)n){replySamples=have/2;ok=true;}
       }
     }
   }
   Serial.printf("Ghost gateway status: %d\n",code);
   http.end();
 }
 heap_caps_free(wav);
 __sync_synchronize();
 if(ok){status="Speaking";phase=PLAYBACK;}
 else{status="Voice request failed";phase=FAILURE;}
 httpTaskHandle=nullptr;
 vTaskDelete(nullptr);
}
static void startRecording(){
 if(phase!=READY||WiFi.status()!=WL_CONNECTED)return;
 if(!recording)recording=(int16_t*)heap_caps_malloc(REC_SAMPLES*2,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
 if(!recording){phase=FAILURE;status="Mic RAM full";return;}
 M5.Mic.end();pendingMic=false;
 M5.Speaker.end();
 if(!M5.Mic.begin()||!M5.Mic.record(recording,REC_SAMPLES,SAMPLE_RATE)){
   phase=FAILURE;status="Mic start failed";return;
 }
 recordAt=millis();recordingInProgress=true;phase=RECORDING;status="Listening...";
}
static void tap(int x,int y){
 if(phase==PORTAL)return;
 if(phase==RECORDING||phase==REQUESTING||phase==PLAYBACK)return;
 if(phase==READY){
   if(x>300&&y<145){startPortal();return;}
   startRecording();return;
 }
 if(phase==CONNECTING){startPortal();return;}
 if(ssid.isEmpty()||apiUrl.isEmpty()||caPem.isEmpty())startPortal();
 else connect();
}
static void tick(){
 const uint32_t now=millis();
 // IMU does not drive the bobbing: the ghost floats even when absolutely still.
 if(M5.Imu.isEnabled()){
   // Read the shared IMU sample gathered by MotionPower. Do not consume
   // a second sample here or undermine the physical inactivity watchdog.
   const auto p=M5.Imu.getImuData();
   tiltX=tiltX*.85f+p.accel.x*.15f;
   tiltY=tiltY*.85f+p.accel.y*.15f;
   motion=motion*.92f+fabsf(p.gyro.z)*.08f;
 }
 if(phase==PORTAL){
   dns.processNextRequest();server.handleClient();
   if(joinedAt && now-joinedAt<4000 && !ssid.isEmpty() && !apiUrl.isEmpty()){} // serve reply
   if(joinedAt && now-joinedAt>=4000 && !ssid.isEmpty() && !apiUrl.isEmpty()){
     joinedAt=0;stopPortal();connect();
   }
 }else if(phase==CONNECTING){
   if(WiFi.status()==WL_CONNECTED){phase=READY;status="Tap to talk";}
   else if(now-joinedAt>16000){phase=FAILURE;status="Wi-Fi failed: tap to retry";}
 }else if(phase==READY && WiFi.status()!=WL_CONNECTED){
   phase=FAILURE;status="Wi-Fi lost: tap to retry";
 }else if(phase==RECORDING){
   if(recordingInProgress && !M5.Mic.isRecording()){
     recordingInProgress=false;phase=REQUESTING;status="Thinking...";
     if(xTaskCreatePinnedToCore(httpWorker,"ghost_https",11264,nullptr,1,&httpTaskHandle,0)!=pdPASS){
       phase=FAILURE;status="Voice task unavailable";
     }
   }
 }else if(phase==PLAYBACK && replySamples){
   M5.Mic.end();M5.Speaker.begin();
   if(!M5.Speaker.playRaw(reply,replySamples,24000,false,1,0)){
     status="Speaker playback failed";phase=FAILURE;M5.Speaker.end();M5.Mic.begin();return;
   }
   replySamples=0; // M5Unified borrows reply PCM memory; keep until playback completes
   status="Speaking"; // finish is observed in next pass
 }else if(phase==PLAYBACK && !M5.Speaker.isPlaying()){
   M5.Speaker.end();M5.Mic.begin();phase=READY;status="Tap to talk";
 }
}
static void ellipseGlow(int x,int y,int rx,int ry,uint16_t color){
 for(int i=15;i>0;i-=3)cv.fillEllipse(x,y,rx+i,ry+i,tint(color,.035f+(15-i)*.005f));
 cv.fillEllipse(x,y,rx,ry,color);
}
static void render(){
 float t=millis()*.001f;
 float bob=sinf(t*2.15f)*11.f+sinf(t*.72f)*3.f;
 int cx=CX+(int)(sinf(t*.71f)*5),cy=CY+(int)bob;
 cv.fillScreen(TFT_BLACK);
 // A little atmosphere: independent twinkling particles and ground glow.
 for(int i=0;i<32;i++){
   float a=i*2.399963f+t*(i%2?-.07f:.045f);
   float rr=142.f+22.f*sinf(t*.45f+i);
   int x=CX+(int)(cosf(a)*rr),y=CY+(int)(sinf(a)*rr);
   cv.fillCircle(x,y,1+(i%9==0),rgb(46+i%4*14,75+i%5*15,121+i%4*17));
 }
 ellipseGlow(CX,CY+141,115,15,rgb(24,39,71));
 // Tail waves are evaluated each frame and float at all times.
 uint16_t shadow=rgb(58,129,170),body=rgb(215,244,250),edge=rgb(116,208,228);
 cv.fillEllipse(cx,cy-28,112,123,shadow);
 cv.fillEllipse(cx,cy-38,108,124,body);
 cv.fillRoundRect(cx-108,cy-32,216,118,48,body);
 for(int j=0;j<5;j++){
   int x=cx-87+j*43;
   int y=cy+84+(int)(9*sinf(t*2.6f+j*1.2f));
   cv.fillCircle(x,y,25,body);
 }
 cv.fillEllipse(cx-41,cy-58,23,9,rgb(248,255,255));
 cv.fillEllipse(cx+57,cy+37,18,8,rgb(234,253,255));
 int gx=(int)(clampf(tiltX*23.f,-15.f,15.f)+eyesX);
 int gy=(int)(clampf(tiltY*18.f,-11.f,11.f)+eyesY);
 // Blinking is interval-based, not synchronized to audio or motion.
 float blink=fabsf(sinf(t*.88f));
 int eh=blink>.994f?3:38;
 if(phase==RECORDING)eh=42;
 for(int sign=-1;sign<=1;sign+=2){
   int ex=cx+sign*43,ey=cy-31;
   cv.fillEllipse(ex,ey,24,eh,rgb(25,42,62));
   if(eh>10)cv.fillEllipse(ex+gx/2,ey+gy/2,8,12,rgb(241,255,255));
 }
 if(phase==PLAYBACK||phase==RECORDING){
   int mouth=9+(int)(7*fabsf(sinf(t*12.f)));
   cv.fillEllipse(cx,cy+31,15,mouth,rgb(29,55,79));
 }else{
   cv.drawArc(cx,cy+14,27,22,30,151,rgb(35,62,81));
 }
 cv.fillEllipse(cx-75,cy+8,17,8,rgb(255,155,187));
 cv.fillEllipse(cx+75,cy+8,17,8,rgb(255,155,187));
 cv.setTextColor(rgb(218,239,249));cv.setTextDatum(middle_center);
 if(phase==PORTAL){
   textC("CONNECT TO",CX,357,rgb(220,253,255),2);
   textC("ZAVEN-GHOST",CX,382,rgb(127,236,237),2);
   textC(apPass,CX,406,rgb(243,245,248),2);
   textC("192.168.4.1",CX,428,rgb(205,225,238),2);
 }else{
   textC(status,CX,394,rgb(194,235,242),2);
   if(phase==READY)textC("WIFI SETTINGS: TOP RIGHT",CX,424,rgb(112,163,183),1);
 }
 if(phase==READY)cv.fillCircle(368,91,12,rgb(31,149,178));
}
static void deactivate(){
 if(phase==PORTAL)stopPortal();
 if(phase==RECORDING)M5.Mic.end();
 if(phase==PLAYBACK){M5.Speaker.stop();M5.Speaker.end();}
 // Requests are not cancelled mid-HTTPS. Caller should avoid changing effects
 // until the worker has finished, preventing Wi-Fi radio teardown races.
 if(phase==REQUESTING)return;
 WiFi.disconnect(true,false);WiFi.mode(WIFI_OFF);
 M5.Mic.begin();pendingMic=false;
 phase=IDLE;status="Tap to connect Wi-Fi";
}
static bool busy(){return phase==RECORDING||phase==REQUESTING||phase==PLAYBACK;}
} // namespace GhostCompanion
