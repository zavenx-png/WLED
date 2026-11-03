// StopWatch V16.7: effects 06-09, native browser-fidelity motion.
// Direct Native Text method (FontTrial #2) is deliberately retained: words
// remain upright, are cached, and follow the original browser positions.
#pragma once
static bool fidelityText167(uint8_t id){
 if(id<29||id>30)return false; // V16.8: both portal effects removed from menu
 black();
 const String&word=artName(effectID);
 float t=T(),hb=beatClock*TAU,pulse=beatPulse(),rot=phase16()*TAU;
 const float drive=clampf(.75f*bass+.70f*energy+.30f*pulse);
 if(id==28){ // 06 Hyperspace Portal: nested perspective word tunnel + jewel
  // Batch by text size: at most three D-cache rasterizations per frame.
  // This prevents the 17 depth layers from repeatedly recreating glyph sprites.
  for(int tier=0;tier<3;tier++)for(int n=16;n>=0;n--){
   float f=frac(n/17.f+beatClock*.013f),r=12+220*f*f;
   int fontTier=(f<.32f?0:f<.68f?1:2);
   if(fontTier!=tier)continue;
   float a=rot+f*.45f+(n%2)*.11f;
   float fade=clampf(f*7)*clampf((1-f)*6)*(.27f+.42f*energy+.19f*pulse);
   float x=CX+cosf(a)*r*.06f,y=CY+sinf(a)*r*.06f;
   trialWord(word,x,y,tier==0?88.f:tier==1?132.f:202.f,a,
             hue(frac(n*.125f+.55f)),fade);
  }
  for(int i=0;i<12;i++){
   float a=i*TAU/12.f-rot*.6f,r=55+12*pulse+12*bass;
   dot(CX+r*cosf(a),CY+r*sinf(a),2,
       tint(hue(.53f+i*.037f),.50f+.4f*bass));
  }
  circ(CX,CY,50+int(pulse*10+bass*8),rgb(87,228,245),2);
  circ(CX,CY,60+int(pulse*10+bass*8),rgb(165,124,255),1);
  dot(CX,CY,35,rgb(7,17,49));
  trialWord(word,CX,CY,192.f,0,rgb(234,248,255),1.f);
  return true;
 }
 if(id==29){ // 07 Electric Aurora: flowing multilayer luminous ribbons + words
  for(int j=0;j<15;j++){
   float yy=90+j*22,px=0,py=0;
   for(int x=0;x<=W;x+=7){
    float v=sinf(x*.011f+t*.36f+j*.3f)*(18+83*drive)
           +sinf(x*.032f-t*.19f+j)*(8+22*drive);
    float y=yy+v;
    if(x)line(px,py,x,y,tint(hue(frac(.38f+j*.058f+t*.013f)),.36f+.26f*bass),2);
    px=x;py=y;
   }
  }
  for(int j=0;j<8;j++){
   float y=67+j*46+20*drive*sinf(hb*.3f+j*.6f);
   float x=CX+sinf(t*.25f+j*.85f)*(55+35*drive)+22*drive*sinf(hb*.75f+j);
   float f=130.f; // one cached native-size bucket (FAST)
   trialWord(word,x,y,f,sinf(t*.14f+j)*.19f,
             tint(hue(frac(.40f+j*.103f+t*.014f)),.55f+.24f*energy),1.f);
  }
  return true;
 }
 if(id==30){ // 08 Flowing Halo: letters attached to real deformed halo rings
  for(int k=11;k>0;k--){
   float r=(22+k*17)*(1+.24f*bass+.13f*energy+.055f*pulse),px=0,py=0;
   for(int j=0;j<=140;j++){
    float a=TAU*j/140.f;
    float rr=r*(1+(.10f+.11f*drive)*sinf(a*7+hb*.12f+k*.4f)
               +(.055f+.07f*drive)*sinf(a*13-hb*.24f));
    float x=CX+cosf(a)*rr,y=CY+sinf(a)*rr;
    if(j)line(px,py,x,y,tint(hue(frac(k*.086f+t*.011f)),.36f+.22f*bass),2);
    px=x;py=y;
   }
  }
  for(int layer=0;layer<4;layer++){
   int k=3+layer*2;float r=(22+k*17)*(1+.24f*bass+.13f*energy+.055f*pulse);
   int copies=(layer&1)?3:2;
   for(int j=0;j<copies;j++){
    float a=j*TAU/copies+t*((layer&1)?-.095f:.13f)+layer*.47f;
    float rr=r*(1+(.10f+.11f*drive)*sinf(a*7+hb*.12f+k*.4f)
               +(.055f+.07f*drive)*sinf(a*13-hb*.24f));
    trialWord(word,CX+cosf(a)*rr,CY+sinf(a)*rr,118.f,a,
              tint(hue(frac(k*.087f+j*.05f+t*.012f)),.65f+.30f*bass),1.f);
   }
  }
  return true;
 }
 if(id==31){ // 09 Orbit Portal: depth-travel orbiting words and visible arcs
  for(int j=0;j<6;j++){
   float rr=38+j*31+bass*12;
   circ(CX,CY,int(rr),tint(hue(.60f+j*.07f),.16f+.22f*energy));
  }
  for(int n=0;n<23;n++){
   float f=frac(n/23.f+beatClock*.012f);
   float rr=27+205*sqrtf(f),a=rot+n*2.399963f*.75f+f*.55f;
   float fade=clampf(f*8)*clampf((1-f)*5)*(.46f+.32f*bass+.15f*pulse);
   trialWord(word,CX+cosf(a)*rr,CY+sinf(a)*rr,90.f,a+PI_F*.5f,
             hue(frac(n*.125f+.59f)),fade);
  }
  for(int j=0;j<4;j++){
   float a=rot+j*TAU/4,r=150+6*sinf(hb*.2f+j);
   trialWord(word,CX+cosf(a)*r,CY+sinf(a)*r,110.f,a+PI_F*.5f,
             tint(hue(frac(.56f+j*.09f)),.77f),.87f);
  }
  dot(CX,CY,22,rgb(5,12,30));
  circ(CX,CY,22+int(10*bass),rgb(173,215,255),2);
  return true;
 }
 return false;
}
