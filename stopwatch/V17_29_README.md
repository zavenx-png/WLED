# ZAVEN StopWatch V17.29 — Warehouse Disco Duo

Based on **V17.28 Art Capsule + Ghost**, with the existing 32 effects preserved and a **33rd** effect appended.

## Effect 33: Warehouse Disco Duo

- Warehouse House Duo's **exact dance choreography** is retained: pink figure on the left, blue figure on the right, both on the original beat-clock phase offsets.
- Warehouse House Duo's original spotlights and sound-reactive dancefloor bars remain.
- The **rotating mirrored disco ball** is reused from Robot Shuffle, with the same suspended mirror tiles and rotation.
- Tap the effect to open the **existing two-name QWERTY editor**: select **PINK** for the left Warehouse dancer or **BLUE** for the right Warehouse dancer. Press physical **B** to save, or **A** to cancel.
- Both names use the same saved settings as Robot Shuffle. Editing either effect updates the names shown by both; the editor starts blank but does not erase an unedited name.
- The original Robot Shuffle and Warehouse House Duo effects remain available and unchanged.
- A/B buttons change effects; the new hybrid is the last menu entry, numbered 33.
- The original 90-second real-motion auto-power-off, AirDrop Art Capsule, Ghost Companion, harness controls, and all original modes remain.

## Persistence and migration

- The saved effect index positions 1–32 remain unchanged; new effect is index 32 / ID 70.
- V17.29 extends the stored gain array from 32 to 33 entries without rewriting previous individual settings.
- A new preferences marker `s1729` supports saving/reloading the new last effect and its favorite status.
- Existing stored robot names are reused; no need to enter them twice.

## Build from Mac

The full downloadable package includes `01_FLASH_StopWatch.command`, `02_BUILD_Any_Version.command`, flash binaries and editable source. Run the first command inside the extracted folder, or rebuild by running:

```bash
bash 02_BUILD_Any_Version.command local
# OR rebuild directly from this branch
bash 02_BUILD_Any_Version.command stopwatch-v17-29-warehouse-disco-duo
```

Official M5Stack core: `m5stack:esp32@3.3.10`.
M5Unified: `0.2.29`; M5GFX: `0.2.36`.
Board: `m5stack:esp32:m5stack_stopwatch` with huge-app, OPI PSRAM, CDC on boot.
AirDrop support requires ESP32Drop pinned at `92bdc1221924ca522f3ccbebbd9353612ce5b2d7`.

## Validation

The GitHub Actions workflow compiles this source into fresh, revision-specific binaries. Successful compilation verifies toolchain compatibility, not in-hand testing of choreography, labels, keyboard interactions, AirDrop, or the 90-second motion power-off. Flash and test on the physical M5StopWatch.
