/*
  ZAVEN STUDIOS / FESTIVAL KIT V7 - Cardputer ADV
  NEW APPLICATION CORE. No V6 UI/rotation code, no portrait mode, no display tests.
  Standard M5Cardputer initialization, landscape ST7789 (240x135), direct drawing.
  ESP-NOW V2 pairs with Harness V5.3 + ZHR2 receiver. WLED Wi-Fi is manual only.
*/
#include "RemoteCore.h"
void setup(){ ZV7::start(); }
void loop(){ ZV7::step(); }