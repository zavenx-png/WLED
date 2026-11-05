#!/usr/bin/env python3
"""GitHub CI: verify original visualizer source and harness-only refinements."""
from pathlib import Path
import json, hashlib, re

root = Path(__file__).resolve().parents[1]
sketch = root / "StopWatchNative31V16"
source = (sketch / "NativeApp.cpp").read_text()
audit = json.loads((root / "V17_24_EFFECT_PRESERVATION_AUDIT.json").read_text())["sha256"]

def function(name):
    match = re.search(r"\b(?:static\s+)?(?:void|int|bool|float|uint8_t)\s+" + re.escape(name) + r"\s*\([^)]*\)\s*\{", source)
    if not match:
        raise AssertionError("Missing visualizer function: " + name)
    depth = 0
    for i in range(match.end() - 1, len(source)):
        if source[i] == "{":
            depth += 1
        elif source[i] == "}":
            depth -= 1
            if depth == 0:
                return source[match.start():i + 1]
    raise AssertionError("Unbalanced function " + name)

for key, expected in audit.items():
    data = (sketch / key[5:]).read_bytes() if key.startswith("file:") else function(key).encode()
    actual = hashlib.sha256(data).hexdigest()
    assert actual == expected, f"Visualizer or asset modified: {key}"

assert "M5.Display.setBrightness(0)" not in source, "Automatic display blanking remains"
assert "remoteIdleSecs>0" not in source
assert "if(remote || !buzzEnabled)return" in source, "Harness haptics still active"
assert "M5.Power.setVibration(0);buzzUntil=0;" in source
assert "M5.Touch.getCount()" in source
assert "remoteTouchXSum" in source and "remoteTouchSamples" in source
assert "RFAV_H=46" in source and "RROW_H=46" in source
assert "remoteLabel(\"BRIGHTNESS\"" in source
buttons = [
    (82,281,83,46), (169,281,128,46), (300,281,83,46),
    (127,338,212,46), (119,397,92,31), (255,397,92,31),
    (117,224,107,50), (242,224,107,50)
]
for x,y,w,h in buttons:
    for xx,yy in [(x,y),(x+w,y),(x,y+h),(x+w,y+h)]:
        assert (xx-233)**2 + (yy-233)**2 < 230**2, (x,y,w,h,xx,yy)
for i,(x,y,w,h) in enumerate(buttons):
    for xx,yy,ww,hh in buttons[i+1:]:
        if i>=6:
            continue
        if xx>100 and xx<300 and yy==224:
            continue
        assert x+w<=xx or xx+ww<=x or y+h<=yy or yy+hh<=y
print("PASS: visualizer parity, always-on display, silent remote, and round-screen touch targets")
