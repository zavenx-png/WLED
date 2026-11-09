# ZAVEN StopWatch V17.31 — Infinity Mirror Club (38 effects)

Based directly on V17.30. **All 33 previous effects are retained**, including the V17.30 one-screen QWERTY keyboard, whole-press touch averaging, eight-second unsaved-name cancellation, Warehouse Disco Duo, AirDrop Art Capsule and Ghost Companion.

Five **new** effects have been appended as effects **34–38**. They are designed to resemble true circular LED infinity mirrors: nested reflections with progressively smaller projected light rings and dark depths, not flat equal-sized rotating circles.

## New effects

| Number | Effect | Visual character | Forward motion |
|---|---|---|---|
| 34 | **Prism Drop** | Rainbow LED beads arranged in deep circular reflected rings; angular RGB color bands inspired by the first reference photo | Smooth inward contracting rainbow mirror tunnel |
| 35 | **Silver Lightwell** | Dense white and ice-blue reflected LED points over a charcoal/metallic cavity, matching the second reference | Slow, deep silver LED well with center vanishing into black |
| 36 | **Neon Helix Chamber** | Cyan and hot-pink LED coils around eight structured helical rail tracks, with small bright white glints | Forward motion through a slowly twisting double-helix chamber |
| 37 | **Ultraviolet Reactor** | Large magenta, purple, cobalt and cyan segmented LED arcs, with reflective dark gaps and high-contrast panel lighting | Continuous inward ring stacking while the panel pattern slowly rotates |
| 38 | **Disco Stargate** | Deep 12-sided mirrored polygonal portal with rainbow corner bulbs, illuminated edges, symmetric vanishing rails and small white light diamonds | The polygonal frames repeatedly descend into a central opening |

## How the infinity effect works

- Every frame uses **real elapsed seconds from `millis()`**, not the microphone BPM or reactive amplitude. Animation continues at a consistent speed when the room is silent.
- The scene projects a sequence of near-to-far mirror planes into the round display. Their radius follows a nonlinear depth falloff. New rings reappear from the outside as deep rings vanish in the center.
- LED dots and facets have individually graded brightness; near rims stay bright, the innermost depths fade to almost black.
- A simulated metal/glass rim helps sell the illusion of a physical mirror with a center aperture.
- Distinct palette and geometry settings for each effect; these are not five recolors of the same spinning graphic.
- The five shaders share a 96-angle trigonometric lookup table for reduced computation and run at approximately 39/31/22 FPS (show/balanced/endurance caps) depending on the existing device profile.
- No audio activation is required. Existing mic, harness, AirDrop and Ghost behavior is unchanged.

## Persistence, safety and migration

- V17.31 adds stable original effect IDs **71–75** after the prior IDs 23 through 70. The first 33 menu positions and existing favorites/scenes keep their meanings.
- Gain presets are extended from 33 to 38 entries using a new one-time NVS flag `s1731`; the existing `gain1728`, `eff1728`, `fav1717` and robot-name records are preserved.
- Existing 90-second **physical motion** power-off remains in place. The illusion of moving through a mirror is graphical animation, not physical movement.
- No internet or server connection is needed for the five new effects; they are local drawing routines.
- This firmware is compiled for the M5Stack StopWatch; actual on-device FPS, colors and touch behavior still require testing.

## Flash or rebuild on macOS

The compiled release package includes application/bootloader/partition binaries, SHA-256 hashes, all editable Arduino sources, and:
- `01_FLASH_StopWatch.command` — flash the included binaries while preserving the NVS partition and saved settings.
- `02_BUILD_Any_Version.command` — rebuild the included source or another GitHub branch.
- The original `Mac_Flasher.sh` plus an M5Stack CI workflow.

To rebuild on a Mac, from the unzipped release folder:

```bash
bash 02_BUILD_Any_Version.command local
# Or rebuild the repository version explicitly:
bash 02_BUILD_Any_Version.command stopwatch-v17-31-infinity-mirror-club
```

Dependencies: Arduino CLI; M5Stack ESP32 core 3.3.10, M5Unified 0.2.29, M5GFX 0.2.36, pinned ESP32Drop SHA `92bdc1221924ca522f3ccbebbd9353612ce5b2d7`. Board options: huge_app, OPI PSRAM, USB CDC on boot.
