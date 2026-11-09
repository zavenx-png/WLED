# StopWatch V17.30 — Name Keyboard UX + 8-second Auto Cancel

Based on the successfully compiled **V17.29 / 33-effect** firmware. All effects—including Warehouse Disco Duo, Robot Shuffle, Warehouse House Duo, Art Capsule, Ghost Companion, and the existing harness remote—are retained.

## Change 1: automatic timeout, with NO changes saved

Every visualizer name-entry screen automatically returns to the running effect after **eight seconds without a recognized keyboard action**.

- A new text editor starts with an eight-second countdown.
- Typing a letter, deleting, switching keyboard pages, switching layouts, or selecting the BLUE / PINK name field restarts the countdown.
- At timeout, the keyboard closes; any incomplete/unsaved names are discarded; the previous saved name(s) remain intact.
- The timeout does **not** save anything to flash. Only **physical B** saves a name. **Physical A** still cancels immediately.
- While a finger is held down on the keyboard, the timeout does not interrupt it. It can expire when the finger leaves the screen.
- The eight-second rule applies to **name keyboards on visualizer effects**, not the separate Wi-Fi/harness password editor (lengthy WPA2 entry would otherwise time out repeatedly).

## Change 2: new LARGE keyboard is the default

The old full QWERTY has ten very narrow keys in one row (41 x 61 physical pixels each). On a 466px circular touchscreen, the large mode now uses **68 x 60px keys (~63% more key area)**, with larger, clearer labels and a touch layout fitted to the circular display.

The larger keys use two pages with familiar QWERTY-style letter sequence:

| Page | Row 1 | Row 2 | Row 3 |
|---|---|---|---|
| **1/2** | Q W E R T | Y U I O P | A S D |
| **2/2** | F G H J K | L Z X C V | B N M |

Bottom controls: **FULL** switches to the all-26-letter single-screen QWERTY; **DEL** deletes the last letter; **NEXT/BACK** switches the large-key pages. In single-screen mode **LARGE** returns to the two-page keyboard. All control buttons fit inside the round display.

## Change 3: faster, more reliable touch handling

- Letters and keyboard controls now commit **on the initial finger-down** event, rather than after finger release.
- Each physical tap can commit **at most one action**: no duplicate letters on release.
- A tap starting slightly outside a target gets one retry after touch has stabilized.
- Key selection uses the same rectangular geometry for rendering and nearest-center touch hit testing; touch coordinates are no longer averaged across an entire name-key press.
- A large visual letter-confirmation popup now remains visible briefly after finger release.
- The name editor uses a faster render cap to improve perceived touch response.
- A live remaining-seconds countdown shows when unsaved entry will be cancelled.

## Compatibility

- 33 existing effect positions are **unchanged**.
- All effect display algorithms remain unchanged.
- Previously saved BLUE/PINK robot names and the shared text-effect name remain saved.
- Saved favorites, scenes and per-effect sensitivity settings retain existing key/index positions.
- No additional Wi-Fi usage, microphone changes, or flash writes during typing.

## Mac installation and source

The full V17.30 download package contains:

- flashable compiled .bin files and SHA-256 checksums;
- `01_FLASH_StopWatch.command` to flash the included firmware;
- `02_BUILD_Any_Version.command` to rebuild from the bundled source or a selected GitHub version;
- the complete editable Arduino sketch, assets, and Ghost gateway;
- this README and the inherited version history.

Use the first command after unzipping, or compile the source using:

```bash
bash 02_BUILD_Any_Version.command local
# optionally fetch the branch directly:
bash 02_BUILD_Any_Version.command stopwatch-v17-30-keyboard-ux-idle8
```

Board and toolchain: `m5stack:esp32:m5stack_stopwatch` / `m5stack:esp32@3.3.10`, M5Unified `0.2.29`, M5GFX `0.2.36`, PSRAM OPI, huge-app partition; ESP32Drop pinned to `92bdc1221924ca522f3ccbebbd9353612ce5b2d7`.

**Verification:** CI builds the actual firmware and validates the keyboard's geometry and state transitions statically. Physical touch responsiveness and rendering still require on-device testing.
