from pathlib import Path
p=Path("Zaven_Festival_Kit_V6/V6_Network.h")
s=p.read_text()
assert s.count("#include <esp_idf_version.h>")==1
s=s.replace("#include <esp_idf_version.h>","#include <esp_idf_version.h>\n#include <esp_heap_caps.h>",1)
start=s.index("// Start the Wi-Fi station driver WITHOUT connecting to any access point.")
end=s.index("static bool radioStart(){",start)
s=s[:start]+r"""// Initialize Wi-Fi exactly once at boot, before the 63KB LCD framebuffer.
// Do not use redundant STA.begin(false) or run automatic init retry storms.
static bool stationBootOK=false;
static void printHeap(const char* stage){
 Serial.printf("V6 HEAP %s internal-free=%u internal-largest=%u minimum-free=%u\n",stage,
  (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
  (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
  (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
}
static bool ensureStationReady(){
 wifi_mode_t mode=WIFI_MODE_NULL;
 esp_err_t modeRC=esp_wifi_get_mode(&mode);
 bool modeOK=(modeRC==ESP_OK && (mode&WIFI_MODE_STA)!=0);
 if(!modeOK){
   modeOK=WiFi.mode(WIFI_STA);
   modeRC=esp_wifi_get_mode(&mode);
 }
 uint8_t mac[6]={0};
 esp_err_t macRC=esp_wifi_get_mac(WIFI_IF_STA,mac);
 const bool ok=modeOK && modeRC==ESP_OK && (mode&WIFI_MODE_STA)!=0 &&
   macRC==ESP_OK && (mac[0]|mac[1]|mac[2]|mac[3]|mac[4]|mac[5])!=0;
 stationBootOK=ok;
 if(ok){
   WiFi.setSleep(false);
   WiFi.setAutoReconnect(false);
   Serial.printf("V6 RADIO STA READY %02X:%02X:%02X:%02X:%02X:%02X (no AP association)\n",
       mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
 }else{
   espReady.store(false);
   radioInitError.store((int)(modeRC!=ESP_OK?modeRC:macRC));
   Serial.printf("V6 RADIO STA START FAILED mode=%d modeRC=%d modeValue=%d macRC=%d\n",
       int(modeOK),int(modeRC),int(mode),int(macRC));
 }
 printHeap("after-WiFi-init");
 return ok;
}
"""+s[end:]
a=s.index("static void networkTask(void*){")
b=s.index(" Profile prof;loadProfileToWorker(prof);",a)
s=s[:a]+"""static void networkTask(void*){
 // The UI setup initialized Wi-Fi once, before the framebuffer was allocated.
 const bool staOK=stationBootOK;
"""+s[b:]
old="""   if(!radioOK && now-lastInitAttempt>=1500U){
       lastInitAttempt=now;
       if(ensureStationReady() && selectChannel(RADIO_LOCKED_CHANNEL)) radioStart();
     }"""
assert s.count(old)==1
s=s.replace(old,"""   // No background Wi-Fi init retries after an allocation failure.
   // Recovery is a restart or an explicit user Wi-Fi action.""",1)
assert s.count("uint32_t lastProbe=0,lastInitAttempt=millis();")==1
s=s.replace("uint32_t lastProbe=0,lastInitAttempt=millis();","uint32_t lastProbe=0;",1)
p.write_text(s)
p=Path("Zaven_Festival_Kit_V6/V6_UI.h")
s=p.read_text()
old=""" const bool landscapeOK=initLandscape();
 loadSettings();M5Cardputer.Display.setBrightness(app.screenLight);
 canvas.setColorDepth(16);spriteOK=landscapeOK && canvas.createSprite(240,135)!=nullptr;"""
new=""" const bool landscapeOK=initLandscape();
 // Initialize radio hardware first; all display drawing geometry is unchanged.
 printHeap("before-WiFi-init");
 const bool wifiAtBoot=ensureStationReady();
 loadSettings();M5Cardputer.Display.setBrightness(app.screenLight);
 canvas.setColorDepth(16);spriteOK=landscapeOK && canvas.createSprite(240,135)!=nullptr;
 printHeap("after-LCD-sprite");"""
assert s.count(old)==1;s=s.replace(old,new,1)
old=' xTaskCreatePinnedToCore(networkTask,"WLED_V6_NET",15000,nullptr,1,nullptr,0);'
new=' BaseType_t taskOK=xTaskCreatePinnedToCore(networkTask,"WLED_V6_NET",15000,nullptr,1,nullptr,0);\n Serial.printf("V6 RADIO network-task-created=%d WiFi-at-boot=%d\\n",int(taskOK==pdPASS),int(wifiAtBoot));'
assert s.count(old)==1;s=s.replace(old,new,1)
p.write_text(s)
print("Applied V6.2.3 staged radio startup, 1-time WiFi initialization and memory metrics.")
