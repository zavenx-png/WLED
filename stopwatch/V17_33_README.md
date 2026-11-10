# StopWatch V17.33 — Responsive Keyboard + Warehouse Disco Duo

Based on **V17.32 (26 curated effects)**. V17.33 adds exactly one copy of effect 3, making **27 effects**, and improves the visualizer name-entry keyboard without adding pages or changing its QWERTY letter layout.

## Effect 4 — Warehouse Disco Duo

- Effect **3 Warehouse House Duo remains unchanged**. The new effect **4 Warehouse Disco Duo** copies the exact same left-pink/right-blue dancing positions, beat timings, audio-reactive floor bars, and spotlights.
- The rotating suspended mirror-tile disco ball is copied from effect **2 Robot Shuffle**.
- Tap effect 4 to open the familiar two-field name keyboard. **PINK** labels the left figure and **BLUE** labels the right figure in the same style and positions as the existing Robot Shuffle labels.
- New effect 4's names are **separate from Robot Shuffle**. Neither editing nor clearing the new effect's names changes the names saved for effect 2.
- **Both new effect names start blank every power-on / reboot.** Saving with button B stores them only in RAM for the current session; they stay while switching between effects, but are never written to NVS or copied to scenes. A cancels an edit; eight seconds of keyboard inactivity discards the unsaved draft.

## Visualizer name keyboard improvements (all effects with name entry)

- **One page of all 26 QWERTY letters**, unchanged. Letter hit-testing still uses the **averaged finger position across the press**, as requested.
- Bottom **DEL** key reduced to 149 × 48 pixels, and a separate **SPACE** key (52 × 48 pixels) is placed immediately to its right.
- The full, *visibly drawn rectangle* of both keys is tappable. Both footer keys respond **on touch-down**, independently of the letter centroid averaging. A one-shot per-contact guard prevents double delete/space on release.
- DEL is labeled **DEL** in the same large text tier (font size 4) as the letter keys; SPACE is labeled **SP** at that same tier to remain legible on its key. The SPACE key inserts an actual whitespace character, subject to the existing 12-character name limit.
- The eight-second unsaved-entry timeout, A cancel / B save and live averaged-key preview remain unchanged. The separate harness Wi-Fi password editor is unaffected.

## Effect order and saved settings

Exactly 27 effects, V17.32 order retained with the new effect **inserted at #4**; previous #4–#26 move to #5–#27. Saved favorites, scenes, and per-effect sensitivity settings automatically remap from the 26-slot menu and retain the meaning of each old effect. Migration writes independent `s1733`, `fav1733`, `gain1733`, `sc1733*`, and `eff1733` keys without overwriting the V17.32 versions, preserving rollback potential. As before, the watch boots to Chromatic EQ (effect 1).

### Exact effect order

1. zaven Chromatic EQ
2. Robot Shuffle
3. Warehouse House Duo
4. Warehouse Disco Duo
5. Matrix Phosphor
6. zaven Psychedelic Halo
7. zaven Electric Aurora
8. zaven Flowing Halo
9. Prism EQ
10. Vapor Scope
11. Frequency Towers
12. Radial Spectrum
13. Heartbeat Neon
14. Round Resonator
15. Kaleidoscope Reactor
16. Infinity Tunnel
17. Cymatic Mandala
18. Orbital Harmonics
19. zaven Kaleidoscope
20. zaven Round Resonator
21. zaven Fibonacci Spiral
22. Spike Ball Nova (variant)
23. String Theory
24. FluidBox Classic
25. Neon Helix Chamber
26. Ultraviolet Reactor
27. Disco Stargate

## Mac package

The ZIP includes the compiled bootloader, partitions and application binaries; the checksum manifest; the fixed Intel Mac and Apple Silicon flasher (`01_FLASH_StopWatch.command`); `02_BUILD_Any_Version.command` to rebuild; full editable source and assets, and the release notes.

Arduino CLI toolchain: M5Stack core 3.3.10, M5Unified 0.2.29 and M5GFX 0.2.36. Board options: huge-app, OPI PSRAM, USB CDC. AirDrop and Ghost gateway are not included in the active firmware.

**Verification:** Compilation and source/static checks do not replace testing physical touch responsiveness, name positioning or battery behavior on the watch.
