# StopWatch V17.32 — Curated 26 Effects

Built from V17.31, with the user's exact effect selection and unchanged order. **26 kept, 12 removed**. The V17.30 one-screen QWERTY with averaged finger-center selection and eight-second unsaved-draft timeout is retained. The M5Stack StopWatch green LED power-off, motion-triggered hard shutdown, existing visualizer sound engine, harness ESP-NOW remote, and remaining effects are retained.

## Final menu (1–26)

1. zaven Chromatic EQ
2. Robot Shuffle
3. Warehouse House Duo
4. Matrix Phosphor
5. zaven Psychedelic Halo
6. zaven Electric Aurora
7. zaven Flowing Halo
8. Prism EQ
9. Vapor Scope
10. Frequency Towers
11. Radial Spectrum
12. Heartbeat Neon
13. Round Resonator
14. Kaleidoscope Reactor
15. Infinity Tunnel
16. Cymatic Mandala
17. Orbital Harmonics
18. zaven Kaleidoscope
19. zaven Round Resonator
20. zaven Fibonacci Spiral
21. Spike Ball Nova (variant)
22. String Theory
23. FluidBox Classic
24. Neon Helix Chamber
25. Ultraviolet Reactor
26. Disco Stargate

## Removed from the menu and runtime

- Psychedelic Bloom
- Fibonacci Sunflower
- Nebula Particles
- Warp Drive Loom
- Spiral Spindle
- Tidal Cable Knit
- Gravity Lab
- AirDrop Art Capsule
- Ghost Companion
- Warehouse Disco Duo
- Prism Drop
- Silver Lightwell

The AirDrop Art Capsule and Ghost Companion code is no longer included or initialized in the application. Their runtime assets are omitted from the source release. The three retained infinity mirrors (Neon Helix Chamber, Ultraviolet Reactor, Disco Stargate) keep their original continuous, sound-independent inward motion.

## Existing user settings

V17.32 maps all previous V17.31 menu indices to the new 26 positions, preserving the matching effect favorites, saved scene effect indices and per-effect gain settings. Scenes targeting removed effects are ignored. New keys `s1732`, `fav1732`, `gain1732`, `eff1732`, `sc1732*` and `man1732` are used so the old V17.31 keys remain available for rollback. Global name and blue/pink robot names remain in their original keys. As in the prior version, the watch starts on effect 1 after reboot.

## Mac firmware package

- Precompiled application .bin, bootloader and partition files, SHA-256 checksums.
- `01_FLASH_StopWatch.command` — one-click Intel/Apple Silicon Mac flashing with a wheel-only Python dependency install; no Rust/OpenSSL source compilation.
- `02_BUILD_Any_Version.command` — source build/rebuild tool.
- `Editable_Source/` — Arduino sketch, all required effect assets and version notes.

Arduino board: `m5stack:esp32:m5stack_stopwatch`; core `m5stack:esp32@3.3.10`, M5Unified `0.2.29`, M5GFX `0.2.36`, huge-app partition and OPI PSRAM. ESP32Drop is **not required** for this release because AirDrop has been removed.

**Validation:** GitHub Actions compiles and checks effect ordering and schema. Actual visual appearance, touch response, and hardware flashing require on-device testing.
