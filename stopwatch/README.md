# StopWatch Club Visualizer + Harness Controller

Target: M5Stack StopWatch C152 (ESP32-S3R8, round AMOLED, mic, 6-axis IMU, vibration). Source: StopwatchClub/StopwatchClub.ino

## 20 sound-reactive effects

The ten requested designs: Blue Note VU (McIntosh-inspired blue analog needle, ballistics), Prism EQ (LED spectrum), Psychedelic Bloom, Vapor Scope, Matrix Phosphor, Dancing Ribbons, Frequency Towers, Radial Spectrum, Heartbeat Neon, Round Resonator.

Ten original designs: Kaleidoscope Reactor, Fibonacci Sunflower (golden-angle phyllotaxis), Infinity Tunnel, Cymatic Mandala, Fractal Lightning, Hyperspace Starfield, Liquid Chrome, Orbital Harmonics, Quantum Moire, Nebula Particles.

Microphone PCM: 16 kHz, 512-point Hann-window FFT, 32 log-spaced bands, adaptively gated sound level, attack/release smoothing, and transient beat events. Not spoken-word recognition. Very low sub-bass cannot be resolved by the 31.25 Hz FFT frequency bins.

## Controls

- Upright / worn: visualizer. Tap A for next effect; tap left or right side of touch panel for previous/next.
- Lay flat face-up steadily for approximately 1.25 seconds to enter harness remote. Hold upright for about 1.05 seconds to return. Auto mode applies a stability filter and hysteresis.
- Tap B to manually lock to the other mode; hold B to return to automatic mode.
- Remote: tap A to select next Z effect; hold A to apply selection. Tap bottom PAGE area to cycle pages. Haptics accompany mode switches, ACKs and taps.
- Remote page 1: Z / HARNESS; all 28 Z effects first, no saved WLED presets required, supports previous/next/apply.
- Remote page 2: PRESETS; recalls existing WLED preset IDs 1 to 250.
- Remote page 3: TUNE; drag or tap brightness, speed and intensity bars.
- Remote page 4: PAIR / RADIO; shows the StopWatch Wi-Fi station MAC and radio channel; tap center to cycle channels 1-11.

## Compatibility and pairing

1. Flash the matching WLED harness branch stopwatch-club-visualizer-espnow onto the existing GLEDOPTO harness controller; keep current OTA backup. Preserve existing ledmap.json (20 virtual sections / 595 virtual positions). Do NOT flash the StopWatch firmware onto the harness.
2. In WLED Config > WiFi Setup, enable ESP-NOW, disable Wi-Fi sleep, enter the displayed StopWatch STA MAC into the Remote List as twelve lowercase hex characters without colons, and restart WLED.
3. Set StopWatch channel equal to the harness WLEDAP access point or connected 2.4 GHz Wi-Fi channel. No Wi-Fi joining is required from StopWatch.
4. Direct Z effect selection uses new command 9: 0-based ordinal among registered WLED mode names starting Z -. This is in registered order, matches the 28 entries in firmware. Does not depend on WLED preset IDs.
5. The old 12-byte ZHR1 protocol commands 1..8 remain unchanged. Command 10 sets absolute effect speed 0..255; command 11 sets absolute effect intensity 0..255. The matching harness branch must be running for commands 9..11.
6. Acknowledgement is the existing 13-byte ZHA1 status packet. ACK means command was accepted/scheduled; not that the physical LED effect transition has finished.

Radio is unencrypted. WLED pairs the sender MAC in its Remote List. If the harness is on another Wi-Fi channel, radio messages cannot cross channels.

## Build and upload

Use official M5Stack board manager 3.3.8 or newer, Board=M5StopWatch (FQBN m5stack:esp32:m5stack_stopwatch), M5Unified >=0.2.19, M5GFX >=0.2.26. Avoid a generic ESP32-S3 board setting; use OPI PSRAM settings supplied by the official board profile.

1. arduino-cli config add board_manager.additional_urls https://static-cdn.m5stack.com/resource/arduino/package_m5stack_index.json
2. arduino-cli core update-index
3. arduino-cli core install m5stack:esp32
4. arduino-cli lib install M5Unified
5. arduino-cli lib install M5GFX
6. arduino-cli compile --fqbn m5stack:esp32:m5stack_stopwatch stopwatch/StopwatchClub
7. arduino-cli upload --fqbn m5stack:esp32:m5stack_stopwatch -p YOUR_USB_PORT stopwatch/StopwatchClub

StopWatch download mode: with USB connected, hold the power button about two seconds until the green LED appears and release. List ports with arduino-cli board list.

GitHub Actions workflow stopwatch-build.yml produces a separately downloadable ESP32-S3 .bin when CI succeeds.

## Safety / testing

Code needs on-device testing and 2.4 GHz pairing at a show; a successful CI compile does not establish hardware or radio performance. In a dark club, choose a lower AMOLED brightness for battery life. Speaker is deliberately disabled during microphone capture. The heart meter detects audio beat transients, not heart rate. Motion-based mode switching requires screen-up placement; a manual B override is available.
On the PAIR / RADIO page, tap POWER TOGGLE to switch the harness on/off. Z-page scrolling includes Nokia Snake and Chunchun Harness before the custom visual suite, preserving the actual registration order.
