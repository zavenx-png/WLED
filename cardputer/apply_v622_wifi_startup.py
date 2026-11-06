from pathlib import Path

p=Path("Zaven_Festival_Kit_V6/V6_Network.h")
s=p.read_text()
helper="""// Start the Wi-Fi station driver WITHOUT connecting to any access point.
// ESP-NOW init alone is insufficient; we must verify an active STA interface.
static bool ensureStationReady(){
 const bool modeOK=WiFi.mode(WIFI_STA);
 const bool beginOK=WiFi.STA.begin(false);
 WiFi.setSleep(false);
 WiFi.setAutoReconnect(false);
 wifi_mode_t mode=WIFI_MODE_NULL;
 const esp_err_t modeRC=esp_wifi_get_mode(&mode);
 uint8_t mac[6]={0};
 const esp_err_t macRC=esp_wifi_get_mac(WIFI_IF_STA,mac);
 const bool ok=modeOK && beginOK && modeRC==ESP_OK &&
               (mode & WIFI_MODE_STA)!=0 && macRC==ESP_OK &&
               (mac[0]|mac[1]|mac[2]|mac[3]|mac[4]|mac[5])!=0;
 if(!ok){
   espReady.store(false);
   Serial.printf("V6 RADIO STA START FAILED mode=%d staBegin=%d getMode-rc=%d modeValue=%d getMac-rc=%d\\n",
       int(modeOK),int(beginOK),int(modeRC),int(mode),int(macRC));
 }else{
   Serial.printf("V6 RADIO STA READY %02X:%02X:%02X:%02X:%02X:%02X (no AP association)\\n",
       mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
 }
 return ok;
}
"""
assert s.count("static bool radioStart(){")==1
s=s.replace("static bool radioStart(){",helper+"""static bool radioStart(){
 // ESP-NOW initialization can succeed without a working Wi-Fi station.
 wifi_mode_t mode=WIFI_MODE_NULL;
 uint8_t ch=0;wifi_second_chan_t sec=WIFI_SECOND_CHAN_NONE;
 const esp_err_t m=esp_wifi_get_mode(&mode);
 const esp_err_t c=esp_wifi_get_channel(&ch,&sec);
 if(m!=ESP_OK || (mode&WIFI_MODE_STA)==0 || c!=ESP_OK || ch!=RADIO_LOCKED_CHANNEL){
   espReady.store(false);radioInitError.store(int(m!=ESP_OK?m:c));
   Serial.printf("V6 RADIO START BLOCKED mode-rc=%d mode=%d channel-rc=%d channel=%d\\n",int(m),int(mode),int(c),int(ch));
   return false;
 }
""",1)
old="""static void networkTask(void*){
 WiFi.mode(WIFI_STA);WiFi.setSleep(false);WiFi.setAutoReconnect(false);
 WiFi.disconnect(false,false);
 Profile prof;loadProfileToWorker(prof);
 selectChannel(RADIO_LOCKED_CHANNEL);
 radioStart();"""
new="""static void networkTask(void*){
 // Establish the STA driver before setting channel or starting ESP-NOW.
 // Do not disconnect an as-yet unstarted STA interface.
 const bool staOK=ensureStationReady();
 Profile prof;loadProfileToWorker(prof);
 if(staOK && selectChannel(RADIO_LOCKED_CHANNEL)) radioStart();"""
assert s.count(old)==1
s=s.replace(old,new,1)
old="lastInitAttempt=now;selectChannel(RADIO_LOCKED_CHANNEL);radioStart();"
assert s.count(old)==1
s=s.replace(old,"lastInitAttempt=now;\n       if(ensureStationReady() && selectChannel(RADIO_LOCKED_CHANNEL)) radioStart();",1)
old="radioStop();selectChannel(RADIO_LOCKED_CHANNEL);radioStart();"
assert s.count(old)==2
s=s.replace(old,"radioStop();if(ensureStationReady() && selectChannel(RADIO_LOCKED_CHANNEL)) radioStart();")
p.write_text(s)
print("V6.2.2 Wi-Fi station initialization fix applied",len(s))
