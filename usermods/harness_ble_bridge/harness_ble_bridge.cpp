#include "wled.h"

#if defined(CONFIG_IDF_TARGET_ESP32)
#include <NimBLEDevice.h>

// Zaven Harness BLE v0.1. Compact newline-delimited WLED JSON commands over
// bonded, authenticated BLE. All WLED state changes run in the WLED loop task;
// radio callbacks only assemble bounded incoming frames.
// Service: 7B7B5A10-6D88-4E45-A782-CCFD3B418800
// Write:   7B7B5A11-6D88-4E45-A782-CCFD3B418800
// Notify:  7B7B5A12-6D88-4E45-A782-CCFD3B418800
namespace {
constexpr char BLE_SERVICE[] = "7b7b5a10-6d88-4e45-a782-ccfd3b418800";
constexpr char BLE_RX[]      = "7b7b5a11-6d88-4e45-a782-ccfd3b418800";
constexpr char BLE_TX[]      = "7b7b5a12-6d88-4e45-a782-ccfd3b418800";
constexpr size_t MAX_IN = 1024;
constexpr size_t NOTIFY_CHUNK = 120;
portMUX_TYPE rxMux = portMUX_INITIALIZER_UNLOCKED;
char rxAssembly[MAX_IN + 1] = {};
size_t rxLength = 0;
bool rxDrop = false;
char rxReady[MAX_IN + 1] = {};
bool hasRxReady = false;

class RxCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* characteristic, NimBLEConnInfo& peer) override {
    if (!peer.isAuthenticated()) return;
    const std::string value = characteristic->getValue();
    portENTER_CRITICAL(&rxMux);
    for (char c : value) {
      if (c == '\n') {
        if (!rxDrop && rxLength > 0 && !hasRxReady) {
          memcpy(rxReady, rxAssembly, rxLength);
          rxReady[rxLength] = 0;
          hasRxReady = true;
        }
        rxLength = 0;
        rxDrop = false;
      } else if (c != '\r' && !rxDrop) {
        if (rxLength < MAX_IN) rxAssembly[rxLength++] = c;
        else { rxLength = 0; rxDrop = true; }
      }
    }
    portEXIT_CRITICAL(&rxMux);
  }
};
RxCallbacks rxCallbacks;

class ZavenHarnessBLE final : public Usermod {
  bool enabled = false;
  uint32_t passkey = 0; // MUST be configured in WLED usermod settings before BLE starts.
  bool started = false;
  NimBLECharacteristic* tx = nullptr;
  String outbound;
  size_t txOffset = 0;
  uint32_t lastTxMs = 0;

  void sendJson(const JsonDocument& doc) {
    String s;
    serializeJson(doc, s);
    if (s.length() > 4095) return;
    outbound = s + '\n';
    txOffset = 0;
    lastTxMs = 0;
  }
  void error(const char* reason) {
    StaticJsonDocument<128> doc;
    doc["type"] = "error"; doc["reason"] = reason;
    sendJson(doc);
  }
  void sendState() {
    StaticJsonDocument<512> doc;
    doc["type"] = "state";
    doc["on"] = bri > 0;
    doc["bri"] = bri;
    doc["ps"] = currentPreset;
    doc["fxTotal"] = strip.getModeCount();
    doc["palTotal"] = getPaletteCount();
    if (strip.getSegmentsNum()) {
      const Segment& s = strip.getMainSegment();
      doc["fx"] = s.mode; doc["pal"] = s.palette;
      doc["sx"] = s.speed; doc["ix"] = s.intensity;
      doc["c1"] = s.custom1; doc["c2"] = s.custom2; doc["c3"] = s.custom3;
      doc["o1"] = s.check1; doc["o2"] = s.check2; doc["o3"] = s.check3;
    }
    sendJson(doc);
  }
  void sendNames(const char* kind, int offset) {
    bool isFx = strcmp(kind, "fx") == 0;
    int total = isFx ? strip.getModeCount() : getPaletteCount();
    offset = constrain(offset, 0, total);
    StaticJsonDocument<1400> doc;
    doc["type"] = kind;
    doc["offset"] = offset;
    doc["total"] = total;
    JsonArray arr = doc.createNestedArray("items");
    for (int i = offset; i < total && i < offset + 8; i++) {
      char name[80] = {};
      extractModeName((uint8_t)i, isFx ? JSON_mode_names : JSON_palette_names, name, sizeof(name) - 1);
      JsonObject item = arr.createNestedObject();
      item["id"] = i; item["name"] = name;
    }
    sendJson(doc);
  }
  void process(char* message) {
    StaticJsonDocument<1152> input;
    if (deserializeJson(input, message)) { error("invalid_json"); return; }
    const char* op = input["op"] | "";
    if (!strcmp(op, "get") || !strcmp(op, "ping")) {
      sendState();
    } else if (!strcmp(op, "fx") || !strcmp(op, "pal")) {
      sendNames(op, input["offset"] | 0);
    } else if (!strcmp(op, "set")) {
      JsonObject state = input["s"].as<JsonObject>();
      if (state.isNull()) { error("missing_state"); return; }
      // Preserve v5.3 usermod's hfx handling of saved slider/palette defaults.
      deserializeState(state, CALL_MODE_DIRECT_CHANGE);
      sendState();
    } else {
      error("unknown_op");
    }
  }

