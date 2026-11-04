# Mandatory StopWatch firmware release package format

Apply to **every future StopWatch C152 firmware version**, beginning with V17.15. A release is incomplete unless the downloadable ZIP contains:

1. **Flashable `.app.bin`**, compiled for `m5stack:esp32:m5stack_stopwatch`, and a tested Mac flashing script.
2. **Rollback binary** from the last confirmed working firmware.
3. **Complete editable project directly in a folder** (`Editable_Source/StopWatchNative31V16/`): `.ino`, `.cpp`, all local `.h` assets, and exact compile workflow/config. Do not deliver binaries alone, incomplete code excerpts, or source that does not match the compiled binary. Optionally include an additional standalone source ZIP.
4. **Version-specific, offline feedback form `.html`** in the same ZIP, with every effect in its real on-device order, badges for changes/new effects, Keep / Change / Remove / Not Reviewed, individual notes and ratings, global firmware notes, filters, browser autosave, Markdown export/copy and JSON backup/import. Preserve feedback-form functionality from the previous version.
5. **README** with simple Mac flashing instructions, rollback procedure, what's new, source location, and testing caveats.
6. **SHA-256 checksums** for binaries and key release files.

Priorities: preserve unchanged effects exactly, preserve effect-switch silence, don't erase NVS during flash, preserve rollback settings, compile and test via GitHub Actions, and never claim to have physically tested hardware unless it was tested.

V17.15 baseline: 26 V17.14 effects plus 5 new effects (#27 Yarnfall Cathedral, #28 Celestial Buttonwork, #29 Spiral Spindle, #30 Aurora Fishing Net, #31 Tidal Cable Knit). Original V17.14 branch `stopwatch-v17-14-three-string-effects-no-switch-haptics`; V17.15 branch `stopwatch-v17-15-five-thread-effects`.

The offline V17.15 feedback HTML and complete editable files are included in `StopWatch_V17_15_31_Effects_COMPLETE_With_Feedback_Editable_Source_Mac.zip` provided in ChatGPT. New releases should reproduce this format rather than merely linking to GitHub source.