# Zaven Harness v5.3 + Bluetooth companion (experimental)

Based on `harness-v5.3-espnow-v2`. V5.3 custom effects, palette-default memory, physical ledmap, presets, Z ordering, and Cardputer ESP-NOW V2 are unchanged.

## Firmware

1. Use the `esp32dev` binary from the [BLE development branch Actions builds](https://github.com/zavenx-png/WLED/actions/workflows/wled-ci.yml?query=branch%3Aharness-v5.3-ble-companion) **only after CI is green**. This is classic ESP32 firmware, **not** ESP32-S3.
2. Back up WLED configuration, presets, LED map, and your working v5.3 firmware first.
3. OTA-flash via WLED > Config > Security & Updates > Manual OTA Update; do not erase flash or reset settings.
4. While still connected over Wi-Fi, open WLED > Config > Usermods, enable **Zaven Harness Bluetooth**, and enter a new six-digit pairing passkey (100000–999999). Save, then **restart WLED**. This key is private and is not part of the code or repository.
5. Scan in the companion app for **Zaven Harness BLE**. On first authenticated write, iOS should request the six-digit passkey through its system pairing dialog. If an old bond is cached after changing the key, forget the accessory in Bluetooth settings and reconnect.
6. You can continue using WLED Wi-Fi and the paired ESP-NOW Cardputer. Evaluate coexistence and response times on the physical harness before event use.

## iPhone app

Requirements: macOS + Xcode; install [XcodeGen](https://github.com/yonaskolb/XcodeGen), e.g. `brew install xcodegen`. In Terminal:

```sh
cd phone-app/ZavenHarnessBLE
xcodegen generate
open ZavenHarnessBLE.xcodeproj
```

In Xcode, select an Apple development team under Signing & Capabilities and select your iPhone 17 Pro Max as the run destination; press Run. An app binary for personal installation **cannot be exported without Apple code signing** and is not included here.

The custom app includes a **Bluetooth / Wi-Fi selector** at its top. Bluetooth uses **CoreBluetooth** and requires neither Internet nor Wi-Fi after pairing: power, brightness, effect/palette selection, v5.3 per-effect settings/default buttons, and presets 1–9. Wi-Fi embeds the device's **complete existing WLED web interface** using WKWebView (default address `http://4.3.2.1` on WLED-AP; enter a different LAN IP as needed). This is a *custom companion*, **not** a modification to the existing official App Store WLED app.

## Protocol (BLE GATT)

- Service `7B7B5A10-6D88-4E45-A782-CCFD3B418800`
- Write `7B7B5A11-6D88-4E45-A782-CCFD3B418800` (authenticated; newline-delimited JSON)
- Notify `7B7B5A12-6D88-4E45-A782-CCFD3B418800`
- Up to 1024 bytes per message; app uses 120-byte write chunks; firmware uses 120-byte notifications.
- `{"op":"get"}` returns compact state; `{"op":"fx","offset":0}` and `{"op":"pal","offset":0}` list eight names at a time; `{"op":"set","s":{"on":true,"bri":100}}` forwards WLED JSON state.
- BLE is **off by default** until an authenticated pairing passkey is set in WLED configuration.
- There is no continuous LED streaming or automatic Wi-Fi/BLE failover in this initial build.

## Important limitations

**Experimental source:** Compile success is not proof that Bluetooth, ESP-NOW, Wi-Fi and the LED renderer coexist reliably on the target device. Bluetooth and Wi-Fi share radio resources. This version is for bench testing and should not replace a working festival build until physically tested.

BLE traffic is protected with NimBLE bonding and passkey-based authentication; do not put your passkey in the public GitHub source. Only the main WLED segment is displayed in the app at present; multi-segment editing and complete WLED configuration pages require additional UI. Firmware update over BLE is not implemented: keep using Wi-Fi OTA.