 public:
  void setup() override {
    if (!enabled || passkey < 100000 || passkey > 999999) return;
    if (!NimBLEDevice::init("Zaven Harness BLE")) return;
    NimBLEDevice::setSecurityAuth(true, true, true);
    NimBLEDevice::setSecurityIOCap(BLE_HS_IO_DISPLAY_ONLY);
    NimBLEDevice::setSecurityPasskey(passkey);
    NimBLEServer* server = NimBLEDevice::createServer();
    server->advertiseOnDisconnect(true);
    NimBLEService* service = server->createService(BLE_SERVICE);
    NimBLECharacteristic* rx = service->createCharacteristic(
      BLE_RX, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_AUTHEN, MAX_IN);
    rx->setCallbacks(&rxCallbacks);
    tx = service->createCharacteristic(
      BLE_TX, NIMBLE_PROPERTY::NOTIFY | NIMBLE_PROPERTY::READ_AUTHEN, NOTIFY_CHUNK);
    service->start();
    NimBLEAdvertising* ads = NimBLEDevice::getAdvertising();
    ads->addServiceUUID(BLE_SERVICE);
    ads->setName("Zaven Harness BLE");
    started = ads->start();
  }
  void loop() override {
    if (!started || !tx) return;
    char command[MAX_IN + 1] = {};
    bool found = false;
    portENTER_CRITICAL(&rxMux);
    if (hasRxReady) {
      memcpy(command, rxReady, MAX_IN + 1);
      hasRxReady = false;
      found = true;
    }
    portEXIT_CRITICAL(&rxMux);
    if (found) process(command);
    if (outbound.length() && millis() - lastTxMs >= 35) {
      const size_t remaining = outbound.length() - txOffset;
      const size_t count = remaining > NOTIFY_CHUNK ? NOTIFY_CHUNK : remaining;
      tx->setValue(reinterpret_cast<const uint8_t*>(outbound.c_str() + txOffset), count);
      if (tx->notify()) txOffset += count;
      lastTxMs = millis();
      if (txOffset == outbound.length()) { outbound = ""; txOffset = 0; }
    }
  }
  void addToConfig(JsonObject& root) override {
    JsonObject top = root.createNestedObject("Zaven Harness Bluetooth");
    top["enabled"] = enabled;
    top["passkey"] = passkey;
  }
  bool readFromConfig(JsonObject& root) override {
    JsonObject top = root["Zaven Harness Bluetooth"];
    if (top.isNull()) return false;
    enabled = top["enabled"] | false;
    passkey = top["passkey"] | 0U;
    return !top["enabled"].isNull() && !top["passkey"].isNull();
  }
  void addToJsonInfo(JsonObject& root) override {
    JsonObject user = root["u"];
    if (user.isNull()) user = root.createNestedObject("u");
    user["Harness BLE"] = started ? "Enabled (paired clients only)" : "Disabled";
  }
};
static ZavenHarnessBLE zavenHarnessBle;
REGISTER_USERMOD(zavenHarnessBle);
#endif
