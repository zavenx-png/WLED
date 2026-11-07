# StopWatch V17.28 — AirDrop Art Capsule + Ghost Companion

**Base:** V17.27 `stopwatch-v17-27-green-power-led-off`. This version preserves the original green PMIC LED disable and all **30 surviving** visual effects, removes Beat Laboratory (previous effect ID 67) and appends Art Capsule (menu index 30, ID 68) and Ghost Companion (menu index 31, ID 69). Total menu: **32 effects**.

## Navigation
- Physical A / B: previous / next visual effect. Their long presses still control brightness and battery readout.
- A+B: harness remote, retaining prior V17.27 mode. The new apps are local watch effects, not WLED effects.
- Effect 31, AirDrop Art Capsule: tap to start **real AirDrop receive mode**; on iPhone use Photos > Share > AirDrop > `ZAVEN ART CAPSULE`, send a JPEG or PNG. This is a 2.4 GHz AWDL implementation (ESP32Drop), *not* a browser transfer or Apple's supported firmware. Contact-only AirDrop is not supported; sender must discover an Everyone-compatible receiver. The received image is displayed as a circular, masked composition, saved in LittleFS, and redisplayed after power cycle.
- Image transfer maximum: **3 MiB decoded archive**. Some original iPhone HEIC photos and full-size photographs may be rejected. Convert to JPEG/PNG and reduce size first. The receiver cannot share ESP-NOW or normal Wi-Fi simultaneously. Returning to other effects requires a **watch restart** to release the receiver. The selected next effect is saved before restarting.
- Effect 32, Ghost Companion: a fully original, always-bobbing animated ghost with gaze/tilt reaction, blinking, glow, a speaking mouth and feedback status. Tap without configuration to start its setup hotspot. Join phone Wi-Fi `ZAVEN-GHOST` (the password appears on the watch) and visit `http://192.168.4.1`. Configure a **2.4 GHz Wi-Fi** SSID, Wi-Fi password, an HTTPS voice gateway URL, a device token and the PEM root certificate of your trusted HTTPS gateway. Tap ghost after connected to record a three-second voice turn. The companion sends 16 kHz mono PCM16 WAV over validated TLS and plays 24 kHz PCM16 returned by the gateway.

## Voice gateway setup
The watch **does not store an AI vendor API key**. The `ghost_gateway` directory contains a FastAPI gateway using an external AI service for transcription, response and text-to-speech.

```sh
cd stopwatch/ghost_gateway
python3 -m pip install -r requirements.txt
export OPENAI_API_KEY='YOUR_PROVIDER_API_KEY'
export GHOST_DEVICE_TOKEN='GENERATE_A_LONG_RANDOM_DEVICE_TOKEN'
uvicorn main:app --host 127.0.0.1 --port 8000
```

Expose the gateway through a **private, TLS-secured reverse proxy** and configure the *HTTPS* URL `https://your-domain/voice` on the watch, with a trusted root CA PEM. Do not run plaintext public HTTP or put your OpenAI API key in firmware. This service is an example integration and requires hosting, credentials and internet access; it is not turnkey cloud service hosting.

The gateway expects a single 3-second 16kHz 16-bit mono WAV body, authenticated by `X-Device-Token`, and replies with **raw 24kHz signed 16-bit mono PCM little endian**. Failure to connect or a wrong certificate produces a visible error on the watch. Playback and recording switch exclusive use of microphone/speaker.

## Motion power protection
- Poll the BMI270 IMU at 20 Hz.
- When acceleration or rotation exceeds the sensitivity thresholds, reset the inactivity timer.
- After **90 seconds** without detected physical movement, call the M5PM1 **hard power off** via `M5.Power.powerOff()`. This is *not* a screen dimmer.
- Touches/button use, ongoing photo transfer or voice conversation also postpone shutdown. A watch sitting immobile while acting as a harness remote **will power off** after 90s, even if the animated picture is moving. This matches the requested physical-movement rule. The sensor must respond correctly before shutdown is enabled.
- Power-button startup is required after hard shutdown.

## Build
The GitHub Actions workflow `.github/workflows/stopwatch-v17-28-build.yml` pins the official StopWatch board, M5Unified/M5GFX versions and ESP32Drop commit `92bdc1221924ca522f3ccbebbd9353612ce5b2d7`. It uses `PartitionScheme=huge_app,PSRAM=opi`. On a successful workflow run, the `StopWatch_V17_28_Art_Capsule_Ghost_32_Effects` artifact contains the binaries, editable source and gateway files. Do not flash until the workflow has succeeded; an actual device test is still required.

## Boundaries and QA
- AirDrop requires exclusive Wi-Fi radio use; stopping the receiver involves reboot. The 3 MiB cap is conservative because the existing sprite and visual engine occupy PSRAM.
- HEIC decoder is not included; supported visual formats are JPEG and PNG. The round photo is centered and clipped; no square corners are shown.
- Voice requests are sequential, not always-listening; the current design supports **tap-to-talk** and one short reply. Wi-Fi/cloud delays require real-device measurement. The companion floats even with no music or Wi-Fi.
- External gateway deployment and valid certificate are prerequisites for conversation. If you do not enter them, the ghost still animates.
- The code has not yet been verified on the user's physical watch. Confirm display composition, exact touch hit boxes, AirDrop discovery, transfer memory stability, live recording, valid TLS connection, speaker playback, IMU threshold, and PMIC shutdown prior to relying on it at a festival.
