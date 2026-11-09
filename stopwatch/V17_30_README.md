# StopWatch V17.30 — One-screen QWERTY, averaged touch + eight-second auto-cancel

Based on V17.29, retaining all **33 effects**, the Warehouse Disco Duo combination, the original Warehouse House Duo and Robot Shuffle, and all other visualizers.

## Eight-second unsaved-name timeout

- **All visualizer NAME keyboards** return to the effect after eight seconds with no recognized typing or name-field activity.
- The timeout **discards the unsaved draft**; no previous names or settings are changed.
- The timer restarts when a letter, DELETE, or a robot-name field is selected.
- **Physical A = cancel**; **physical B = save**.
- During a live finger press the editor does not close. The count resumes once the finger is released.
- The separate Wi-Fi/harness password editor is not affected.

## Keep all 26 letters on one screen

There is **no second page, page toggle, or hidden letters**. QWERTY layout is unchanged:

```
Q W E R T Y U I O P
 A S D F G H J K L
    Z X C V B N M
```

The 466px circular display has less width toward the top and bottom. The three letter rows now have individually tuned positions, widths, and separation to better use the real circular touch surface:

- Top 10 keys: 40 x 59px, with full row inside the circle.
- Middle 9 keys: 45 x 59px.
- Bottom 7 keys: 48 x 59px.
- A much bigger, centered 190 x 48px DELETE key is at the bottom, with an enlarged invisible hit target.
- A countdown reads `AUTO EXIT 8s` down to `1s`.

**Note:** For ten letters in the top QWERTY row, the top keys cannot become much wider without clipping outside the watch face. The bigger usable areas are in the middle and bottom rows.

## Preserve touch averaging — deliberately

The key selected is the one **nearest the average center of the entire physical finger press**, not the initial touch-down point.

- Touch coordinates accumulate over the held press (up to 36 valid samples).
- The live enlarged-key popup follows the *averaged* candidate, showing the letter under the finger.
- On finger release, the averaged position is hit-tested and exactly **one** key is committed. A press is never entered twice.
- The popup remains visible briefly after release; the editor gets the faster refresh cap introduced in V17.30.
- This preserves the user's established small-target accuracy behavior and prevents off-center finger-down jitter from mis-registering letters.
- Touching DELETE or either robot-name field also uses the averaged final contact position.

## Compatibility and builds

- None of the 33 visualizer drawing algorithms or their menu positions change.
- Shared names, BLUE and PINK robot names, settings, favorites, and scenes keep their existing persistence keys.
- The no-movement 90s hard power-off, green LED, ESP-NOW harness remote, AirDrop Art Capsule and Ghost Companion are unchanged.
- Hardware validation of the new touchscreen behavior is still needed.

The ZIP includes compiled binaries, editable source, `01_FLASH_StopWatch.command`, `02_BUILD_Any_Version.command`, previous readmes, and checksums.

```bash
bash 02_BUILD_Any_Version.command local
# or compile this revision by GitHub branch name:
bash 02_BUILD_Any_Version.command stopwatch-v17-30-keyboard-ux-idle8
```
