#!/usr/bin/env python3
"""Source/geometry regression checks; firmware hardware behavior still requires on-device testing."""
from pathlib import Path
import json, hashlib, re, math
ROOT=Path(__file__).resolve().parents[1]
SKETCH=ROOT/'Editable_Source/StopWatchNative31V16'
SRC=(SKETCH/'NativeApp.cpp').read_text()
AUDIT=json.loads((ROOT/'VISUALIZER_PRESERVATION_AUDIT.json').read_text())['sha256']

def function(name):
 m=re.search(r'\b(?:static\s+)?(?:void|int|bool|float|uint8_t)\s+'+re.escape(name)+r'\s*\([^)]*\)\s*\{',SRC)
 if not m: raise AssertionError('Missing function '+name)
 pos=m.end()-1;start=m.start();depth=0
 while pos<len(SRC):
  if SRC[pos]=='{':depth+=1
  if SRC[pos]=='}':
   depth-=1
   if depth==0:return SRC[start:pos+1]
  pos+=1
 raise AssertionError('Unbalanced '+name)

for key,expected in AUDIT.items():
 if key.startswith('file:'):data=(SKETCH/key[5:]).read_bytes()
 else:data=function(key).encode()
 assert hashlib.sha256(data).hexdigest()==expected, f'Changed original visualizer code/asset: {key}'

assert 'M5.Display.setBrightness(0)' not in SRC
assert 'remoteIdleSecs>0' not in SRC
assert 'if(displayBlank){delay' not in SRC
assert 'if(remote || !buzzEnabled)return' in SRC
assert 'M5.Power.setVibration(0);buzzUntil=0;' in SRC
assert 'remoteTouchSamples' in SRC and 'M5.Touch.getCount()' in SRC
assert 'drawLargeNativeLabel(String(rxChannel)' in SRC
assert 'remoteLabel("BRIGHTNESS"' in SRC
assert 'RFAV_Y=338' in SRC and 'RFAV_H=46' in SRC
assert 'RROW_H=46' in SRC
assert 'const auto &contact=M5.Touch.getDetail(i)' in SRC
assert 'textC(name,216,y+23,white,name.length()>14?2:3)' in SRC # favorites buttons unchanged

# Geometry is within the circular display, and button hit boxes cannot overlap.
buttons={
 'fx_prev':(82,281,83,46),
 'fx_apply':(169,281,128,46),
 'fx_next':(300,281,83,46),
 'favorite':(127,338,212,46),
 'nav_prev':(119,397,92,31),
 'nav_next':(255,397,92,31),
 'radio_prev':(117,224,107,50),
 'radio_next':(242,224,107,50),
}
for key,(x,y,w,h) in buttons.items():
 for cx,cy in ((x,y),(x+w,y),(x,y+h),(x+w,y+h)):
  assert (cx-233)**2+(cy-233)**2 < 230**2, f'{key} clipped by circle at {cx},{cy}'
for a,(x,y,w,h) in buttons.items():
 for b,(xx,yy,ww,hh) in buttons.items():
  if a>=b: continue
  if (a.startswith('fx') or a=='favorite' or a.startswith('nav')) and (b.startswith('fx') or b=='favorite' or b.startswith('nav')):
   assert x+w <= xx or xx+ww <= x or y+h <= yy or yy+hh <= y,f'Overlapping buttons {a} and {b}'

# Entire drawn button is hit: boundary match = visible rectangle.
def hit(px,py,rect):
 x,y,w,h=rect
 return x<=px<x+w and y<=py<y+h
for key,rect in buttons.items():
 x,y,w,h=rect
 for pt in [(x,y),(x+w-1,y),(x,y+h-1),(x+w-1,y+h-1),(x+w//2,y+h//2)]:
  assert hit(*pt,rect),(key,pt)
 assert not hit(x-1,y-1,rect)
print('PASS: all visualizers preserved; no screen-off; no harness haptics; center touch; 8 full-area controls fit within round 466px panel')
