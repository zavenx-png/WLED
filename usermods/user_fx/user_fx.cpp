#include "wled.h"

// for information how FX metadata strings work see https://kno.wled.ge/interfaces/json-api/#effect-metadata

// paletteBlend: 0 - wrap when moving, 1 - always wrap, 2 - never wrap, 3 - none (undefined)
#define PALETTE_SOLID_WRAP   (paletteBlend == 1 || paletteBlend == 3)

#define indexToVStrip(index, stripNr) ((index) | (int((stripNr)+1)<<16))

// static effect, used if an effect fails to initialize
static void mode_static(void) {
  SEGMENT.fill(SEGCOLOR(0));
}

#define FX_FALLBACK_STATIC { mode_static(); return; }

// If you define configuration options in your class and need to reference them in your effect function, add them here.
// If you only need to use them in your class you can define them as class members instead.
// bool myConfigValue = false;

/////////////////////////
//  User FX functions  //
/////////////////////////

// Diffusion Fire: fire effect intended for 2D setups smaller than 16x16
static void mode_diffusionfire(void) {
  if (!strip.isMatrix || !SEGMENT.is2D())
    FX_FALLBACK_STATIC;  // not a 2D set-up

  const int cols = SEG_W;
  const int rows = SEG_H;
  const auto XY = [&](int x, int y) { return x + y * cols; };

  const uint8_t refresh_hz = map(SEGMENT.speed, 0, 255, 20, 80);
  const unsigned refresh_ms = 1000 / refresh_hz;
  const int16_t diffusion = map(SEGMENT.custom1, 0, 255, 0, 100);
  const uint8_t spark_rate = SEGMENT.intensity;
  const uint8_t turbulence = SEGMENT.custom2;

unsigned dataSize = cols * rows;  // SEGLEN (virtual length) is equivalent to vWidth()*vHeight() for 2D
  if (!SEGENV.allocateData(dataSize))
    FX_FALLBACK_STATIC;  // allocation failed

  if (SEGENV.call == 0) {
    SEGMENT.fill(BLACK);
    SEGENV.step = 0;
  }

  if ((strip.now - SEGENV.step) >= refresh_ms) {
    // Keep for ≤~1 KiB; otherwise consider heap or reuse SEGENV.data as scratch.
    uint8_t tmp_row[cols];
    SEGENV.step = strip.now;
    // scroll up
    for (unsigned y = 1; y < rows; y++)
      for (unsigned x = 0; x < cols; x++) {
        unsigned src = XY(x, y);
        unsigned dst = XY(x, y - 1);
        SEGENV.data[dst] = SEGENV.data[src];
      }

    if (hw_random8() > turbulence) {
      // create new sparks at bottom row
      for (unsigned x = 0; x < cols; x++) {
        uint8_t p = hw_random8();
        if (p < spark_rate) {
          unsigned dst = XY(x, rows - 1);
          SEGENV.data[dst] = 255;
        }
      }
    }

    // diffuse
    for (unsigned y = 0; y < rows; y++) {
      for (unsigned x = 0; x < cols; x++) {
        unsigned v = SEGENV.data[XY(x, y)];
        if (x > 0) {
          v += SEGENV.data[XY(x - 1, y)];
        }
        if (x < (cols - 1)) {
          v += SEGENV.data[XY(x + 1, y)];
        }
        tmp_row[x] = min(255, (int)(v * 100 / (300 + diffusion)));
      }

      for (unsigned x = 0; x < cols; x++) {
        SEGENV.data[XY(x, y)] = tmp_row[x];
        if (SEGMENT.check1) {
          uint32_t color = SEGMENT.color_from_palette(tmp_row[x], true, false, 0);
          SEGMENT.setPixelColorXY(x, y, color);
        } else {
          uint32_t base = SEGCOLOR(0);
          SEGMENT.setPixelColorXY(x, y, color_fade(base, tmp_row[x]));
        }
      }
    }
  }
}
static const char _data_FX_MODE_DIFFUSIONFIRE[] PROGMEM = "Diffusion Fire@!,Spark rate,Diffusion Speed,Turbulence,,Use palette;;Color;;2;pal=35";


/*
 * Spinning Wheel effect - LED animates around 1D strip (or each column in a 2D matrix), slows down and stops at random position
 *  Created by Bob Loeffler and claude.ai
 *  First slider (Spin speed) is for the speed of the moving/spinning LED (random number within a narrow speed range).
 *     If value is 0, a random speed will be selected from the full range of values.
 *  Second slider (Spin slowdown start time) is for how long before the slowdown phase starts (random number within a narrow time range).
 *     If value is 0, a random time will be selected from the full range of values.
 *  Third slider (Spinner size) is for the number of pixels that make up the spinner.
 *  Fourth slider (Spin delay) is for how long it takes for the LED to start spinning again after the previous spin.
 *  The first checkbox allows the spinner to spin. If it's enabled, the spinner will do its thing. If it's not enabled, it will wait for the user to enable
 *     it either by clicking the checkbox or by pressing a physical button (e.g. using a playlist to run a couple presets that have JSON API codes).
 *  The second checkbox sets "color per block" mode. Enabled means that each spinner block will be the same color no matter what its LED position is.
 *  The third checkbox enables synchronized restart (all spinners restart together instead of individually).
 *  aux0 stores the settings checksum to detect changes
 *  aux1 stores the color scale for performance
 */

static void mode_spinning_wheel(void) {
  if (SEGLEN < 1) FX_FALLBACK_STATIC;

  unsigned strips = SEGMENT.nrOfVStrips();
  if (strips == 0) FX_FALLBACK_STATIC;

  constexpr unsigned stateVarsPerStrip = 8;
  unsigned dataSize = sizeof(uint32_t) * stateVarsPerStrip;
  if (!SEGENV.allocateData(dataSize * strips)) FX_FALLBACK_STATIC;
  uint32_t* state = reinterpret_cast<uint32_t*>(SEGENV.data);
  // state[0] = current position (fixed point: upper 16 bits = position, lower 16 bits = fraction)
  // state[1] = velocity (fixed point: pixels per frame * 65536)
  // state[2] = phase (0=fast spin, 1=slowing, 2=wobble, 3=stopped)
  // state[3] = stop time (when phase 3 was entered)
  // state[4] = wobble step (0=at stop pos, 1=moved back, 2=returned to stop)
  // state[5] = slowdown start time (when to transition from phase 0 to phase 1)
  // state[6] = wobble timing (for 200ms / 400ms / 300ms delays)
  // state[7] = store the stop position per strip

  // state[] index values for easier readability
  constexpr unsigned CUR_POS_IDX       = 0;  // state[0]
  constexpr unsigned VELOCITY_IDX      = 1;
  constexpr unsigned PHASE_IDX         = 2;
  constexpr unsigned STOP_TIME_IDX     = 3;
  constexpr unsigned WOBBLE_STEP_IDX   = 4;
  constexpr unsigned SLOWDOWN_TIME_IDX = 5;
  constexpr unsigned WOBBLE_TIME_IDX   = 6;
  constexpr unsigned STOP_POS_IDX      = 7;

  SEGMENT.fill(SEGCOLOR(1));

  // Handle random seeding globally (outside the virtual strip)
  if (SEGENV.call == 0) {
    SEGENV.aux1 = (255 << 8) / SEGLEN; // Cache the color scaling
  }

  // Check if settings changed (do this once, not per virtual strip)
  uint32_t settingssum = SEGMENT.speed + SEGMENT.intensity + SEGMENT.custom1 + SEGMENT.custom3 + SEGMENT.check1 + SEGMENT.check3;
  bool settingsChanged = (SEGENV.aux0 != settingssum);
  if (settingsChanged) {
    SEGENV.aux0 = settingssum;
  }

  // Check if all spinners are stopped and ready to restart (for synchronized restart)
  bool allReadyToRestart = true;
  if (SEGMENT.check3) {
    uint8_t spinnerSize = map(SEGMENT.custom1, 0, 255, 1, 10);
    uint16_t spin_delay = map(SEGMENT.custom3, 0, 31, 2000, 15000);
    uint32_t now = strip.now;

    for (unsigned stripNr = 0; stripNr < strips; stripNr += spinnerSize) {
      uint32_t* stripState = &state[stripNr * stateVarsPerStrip];
      // Check if this spinner is stopped AND has waited its delay
      if (stripState[PHASE_IDX] != 3 || stripState[STOP_TIME_IDX] == 0) {
        allReadyToRestart = false;
        break;
      }
      // Check if delay has elapsed
      if ((now - stripState[STOP_TIME_IDX]) < spin_delay) {
        allReadyToRestart = false;
        break;
      }
    }
  }
 
  struct virtualStrip {
    static void runStrip(uint16_t stripNr, uint32_t* state, bool settingsChanged, bool allReadyToRestart, unsigned strips) {
      uint8_t phase = state[PHASE_IDX];
      uint32_t now = strip.now;

      // Check for restart conditions
      bool needsReset = false;
      if (SEGENV.call == 0) {
        needsReset = true;
      } else if (settingsChanged && SEGMENT.check1) {
        needsReset = true;
      } else if (phase == 3 && state[STOP_TIME_IDX] != 0) {
          // If synchronized restart is enabled, only restart when all strips are ready
          if (SEGMENT.check3) {
            if (allReadyToRestart) {
              needsReset = true;
            }
          } else {
            // Normal mode: restart after individual strip delay
            uint16_t spin_delay = map(SEGMENT.custom3, 0, 31, 2000, 15000);
            if ((now - state[STOP_TIME_IDX]) >= spin_delay) {
              needsReset = true;
            }
          }
      }

      // Initialize or restart
      if (needsReset && SEGMENT.check1) {   // spin the spinner(s) only if the "Spin me!" checkbox is enabled
        state[CUR_POS_IDX] = 0;

        // Set velocity
        uint16_t speed = map(SEGMENT.speed, 0, 255, 300, 800);
        if (speed == 300) {  // random speed (user selected 0 on speed slider)
          state[VELOCITY_IDX] = hw_random16(200, 900) * 655;   // fixed-point velocity scaling (approx. 65536/100) 
        } else {
          state[VELOCITY_IDX] = hw_random16(speed - 100, speed + 100) * 655;
        }

        // Set slowdown start time
        uint16_t slowdown = map(SEGMENT.intensity, 0, 255, 3000, 5000);
        if (slowdown == 3000) {  // random slowdown start time (user selected 0 on intensity slider)
          state[SLOWDOWN_TIME_IDX] = now + hw_random16(2000, 6000);
        } else {
          state[SLOWDOWN_TIME_IDX] = now + hw_random16(slowdown - 1000, slowdown + 1000);
        }

        state[PHASE_IDX] = 0;
        state[STOP_TIME_IDX] = 0;
        state[WOBBLE_STEP_IDX] = 0;
        state[WOBBLE_TIME_IDX] = 0;
        state[STOP_POS_IDX] = 0; // Initialize stop position
        phase = 0;
      }

      uint32_t pos_fixed = state[CUR_POS_IDX];
      uint32_t velocity = state[VELOCITY_IDX];
      
      // Phase management
      if (phase == 0) {
        // Fast spinning phase
        if ((int32_t)(now - state[SLOWDOWN_TIME_IDX]) >= 0) {
          phase = 1;
          state[PHASE_IDX] = 1;
        }
      } else if (phase == 1) {
        // Slowing phase - apply deceleration
        uint32_t decel = velocity / 80;
        if (decel < 100) decel = 100;

        velocity = (velocity > decel) ? velocity - decel : 0;
        state[VELOCITY_IDX] = velocity;

        // Check if stopped
        if (velocity < 2000) {
          velocity = 0;
          state[VELOCITY_IDX] = 0;
          phase = 2;
          state[PHASE_IDX] = 2;
          state[WOBBLE_STEP_IDX] = 0;
          uint16_t stop_pos = (pos_fixed >> 16) % SEGLEN;
          state[STOP_POS_IDX] = stop_pos;
          state[WOBBLE_TIME_IDX] = now;
        }
      } else if (phase == 2) {
        // Wobble phase (moves the LED back one and then forward one)
        uint32_t wobble_step = state[WOBBLE_STEP_IDX];
        uint16_t stop_pos = state[STOP_POS_IDX];
        uint32_t elapsed = now - state[WOBBLE_TIME_IDX];

        if (wobble_step == 0 && elapsed >= 200) {
          // Move back one LED from stop position
          uint16_t back_pos = (stop_pos == 0) ? SEGLEN - 1 : stop_pos - 1;
          pos_fixed = ((uint32_t)back_pos) << 16;
          state[CUR_POS_IDX] = pos_fixed;
          state[WOBBLE_STEP_IDX] = 1;
          state[WOBBLE_TIME_IDX] = now;
        } else if (wobble_step == 1 && elapsed >= 400) {
          // Move forward to the stop position
          pos_fixed = ((uint32_t)stop_pos) << 16;
          state[CUR_POS_IDX] = pos_fixed;
          state[WOBBLE_STEP_IDX] = 2;
          state[WOBBLE_TIME_IDX] = now;
        } else if (wobble_step == 2 && elapsed >= 300) {
          // Wobble complete, enter stopped phase
          phase = 3;
          state[PHASE_IDX] = 3;
          state[STOP_TIME_IDX] = now;
        }
      }

      // Update position (phases 0 and 1 only)
      if (phase == 0 || phase == 1) {
        pos_fixed += velocity;
        state[CUR_POS_IDX] = pos_fixed;
      }

      // Draw LED for all phases
      uint16_t pos = (pos_fixed >> 16) % SEGLEN;

      uint8_t spinnerSize = map(SEGMENT.custom1, 0, 255, 1, 10);

      // Calculate color once per spinner block (based on strip number, not position)
      uint8_t hue;
      if (SEGMENT.check2) {
        // Each spinner block gets its own color based on strip number
        uint16_t numSpinners = max(1U, (strips + spinnerSize - 1) / spinnerSize);
        hue = (uint32_t)(255) * (stripNr / spinnerSize) / numSpinners;
      } else {
        // Color changes with position
        hue = (SEGENV.aux1 * pos) >> 8;
      }

      uint32_t color = ColorFromPalette(SEGPALETTE, hue, 255, LINEARBLEND);

      // Draw the spinner with configurable size (1-10 LEDs)
      for (int8_t x = 0; x < spinnerSize; x++) {
        for (uint8_t y = 0; y < spinnerSize; y++) {
          uint16_t drawPos = (pos + y) % SEGLEN;
          int16_t drawStrip = stripNr + x;

          // Wrap horizontally if needed, or skip if out of bounds
          if (drawStrip >= 0 && drawStrip < strips) {
            SEGMENT.setPixelColor(indexToVStrip(drawPos, drawStrip), color);
          }
        }
      }
    }
  };

  for (unsigned stripNr = 0; stripNr < strips; stripNr++) {
    // Only run on strips that are multiples of spinnerSize to avoid overlap
    uint8_t spinnerSize = map(SEGMENT.custom1, 0, 255, 1, 10);
    if (stripNr % spinnerSize == 0) {
      virtualStrip::runStrip(stripNr, &state[stripNr * stateVarsPerStrip], settingsChanged, allReadyToRestart, strips);
    }
  }
}
static const char _data_FX_MODE_SPINNINGWHEEL[] PROGMEM = "Spinning Wheel@Speed (0=random),Slowdown (0=random),Spinner size,,Spin delay,Spin me!,Color per block,Sync restart;!,!;!;;m12=1,c1=1,c3=8,o1=1,o3=1";


/*
/  Lava Lamp 2D effect
*   Uses particles to simulate rising blobs of "lava" or wax
*   Particles slowly rise, merge to create organic flowing shapes, and then fall to the bottom to start again
*   Created by Bob Loeffler using claude.ai
*   The first slider sets the number of active blobs
*   The second slider sets the size range of the blobs
*   The third slider sets the damping value for horizontal blob movement
*   The Attract checkbox sets the attraction of blobs (checked will make the blobs attract other close blobs horizontally)
*   The Keep Color Ratio checkbox sets whether we preserve the color ratio when displaying pixels that are in 2 or more overlapping blobs
*   aux0 keeps track of the blob size value
*   aux1 keeps track of the number of blobs
*/

typedef struct LavaParticle {
  float    x, y;         // Position
  float    vx, vy;       // Velocity
  float    size;         // Blob size
  uint8_t  hue;          // Color
  bool     active;       // will not be displayed if false
  uint16_t delayTop;     // number of frames to wait at top before falling again
  bool     idleTop;      // sitting idle at the top
  uint16_t delayBottom;  // number of frames to wait at bottom before rising again
  bool     idleBottom;   // sitting idle at the bottom
} LavaParticle;

static void mode_2D_lavalamp(void) {
  if (!strip.isMatrix || !SEGMENT.is2D()) FX_FALLBACK_STATIC; // not a 2D set-up

  const uint16_t cols = SEG_W;
  const uint16_t rows = SEG_H;
  constexpr float MAX_BLOB_RADIUS = 20.0f;  // cap to prevent frame rate drops on large matrices
  constexpr size_t MAX_LAVA_PARTICLES = 34;  // increasing this value could cause slowness for large matrices
  constexpr size_t MAX_TOP_FPS_DELAY = 900;  // max delay when particles are at the top
  constexpr size_t MAX_BOTTOM_FPS_DELAY = 1200;  // max delay when particles are at the bottom

  // Allocate per-segment storage
  if (!SEGENV.allocateData(sizeof(LavaParticle) * MAX_LAVA_PARTICLES)) FX_FALLBACK_STATIC;
  LavaParticle* lavaParticles = reinterpret_cast<LavaParticle*>(SEGENV.data);

  // Initialize particles on first call
  if (SEGENV.call == 0) {
    for (int i = 0; i < MAX_LAVA_PARTICLES; i++) {
      lavaParticles[i].active = false;
    }
  }

  // Track particle size and particle count slider changes, re-initialize if either changes
  uint8_t currentNumParticles = (SEGMENT.intensity >> 3) + 3;
  uint8_t currentSize = SEGMENT.custom1;
  if (currentNumParticles > MAX_LAVA_PARTICLES) currentNumParticles = MAX_LAVA_PARTICLES;
  bool needsReinit = (currentSize != SEGENV.aux0) || (currentNumParticles != SEGENV.aux1);

  if (needsReinit) {
    for (int i = 0; i < MAX_LAVA_PARTICLES; i++) {
      lavaParticles[i].active = false;
    }
    SEGENV.aux0 = currentSize;
    SEGENV.aux1 = currentNumParticles;
  }

  uint8_t size = currentSize;
  uint8_t numParticles = currentNumParticles;

  // blob size based on matrix width
  const float minSize = cols * 0.15f; // Minimum 15% of width
  const float maxSize = cols * 0.4f;  // Maximum 40% of width
  float sizeRange = (maxSize - minSize) * (size / 255.0f);
  int rangeInt = max(1, (int)(sizeRange));

  // calculate the spawning area for the particles
  const float spawnXStart = cols * 0.20f;
  const float spawnXWidth = cols * 0.60f;
  int spawnX = max(1, (int)(spawnXWidth));

  bool preserveColorRatio = SEGMENT.check3;

  // Spawn new particles at the bottom near the center
  for (int i = 0; i < MAX_LAVA_PARTICLES; i++) {
    if (!lavaParticles[i].active && hw_random8() < 32) { // spawn when slot available
      // Spawn in the middle 60% of the matrix width
      lavaParticles[i].x = spawnXStart + (float)hw_random16(spawnX);
      lavaParticles[i].y = rows - 1;
      lavaParticles[i].vx = (hw_random16(7) - 3) / 250.0f;
      lavaParticles[i].vy = -(hw_random16(20) + 10) / 100.0f * 0.3f;

      lavaParticles[i].size = minSize + (float)hw_random16(rangeInt);
      if (lavaParticles[i].size > MAX_BLOB_RADIUS) lavaParticles[i].size = MAX_BLOB_RADIUS;

      lavaParticles[i].hue = hw_random8();
      lavaParticles[i].active = true;

      // Set random delays when particles are at top and bottom
      lavaParticles[i].delayTop = hw_random16(MAX_TOP_FPS_DELAY);
      lavaParticles[i].delayBottom = hw_random16(MAX_BOTTOM_FPS_DELAY);
      lavaParticles[i].idleBottom = true;
      break;
    }
  }

  // Fade background slightly for trailing effect
  SEGMENT.fadeToBlackBy(40);

  // Update and draw particles
  int activeCount = 0;
  unsigned long currentMillis = strip.now;
  for (int i = 0; i < MAX_LAVA_PARTICLES; i++) {
    if (!lavaParticles[i].active) continue;
    activeCount++;

    // Keep particle count on target by deactivating excess particles
    if (activeCount > numParticles) {
      lavaParticles[i].active = false;
      activeCount--;
      continue;
    }

    LavaParticle *p = &lavaParticles[i];

    // Physics update
    p->x += p->vx;
    p->y += p->vy;

    // Optional particle/blob attraction
    if (SEGMENT.check2) {
      for (int j = 0; j < MAX_LAVA_PARTICLES; j++) {
        if (i == j || !lavaParticles[j].active) continue;

        LavaParticle *other = &lavaParticles[j];

        // Skip attraction if moving in same vertical direction (both up or both down)
        if ((p->vy < 0 && other->vy < 0) || (p->vy > 0 && other->vy > 0)) continue;

        float dx = other->x - p->x;
        float dy = other->y - p->y;

        // Apply weak horizontal attraction only
        float attractRange = p->size + other->size;
        float distSq = dx*dx + dy*dy;
        float attractRangeSq = attractRange * attractRange;
        if (distSq > 0 && distSq < attractRangeSq) {
          float dist = sqrt(distSq); // Only compute sqrt when needed
          float force = (1.0f - (dist / attractRange)) * 0.0001f;
          p->vx += (dx / dist) * force;
        }
      }
    }

    // Horizontal oscillation (makes it more organic)
    float damping= map(SEGMENT.custom2, 0, 255, 97, 87) / 100.0f;
    p->vx += sin((currentMillis / 1000.0f + i) * 0.5f) * 0.002f; // Reduced oscillation
    p->vx *= damping; // damping for more or less horizontal drift

    // Bounce off sides (don't affect vertical velocity)
    if (p->x < 0) {
      p->x = 0;
      p->vx = abs(p->vx); // reverse horizontal
    }
    if (p->x >= cols) {
      p->x = cols - 1;
      p->vx = -abs(p->vx); // reverse horizontal
    }

    // Adjust rise/fall velocity depending on approx distance from heat source (at bottom)
    // In top 1/4th of rows...
    if (p->y < rows * .25f) {
      if (p->vy >= 0) {  // if going down, delay the particles so they won't go down immediately
        if (p->delayTop > 0 && p->idleTop) {
          p->vy = 0.0f;
          p->delayTop--;
          p->idleTop = true;
        } else {
          p->vy = 0.01f;
          p->delayTop = hw_random16(MAX_TOP_FPS_DELAY);
          p->idleTop = false;
        }
      } else if (p->vy <= 0) {  // if going up, slow down the rise rate
        p->vy = -0.03f;
      }
    }

    // In next 1/4th of rows...
    if (p->y <= rows * .50f && p->y >= rows * .25f) {
      if (p->vy > 0) {  // if going down, speed up the fall rate
        p->vy = 0.03f;
      } else if (p->vy <= 0) {  // if going up, speed up the rise rate a little more
        p->vy = -0.05f;
      }
    }

    // In next 1/4th of rows...
    if (p->y <= rows * .75f && p->y >= rows * .50f) {
      if (p->vy > 0) {  // if going down, speed up the fall rate a little more
        p->vy = 0.04f;
      } else if (p->vy <= 0) {  // if going up, speed up the rise rate
        p->vy = -0.03f;
      }
    }

    // In bottom 1/4th of rows...
    if (p->y > rows * .75f) {
      if (p->vy >= 0) {  // if going down, slow down the fall rate
        p->vy = 0.02f;
      } else if (p->vy <= 0) {  // if going up, delay the particles so they won't go up immediately
        if (p->delayBottom > 0 && p->idleBottom) {
          p->vy = 0.0f;
          p->delayBottom--;
          p->idleBottom = true;
        } else {
          p->vy = -0.01f;
          p->delayBottom = hw_random16(MAX_BOTTOM_FPS_DELAY);
          p->idleBottom = false;
        }
      }
    }

    // Boundary handling with reversal of direction
    // When reaching TOP (y=0 area), reverse to fall back down, but need to delay first
    if (p->y <= 0.5f * p->size) {
      p->y = 0.5f * p->size;
      if (p->vy < 0) {
        p->vy = 0.005f;  // set to a tiny positive value to start falling very slowly
        p->idleTop = true;
      }
    }

    // When reaching BOTTOM (y=rows-1 area), reverse to rise back up, but need to delay first
    if (p->y >= rows - 0.5f * p->size) {
      p->y = rows - 0.5f * p->size;
      if (p->vy > 0) {
        p->vy = -0.005f;  // set to a tiny negative value to start rising very slowly
        p->idleBottom = true;
      }
    }

    // Get color
    uint32_t color;
    color = SEGMENT.color_from_palette(p->hue, true, PALETTE_SOLID_WRAP, 0);

    // Extract RGB and apply life/opacity
    uint8_t w = (W(color) * 255) >> 8;
    uint8_t r = (R(color) * 255) >> 8;
    uint8_t g = (G(color) * 255) >> 8;
    uint8_t b = (B(color) * 255) >> 8;

    // Draw blob with sub-pixel accuracy using bilinear distribution
    float sizeSq = p->size * p->size;

    // Get fractional offsets of particle center
    float fracX = p->x - floorf(p->x);
    float fracY = p->y - floorf(p->y);
    int centerX = (int)floorf(p->x);
    int centerY = (int)floorf(p->y);

    for (int dy = -(int)p->size - 1; dy <= (int)p->size + 1; dy++) {
      for (int dx = -(int)p->size - 1; dx <= (int)p->size + 1; dx++) {
        int px = centerX + dx;
        int py = centerY + dy;

        if (px < 0 || px >= cols || py < 0 || py >= rows) continue;

        // Sub-pixel distance: measure from true float center to pixel center
        float subDx = dx - fracX;  // distance from true center to this pixel's center
        float subDy = dy - fracY;
        float distSq = subDx * subDx + subDy * subDy;

        if (distSq < sizeSq) {
          float intensity = 1.0f - (distSq / sizeSq);
          intensity = intensity * intensity; // smooth falloff

          uint8_t bw = (uint8_t)(w * intensity);
          uint8_t br = (uint8_t)(r * intensity);
          uint8_t bg = (uint8_t)(g * intensity);
          uint8_t bb = (uint8_t)(b * intensity);

          uint32_t existing = SEGMENT.getPixelColorXY(px, py);
          uint32_t newColor = RGBW32(br, bg, bb, bw);
          SEGMENT.setPixelColorXY(px, py, color_add(existing, newColor, preserveColorRatio ? true : false));
        }
      }
    }
  }
}
static const char _data_FX_MODE_2D_LAVALAMP[] PROGMEM = "Lava Lamp@,# of blobs,Blob size,H. Damping,,,Attract,Keep Color Ratio;;!;2;ix=64,c2=192,o2=1,o3=1,pal=47";


/*
/  Magma effect
*   2D magma/lava animation
*   Adapted from FireLamp_JeeUI implementation (https://github.com/DmytroKorniienko/FireLamp_JeeUI/tree/dev)
*   Original idea by SottNick, remastered by kostyamat
*   Adapted to WLED by Bob Loeffler and claude.ai
*   First slider (speed) is for the speed or flow rate of the moving magma.
*   Second slider (intensity) is for the height of the magma.
*   Third slider (lava bombs) is for the number of lava bombs (particles).  The max # is 1/2 the number of columns on the 2D matrix.
*   Fourth slider (gravity) is for how high the lava bombs will go.
*   The checkbox (check2) is for whether the lava bombs can be seen in the magma or behind it.
*/

// Draw the magma
static void drawMagma(const uint16_t width, const uint16_t height, float *ff_y, float *ff_z, uint8_t *shiftHue) {
  // Noise parameters - adjust these for different magma characteristics
  // deltaValue: higher = more detailed/turbulent magma
  // deltaHue: higher = taller magma structures
  constexpr uint8_t magmaDeltaValue = 12U;
  constexpr uint8_t magmaDeltaHue   = 10U;

  uint16_t ff_y_int = (uint16_t)*ff_y;
  uint16_t ff_z_int = (uint16_t)*ff_z;

  for (uint16_t i = 0; i < width; i++) {
    for (uint16_t j = 0; j < height; j++) {
      // Generate Perlin noise value (0-255)
      uint8_t noise = perlin8(i * magmaDeltaValue, (j + ff_y_int + hw_random8(2)) * magmaDeltaHue, ff_z_int);
      uint8_t paletteIndex = qsub8(noise, shiftHue[j]);  // Apply the vertical fade gradient
      CRGB col = SEGMENT.color_from_palette(paletteIndex, false, PALETTE_SOLID_WRAP, 0);  // Get color from palette
      SEGMENT.addPixelColorXY(i, height - 1 - j, col);  // magma rises from bottom of display
    }
  }
}

// Move and draw lava bombs (particles)
static void drawLavaBombs(const uint16_t width, const uint16_t height, float *particleData, float gravity, uint8_t particleCount) {
  for (uint16_t i = 0; i < particleCount; i++) {
    uint16_t idx = i * 4;

    particleData[idx + 3] -= gravity;
    particleData[idx + 0] += particleData[idx + 2];
    particleData[idx + 1] += particleData[idx + 3];

    float posX = particleData[idx + 0];
    float posY = particleData[idx + 1];

    if (posY > height + height / 4) {
      particleData[idx + 3] = -particleData[idx + 3] * 0.8f;
    }

    if (posY < (float)(height / 8) - 1.0f || posX < 0 || posX >= width) {
      particleData[idx + 0] = hw_random(0, width * 100) / 100.0f;
      particleData[idx + 1] = hw_random(0, height * 25) / 100.0f;
      particleData[idx + 2] = hw_random(-75, 75) / 100.0f;

      float baseVelocity = hw_random(60, 120) / 100.0f;
      if (hw_random8() < 50) {
        baseVelocity *= 1.6f;
      }
      particleData[idx + 3] = baseVelocity;
      continue;
    }

    int16_t xi = (int16_t)posX;
    int16_t yi = (int16_t)posY;

    if (xi >= 0 && xi < width && yi >= 0 && yi < height) {
      // Get a random color from the current palette
      uint8_t randomIndex = hw_random8(64, 128);
      CRGB pcolor = ColorFromPalette(SEGPALETTE, randomIndex, 255, LINEARBLEND);

      // Pre-calculate anti-aliasing weights
      float xf = posX - xi;
      float yf = posY - yi;
      float ix = 1.0f - xf;
      float iy = 1.0f - yf;

      uint8_t w0 = 255 * ix * iy;
      uint8_t w1 = 255 * xf * iy;
      uint8_t w2 = 255 * ix * yf;
      uint8_t w3 = 255 * xf * yf;

      int16_t yFlipped = height - 1 - yi;  // Flip Y coordinate

      SEGMENT.addPixelColorXY(xi, yFlipped, pcolor.scale8(w0));
      if (xi + 1 < width) 
        SEGMENT.addPixelColorXY(xi + 1, yFlipped, pcolor.scale8(w1));
      if (yFlipped - 1 >= 0)
        SEGMENT.addPixelColorXY(xi, yFlipped - 1, pcolor.scale8(w2));
      if (xi + 1 < width && yFlipped - 1 >= 0)
        SEGMENT.addPixelColorXY(xi + 1, yFlipped - 1, pcolor.scale8(w3));
    }
  }
}

static void mode_2D_magma(void) {
  if (!strip.isMatrix || !SEGMENT.is2D()) FX_FALLBACK_STATIC;  // not a 2D set-up
  const uint16_t width = SEG_W;
  const uint16_t height = SEG_H;
  const uint8_t MAGMA_MAX_PARTICLES = width / 2;
  if (MAGMA_MAX_PARTICLES < 2) FX_FALLBACK_STATIC;  // matrix too narrow for lava bombs
  constexpr size_t SETTINGS_SUM_BYTES = 4; // 4 bytes for settings sum

  // Allocate memory: particles (4 floats each) + 2 floats for noise counters + shiftHue cache + settingsSum
  const uint16_t dataSize = (MAGMA_MAX_PARTICLES * 4 + 2) * sizeof(float) + height * sizeof(uint8_t) + SETTINGS_SUM_BYTES;
  if (!SEGENV.allocateData(dataSize)) FX_FALLBACK_STATIC;  // allocation failed

  float* particleData = reinterpret_cast<float*>(SEGENV.data);
  float* ff_y = &particleData[MAGMA_MAX_PARTICLES * 4];
  float* ff_z = &particleData[MAGMA_MAX_PARTICLES * 4 + 1];
  uint32_t* settingsSumPtr = reinterpret_cast<uint32_t*>(&particleData[MAGMA_MAX_PARTICLES * 4 + 2]);
  uint8_t* shiftHue = reinterpret_cast<uint8_t*>(reinterpret_cast<uint8_t*>(settingsSumPtr) + SETTINGS_SUM_BYTES);

  // Check if settings changed
  uint32_t settingsKey = (uint32_t)SEGMENT.speed | ((uint32_t)SEGMENT.intensity << 8) |
      ((uint32_t)SEGMENT.custom1 << 16) | ((uint32_t)SEGMENT.custom2 << 24);
  bool settingsChanged = (*settingsSumPtr != settingsKey);

  if (SEGENV.call == 0 || settingsChanged) {
    // Intensity slider controls magma height
    uint16_t intensity = SEGMENT.intensity;
    uint16_t fadeRange = map(intensity, 0, 255, height / 3, height);

    // shiftHue controls the vertical color gradient (magma fades out toward top)
    for (uint16_t j = 0; j < height; j++) {
      if (j < fadeRange) {
        // prevent division issues and ensure smooth gradient
        if (fadeRange > 1) {
          shiftHue[j] = (uint8_t)(j * 255 / (fadeRange - 1));
        } else {
          shiftHue[j] = 0;  // Single row magma = no fade
        }
      } else {
        shiftHue[j] = 255;
      }
    }

    // Initialize all particles
    for (uint16_t i = 0; i < MAGMA_MAX_PARTICLES; i++) {
      uint16_t idx = i * 4;
      particleData[idx + 0] = hw_random(0, width * 100) / 100.0f;
      particleData[idx + 1] = hw_random(0, height * 25) / 100.0f;
      particleData[idx + 2] = hw_random(-75, 75) / 100.0f;

      float baseVelocity = hw_random(60, 120) / 100.0f;
      if (hw_random8() < 50) {
        baseVelocity *= 1.6f;
      }
      particleData[idx + 3] = baseVelocity;
    }
    *ff_y = 0.0f;
    *ff_z = 0.0f;
    *settingsSumPtr = settingsKey;
  }

  if (!shiftHue) FX_FALLBACK_STATIC;   // safety check

  // Speed control
  float speedfactor = SEGMENT.speed / 255.0f;
  speedfactor = speedfactor * speedfactor * 1.5f;
  if (speedfactor < 0.001f) speedfactor = 0.001f;

  // Gravity control
  float gravity = map(SEGMENT.custom2, 0, 255, 5, 20) / 100.0f;

  // Number of particles (lava bombs)
  uint8_t particleCount = map(SEGMENT.custom1, 0, 255, 0, MAGMA_MAX_PARTICLES);
  particleCount = constrain(particleCount, 0, MAGMA_MAX_PARTICLES);

  // Draw lava bombs in front of magma (or behind it)
  if (SEGMENT.check2) {
    drawMagma(width, height, ff_y, ff_z, shiftHue);
    SEGMENT.fadeToBlackBy(70);    // Dim the entire display to create trailing effect
    if (particleCount > 0) drawLavaBombs(width, height, particleData, gravity, particleCount);
  }
  else {
    if (particleCount > 0) drawLavaBombs(width, height, particleData, gravity, particleCount);
    SEGMENT.fadeToBlackBy(70);    // Dim the entire display to create trailing effect
    drawMagma(width, height, ff_y, ff_z, shiftHue);
  }

  // noise counters based on speed slider
  *ff_y += speedfactor * 2.0f;
  *ff_z += speedfactor;

  SEGENV.step++;
}
static const char _data_FX_MODE_2D_MAGMA[] PROGMEM = "Magma@Flow rate,Magma height,Lava bombs,Gravity,,,Bombs in front;;!;2;ix=192,c2=32,o2=1,pal=35";


/*
/  Ants (created by making modifications to the Rolling Balls code) - Bob Loeffler 2025
*   First slider is for the ants' speed.
*   Second slider is for the # of ants.
*   Third slider is for the Ants' size.
*   Fourth slider (custom2) is for blurring the LEDs in the segment.
*   Checkbox1 is for Gathering food (enabled if you want the ants to gather food, disabled if they are just walking).
*     We will switch directions when they get to the beginning or end of the segment when gathering food.
*     When gathering food, the Pass By option will automatically be enabled so they can drop off their food easier (and look for more food).
*   Checkbox2 is for Smear mode (enabled is smear pixel colors, disabled is no smearing)
*   Checkbox3 is for whether the ants will bump into each other (disabled) or just pass by each other (enabled)
*/

// Ant structure representing each ant's state
struct Ant {
  unsigned long lastBumpUpdate;  // the last time the ant bumped into another ant
  bool hasFood;
  float velocity;
  float position;  // (0.0 to 1.0 range)
};

constexpr unsigned MAX_ANTS = 32;
constexpr float MIN_COLLISION_TIME_MS = 2.0f;
constexpr float VELOCITY_MIN = 2.0f;
constexpr float VELOCITY_MAX = 10.0f;
constexpr unsigned ANT_SIZE_MIN = 1;
constexpr unsigned ANT_SIZE_MAX = 20;

// Helper function to get food pixel color based on ant and background colors
static uint32_t getFoodColor(uint32_t antColor, uint32_t backgroundColor) {
  if (antColor == WHITE)
    return (backgroundColor == YELLOW) ? GRAY : YELLOW;
  return (backgroundColor == WHITE) ? YELLOW : WHITE;
}

// Helper function to handle ant boundary wrapping or bouncing
static void handleBoundary(Ant& ant, float& position, bool gatherFood, bool atStart, unsigned long currentTime) {
  if (gatherFood) {
    // Bounce mode: reverse direction and update food status
    position = atStart ? 0.0f : 1.0f;
    ant.velocity = -ant.velocity;
    ant.lastBumpUpdate = currentTime;
    ant.position = position;
    ant.hasFood = atStart;  // Has food when leaving start, drops it at end
  } else {
    // Wrap mode: teleport to opposite end
    position = atStart ? 1.0f : 0.0f;
    ant.lastBumpUpdate = currentTime;
    ant.position = position;
  }
}

// Helper function to calculate ant color
static uint32_t getAntColor(int antIndex, int numAnts, bool usePalette) {
  if (usePalette)
    return SEGMENT.color_from_palette(antIndex * 255 / numAnts, false, (paletteBlend == 1 || paletteBlend == 3), 255);
  // Alternate between two colors for default palette
  return (antIndex % 3 == 1) ? SEGCOLOR(0) : SEGCOLOR(2);
}

// Helper function to render a single ant pixel with food handling
static void renderAntPixel(int pixelIndex, int pixelOffset, int antSize, const Ant& ant, uint32_t antColor, uint32_t backgroundColor, bool gatherFood) {
  bool isMovingBackward = (ant.velocity < 0);
  bool isFoodPixel = gatherFood && ant.hasFood && ((isMovingBackward && pixelOffset == 0) || (!isMovingBackward && pixelOffset == antSize - 1));
  if (isFoodPixel) {
    SEGMENT.setPixelColor(pixelIndex, getFoodColor(antColor, backgroundColor));
  } else {
    SEGMENT.setPixelColor(pixelIndex, antColor);
  }
}

static void mode_ants(void) {
  if (SEGLEN <= 1) FX_FALLBACK_STATIC;

  // Allocate memory for ant data
  uint32_t backgroundColor = SEGCOLOR(1);
  unsigned dataSize = sizeof(Ant) * MAX_ANTS;
  if (!SEGENV.allocateData(dataSize)) FX_FALLBACK_STATIC;  // Allocation failed

  Ant* ants = reinterpret_cast<Ant*>(SEGENV.data);

  // Extract configuration from segment settings
  unsigned numAnts = min(1 + (SEGLEN * SEGMENT.intensity >> 12), MAX_ANTS);
  bool gatherFood = SEGMENT.check1;
  bool SmearMode = SEGMENT.check2;
  bool passBy = SEGMENT.check3 || gatherFood;  // global no‑collision when gathering food is enabled
  unsigned antSize = map(SEGMENT.custom1, 0, 255, ANT_SIZE_MIN, ANT_SIZE_MAX) + (gatherFood ? 1 : 0);

  // Initialize ants on first call
  if (SEGENV.call == 0) {
    int confusedAntIndex = hw_random(0, numAnts);   // the first random ant to go backwards

    for (int i = 0; i < MAX_ANTS; i++) {
      ants[i].lastBumpUpdate = strip.now;

      // Random velocity
      float velocity = VELOCITY_MIN + (VELOCITY_MAX - VELOCITY_MIN) * hw_random16(1000, 5000) / 5000.0f;
      // One random ant moves in opposite direction
      ants[i].velocity = (i == confusedAntIndex) ? -velocity : velocity;
      // Random starting position (0.0 to 1.0)
      ants[i].position = hw_random16(0, 10000) / 10000.0f;
      // Ants don't have food yet
      ants[i].hasFood = false;
    }
  }

  // Calculate time conversion factor based on speed slider
  float timeConversionFactor = float(scale8(8, 255 - SEGMENT.speed) + 1) * 20000.0f;

  // Clear background if not in Smear mode
  if (!SmearMode) SEGMENT.fill(backgroundColor);

  // Update and render each ant
  for (int i = 0; i < numAnts; i++) {
    float timeSinceLastUpdate = float(int(strip.now - ants[i].lastBumpUpdate)) / timeConversionFactor;
    float newPosition = ants[i].position + ants[i].velocity * timeSinceLastUpdate;

    // Reset ants that wandered too far off-track (e.g., after intensity change)
    if (newPosition < -0.5f || newPosition > 1.5f) {
      newPosition = ants[i].position = hw_random16(0, 10000) / 10000.0f;
      ants[i].lastBumpUpdate = strip.now;
    }

    // Handle boundary conditions (bounce or wrap)
    if (newPosition <= 0.0f && ants[i].velocity < 0.0f) {
      handleBoundary(ants[i], newPosition, gatherFood, true, strip.now);
    } else if (newPosition >= 1.0f && ants[i].velocity > 0.0f) {
      handleBoundary(ants[i], newPosition, gatherFood, false, strip.now);
    }

    // Handle collisions between ants (if not passing by)
    if (!passBy) {
      for (int j = i + 1; j < numAnts; j++) {
        if (fabsf(ants[j].velocity - ants[i].velocity) < 0.001f) continue;  // Moving in same direction at same speed; avoids tiny denominators

        // Calculate collision time using physics -  collisionTime formula adapted from rolling_balls
        float timeOffset = float(int(ants[j].lastBumpUpdate - ants[i].lastBumpUpdate));
        float collisionTime = (timeConversionFactor * (ants[i].position - ants[j].position) + ants[i].velocity * timeOffset) / (ants[j].velocity - ants[i].velocity);

        // Check if collision occurred in valid time window
        float timeSinceJ = float(int(strip.now - ants[j].lastBumpUpdate));
        if (collisionTime > MIN_COLLISION_TIME_MS && collisionTime < timeSinceJ) {
          // Update positions to collision point
          float adjustedTime = (collisionTime + float(int(ants[j].lastBumpUpdate - ants[i].lastBumpUpdate))) / timeConversionFactor;
          ants[i].position += ants[i].velocity * adjustedTime;
          ants[j].position = ants[i].position;

          // Update collision time
          unsigned long collisionMoment = static_cast<unsigned long>(collisionTime + 0.5f) + ants[j].lastBumpUpdate;
          ants[i].lastBumpUpdate = collisionMoment;
          ants[j].lastBumpUpdate = collisionMoment;

          // Reverse the ant with greater speed magnitude
          if (fabsf(ants[i].velocity) > fabsf(ants[j].velocity)) {
            ants[i].velocity = -ants[i].velocity;
          } else {
            ants[j].velocity = -ants[j].velocity;
          }

          // Recalculate position after collision
          newPosition = ants[i].position + ants[i].velocity * float(int(strip.now - ants[i].lastBumpUpdate)) / timeConversionFactor;
        }
      }
    }

    // Clamp position to valid range
    newPosition = constrain(newPosition, 0.0f, 1.0f);
    unsigned pixelPosition = roundf(newPosition * (SEGLEN - 1));

    // Determine ant color
    uint32_t antColor = getAntColor(i, numAnts, SEGMENT.palette != 0);

    // Render ant pixels
    for (int pixelOffset = 0; pixelOffset < antSize; pixelOffset++) {
      unsigned currentPixel = pixelPosition + pixelOffset;
      if (currentPixel >= SEGLEN) break;
      renderAntPixel(currentPixel, pixelOffset, antSize, ants[i], antColor, backgroundColor, gatherFood);
    }

    // Update ant state
    ants[i].lastBumpUpdate = strip.now;
    ants[i].position = newPosition;
  }

  SEGMENT.blur(SEGMENT.custom2>>1);
}
static const char _data_FX_MODE_ANTS[] PROGMEM = "Ants@Ant speed,# of ants,Ant size,Blur,,Gathering food,Smear,Pass by;!,!,!;!;1;sx=192,ix=255,c1=32,c2=0,o1=1,o3=1";


/*
/  Morse Code by Bob Loeffler
*   Adapted from code by automaticaddison.com and then optimized by claude.ai
*   aux0 is the pattern offset for scrolling
*   aux1 saves settings: check2 (1 bit), check3 (1 bit), text hash (4 bits) and pattern length (10 bits)
*   The first slider (sx) selects the scrolling speed
*   The second slider selects the color mode (lower half selects color wheel, upper half selects color palettes)
*   Checkbox1 displays all letters in a word with the same color
*   Checkbox2 displays punctuation or not
*   Checkbox3 displays the End-of-message code or not
*   We get the text from the SEGMENT.name and convert it to morse code
*   This effect uses a bit array, instead of bool array, for efficient storage - 8x memory reduction (128 bytes vs 1024 bytes)
*
*   Morse Code rules:
*    - a dot is 1 pixel/LED; a dash is 3 pixels/LEDs
*    - there is 1 space between each dot or dash that make up a letter/number/punctuation
*    - there are 3 spaces between each letter/number/punctuation
*    - there are 7 spaces between each word
*/

// Bit manipulation macros
#define SET_BIT8(arr, i) ((arr)[(i) >> 3] |= (1 << ((i) & 7)))
#define GET_BIT8(arr, i) (((arr)[(i) >> 3] & (1 << ((i) & 7))) != 0)

// Build morse code pattern into a buffer
static void build_morsecode_pattern(const char *morse_code, uint8_t *pattern, uint8_t *wordIndex, uint16_t &index, uint8_t currentWord, int maxSize) {
  const char *c = morse_code;

  // Build the dots and dashes into pattern array
  while (*c != '\0') {
    // it's a dot which is 1 pixel
    if (*c == '.') {
      if (index >= maxSize - 1) return;
      SET_BIT8(pattern, index);
      wordIndex[index] = currentWord;
      index++;
    }
    else { // Must be a dash which is 3 pixels
      if (index >= maxSize - 3) return;
      SET_BIT8(pattern, index);
      wordIndex[index] = currentWord;
      index++;
      SET_BIT8(pattern, index);
      wordIndex[index] = currentWord;
      index++;
      SET_BIT8(pattern, index);
      wordIndex[index] = currentWord;
      index++;
    }

    c++;

    // 1 space between parts of a letter/number/punctuation (but not after the last one)
    if (*c != '\0') {
      if (index >= maxSize) return;
      wordIndex[index] = currentWord;
      index++;
    }
  }

  // 3 spaces between two letters/numbers/punctuation
  if (index >= maxSize - 2) return;
  wordIndex[index] = currentWord;
  index++;
  if (index >= maxSize - 1) return;
  wordIndex[index] = currentWord;
  index++;
  if (index >= maxSize) return;
  wordIndex[index] = currentWord;
  index++;
}

static void mode_morsecode(void) {
  if (SEGLEN < 1) FX_FALLBACK_STATIC;

  // A-Z in Morse Code
  static const char * letters[] = {".-", "-...", "-.-.", "-..", ".", "..-.", "--.", "....", "..", ".---", "-.-", ".-..", "--",
                     "-.", "---", ".--.", "--.-", ".-.", "...", "-", "..-", "...-", ".--", "-..-", "-.--", "--.."};
  // 0-9 in Morse Code
  static const char * numbers[] = {"-----", ".----", "..---", "...--", "....-", ".....", "-....", "--...", "---..", "----."};

  // Punctuation in Morse Code
  struct PunctuationMapping {
    char character;
    const char* code;
  };

  static const PunctuationMapping punctuation[] = {
    {'.', ".-.-.-"}, {',', "--..--"}, {'?', "..--.."}, 
    {':', "---..."}, {'-', "-....-"}, {'!', "-.-.--"},
    {'&', ".-..."}, {'@', ".--.-."}, {')', "-.--.-"},
    {'(', "-.--."}, {'/', "-..-."}, {'\'', ".----."}
  };

  // Get the text to display
  char text[WLED_MAX_SEGNAME_LEN+1] = {'\0'};
  size_t len = 0;

  if (SEGMENT.name) len = strlen(SEGMENT.name);
  if (len == 0) {
    strcpy_P(text, PSTR("I Love WLED!"));
  } else {
    strcpy(text, SEGMENT.name);
  }

  // Convert to uppercase in place
  for (char *p = text; *p; p++) {
    *p = toupper(*p);
  }

  // Allocate per-segment storage for pattern (1023 bits = 127 bytes) + word index array (1024 bytes) + word count (1 byte)
  constexpr size_t MORSECODE_MAX_PATTERN_SIZE = 1023;
  constexpr size_t MORSECODE_PATTERN_BYTES = (MORSECODE_MAX_PATTERN_SIZE + 7) / 8; // 128 bytes
  constexpr size_t MORSECODE_WORD_INDEX_BYTES = MORSECODE_MAX_PATTERN_SIZE; // 1 byte per bit position
  constexpr size_t MORSECODE_WORD_COUNT_BYTES = 1; // 1 byte for word count
  if (!SEGENV.allocateData(MORSECODE_PATTERN_BYTES + MORSECODE_WORD_INDEX_BYTES + MORSECODE_WORD_COUNT_BYTES)) FX_FALLBACK_STATIC;
  uint8_t* morsecodePattern = reinterpret_cast<uint8_t*>(SEGENV.data);
  uint8_t* wordIndexArray = reinterpret_cast<uint8_t*>(SEGENV.data + MORSECODE_PATTERN_BYTES);
  uint8_t* wordCountPtr = reinterpret_cast<uint8_t*>(SEGENV.data + MORSECODE_PATTERN_BYTES + MORSECODE_WORD_INDEX_BYTES);

  // SEGENV.aux1 stores: [bit 15: check2] [bit 14: check3] [bits 10-13: text hash (4 bits)] [bits 0-9: pattern length]
  bool lastCheck2 = (SEGENV.aux1 & 0x8000) != 0;
  bool lastCheck3 = (SEGENV.aux1 & 0x4000) != 0;
  uint16_t lastHashBits = (SEGENV.aux1 >> 10) & 0xF; // 4 bits of hash
  uint16_t patternLength = SEGENV.aux1 & 0x3FF; // Lower 10 bits for length (up to 1023)

  // Compute text hash
  uint16_t textHash = 0;
  for (char *p = text; *p; p++) {
    textHash = ((textHash << 5) + textHash) + *p;
  }
  uint16_t currentHashBits = (textHash >> 12) & 0xF; // Use upper 4 bits of hash

  bool textChanged = (currentHashBits != lastHashBits) && (SEGENV.call > 0);

  // Check if we need to rebuild the pattern
  bool needsRebuild = (SEGENV.call == 0) || textChanged || (SEGMENT.check2 != lastCheck2) || (SEGMENT.check3 != lastCheck3);

  // Initialize on first call or rebuild pattern
  if (needsRebuild) {
    patternLength = 0;

    // Clear the bit array and word index array first
    memset(morsecodePattern, 0, MORSECODE_PATTERN_BYTES);
    memset(wordIndexArray, 0, MORSECODE_WORD_INDEX_BYTES);

    // Track current word index
    uint8_t currentWordIndex = 0;

    // Build complete morse code pattern
    for (char *c = text; *c; c++) {
      if (patternLength >= MORSECODE_MAX_PATTERN_SIZE - 10) break;

      if (*c >= 'A' && *c <= 'Z') {
        build_morsecode_pattern(letters[*c - 'A'], morsecodePattern, wordIndexArray, patternLength, currentWordIndex, MORSECODE_MAX_PATTERN_SIZE);
      }
      else if (*c >= '0' && *c <= '9') {
        build_morsecode_pattern(numbers[*c - '0'], morsecodePattern, wordIndexArray, patternLength, currentWordIndex, MORSECODE_MAX_PATTERN_SIZE);
      }
      else if (*c == ' ') {
        // Space between words - increment word index for next word
        currentWordIndex++;
        // Add 4 additional spaces (7 total with the 3 after each letter)
        for (int x = 0; x < 4; x++) {
          if (patternLength >= MORSECODE_MAX_PATTERN_SIZE) break;
          wordIndexArray[patternLength] = currentWordIndex;
          patternLength++;
        }
      }
      else if (SEGMENT.check2) {
        const char *punctuationCode = nullptr;
        for (const auto& p : punctuation) {
          if (*c == p.character) {
            punctuationCode = p.code;
            break;
          }
        }
        if (punctuationCode) {
          build_morsecode_pattern(punctuationCode, morsecodePattern, wordIndexArray, patternLength, currentWordIndex, MORSECODE_MAX_PATTERN_SIZE);
        }
      }
    }

    if (SEGMENT.check3) {
      build_morsecode_pattern(".-.-.", morsecodePattern, wordIndexArray, patternLength, currentWordIndex, MORSECODE_MAX_PATTERN_SIZE);
    }

    for (int x = 0; x < 7; x++) {
      if (patternLength >= MORSECODE_MAX_PATTERN_SIZE) break;
      wordIndexArray[patternLength] = currentWordIndex;
      patternLength++;
    }

    // Store the total number of words (currentWordIndex + 1 because it's 0-indexed)
    *wordCountPtr = currentWordIndex + 1;

    // Store pattern length, checkbox states, and hash bits in aux1
    SEGENV.aux1 = patternLength | (currentHashBits << 10) | (SEGMENT.check2 ? 0x8000 : 0) | (SEGMENT.check3 ? 0x4000 : 0);

    // Reset the scroll offset
    SEGENV.aux0 = 0;
  }

  // if pattern is empty for some reason, display black background only
  if (patternLength == 0) {
    SEGMENT.fill(BLACK);
    return;
  }

  // Update offset to make the morse code scroll
  // Use step for scroll timing only
  uint32_t cycleTime = 50 + (255 - SEGMENT.speed)*3;
  uint32_t it = strip.now / cycleTime;
  if (SEGENV.step != it) {
    SEGENV.aux0++;
    SEGENV.step = it;
  }

  // Clear background
  SEGMENT.fill(BLACK);

  // Draw the scrolling pattern
  int offset = SEGENV.aux0 % patternLength;

  // Get the word count and calculate color spacing
  uint8_t wordCount = *wordCountPtr;
  if (wordCount == 0) wordCount = 1;
  uint8_t colorSpacing = 255 / wordCount; // Distribute colors evenly across color wheel/palette

  for (int i = 0; i < SEGLEN; i++) {
    int patternIndex = (offset + i) % patternLength;
    if (GET_BIT8(morsecodePattern, patternIndex)) {
      uint8_t wordIdx = wordIndexArray[patternIndex];
      if (SEGMENT.check1) {  // make each word a separate color
        if (SEGMENT.custom3 < 16)
          // use word index to select base color, add slight offset for animation
          SEGMENT.setPixelColor(i, SEGMENT.color_wheel((wordIdx * colorSpacing) + (SEGENV.aux0 / 4)));
        else
          SEGMENT.setPixelColor(i, SEGMENT.color_from_palette(wordIdx * colorSpacing, true, PALETTE_SOLID_WRAP, 0));
      }
      else {
        if (SEGMENT.custom3 < 16)
          SEGMENT.setPixelColor(i, SEGMENT.color_wheel(SEGENV.aux0 + i));
        else
          SEGMENT.setPixelColor(i, SEGMENT.color_from_palette(i, true, PALETTE_SOLID_WRAP, 0));
      }
    }
  }
}
static const char _data_FX_MODE_MORSECODE[] PROGMEM = "Morse Code@Speed,,,,Color mode,Color by Word,Punctuation,EndOfMessage;;!;1;sx=192,c3=8,o1=1,o2=1";


/*
 * Dissolve Plus
 *   Modifications to original Dissolve effect by Bob Loeffler
 *   slider 1 is for the delay interval between dissolving and filling
 *   slider 2 is for the dissolving speed
 *   slider 3 is for the filling speed
 *   slider 4 is for the delay when only one LED is lit and when in lastOne mode (not used if Last One checkbox is not selected)
 *     If set to max value (255), the effect will not redraw any LEDs, so this can be used with a playlist and physical button to,
 *     for example, restart the animation by unchecking checkbox 3 in a preset and then checking it again with another preset.
 *     This was requested in https://github.com/wled/WLED/issues/1044
 *   slider 5 is for the rate at which the LEDs will fade away (if set to 0, they will immediately change to the background color; this is the original Dissolve FX rate)
 *   checkbox 1 is to select random colors
 *   checkbox 2 is to force it to wait until all LEDs have been completely filled or dissolved
 *   checkbox 3 is to select whether one last LED will stay lit (like a "last one standing" or "sole survivor")
 *   aux0: 3 packed values: phase/stage of dissolve/refill process, whether done dissolving/refilling, and previous value of lastOneMode
 *   aux1: random survivor pixel index
 */
#define DISSOLVE_PHASE          (SEGENV.aux0 & 0xFF)
#define DISSOLVE_DONE           ((SEGENV.aux0 >> 8) & 0x01)
#define DISSOLVE_PREV_LAST_ONE  ((SEGENV.aux0 >> 9) & 0x01)
#define SET_PHASE(p)            (SEGENV.aux0 = (SEGENV.aux0 & 0xFF00) | (p))
#define SET_DONE(d)             (SEGENV.aux0 = (SEGENV.aux0 & 0xFEFF) | ((d) << 8))
#define SET_PREV_LAST_ONE(d)    (SEGENV.aux0 = (SEGENV.aux0 & 0xFDFF) | ((d) << 9))

static void mode_dissolveplus(void) {
  unsigned dataSize = sizeof(uint32_t) * (SEGLEN + 1);
  if (!SEGENV.allocateData(dataSize)) FX_FALLBACK_STATIC; //allocation failed
  uint32_t* pixels = reinterpret_cast<uint32_t*>(SEGENV.data);
  uint32_t& storedBg = pixels[SEGLEN];

  constexpr unsigned PHASE_FILL = 0;
  constexpr unsigned PHASE_DISSOLVE = 1;
  constexpr unsigned PHASE_FILL_SURVIVOR = 2;
  constexpr unsigned PHASE_DISSOLVE_SURVIVOR = 3;
  constexpr unsigned PHASE_PAUSE_SURVIVOR = 4;

  bool lastOneMode = SEGMENT.check3;

  if (SEGENV.call == 0) {
    for (unsigned i = 0; i < SEGLEN; i++) pixels[i] = SEGCOLOR(1);
    storedBg = SEGCOLOR(1);
    SET_PHASE(PHASE_DISSOLVE);
    SEGENV.aux1 = hw_random16(SEGLEN);
    SET_DONE(0);
    SET_PREV_LAST_ONE(lastOneMode ? 1 : 0);
  } else if (storedBg != SEGCOLOR(1)) {
    for (unsigned i = 0; i < SEGLEN; i++) {
      if (pixels[i] == storedBg) pixels[i] = SEGCOLOR(1);
    }
    storedBg = SEGCOLOR(1);
  }

  // Restart if lastOneMode changed to true
  if ((bool)DISSOLVE_PREV_LAST_ONE != lastOneMode) {
    if (lastOneMode) {
      SET_PHASE(PHASE_DISSOLVE);
      unsigned attempts = 0;
      do {
        SEGENV.aux1 = hw_random16(SEGLEN);
        attempts++;
      } while (pixels[SEGENV.aux1] == SEGCOLOR(1) && attempts < SEGLEN);
    } else {
      if (DISSOLVE_PHASE == PHASE_DISSOLVE_SURVIVOR) {
        SET_PHASE(PHASE_DISSOLVE);
      } else if (DISSOLVE_PHASE == PHASE_FILL_SURVIVOR) {
        SET_PHASE(PHASE_FILL);      
      }
    }
    SET_DONE(0);
    SEGENV.step = 0;
    SET_PREV_LAST_ONE(lastOneMode ? 1 : 0);
  }

  // Phase 4: pause and keep only one pixel lit
  if (DISSOLVE_PHASE == PHASE_PAUSE_SURVIVOR) {
    uint16_t lastOneDelay = SEGMENT.custom2 << 1;
    if (lastOneDelay < 1) lastOneDelay = 1;
    bool freezeForever = lastOneMode && SEGMENT.custom2 == 255;
    if (freezeForever) {
      SEGENV.step++;
      return;
    }
    if (SEGENV.step >= lastOneDelay) {
      SEGENV.step = 0;
      SET_PHASE(PHASE_FILL_SURVIVOR);
      SET_DONE(0);
    } else {
      SEGENV.step++;
    }
    for (unsigned i = 0; i < SEGLEN; i++)
      SEGMENT.setPixelColor(i, pixels[i]);
    return;
  }

  bool filling = (DISSOLVE_PHASE == PHASE_FILL || DISSOLVE_PHASE == PHASE_FILL_SURVIVOR);
  bool forceComplete = lastOneMode && (DISSOLVE_PHASE == PHASE_FILL_SURVIVOR || DISSOLVE_PHASE == PHASE_DISSOLVE_SURVIVOR);

  for (unsigned j = 0; j <= SEGLEN / 15; j++) {
    if (hw_random8() <= (filling ? SEGMENT.custom1 : SEGMENT.intensity)) { // set the fill or dissolve speed
      for (size_t times = 0; times < 10; times++) { //attempt to spawn a new pixel 10 times
        unsigned i = hw_random16(SEGLEN);
        if (lastOneMode && DISSOLVE_PHASE == PHASE_DISSOLVE_SURVIVOR && i == SEGENV.aux1) continue;

        if (filling) { // fill with primary/palette color
          if (pixels[i] == storedBg) {
            uint32_t c;
            if (SEGMENT.check1) {
              uint8_t pId = SEGMENT.palette;
              c = (pId == 0) ? SEGMENT.color_wheel(hw_random8()) : SEGMENT.color_from_palette(hw_random16(SEGLEN), true, PALETTE_SOLID_WRAP, 0);
              if (c == SEGCOLOR(1)) c ^= 0x00000001;  // flip the last bit to make sure it is slightly different than the background color
              pixels[i] = c;
            } else {
              c = SEGMENT.color_from_palette(i, true, PALETTE_SOLID_WRAP, 0);
              if (c == SEGCOLOR(1)) c ^= 0x00000001;
              pixels[i] = c;
            }
            break;
          }
        } else {  //dissolve to secondary/background color
          if (pixels[i] != storedBg) {
            uint8_t fadeRate = SEGENV.custom3;  // (slider values are 0 -> 31)
            if (fadeRate > 0) {  // fade progressively towards the background color by the fadeRate value
              uint32_t c = color_blend(pixels[i], SEGCOLOR(1), fadeRate << 2);
              pixels[i] = c;
            } else {  // quickly fade to the background color if 0 is selected on custom3 slider)
              pixels[i] = storedBg;
            }
            break;
          }
        }
      }
    }
  }

  if (lastOneMode && (DISSOLVE_PHASE == PHASE_FILL_SURVIVOR || DISSOLVE_PHASE == PHASE_DISSOLVE_SURVIVOR)) {
    if (pixels[SEGENV.aux1] == SEGCOLOR(1)) {
      uint32_t c;
      if (SEGMENT.check1) {
        uint8_t pId = SEGMENT.palette;
        c = (pId == 0) ? SEGMENT.color_wheel(hw_random8()) : SEGMENT.color_from_palette(hw_random16(SEGLEN), true, PALETTE_SOLID_WRAP, 0);
      } else {
        c = SEGMENT.color_from_palette(SEGENV.aux1, true, PALETTE_SOLID_WRAP, 0);
      }
      if (c == SEGCOLOR(1)) c ^= 0x00000001;
      pixels[SEGENV.aux1] = c;
    }
  }

  unsigned incompletePixels = 0;
  for (unsigned i = 0; i < SEGLEN; i++) {
    SEGMENT.setPixelColor(i, pixels[i]); // fix for #4401
    if (SEGMENT.check2 || forceComplete) {
      if (lastOneMode && DISSOLVE_PHASE == PHASE_DISSOLVE_SURVIVOR && i == SEGENV.aux1) continue;
      if (filling) {
        if (pixels[i] == storedBg) incompletePixels++;
      } else {
        if (pixels[i] != storedBg) incompletePixels++;
      }
    }
  }

  if ((SEGMENT.check2 || forceComplete) && incompletePixels == 0 && !DISSOLVE_DONE) {
    SET_DONE(1);
    SEGENV.step = 0;
  }

  bool stepReady = (DISSOLVE_DONE || (!SEGMENT.check2 && !forceComplete)) && SEGENV.step > (255 - SEGMENT.speed) + 15U;

  if (stepReady) {
    SEGENV.step = 0;
    SET_DONE(0);
    if (!lastOneMode) {
      SET_PHASE(DISSOLVE_PHASE == PHASE_FILL ? PHASE_DISSOLVE : PHASE_FILL);
    } else {
      switch (DISSOLVE_PHASE) {
        case PHASE_FILL: SET_PHASE(PHASE_DISSOLVE); break;
        case PHASE_DISSOLVE: SET_PHASE(PHASE_FILL_SURVIVOR); break;
        case PHASE_FILL_SURVIVOR: {
          unsigned attempts = 0;
          do {
            SEGENV.aux1 = hw_random16(SEGLEN);
            attempts++;
          } while (pixels[SEGENV.aux1] == SEGCOLOR(1) && attempts < SEGLEN);
          SET_PHASE(PHASE_DISSOLVE_SURVIVOR);
          break;
        }
        case PHASE_DISSOLVE_SURVIVOR: SET_PHASE(PHASE_PAUSE_SURVIVOR); break;
        case PHASE_PAUSE_SURVIVOR: SET_PHASE(PHASE_FILL_SURVIVOR); break;
      }
    }
  } else {
    SEGENV.step++;
  }
}
#undef DISSOLVE_PHASE
#undef DISSOLVE_DONE
#undef DISSOLVE_PREV_LAST_ONE
#undef SET_PHASE
#undef SET_DONE
#undef SET_PREV_LAST_ONE

static const char _data_FX_MODE_DISSOLVEPLUS[] PROGMEM = "Dissolve Plus@Repeat speed,Dissolve speed,Fill speed,Last one delay,Fade rate,Random,Complete,Last one;!,!;!;;o2=1";




/*
 * Nokia-style Growing Snake with Food + Crash/Restart
 * - Starts at 5 LEDs
 * - Food appears 6–9 LEDs ahead
 * - Eating food makes the snake grow by +2
 * - Nice fade on the tail
 * - Runs continuously and wraps around
 * - When snake becomes too long → crashes and restarts at 5
 */
static void mode_nokia_snake(void) {

  // ----- Timing -----
  uint16_t cycleTime = 35 + ((255 - SEGMENT.speed) * 3);
  uint32_t now = strip.now;

  if (now - SEGENV.step < cycleTime) return;
  SEGENV.step = now;

  // ----- Persistent state -----
  static uint16_t foodPos = 0;
  static bool foodActive = false;

  // First run or after crash
  if (SEGENV.call == 0 || SEGENV.aux1 == 0) {
    SEGENV.aux0 = 4;          // head position
    SEGENV.aux1 = 5;          // start length = 5
    foodActive = false;
  }

  uint16_t head = SEGENV.aux0;
  uint16_t len  = SEGENV.aux1;

  // ----- Spawn food if needed -----
  if (!foodActive) {
    uint8_t dist = 14 + (hw_random8() % 5);   // 14–18 LEDs ahead
    foodPos = (head + dist) % SEGLEN;
    foodActive = true;
  }

  // ----- Move head -----
  head = (head + 1) % SEGLEN;
  SEGENV.aux0 = head;

  // ----- Check if we ate the food -----
  if (foodActive && head == foodPos) {
    if (len < SEGLEN - 4) {
      len += 1;                 // grow by +1 per food eaten
      SEGENV.aux1 = len;
    }
    foodActive = false;
  }

  // ----- Crash condition -----
  // Restart when the snake gets very long (you can change the number)
  if (len > SEGLEN * 0.75) {   // crash when longer than 75% of the strip
    SEGENV.aux1 = 0;           // forces restart next frame
    return;
  }

  // ----- Draw everything -----
  SEGMENT.fill(SEGCOLOR(1));   // background

  // Draw snake: solid/sharp near the head, fading gradually toward the tail.
  // The solid (sharp) portion scales with snake length - a longer snake keeps
  // proportionally more of its body at full brightness, just like the classic game,
  // with only the trailing tip actually fading out.
  uint16_t sharpLen = (uint16_t)(len * 0.6f);
  if (sharpLen < 1) sharpLen = 1;
  uint16_t fadeSpan = (len > sharpLen) ? (len - sharpLen) : 1;

  for (uint16_t i = 0; i < len; i++) {
    uint16_t pos = (head - i + SEGLEN) % SEGLEN;

    uint32_t col;
    if (i == 0) {
      col = SEGCOLOR(0);                     // head marker, always full brightness
    } else {
      uint32_t baseColor = SEGMENT.palette
        ? SEGMENT.color_from_palette((i * 255) / len, false, PALETTE_SOLID_WRAP, 0)
        : SEGCOLOR(0);

      if (i < sharpLen) {
        col = baseColor;                     // solid body zone - no fade
      } else {
        float t = (float)(i - sharpLen) / (float)fadeSpan;
        float ease = 1.0f - t;
        ease = ease * ease;                  // quadratic ease-out for a smooth, natural fade
        uint8_t fade = (uint8_t)(ease * 230.0f) + 20; // floor at 20 so the tail doesn't vanish to black
        col = color_fade(baseColor, fade);
      }
    }
    SEGMENT.setPixelColor(pos, col);
  }

  // Draw food
  if (foodActive) {
    uint32_t foodCol = SEGCOLOR(2);
    if (foodCol == 0) foodCol = 0x00FF00;    // bright green fallback
    SEGMENT.setPixelColor(foodPos, foodCol);
  }
}

static const char _data_FX_MODE_NOKIA_SNAKE[] PROGMEM =
  "Z - Nokia Snake@Speed,!;!,!;!;01";


// ============================================================================
// Custom Chunchun for the LED harness
//
// IMPORTANT:
// This is intentionally based directly on WLED's original mode_chunchun()
// implementation. The animation math, timing, fading, bird count formula,
// sine-wave positioning, color calculation, and rendering are kept the same.
//
// The ONLY functional change is the coordinate system:
//   - normal WLED effects use the existing 292-LED logical segment + ledmap
//   - this effect uses a 410-position virtual path
//   - each virtual position is translated to a 292-LED logical position
//
// Therefore the existing ledmap.json is left untouched.
// ============================================================================

#define CHUNCHUN_HARNESS_PATH_LEN 410

// 410 virtual positions following the requested harness path:
//
//   Section 0 forward
//   Section 1 forward
//   Section 2 forward
//   Section 3 forward
//   Section 0 forward
//   Section 4 forward
//   Section 5 forward
//   Section 6 forward
//   Section 7 reverse (handled through the existing reversed ledmap)
//   Section 8 reverse (handled through the existing reversed ledmap)
//   Section 5 forward
//   Section 6 forward
//   Section 7 reverse (handled through the existing reversed ledmap)
//   Section 9 reverse (handled through the existing reversed ledmap)
//
// Values are LOGICAL LED indices. The normal WLED ledmap then converts
// those logical indices to the physical LEDs.
// IMPORTANT: sections 7, 8, and 9 are already physically reversed by the
// user's normal ledmap.json. Therefore, to make Chunchun travel those
// physical sections in REVERSE order, the virtual path uses their logical
// indices in FORWARD order. The ledmap supplies the physical reversal.
static const uint16_t chunchunHarnessPath[CHUNCHUN_HARNESS_PATH_LEN] PROGMEM = {
  // Section 0: 0..35
   0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,
  16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
  32, 33, 34, 35,

  // Section 1: 36..53
  36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51,
  52, 53,

  // Section 2: 54..93
  54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67,
  68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81,
  82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93,

  // Section 3: 94..119
  94, 95, 96, 97, 98, 99,100,101,102,103,104,105,106,107,
 108,109,110,111,112,113,114,115,116,117,118,119,

  // Section 0 again: 0..35
   0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,
  16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
  32, 33, 34, 35,

  // Section 4: 120..151
 120,121,122,123,124,125,126,127,128,129,130,131,132,133,134,135,
 136,137,138,139,140,141,142,143,144,145,146,147,148,149,150,151,

  // Section 5: 152..181
 152,153,154,155,156,157,158,159,160,161,162,163,164,165,166,167,
 168,169,170,171,172,173,174,175,176,177,178,179,180,181,

  // Section 6: 182..204
 182,183,184,185,186,187,188,189,190,191,192,193,194,195,196,197,
 198,199,200,201,202,203,204,

  // Section 7 reversed physically: logical indices must be FORWARD here
 205,206,207,208,209,210,211,212,213,214,215,216,217,218,219,220,
 221,222,223,224,225,226,227,228,229,230,231,232,233,234,

  // Section 8 reversed physically: logical indices must be FORWARD here
 235,236,237,238,239,240,241,242,243,244,245,246,247,248,249,250,
 251,252,253,254,255,256,

  // Section 5 again: 152..181
 152,153,154,155,156,157,158,159,160,161,162,163,164,165,166,167,
 168,169,170,171,172,173,174,175,176,177,178,179,180,181,

  // Section 6 again: 182..204
 182,183,184,185,186,187,188,189,190,191,192,193,194,195,196,197,
 198,199,200,201,202,203,204,

  // Section 7 again reversed physically: logical indices must be FORWARD here
 205,206,207,208,209,210,211,212,213,214,215,216,217,218,219,220,
 221,222,223,224,225,226,227,228,229,230,231,232,233,234,

  // Section 9 reversed physically: logical indices must be FORWARD here
 257,258,259,260,261,262,263,264,265,266,267,268,269,270,271,272,
 273,274,275,276,277,278,279,280,281,282,283,284,285,286,287,288,
 289,290
};


// This is WLED's original Chunchun effect with the virtual-path coordinate
// substitution described above. Compare against WLED's stock mode_chunchun():
//
//   SEGMENT.fade_out(254);
//   counter = strip.now * (6 + (SEGMENT.speed >> 4));
//   numBirds = 2 + (SEGLEN >> 3);
//   span = (SEGMENT.intensity << 8) / numBirds;
//   counter -= span;
//   megumin = sin16_t(counter) + 0x8000;
//   bird = uint32_t(megumin * SEGLEN) >> 16;
//   constrain(...);
//   color_from_palette(...);
//   setPixelColor(...);
//
// The original 292-LED SEGLEN is replaced by the 410-position virtual
// harness path only where it defines the animation coordinate system.
static void mode_chunchun_harness(void)
{
  if (SEGLEN <= 1) FX_FALLBACK_STATIC;

  // Original WLED Chunchun line -- unchanged.
  SEGMENT.fade_out(254); // add a bit of trail

  // Original WLED Chunchun line -- unchanged.
  unsigned counter = strip.now * (6 + (SEGMENT.speed >> 4));

  // Original formula, but applied to the 410-position virtual path.
  unsigned numBirds = 2 + (CHUNCHUN_HARNESS_PATH_LEN >> 3);

  // Original WLED Chunchun line -- unchanged.
  unsigned span = (SEGMENT.intensity << 8) / numBirds;

  for (unsigned i = 0; i < numBirds; i++)
  {
    // Original WLED Chunchun line -- unchanged.
    counter -= span;

    // Original WLED Chunchun line -- unchanged.
    unsigned megumin = sin16_t(counter) + 0x8000;

    // Original calculation, with the virtual path length substituted for
    // the physical/logical segment length.
    unsigned bird = uint32_t(megumin * CHUNCHUN_HARNESS_PATH_LEN) >> 16;

    bird = constrain(bird, 0U, CHUNCHUN_HARNESS_PATH_LEN - 1U);

    // Translate the virtual bird position into the normal 292-LED logical
    // coordinate space. The existing ledmap.json then performs the physical
    // mapping exactly as it does for every other WLED effect.
    uint16_t logicalBird = pgm_read_word(&chunchunHarnessPath[bird]);

    // Original WLED Chunchun color calculation -- unchanged.
    SEGMENT.setPixelColor(
      logicalBird,
      SEGMENT.color_from_palette(
        (i * 255) / numBirds,
        false,
        false,
        0
      )
    );
  }
}

static const char _data_FX_MODE_CHUNCHUN_HARNESS[] PROGMEM =
  "Z - Chunchun Harness@!,Gap size;!,!;!";


// ============================================================================
// Z - Harness Visual Suite
//
// All effects below use the same 410-position virtual harness path as the
// custom Chunchun effect above. The path is expressed in the normal logical
// LED coordinate space so the existing one-to-one ledmap.json remains intact.
// In particular, Sections 7, 8 and 9 are supplied in logical FORWARD order;
// the existing ledmap performs their physical reversal.
//
// The effects intentionally use the full topology rather than treating the
// strip as a simple 1D line. Repeated sections therefore participate in the
// animation as branches/rejoins while normal WLED effects remain untouched.
// ============================================================================

static inline uint16_t zh_path_led(uint16_t p)
{
  return pgm_read_word(&chunchunHarnessPath[p % CHUNCHUN_HARNESS_PATH_LEN]);
}

static inline uint16_t zh_wrap(uint32_t x)
{
  return (uint16_t)(x % CHUNCHUN_HARNESS_PATH_LEN);
}

static inline uint16_t zh_cyclic_distance(uint16_t a, uint16_t b)
{
  uint16_t d = (a > b) ? (a - b) : (b - a);
  uint16_t other = CHUNCHUN_HARNESS_PATH_LEN - d;
  return d < other ? d : other;
}

static inline uint8_t zh_tri(uint16_t distance, uint16_t width)
{
  if (distance >= width) return 0;
  return (uint8_t)(255U - ((uint32_t)distance * 255U / width));
}

static inline uint8_t zh_soft(uint16_t distance, uint16_t width)
{
  if (distance >= width) return 0;
  uint32_t x = 255U - ((uint32_t)distance * 255U / width);
  return (uint8_t)((x * x) >> 8);
}

static inline uint32_t zh_palette(uint16_t hue, uint8_t brightness)
{
  uint32_t c = SEGMENT.color_from_palette((uint8_t)(hue >> 8), true, true, 0);
  return color_fade(c, brightness);
}

// ---------------------------------------------------------------------------
// Z - Neural Pulse
// Multiple pulses propagate through the harness topology. Different pulse
// velocities create apparent splitting/recombination when the virtual path
// revisits a physical section.
// ---------------------------------------------------------------------------
static void mode_z_neural_pulse(void)
{
  if (SEGLEN <= 1) FX_FALLBACK_STATIC;
  SEGMENT.fade_out(238);

  const uint32_t t = strip.now;
  const uint16_t L = CHUNCHUN_HARNESS_PATH_LEN;
  const uint16_t width = 9 + (SEGMENT.intensity >> 5);
  const uint16_t p0 = zh_wrap((t * (2 + (SEGMENT.speed >> 5))) / 8U);
  const uint16_t p1 = zh_wrap((t * (3 + (SEGMENT.speed >> 6))) / 11U + 137U);
  const uint16_t p2 = zh_wrap((t * (1 + (SEGMENT.speed >> 6))) / 6U + 276U);

  for (uint16_t p = 0; p < L; p++) {
    uint8_t b0 = zh_soft(zh_cyclic_distance(p, p0), width);
    uint8_t b1 = zh_soft(zh_cyclic_distance(p, p1), width + 3);
    uint8_t b2 = zh_soft(zh_cyclic_distance(p, p2), width + 5);
    uint8_t b = max(b0, max(b1, b2));
    if (b < 3) continue;

    uint16_t h = (uint16_t)(p * 65535UL / L) + (uint16_t)(t * 19U);
    if (b1 > b && b1 >= b0 && b1 >= b2) h += 21000;
    else if (b2 > b0 && b2 >= b1) h += 43000;

    uint8_t boosted = qadd8(b, (uint8_t)(SEGMENT.intensity >> 2));
    SEGMENT.setPixelColor(zh_path_led(p), zh_palette(h, boosted));
  }
}

static const char _data_FX_MODE_Z_NEURAL_PULSE[] PROGMEM =
  "Z - Neural Pulse@Speed,Energy,Width,Chaos;!,!;!;01";

// ---------------------------------------------------------------------------
// Z - Plasma Veins
// Several low-frequency fields interfere to create continuously morphing
// plasma. It is intentionally smooth and organic rather than a simple chase.
// ---------------------------------------------------------------------------
static void mode_z_plasma_veins(void)
{
  if (SEGLEN <= 1) FX_FALLBACK_STATIC;
  const uint32_t t = strip.now;
  const uint16_t L = CHUNCHUN_HARNESS_PATH_LEN;

  for (uint16_t p = 0; p < L; p++) {
    uint16_t x = (uint32_t)p * 65535UL / L;
    uint16_t w1 = x * 2U + (uint16_t)(t * (1 + (SEGMENT.speed >> 6)));
    uint16_t w2 = x * 5U - (uint16_t)(t * 2U);
    uint16_t w3 = x * 11U + (uint16_t)(t * 3U);
    uint8_t f1 = (uint8_t)((sin16_t(w1) + 32768) >> 8);
    uint8_t f2 = (uint8_t)((sin16_t(w2) + 32768) >> 8);
    uint8_t f3 = (uint8_t)((sin16_t(w3) + 32768) >> 8);
    uint8_t field = (uint8_t)(((uint16_t)f1 * 110U + (uint16_t)f2 * 90U + (uint16_t)f3 * 55U) / 255U);
    uint8_t pulse = qadd8(field, (uint8_t)(sin16_t(x + t * 5U) >> 9));
    uint8_t brightness = qadd8(18, scale8(pulse, 220));
    brightness = qadd8(brightness, SEGMENT.intensity >> 3);
    uint16_t hue = x + (uint16_t)(f2 * 180U) + (uint16_t)(t * 9U);
    SEGMENT.setPixelColor(zh_path_led(p), zh_palette(hue, brightness));
  }
}

static const char _data_FX_MODE_Z_PLASMA_VEINS[] PROGMEM =
  "Z - Plasma Veins@Speed,Plasma,Glow,Drift;!,!;!;01";

// ---------------------------------------------------------------------------
// Z - Electric Organism
// Mostly dark, with stochastic electrical impulses. A deterministic moving
// field is combined with real hardware randomness so successive cycles do
// not look identical.
// ---------------------------------------------------------------------------
static void mode_z_electric_organism(void)
{
  if (SEGLEN <= 1) FX_FALLBACK_STATIC;
  SEGMENT.fade_out(218);

  if (SEGENV.call == 0) {
    SEGENV.aux0 = 0;
    SEGENV.aux1 = hw_random16() % CHUNCHUN_HARNESS_PATH_LEN;
  }

  if (SEGENV.aux0 > 0) SEGENV.aux0--;
  if (SEGENV.aux0 == 0) {
    SEGENV.aux1 = hw_random16() % CHUNCHUN_HARNESS_PATH_LEN;
    SEGENV.aux0 = 10 + (hw_random8() % (45 + ((255 - SEGMENT.speed) >> 3)));
  }

  const uint16_t head = SEGENV.aux1 % CHUNCHUN_HARNESS_PATH_LEN;
  const uint16_t width = 4 + (SEGMENT.intensity >> 6);
  const uint32_t t = strip.now;
  for (uint16_t p = 0; p < CHUNCHUN_HARNESS_PATH_LEN; p++) {
    uint8_t b = zh_soft(zh_cyclic_distance(p, head), width);
    if (b < 4) continue;
    uint16_t h = (uint16_t)(t * 45U) + (uint16_t)(p * 401U);
    uint8_t flicker = 180 + (hw_random8() >> 4);
    b = scale8(b, flicker);
    uint32_t c = zh_palette(h, b);
    if (b > 220) c = color_fade(0xFFFFFFFF, b);
    SEGMENT.setPixelColor(zh_path_led(p), c);
  }

  // A synchronized electrical discharge every so often.
  if ((t / 900U) % 7U == 3U) {
    uint8_t flash = (uint8_t)(255U - ((t % 900U) * 255U / 900U));
    if (flash > 190) {
      uint16_t center = (uint16_t)(CHUNCHUN_HARNESS_PATH_LEN / 2U);
      for (uint16_t p = 0; p < CHUNCHUN_HARNESS_PATH_LEN; p++) {
        uint8_t b = zh_soft(zh_cyclic_distance(p, center), 20);
        if (b > 0) SEGMENT.setPixelColor(zh_path_led(p), color_fade(0xFFFFFFFF, scale8(b, flash)));
      }
    }
  }
}

static const char _data_FX_MODE_Z_ELECTRIC_ORGANISM[] PROGMEM =
  "Z - Electric Organism@Speed,Voltage,Arc width,Chaos;!,!;!;01";

// ---------------------------------------------------------------------------
// Z - Gravity Well
// Energy appears to fall toward a central attractor, accelerates into it,
// then erupts outward as a luminous shockwave.
// ---------------------------------------------------------------------------
static void mode_z_gravity_well(void)
{
  if (SEGLEN <= 1) FX_FALLBACK_STATIC;
  const uint16_t L = CHUNCHUN_HARNESS_PATH_LEN;
  const uint32_t cycle = 3000U - (uint32_t)SEGMENT.speed * 7U;
  const uint32_t phase = strip.now % (cycle < 900U ? 900U : cycle);
  const uint16_t center = (uint16_t)(L / 2U);
  const uint32_t convergeEnd = (cycle * 62U) / 100U;

  SEGMENT.fade_out(220);

  if (phase < convergeEnd) {
    uint16_t maxDist = L / 2U;
    uint16_t remaining = (uint16_t)(convergeEnd - phase);
    uint16_t d = (uint32_t)maxDist * remaining / convergeEnd;
    uint16_t a = zh_wrap(center + d);
    uint16_t b = zh_wrap(center + L - d);
    uint16_t width = 7 + (SEGMENT.intensity >> 5);

    for (uint16_t p = 0; p < L; p++) {
      uint8_t ba = zh_soft(zh_cyclic_distance(p, a), width);
      uint8_t bb = zh_soft(zh_cyclic_distance(p, b), width);
      uint8_t br = max(ba, bb);
      if (br < 3) continue;
      uint16_t h = (uint16_t)(phase * 27U) + (uint16_t)(p * 151U);
      SEGMENT.setPixelColor(zh_path_led(p), zh_palette(h, br));
    }
  } else {
    uint32_t shockPhase = phase - convergeEnd;
    uint32_t shockLen = (cycle > convergeEnd) ? (cycle - convergeEnd) : 1U;
    uint16_t radius = (uint32_t)(L / 2U) * shockPhase / shockLen;
    uint16_t width = 5 + (SEGMENT.intensity >> 6);
    for (uint16_t p = 0; p < L; p++) {
      uint16_t d = zh_cyclic_distance(p, center);
      uint16_t diff = (d > radius) ? (d - radius) : (radius - d);
      uint8_t br = zh_soft(diff, width);
      if (br < 3) continue;
      br = scale8(br, (uint8_t)(255U - (shockPhase * 150U / shockLen)));
      SEGMENT.setPixelColor(zh_path_led(p), zh_palette((uint16_t)(shockPhase * 31U), br));
    }
  }
}

static const char _data_FX_MODE_Z_GRAVITY_WELL[] PROGMEM =
  "Z - Gravity Well@Speed,Mass,Shock width,Color;!,!;!;01";

// ---------------------------------------------------------------------------
// Z - Wormhole
// A luminous point accelerates around the complete topology. Its trail is
// compressed near the head and stretched behind it, producing a tunnel-like
// sense of motion.
// ---------------------------------------------------------------------------
static void mode_z_wormhole(void)
{
  if (SEGLEN <= 1) FX_FALLBACK_STATIC;
  SEGMENT.fade_out(224);

  const uint16_t L = CHUNCHUN_HARNESS_PATH_LEN;
  const uint32_t cycle = 1800U - (uint32_t)SEGMENT.speed * 4U;
  const uint32_t c = cycle < 650U ? 650U : cycle;
  const uint32_t ph = strip.now % c;
  const uint16_t head = (uint32_t)L * ph * ph / (uint64_t)c / c;
  const uint16_t width = 18 + (SEGMENT.intensity >> 4);

  for (uint16_t p = 0; p < L; p++) {
    uint16_t d = zh_cyclic_distance(p, head);
    uint8_t br = zh_soft(d, width);
    if (br < 2) continue;
    uint8_t tail = (d < width / 2U) ? 255 : (uint8_t)(255U - (uint32_t)(d - width / 2U) * 180U / width);
    br = scale8(br, tail);
    uint16_t hue = (uint16_t)(ph * 43U) + (uint16_t)(d * 390U);
    if (d < 4) br = 255;
    SEGMENT.setPixelColor(zh_path_led(p), zh_palette(hue, br));
  }
}

static const char _data_FX_MODE_Z_WORMHOLE[] PROGMEM =
  "Z - Wormhole@Speed,Energy,Tunnel width,Color;!,!;!;01";

// ---------------------------------------------------------------------------
// Z - Rainbow Fracture
// A coherent chromatic wave periodically breaks into multiple waves, then
// locks back together. The fracture timing is smooth rather than a hard cut.
// ---------------------------------------------------------------------------
static void mode_z_rainbow_fracture(void)
{
  if (SEGLEN <= 1) FX_FALLBACK_STATIC;
  const uint32_t t = strip.now;
  const uint16_t L = CHUNCHUN_HARNESS_PATH_LEN;
  uint16_t phase = (uint16_t)(t * (2 + (SEGMENT.speed >> 5)));
  uint16_t fracture = (uint16_t)((sin16_t(t * 2U) + 32768) >> 1);

  for (uint16_t p = 0; p < L; p++) {
    uint16_t x = (uint32_t)p * 65535UL / L;
    uint16_t offset = (uint16_t)((uint32_t)fracture * sin16_t(x * 3U + t * 3U) / 32768L);
    uint16_t q = x + phase + offset;
    uint8_t wave = (uint8_t)((sin16_t(q) + 32768) >> 8);
    uint8_t sharp = wave > 128 ? (uint8_t)((wave - 128) * 2U) : (uint8_t)((128 - wave) * 2U);
    uint8_t br = qadd8(35, scale8(sharp, 210));
    br = qadd8(br, SEGMENT.intensity >> 3);
    uint16_t hue = q + (uint16_t)(t * 15U);
    SEGMENT.setPixelColor(zh_path_led(p), zh_palette(hue, br));
  }
}

static const char _data_FX_MODE_Z_RAINBOW_FRACTURE[] PROGMEM =
  "Z - Rainbow Fracture@Speed,Fracture,Saturation,Glow;!,!;!;01";

// ---------------------------------------------------------------------------
// Z - Starlight Nervous System
// Deep darkness with sparse stars, drifting constellation links, and rare
// coordinated neural flashes. Designed to look spectacular from a distance
// without continuously blasting the viewer with light.
// ---------------------------------------------------------------------------
static void mode_z_starlight(void)
{
  if (SEGLEN <= 1) FX_FALLBACK_STATIC;
  SEGMENT.fade_out(205);

  const uint32_t t = strip.now;
  const uint16_t L = CHUNCHUN_HARNESS_PATH_LEN;
  const uint16_t constellation = zh_wrap((t * (1 + (SEGMENT.speed >> 6))) / 20U);

  for (uint16_t p = 0; p < L; p++) {
    // Cheap integer hash: stable for a frame but changes as time advances.
    uint32_t h = (uint32_t)p * 1103515245UL + (t / 75U) * 12345UL + 0x9E3779B9UL;
    h ^= h >> 16;
    h *= 2246822519UL;
    h ^= h >> 13;
    uint8_t star = (uint8_t)(h >> 24);

    uint8_t br = 0;
    if (star > (245U - (SEGMENT.intensity >> 4))) br = 150 + (star >> 2);
    uint8_t link = zh_soft(zh_cyclic_distance(p, constellation), 13);
    br = qadd8(br, scale8(link, 110));
    if (br < 4) continue;

    uint16_t hue = 40500U + (uint16_t)(star * 70U) + (uint16_t)(t * 3U);
    uint32_t c = zh_palette(hue, br);
    if (br > 235) c = color_fade(0xFFFFFFFF, br);
    SEGMENT.setPixelColor(zh_path_led(p), c);
  }

  // A rare white neural flash crossing the full topology.
  if ((t / 1400U) % 9U == 4U) {
    uint16_t head = zh_wrap((t * 2U) / 5U);
    for (uint16_t p = 0; p < L; p++) {
      uint8_t br = zh_soft(zh_cyclic_distance(p, head), 5);
      if (br > 0) SEGMENT.setPixelColor(zh_path_led(p), color_fade(0xFFFFFFFF, br));
    }
  }
}

static const char _data_FX_MODE_Z_STARLIGHT[] PROGMEM =
  "Z - Starlight Nervous System@Speed,Stars,Flash,Drift;!,!;!;01";


// ============================================================================
// Z - Harness Visual Suite II
//
// 15 additional effects, all built on the same 410-position virtual harness
// path (CHUNCHUN_HARNESS_PATH_LEN / chunchunHarnessPath / zh_* helpers
// defined above). Nothing above this block is modified - these are pure
// additions. None of these are games; they are all ambient / dancing /
// generative light art intended to look as striking as possible across the
// full harness topology.
// ============================================================================

// ---------------------------------------------------------------------------
// Z - Phoenix Flight
// A blazing white-hot comet races continuously around the full harness,
// cooling from white through yellow to deep red along its tail, with
// crackling embers randomly spawning behind it.
// ---------------------------------------------------------------------------
static void mode_z_phoenix_flight(void)
{
  if (SEGLEN <= 1) FX_FALLBACK_STATIC;
  SEGMENT.fade_out(228);

  const uint32_t t = strip.now;
  const uint16_t L = CHUNCHUN_HARNESS_PATH_LEN;
  uint32_t cycle = 2600U - (uint32_t)SEGMENT.speed * 6U;
  if (cycle < 800U) cycle = 800U;
  const uint16_t head = zh_wrap(((uint64_t)t * L) / cycle);
  const uint16_t width = 12 + (SEGMENT.intensity >> 4);

  for (uint16_t p = 0; p < L; p++) {
    uint16_t d = zh_cyclic_distance(p, head);
    if (d >= width) continue;
    uint8_t br = zh_soft(d, width);
    uint16_t hue = 4000U + (uint16_t)(d * 40U); // white-yellow core cooling to deep red tail
    uint32_t col = zh_palette(hue, br);
    if (br > 225) col = color_fade(0xFFFFFFFF, br);
    SEGMENT.setPixelColor(zh_path_led(p), col);
  }

  for (uint8_t i = 0; i < 3; i++) {
    if (hw_random8() < 60) {
      uint16_t emberOffset = hw_random16(width, (uint16_t)(width * 4));
      uint16_t emberPos = zh_wrap((uint32_t)head + L - emberOffset);
      uint8_t emberBr = 100 + hw_random8(100);
      SEGMENT.setPixelColor(zh_path_led(emberPos), zh_palette((uint16_t)(3500 + hw_random16(2500)), emberBr));
    }
  }
}

static const char _data_FX_MODE_Z_PHOENIX_FLIGHT[] PROGMEM =
  "Z - Phoenix Flight@Speed,Blaze,Tail width,Embers;!,!;!;01";

// ---------------------------------------------------------------------------
// Z - Comet Storm
// Several independent comets, each with its own speed, direction and hue,
// race continuously around the full harness with soft triangular tails,
// producing a constantly-changing storm of streaking light.
// ---------------------------------------------------------------------------
struct ZComet { uint16_t pos; int16_t vel; uint16_t hue; };
constexpr uint8_t Z_MAX_COMETS = 6;

static void mode_z_comet_storm(void)
{
  if (SEGLEN <= 1) FX_FALLBACK_STATIC;
  unsigned dataSize = sizeof(ZComet) * Z_MAX_COMETS;
  if (!SEGENV.allocateData(dataSize)) FX_FALLBACK_STATIC;
  ZComet* comets = reinterpret_cast<ZComet*>(SEGENV.data);
  const uint16_t L = CHUNCHUN_HARNESS_PATH_LEN;

  if (SEGENV.call == 0) {
    for (uint8_t i = 0; i < Z_MAX_COMETS; i++) {
      comets[i].pos = hw_random16(L);
      comets[i].vel = (int16_t)((hw_random8(2) ? 1 : -1) * (int16_t)hw_random16(1, 4));
      comets[i].hue = hw_random16();
    }
  }

  SEGMENT.fade_out(220);
  const uint8_t speedScale = 1 + (SEGMENT.speed >> 5);
  const uint8_t numComets = 2 + (SEGMENT.intensity >> 6); // 2 - 5

  for (uint8_t i = 0; i < numComets && i < Z_MAX_COMETS; i++) {
    int32_t np = (int32_t)comets[i].pos + comets[i].vel * speedScale;
    while (np < 0) np += L;
    while (np >= (int32_t)L) np -= L;
    comets[i].pos = (uint16_t)np;

    const uint16_t tailLen = 14;
    int8_t dir = (comets[i].vel > 0) ? 1 : -1;
    for (uint16_t k = 0; k < tailLen; k++) {
      int32_t tp = (int32_t)comets[i].pos - dir * k;
      tp = ((tp % (int32_t)L) + (int32_t)L) % (int32_t)L;
      uint8_t br = zh_tri(k, tailLen);
      if (br < 4) continue;
      SEGMENT.setPixelColor(zh_path_led((uint16_t)tp), zh_palette(comets[i].hue, br));
    }
  }
}

static const char _data_FX_MODE_Z_COMET_STORM[] PROGMEM =
  "Z - Comet Storm@Speed,Comets,,;!,!;!;01";

// ---------------------------------------------------------------------------
// Z - Deep Current
// Slow undulating blue-green waves flow along the harness like light
// filtering through deep ocean water, with a fine high-frequency shimmer
// layered on top to suggest sunlight glinting through the surface above.
// ---------------------------------------------------------------------------
static void mode_z_deep_current(void)
{
  if (SEGLEN <= 1) FX_FALLBACK_STATIC;
  const uint32_t t = strip.now;
  const uint16_t L = CHUNCHUN_HARNESS_PATH_LEN;

  for (uint16_t p = 0; p < L; p++) {
    uint16_t x = (uint32_t)p * 65535UL / L;
    uint16_t flow = x + (uint16_t)(t * (1 + (SEGMENT.speed >> 6)));
    uint8_t wave = (uint8_t)((sin16_t(flow) + 32768) >> 8);
    uint8_t shimmer = (uint8_t)((sin16_t(x * 9U + t * 5U) + 32768) >> 11);
    uint8_t br = qadd8(20, scale8(wave, 150));
    br = qadd8(br, shimmer);
    br = qadd8(br, SEGMENT.intensity >> 4);
    uint16_t hue = 30000U + (uint16_t)(wave * 40U);
    SEGMENT.setPixelColor(zh_path_led(p), zh_palette(hue, br));
  }
}

static const char _data_FX_MODE_Z_DEEP_CURRENT[] PROGMEM =
  "Z - Deep Current@Speed,Waves,Shimmer,;!,!;!;01";

// ============================================================================
// Z - Harness Visual Suite III
//
// Five effects, deliberately slow. Every one of them is built from a real,
// named physical or mathematical phenomenon rather than an arbitrary
// formula, because that's what makes the emergent complexity trustworthy -
// each is "hard to have come up with" precisely because it wasn't invented,
// it was borrowed from how pendulums, waves, orbits and coherent noise
// actually behave, then translated onto the 410-position harness path.
//
// Design constraints followed throughout, based on photosensitive-epilepsy
// guidance (WCAG 2.3.1 / Ofcom / Epilepsy Foundation, which put the danger
// band at roughly 3-60 Hz with peak sensitivity around 15-20 Hz, and
// recommend venues stay under ~3-4 Hz even for brief flashes):
//   - No full-path brightness swing ever completes faster than ~1 second,
//     and most run far slower than that (multi-second periods).
//   - Every brightness change is a smooth sine/noise curve, never a hard
//     on/off step, and no effect relies on a synchronized whole-strip flash.
//   - Nothing here is locked to saturated red, the specific hue flagged as
//     highest-risk in flash guidance.
// ============================================================================

// ---------------------------------------------------------------------------
// Z - Tidal Interference
// The pattern a ripple tank makes: two or more point sources each radiate
// slow circular waves outward, and wherever those waves overlap they either
// reinforce (bright) or cancel (dark) depending on the difference in the
// distance traveled from each source. Three fixed sources are placed around
// the harness loop here, each radiating at a very slightly different, very
// low frequency, so the interference fringes themselves slowly rotate and
// breathe over time rather than sitting still - constantly-shifting moire
// bands with no motion faster than roughly one cycle every several seconds.
// ---------------------------------------------------------------------------
static void mode_z_tidal_interference(void)
{
  if (SEGLEN <= 1) FX_FALLBACK_STATIC;

  const uint32_t t = strip.now;
  const uint16_t L = CHUNCHUN_HARNESS_PATH_LEN;

  const uint16_t src0 = 0;
  const uint16_t src1 = L / 3U;
  const uint16_t src2 = (L * 2U) / 3U;
  const uint16_t k = 40U + (SEGMENT.intensity >> 2); // spatial wavelength
  const uint16_t w = 6U + (SEGMENT.speed >> 5);      // temporal frequency (slow)

  for (uint16_t p = 0; p < L; p++) {
    uint16_t d0 = zh_cyclic_distance(p, src0);
    uint16_t d1 = zh_cyclic_distance(p, src1);
    uint16_t d2 = zh_cyclic_distance(p, src2);

    int32_t s0 = sin16_t((uint16_t)(d0 * k - t * w));
    int32_t s1 = sin16_t((uint16_t)(d1 * k - t * (w + 1U)));
    int32_t s2 = sin16_t((uint16_t)(d2 * k - t * (w + 2U)));

    int32_t sum = s0 + s1 + s2; // range approx -98304..98301
    uint8_t br = (uint8_t)(((sum + 98304) * 255L) / 196608L);
    br = scale8(br, br); // compress toward dark so interference minima read as calm troughs
    if (br < 4) continue;
    uint16_t hue = 34000U + (uint16_t)(br * 60U) + (uint16_t)(t / 40U);
    SEGMENT.setPixelColor(zh_path_led(p), zh_palette(hue, br));
  }
}

static const char _data_FX_MODE_Z_TIDAL_INTERFERENCE[] PROGMEM =
  "Z - Tidal Interference@Speed,Wavelength,,;!,!;!;01";

// ---------------------------------------------------------------------------
// Z - Ember Drift
// Uses coherent (Perlin) gradient noise - the same smooth-noise technique
// already driving the Magma effect above, and the standard tool computer
// graphics has used for organic-looking motion since the 1980s - instead of
// a repeating sine wave. A single slowly-scrolling slice of 3D noise drives
// both brightness and a gentle hue drift, so the glow wanders continuously
// the way real embers or bioluminescent plankton drift on a slow current:
// never flashing, never exactly repeating, never visibly "looping."
// ---------------------------------------------------------------------------
static void mode_z_ember_drift(void)
{
  if (SEGLEN <= 1) FX_FALLBACK_STATIC;

  const uint32_t t = strip.now;
  const uint16_t L = CHUNCHUN_HARNESS_PATH_LEN;
  const uint16_t z = (uint16_t)((t * (2U + (SEGMENT.speed >> 5))) / 40U);
  const uint16_t xScale = 5U + (SEGMENT.intensity >> 5);

  for (uint16_t p = 0; p < L; p++) {
    uint16_t nx = p * xScale;
    uint8_t n = perlin8(nx, z, 0);
    uint8_t n2 = perlin8(nx + 4000U, z + 1700U, 900U);

    uint8_t br = scale8(n, n); // gentle compression toward dark, ember-like glow
    br = qadd8(br, SEGMENT.custom1 >> 4);
    if (br < 5) continue;
    uint16_t hue = 3500U + (uint16_t)(n2 * 90U);
    SEGMENT.setPixelColor(zh_path_led(p), zh_palette(hue, br));
  }
}

static const char _data_FX_MODE_Z_EMBER_DRIFT[] PROGMEM =
  "Z - Ember Drift@Speed,Grain,Floor,;!,!;!;01";

// ---------------------------------------------------------------------------
// Z - Orbital Waltz
// Two glowing bodies orbit the harness loop on independent periods chosen
// near a simple 2:3 ratio - an orbital resonance, the same kind of relation
// that locks Neptune and Pluto together - so they drift toward and away
// from conjunction slowly instead of lining up (or never meeting) every
// single lap. Each body's angular speed is warped with the "equation of
// center," theta = M + 2e*sin(M): the standard first-order approximation
// astronomers use for how a body on an elliptical orbit actually moves,
// gliding slowly through its far arc and quickening through its close arc.
// Nothing here is a plain constant-speed loop.
// ---------------------------------------------------------------------------
static void mode_z_orbital_waltz(void)
{
  if (SEGLEN <= 1) FX_FALLBACK_STATIC;
  SEGMENT.fade_out(232); // short glowing trail behind each body

  const uint32_t t = strip.now;
  const uint16_t L = CHUNCHUN_HARNESS_PATH_LEN;

  uint32_t periodBase = 9000U - (uint32_t)(SEGMENT.speed) * 24U; // ~3-9s per lap
  if (periodBase < 2600U) periodBase = 2600U;
  const uint16_t ecc = 6000U + ((uint16_t)SEGMENT.intensity << 5); // eccentricity strength

  for (uint8_t body = 0; body < 2; body++) {
    uint32_t period = (body == 0) ? periodBase : (periodBase * 3U) / 2U; // 2:3 resonance
    uint16_t M = (uint16_t)(((uint64_t)t * 65536U) / period);
    int32_t centerEq = ((int32_t)sin16_t(M) * (int32_t)ecc) >> 14; // ~2*e*sin(M)
    uint16_t theta = (uint16_t)((uint32_t)M + centerEq + (body * 32768U)); // start bodies opposed
    uint16_t pos = (uint16_t)(((uint32_t)theta * L) >> 16);

    uint16_t width = 10U + (SEGMENT.custom1 >> 4);
    int32_t hueBase = body == 0 ? 2800 : 40000;
    for (int16_t d = -(int16_t)width; d <= (int16_t)width; d++) {
      int32_t pp = (int32_t)pos + d;
      pp = ((pp % (int32_t)L) + (int32_t)L) % (int32_t)L;
      uint8_t br = zh_soft((uint16_t)abs(d), width);
      if (br < 4) continue;
      uint32_t col = zh_palette((uint16_t)(hueBase + d * 20), br);
      if (br > 220) col = color_fade(0xFFFFFFFF, br);
      SEGMENT.setPixelColor(zh_path_led((uint16_t)pp), col);
    }
  }
}

static const char _data_FX_MODE_Z_ORBITAL_WALTZ[] PROGMEM =
  "Z - Orbital Waltz@Speed,Eccentricity,Width,;!,!;!;01";




// ============================================================================
// Z - Harness Visual Suite IV
// Fifteen slow, high-quality effects designed for the 410-position virtual
// harness path. All motion is deliberately multi-second, smooth, and free of
// hard flashes. They reuse the existing zh_* helpers and chunchunHarnessPath
// so the topology (including repeated / reversed sections) is fully expressed.
// ============================================================================

// ---------------------------------------------------------------------------
// Z - Silk Ribbon
// A single luminous ribbon of variable width snakes through the entire
// topology with a gentle undulation. The ribbon never snaps; it breathes.
// ---------------------------------------------------------------------------
static void mode_z_silk_ribbon(void)
{
  if (SEGLEN <= 1) FX_FALLBACK_STATIC;
  SEGMENT.fade_out(40 + (SEGMENT.custom1 >> 3));

  const uint32_t t = strip.now;
  const uint16_t L = CHUNCHUN_HARNESS_PATH_LEN;
  uint16_t head = zh_wrap((t * (6U + (SEGMENT.speed >> 5))) / 32U);
  uint16_t width = 18U + (SEGMENT.intensity >> 3);

  for (int16_t d = -(int16_t)width; d <= (int16_t)width; d++) {
    uint16_t p = zh_wrap((uint32_t)head + d);
    uint8_t br = zh_soft((uint16_t)abs(d), width);
    if (br < 4) continue;
    uint8_t shimmer = (uint8_t)((sin16_t((uint16_t)(p * 40U + t * 3U)) + 32768) >> 10);
    br = qadd8(br, shimmer);
    uint16_t hue = 12000U + (uint16_t)(p * 20U) + (uint16_t)(t / 60U);
    SEGMENT.setPixelColor(zh_path_led(p), zh_palette(hue, br));
  }
}
static const char _data_FX_MODE_Z_SILK_RIBBON[] PROGMEM =
  "Z - Silk Ribbon@Speed,Width,Persistence,;!,!;!;01";

// ---------------------------------------------------------------------------
// Z - Twin Moons
// Two soft luminous bodies orbit the path on a near 3:2 resonance.
// Their overlapping glow creates slow, breathing bright zones.
// ---------------------------------------------------------------------------
static void mode_z_twin_moons(void)
{
  if (SEGLEN <= 1) FX_FALLBACK_STATIC;
  SEGMENT.fade_out(50);

  const uint32_t t = strip.now;
  const uint16_t L = CHUNCHUN_HARNESS_PATH_LEN;
  uint32_t period = 11000U - (uint32_t)SEGMENT.speed * 28U;
  if (period < 4000U) period = 4000U;

  for (uint8_t m = 0; m < 2; m++) {
    uint32_t pperiod = (m == 0) ? period : (period * 3U) / 2U;
    uint16_t pos = zh_wrap((uint32_t)((t * L) / pperiod) + m * (L / 2U));
    uint16_t width = 22U + (SEGMENT.intensity >> 3);

    for (uint16_t p = 0; p < L; p++) {
      uint8_t br = zh_soft(zh_cyclic_distance(p, pos), width);
      if (br < 4) continue;
      uint16_t hue = (m == 0) ? 8000U : 36000U;
      hue += (uint16_t)(t / 70U);
      SEGMENT.setPixelColor(zh_path_led(p), zh_palette(hue, br));
    }
  }
}
static const char _data_FX_MODE_Z_TWIN_MOONS[] PROGMEM =
  "Z - Twin Moons@Speed,Size,,;!,!;!;01";

// ---------------------------------------------------------------------------
// Z - Liquid Mercury
// Thick, reflective liquid flows slowly along the path with surface
// tension-like beading. High contrast yet completely smooth.
// ---------------------------------------------------------------------------
static void mode_z_liquid_mercury(void)
{
  if (SEGLEN <= 1) FX_FALLBACK_STATIC;
  const uint32_t t = strip.now;
  const uint16_t L = CHUNCHUN_HARNESS_PATH_LEN;
  const uint16_t flow = (uint16_t)(t * (3U + (SEGMENT.speed >> 5)) / 24U);

  for (uint16_t p = 0; p < L; p++) {
    uint16_t x = p + flow;
    uint8_t n = perlin8(x * 4U, 0, 0);
    n = qadd8(n, perlin8(x * 9U, 1000U, 0) / 3);
    uint8_t br = scale8(n, n);
    br = qadd8(br, SEGMENT.custom1 >> 5);
    if (br < 15) continue;
    uint16_t hue = 48000U + (uint16_t)(n * 20U);
    uint32_t c = zh_palette(hue, br);
    if (br > 200) c = color_fade(0xFFFFFFFF, br);
    SEGMENT.setPixelColor(zh_path_led(p), c);
  }
}
static const char _data_FX_MODE_Z_LIQUID_MERCURY[] PROGMEM =
  "Z - Liquid Mercury@Speed,Beading,Floor,;!,!;!;01";

// ---------------------------------------------------------------------------
// Z - Echo Chamber
// Soft pulses travel the path and leave long, decaying echoes that
// recombine when the topology revisits a physical section.
// ---------------------------------------------------------------------------
static void mode_z_echo_chamber(void)
{
  if (SEGLEN <= 1) FX_FALLBACK_STATIC;
  SEGMENT.fade_out(22 + (SEGMENT.custom1 >> 4));

  const uint32_t t = strip.now;
  const uint16_t L = CHUNCHUN_HARNESS_PATH_LEN;
  uint16_t head = zh_wrap((t * (5U + (SEGMENT.speed >> 5))) / 28U);
  uint16_t width = 14U + (SEGMENT.intensity >> 4);

  for (int16_t d = -(int16_t)width; d <= (int16_t)width; d++) {
    uint16_t p = zh_wrap((uint32_t)head + d);
    uint8_t br = zh_soft((uint16_t)abs(d), width);
    if (br < 5) continue;
    SEGMENT.setPixelColor(zh_path_led(p), zh_palette((uint16_t)(t / 40U), br));
  }

  uint16_t echo = zh_wrap(L - head / 2U);
  for (int16_t d = -10; d <= 10; d++) {
    uint16_t p = zh_wrap((uint32_t)echo + d);
    uint8_t br = zh_soft((uint16_t)abs(d), 11);
    br = scale8(br, 140);
    if (br < 5) continue;
    SEGMENT.setPixelColor(zh_path_led(p), zh_palette((uint16_t)(t / 40U + 20000U), br));
  }
}
static const char _data_FX_MODE_Z_ECHO_CHAMBER[] PROGMEM =
  "Z - Echo Chamber@Speed,Width,Persistence,;!,!;!;01";

// ---------------------------------------------------------------------------
// Z - Prism Cascade
// Soft colour bands cascade slowly along the path; each band has a
// different velocity so they shear and recombine beautifully.
// ---------------------------------------------------------------------------
static void mode_z_prism_cascade(void)
{
  if (SEGLEN <= 1) FX_FALLBACK_STATIC;
  const uint32_t t = strip.now;
  const uint16_t L = CHUNCHUN_HARNESS_PATH_LEN;

  for (uint16_t p = 0; p < L; p++) {
    uint8_t b0 = (uint8_t)((sin16_t((uint16_t)(p * 12U + t * 2U)) + 32768) >> 8);
    uint8_t b1 = (uint8_t)((sin16_t((uint16_t)(p * 19U - t * 3U)) + 32768) >> 8);
    uint8_t b2 = (uint8_t)((sin16_t((uint16_t)(p * 27U + t )) + 32768) >> 8);
    uint8_t br = scale8(qadd8(qadd8(b0, b1) / 2, b2 / 3), 190);
    br = qadd8(br, SEGMENT.custom1 >> 5);
    if (br < 8) continue;
    uint16_t hue = (uint16_t)(p * 40U + t / 40U);
    SEGMENT.setPixelColor(zh_path_led(p), zh_palette(hue, br));
  }
}
static const char _data_FX_MODE_Z_PRISM_CASCADE[] PROGMEM =
  "Z - Prism Cascade@Speed,Complexity,Floor,;!,!;!;01";

// ---------------------------------------------------------------------------
// Z - Gravity Lens
// Light appears to bend around two slowly moving masses, creating
// bright arcs and dark voids that drift across the topology.
// ---------------------------------------------------------------------------
static void mode_z_gravity_lens(void)
{
  if (SEGLEN <= 1) FX_FALLBACK_STATIC;
  const uint32_t t = strip.now;
  const uint16_t L = CHUNCHUN_HARNESS_PATH_LEN;

  uint16_t m1 = zh_wrap((t * 3U) / 40U);
  uint16_t m2 = zh_wrap((t * 5U) / 55U + L / 2U);
  uint16_t influence = 50U + (SEGMENT.intensity >> 2);

  for (uint16_t p = 0; p < L; p++) {
    uint16_t d1 = zh_cyclic_distance(p, m1);
    uint16_t d2 = zh_cyclic_distance(p, m2);
    uint16_t inv1 = (d1 < influence) ? (influence - d1) : 0;
    uint16_t inv2 = (d2 < influence) ? (influence - d2) : 0;
    uint8_t br = scale8(qadd8(inv1 * 2, inv2 * 2), 180);
    br = qadd8(br, SEGMENT.custom1 >> 5);
    if (br < 8) continue;
    uint16_t hue = 5000U + (uint16_t)(inv1 * 40U) + (uint16_t)(t / 90U);
    SEGMENT.setPixelColor(zh_path_led(p), zh_palette(hue, br));
  }
}
static const char _data_FX_MODE_Z_GRAVITY_LENS[] PROGMEM =
  "Z - Gravity Lens@Speed,Influence,Floor,;!,!;!;01";

// ---------------------------------------------------------------------------
// Z - Quiet Storm
// Distant, rolling energy fronts move through the harness. Brightness
// rises and falls over many seconds; colour is cool and restrained.
// ---------------------------------------------------------------------------
static void mode_z_quiet_storm(void)
{
  if (SEGLEN <= 1) FX_FALLBACK_STATIC;
  const uint32_t t = strip.now;
  const uint16_t L = CHUNCHUN_HARNESS_PATH_LEN;
  const uint16_t z = (uint16_t)(t / (45U + (255 - SEGMENT.speed) / 5U));

  for (uint16_t p = 0; p < L; p++) {
    uint8_t n = perlin8(p * 2U, z, 0);
    uint8_t n2 = perlin8(p * 6U + 3000U, z / 2U, 800U);
    uint8_t br = scale8(n, 140);
    br = qadd8(br, scale8(n2, 60));
    br = qadd8(br, SEGMENT.custom1 >> 4);
    if (br < 10) continue;
    uint16_t hue = 36000U + (uint16_t)(n2 * 25U);
    SEGMENT.setPixelColor(zh_path_led(p), zh_palette(hue, br));
  }
}
static const char _data_FX_MODE_Z_QUIET_STORM[] PROGMEM =
  "Z - Quiet Storm@Speed,Grain,Floor,;!,!;!;01";

// ---------------------------------------------------------------------------
// Z - Infinite Corridor
// Perspective-like bright rings travel along the path, giving a strong
// sense of depth and motion through a tunnel.
// ---------------------------------------------------------------------------
static void mode_z_infinite_corridor(void)
{
  if (SEGLEN <= 1) FX_FALLBACK_STATIC;
  const uint32_t t = strip.now;
  const uint16_t L = CHUNCHUN_HARNESS_PATH_LEN;
  const uint16_t speed = 8U + (SEGMENT.speed >> 4);
  const uint16_t spacing = 55U + (SEGMENT.intensity >> 2);

  for (uint16_t p = 0; p < L; p++) {
    uint16_t dist = (p + (t * speed) / 32U) % spacing;
    uint8_t br = zh_soft(dist, spacing / 3U);
    br = qadd8(br, SEGMENT.custom1 >> 5);
    if (br < 8) continue;
    uint16_t hue = 20000U + (uint16_t)(dist * 80U) + (uint16_t)(t / 100U);
    SEGMENT.setPixelColor(zh_path_led(p), zh_palette(hue, br));
  }
}
static const char _data_FX_MODE_Z_INFINITE_CORRIDOR[] PROGMEM =
  "Z - Infinite Corridor@Speed,Spacing,Floor,;!,!;!;01";


/////////////////
//  UserMod Class  //
/////////////////////

// ============================================================================
// HAT FX PACK — 144 LEDs
// Designed for this physical path:
//   top row:    LED 1..51, front center at LED 25
//   connector:  LED 52..93, rear center at LED 71 (index 70)
//   bottom row: LED 94..144, front center at LED 117
//
// WLED pixel indexes are zero-based in code. This pack addresses the physical
// 144-LED segment directly and does NOT use the old 410-position harness map.
// Add these functions before UserFxUsermod, then add the five registrations
// shown at the bottom of this file inside UserFxUsermod::setup().
// ============================================================================

static inline uint16_t hat_wrap(int32_t p) {
  while (p < 0) p += 144;
  return (uint16_t)(p % 144);
}

static inline uint32_t hat_palette(uint8_t index, uint8_t bri = 255) {
  uint32_t c = SEGMENT.color_from_palette(index, true, true, 0);
  return color_fade(c, bri);
}

static inline uint8_t hat_falloff(uint16_t d, uint16_t width) {
  if (d >= width) return 0;
  uint16_t x = 255U - ((uint32_t)d * 255U / width);
  return (uint8_t)((x * x) >> 8);
}

static inline uint16_t hat_dist(uint16_t a, uint16_t b) {
  return (a > b) ? (a - b) : (b - a);
}

// 1) Front Brake — fast along the strip, eases near each front-center LED,
// then accelerates again. A bright comet head leaves a soft tail.
// 2) Twin Front Comets — matching comets launch from the front centers and
// travel toward opposite sides of both rows, giving a mirrored two-row look.


// 3) Liquid Aurora — slow flowing color bands across both rows, with a
// stronger luminous pool at the two front-center LEDs. Smooth, non-flashing.


// 4) Plasma Fork — a hot pulse travels along the top row, splits across the
// front into the bottom row, and sends a secondary ripple around the rear arc.


// 5) Front-Center Ripple — concentric-looking waves radiate away from both
// front-center points along the two visible rows. The rear connector glows
// with a complementary traveling wave.
static void mode_hat_front_ripple(void) {
  if (SEGLEN < 144) { FX_FALLBACK_STATIC; }
  uint32_t t = strip.now;
  float speed = 0.004f + SEGMENT.speed * 0.000018f;
  for (uint16_t i = 0; i < 144; i++) {
    float distTop = fabsf((float)i - 24.0f);
    float distBottom = fabsf((float)i - 116.0f);
    float dist = min(distTop, distBottom);
    float wave = 0.5f + 0.5f * sinf(dist * 0.42f - t * speed);
    float crest = powf(wave, 5.0f);
    uint8_t bri = (uint8_t)(18 + crest * 235.0f);
    uint8_t hue = (uint8_t)(t / 40 + dist * 3.0f);
    SEGMENT.setPixelColor(i, hat_palette(hue, bri));
  }
  // Give the rear connector its own flowing wave so it doesn't look detached.
  for (int i = 51; i <= 92; i++) {
    float x = (float)(i - 51) / 41.0f;
    float wave = 0.5f + 0.5f * sinf(x * 12.566f - t * speed * 0.7f);
    SEGMENT.setPixelColor(i, hat_palette((uint8_t)(160 + t / 55 + i), (uint8_t)(30 + wave * 115)));
  }
  hat_rear_stage((uint8_t)(t/46),204);
}





static const char _data_FX_MODE_HAT_FRONT_RIPPLE[] PROGMEM =
  "Hat - Front-Center Ripple@Speed,Wave width,,,,;;";


// ============================================================================
// HAT FX PACK II — 144 LEDs
//
// Geometry (zero-based indexes used in code):
//   Top row    : idx   0..50   (LED   1..51), front center idx  24 (LED  25)
//   Connector  : idx  51..92   (LED  52..93), rear center idx 70 (LED 71), the rear arc between the rows
//   Bottom row : idx  93..143  (LED  94..144), front center idx 116 (LED 117)
//
// Both rows run in the SAME direction: side -> front center -> other side.
// Row-local x=0..50: front = x24 upper (LED25), x23 lower (LED117).
//   top pixel    = HAT_TOP_BASE + x
//   bottom pixel = HAT_BOT_BASE + x
//
// "Brake" effects (first five) share one speed profile: fast leaving the
// side (x = 0), easing down near the physical front center (top x24,
// bottom x23), then accelerating again to the far side (x = 50). The profile is
// integrated, so the head genuinely spends more TIME near the center.
//
// Everything is time-derived (stateless) unless noted, and all brightness
// changes are smooth - nothing here strobes.
// ============================================================================

#define HAT_ROW_LEN   51
#define HAT_TOP_BASE  0
#define HAT_BOT_BASE  93
#define HAT_CENTER_X  24.0f
// True physical LED centers, with distinct local positions for the two arcs.
#define HAT_TOP_CENTER 24
#define HAT_BOTTOM_CENTER 23
#define HAT_BACK_CENTER 70
static inline int16_t hat_row_center(uint8_t row) { return row ? HAT_BOTTOM_CENTER : HAT_TOP_CENTER; }
static inline int16_t hat_front_pixel(uint8_t row) { return row ? 116 : 24; }
static inline uint8_t hat_rear_peak(int16_t d,uint8_t radius){
  if(d<0)d=-d;
  if(d>=radius || radius==0)return 0;
  uint16_t q=255U-((uint16_t)d*255U/radius);
  return (uint8_t)((q*q)>>8);
}
static inline uint8_t hat3_sin8(uint8_t phase);
// Composed rear movement: two lanes meet at physical LED 71, trade directions,
// and unfold again. This fills the full connector without a permanently lit
// center. Brightness is low between accents, high on traveling strokes.
static void hat_rear_stage(uint8_t hue,uint8_t gain) {
  uint8_t phase=(uint8_t)(strip.now/24U);
  uint8_t travel=(uint8_t)((uint16_t)hat3_sin8(phase)*20U/255U);
  for(int16_t i=51;i<=92;i++){
    int16_t d=i-HAT_BACK_CENTER;
    uint8_t moving=qadd8(hat_rear_peak(d-(int16_t)travel,4),
                         hat_rear_peak(d+(int16_t)travel,4));
    uint8_t counter=hat_rear_peak(d-(int16_t)(20-travel),3);
    uint8_t shimmer=scale8(hat3_sin8((uint8_t)(i*19+phase*2)),42);
    uint8_t luminance=scale8(qadd8(moving,scale8(counter,145)),gain);
    luminance=qadd8(luminance,shimmer);
    uint32_t col=hat_palette((uint8_t)(hue+d*6+phase/4),luminance);
    uint32_t old=SEGMENT.getPixelColor(i);
    SEGMENT.setPixelColor(i,RGBW32(
      (R(old)>R(col))?R(old):R(col),
      (G(old)>G(col))?G(old):G(col),
      (B(old)>B(col))?B(old):B(col),0));
  }
}


static inline void hat_add(uint16_t i, uint32_t c) {
  SEGMENT.setPixelColor(i, color_add(SEGMENT.getPixelColor(i), c, false));
}

// Relative speed (0.15 .. 1.0) at row position x. Slowest at the front center.
static inline float hat_brake_speed(float x) {
  float n = 1.0f - fabsf(x - HAT_CENTER_X) / 14.0f;
  if (n < 0.0f) n = 0.0f;
  return 1.0f - 0.85f * n * n;
}

// Total "travel time" for one pass along a row (sum of 1/speed per LED).
static float hat_brake_total(void) {
  float total = 0.0f;
  for (int i = 0; i < HAT_ROW_LEN; i++) total += 1.0f / hat_brake_speed((float)i);
  return total;
}

// Convert uniform time phase u (0..1) into row position x (0..50) following
// the brake profile.
static float hat_brake_pos(float u) {
  float target = u * hat_brake_total();
  float acc = 0.0f;
  for (int i = 0; i < HAT_ROW_LEN; i++) {
    float step = 1.0f / hat_brake_speed((float)i);
    if (acc + step >= target) return (float)i + (target - acc) / step;
    acc += step;
  }
  return (float)(HAT_ROW_LEN - 1);
}

static inline uint32_t hat_brake_cycle(void) {
  return 4400U - (uint32_t)SEGMENT.speed * 14U;   // ~0.83 s .. 4.4 s per pass
}

static inline float hat_brake_phase(void) {
  uint32_t c = hat_brake_cycle();
  return (float)(strip.now % c) / (float)c;
}

// Draw a comet in one row. dir = +1 (tail behind toward x=0) or -1.
static void hat_row_comet(uint16_t base, float x, int dir, uint8_t hue, int tailLen, uint8_t gain = 255) {
  for (int k = 0; k <= tailLen; k++) {
    int xi = (int)roundf(x - (float)(dir * k));
    if (xi < 0 || xi >= HAT_ROW_LEN) continue;
    uint8_t b = (k == 0) ? 255 : hat_falloff(k, tailLen + 1);
    b = scale8(b, gain);
    if (b < 3) continue;
    hat_add(base + xi, hat_palette((uint8_t)(hue + k * 3), b));
  }
}

// Soft glow at both front centers so the "stations" read even between passes.


// Dim breathing glow on the rear connector so it never looks dead.
static void hat_connector_glow(uint8_t hue, uint8_t amt) {
  hat_rear_stage(hue, (uint8_t)(190U + (uint16_t)amt/4U));
}

// ---------------------------------------------------------------------------
// BRAKE 1 - Hat - Brake Comet
// One comet per row, launched together at the sides. Fast away from the side,
// glides through the front center, then rockets out the far side. The tail
// stretches when fast and tightens when slow (motion-blur feel).
// ---------------------------------------------------------------------------
static void mode_hat_brake_comet(void) {
  if (SEGLEN < 144) { FX_FALLBACK_STATIC; }
  SEGMENT.fill(BLACK);
  uint32_t t = strip.now;
  float x = hat_brake_pos(hat_brake_phase());
  float spd = hat_brake_speed(x);
  int tail = 5 + (int)(16.0f * spd) + (SEGMENT.intensity >> 5);
  uint8_t hue = (uint8_t)(t / 25);

  hat_connector_glow(hue, 30);
  hat_row_comet(HAT_TOP_BASE, x, 1, hue, tail);
  hat_row_comet(HAT_BOT_BASE, x - 1.0f, 1, (uint8_t)(hue + 128), tail);
}

// ---------------------------------------------------------------------------
// BRAKE 2 - Hat - Brake Train
// A train of 3-4 evenly spaced comets, all following the brake profile.
// Because each one slows at the front center, the train visibly bunches up
// there and then stretches back out as the cars speed away.
// ---------------------------------------------------------------------------


// ---------------------------------------------------------------------------
// BRAKE 3 - Hat - Brake Wake
// Instead of a small comet, the moving head paints a long glowing wake behind
// it. Each LED remembers WHEN the head passed (using the time-warped profile),
// so the wake is compressed and bright through the slow front-center zone and
// stretched thin through the fast zones.
// ---------------------------------------------------------------------------


// ---------------------------------------------------------------------------
// BRAKE 4 - Hat - Brake Opposing
// The top row runs side -> center -> far side while the bottom row runs the
// opposite way, mirrored about the front center. Both slow to a crawl at the
// front at exactly the same moment, so the two heads "meet" and linger at the
// front of the hat, then fly apart.
// ---------------------------------------------------------------------------


// ---------------------------------------------------------------------------
// BRAKE 5 - Hat - Brake Burst
// Comet with a payoff: as it crawls through the front center, it throws soft
// glittering sparks into the surrounding LEDs, scaled by how slow it is, then
// the sparks fade out as it accelerates away.
// ---------------------------------------------------------------------------


// ---------------------------------------------------------------------------
// Hat - Lighthouse
// A wide soft beam sweeps the full 144-LED loop at a steady pace over a dim
// colour-drifting ambient glow. Simple, clean, reads well from a distance.
// ---------------------------------------------------------------------------


// ---------------------------------------------------------------------------
// Hat - Halo Orbit
// Three soft glowing bodies orbit the whole loop at different speeds and in
// alternating directions. Where they overlap their colours add together, so
// you get slow-moving bright merges that split apart again.
// ---------------------------------------------------------------------------


// ---------------------------------------------------------------------------
// Hat - Mirror Bloom
// Everything is computed from distance to the nearest front-center LED, so the
// pattern is perfectly symmetric about the front of the hat. Smooth Perlin
// noise gives organic colour; a bright ring slowly expands outward from the
// front on top of it.
// ---------------------------------------------------------------------------
static void mode_hat_mirror_bloom(void) {
  if (SEGLEN < 144) { FX_FALLBACK_STATIC; }
  uint32_t t = strip.now;
  uint16_t z = (uint16_t)(t / (20U + ((255 - SEGMENT.speed) >> 2)));
  uint16_t R = (uint16_t)(((t * (2U + (SEGMENT.speed >> 5))) / 80U) % 72U);
  uint16_t ringW = 6 + (SEGMENT.intensity >> 6);

  for (uint16_t i = 0; i < 144; i++) {
    uint16_t d = min(hat_dist(i, 24), hat_dist(i, 116));
    uint8_t n = perlin8(d * 7U, z, 0);
    uint8_t ring = hat_falloff(hat_dist(d, R), ringW);
    uint8_t b = qadd8(qadd8(scale8(n, n) >> 1, ring >> 1), 8);
    SEGMENT.setPixelColor(i, hat_palette((uint8_t)(n + d * 2 + t / 50), b));
  }
  hat_rear_stage((uint8_t)(t/37),212);
}

// ---------------------------------------------------------------------------
// Hat - Double Helix
// Two interleaved sine strands (half a wave apart, different colours) twist
// along the whole strip like a DNA model, with faint white "rungs" every 12
// LEDs. Rows pick up the strands in sequence, so it flows across the hat.
// ---------------------------------------------------------------------------
static void mode_hat_double_helix(void) {
  if (SEGLEN < 144) { FX_FALLBACK_STATIC; }
  uint32_t t = strip.now;
  uint32_t P = 5200U - (uint32_t)SEGMENT.speed * 12U;
  if (P < 1900U) P = 1900U;
  float phase = 6.28318f * (float)(t % P) / (float)P;
  float freq = 0.22f + (SEGMENT.intensity >> 4) * 0.01f;
  uint8_t hue = (uint8_t)(t / 45);

  for (uint16_t i = 0; i < 144; i++) {
    float a = 0.5f + 0.5f * sinf(i * freq - phase);
    float b = 0.5f + 0.5f * sinf(i * freq - phase + 3.14159f);
    uint8_t ba = (uint8_t)(a * a * a * 255.0f);
    uint8_t bb = (uint8_t)(b * b * b * 255.0f);
    uint32_t c = color_add(hat_palette(hue, ba), hat_palette((uint8_t)(hue + 110), bb), false);
    if ((i % 12) == 0) c = color_add(c, color_fade(0xFFFFFF, 40), false);
    SEGMENT.setPixelColor(i, c);
  }
  hat_rear_stage((uint8_t)(t/40),211);
}

// ---------------------------------------------------------------------------
// Hat - Heartbeat
// A calm "lub-dub" glow swells outward from both front centers and spreads
// toward the rear arc, then settles. Brightness follows smooth cosine bumps,
// and the cycle is never faster than ~1.2 s, so it pulses without flashing.
// ---------------------------------------------------------------------------
static inline float hat_bump(float ph, float c, float w) {
  float d = fabsf(ph - c);
  if (d >= w) return 0.0f;
  return 0.5f + 0.5f * cosf(3.14159f * d / w);
}



// ---------------------------------------------------------------------------
// Hat - Meteor Rain
// Meteors spawn at the two front centers and shoot outward along their row,
// toward either side, trailing long fading tails. Spawn rate and speed are
// adjustable. Uses a small persistent pool of meteors.
// ---------------------------------------------------------------------------
struct HatMeteor {
  float x;
  int8_t dir;
  uint8_t hue;
  uint8_t row;       // 0 = top, 1 = bottom
  bool active;
};
constexpr uint8_t HAT_MAX_METEORS = 8;

static void mode_hat_meteor_rain(void) {
  if (SEGLEN < 144) { FX_FALLBACK_STATIC; }
  if (!SEGENV.allocateData(sizeof(HatMeteor) * HAT_MAX_METEORS)) { FX_FALLBACK_STATIC; }
  HatMeteor* m = reinterpret_cast<HatMeteor*>(SEGENV.data);

  uint32_t now = strip.now;
  if (SEGENV.call == 0) {
    for (uint8_t i = 0; i < HAT_MAX_METEORS; i++) m[i].active = false;
    SEGENV.step = now;
  }
  uint32_t dt = now - SEGENV.step;
  if (dt > 100) dt = 100;
  SEGENV.step = now;

  // Spawn
  if (hw_random8() < (3 + (SEGMENT.intensity >> 5))) {
    for (uint8_t i = 0; i < HAT_MAX_METEORS; i++) {
      if (!m[i].active) {
        m[i].active = true;
        m[i].dir = hw_random8(2) ? 1 : -1;
        m[i].hue = hw_random8();
        m[i].row = hw_random8(2);
        m[i].x = (float)hat_row_center(m[i].row);
        break;
      }
    }
  }

  SEGMENT.fill(BLACK);
  float v = 20.0f + SEGMENT.speed * 0.35f;                  // LEDs per second
  for (uint8_t i = 0; i < HAT_MAX_METEORS; i++) {
    if (!m[i].active) continue;
    m[i].x += m[i].dir * v * (dt / 1000.0f);
    if (m[i].x < -14.0f || m[i].x > (float)(HAT_ROW_LEN - 1) + 14.0f) { m[i].active = false; continue; }
    uint16_t base = m[i].row ? HAT_BOT_BASE : HAT_TOP_BASE;
    hat_row_comet(base, m[i].x, m[i].dir, m[i].hue, 13);
  }
  hat_rear_stage((uint8_t)(strip.now/32U),219);
}

// ---------------------------------------------------------------------------
// Hat - Fire Crown
// A heat simulation along the strip. Sparks ignite around both front centers,
// and heat drifts outward along each row while diffusing and cooling, so
// flames lick away from the front toward the sides. Uses its own black-body
// palette (palette selector is not used).
// ---------------------------------------------------------------------------
static void mode_hat_fire_crown(void) {
  if (SEGLEN < 144) { FX_FALLBACK_STATIC; }
  if (!SEGENV.allocateData(288)) { FX_FALLBACK_STATIC; }
  uint8_t* heat = SEGENV.data;
  uint8_t* tmp  = SEGENV.data + 144;
  uint32_t now = strip.now;

  if (SEGENV.call == 0) {
    memset(SEGENV.data, 0, 288);
    SEGENV.step = now;
  }

  if (now - SEGENV.step >= 30) {
    SEGENV.step = now;
    // Cool
    for (int i = 0; i < 144; i++) heat[i] = qsub8(heat[i], hw_random8(0, 6 + ((255 - SEGMENT.speed) >> 5)));
    // Diffuse, biased so heat flows away from the nearest front center
    for (int i = 0; i < 144; i++) {
      uint16_t mid = heat[i];
      uint16_t l = heat[i > 0 ? i - 1 : 0];
      uint16_t r = heat[i < 143 ? i + 1 : 143];
      int c = (i < 72) ? 24 : 116;
      if (i < c)      tmp[i] = (l + 2 * mid + 3 * r) / 6;
      else if (i > c) tmp[i] = (3 * l + 2 * mid + r) / 6;
      else            tmp[i] = (l + 2 * mid + r) / 4;
    }
    memcpy(heat, tmp, 144);
    // Ignite
    for (uint8_t s = 0; s < 2; s++) {
      if (hw_random8() < (40 + (SEGMENT.intensity >> 1))) {
        int c = hw_random8(2) ? 116 : 24;
        int p = c + (int)hw_random8(7) - 3;
        heat[p] = qadd8(heat[p], hw_random8(140, 255));
      }
    }
  }

  for (int i = 0; i < 144; i++) {
    uint16_t h = heat[i];
    uint16_t r = h * 3;               if (r > 255) r = 255;
    uint16_t g = h > 85  ? (h - 85)  * 3 : 0; if (g > 255) g = 255;
    uint16_t b = h > 170 ? (h - 170) * 3 : 0; if (b > 255) b = 255;
    SEGMENT.setPixelColor(i, RGBW32((uint8_t)r, (uint8_t)g, (uint8_t)b, 0));
  }
  hat_rear_stage((uint8_t)(strip.now/57U+156),211);
}

// ---------------------------------------------------------------------------
// Hat - Pendulum
// A bright bob swings back and forth along each row like a pendulum: fastest
// through the front center, slowing and lingering at the far ends - the exact
// opposite rhythm of the Brake effects. The two rows swing half a period
// apart, and the bob's colour shifts with its velocity.
// ---------------------------------------------------------------------------
static void mode_hat_pendulum(void) {
  if (SEGLEN < 144) { FX_FALLBACK_STATIC; }
  SEGMENT.fadeToBlackBy(60);
  uint32_t t = strip.now;
  uint32_t P = 3800U - (uint32_t)SEGMENT.speed * 8U;
  if (P < 1700U) P = 1700U;
  float u = (float)(t % P) / (float)P;
  int w = 2 + (SEGMENT.intensity >> 7);

  for (uint8_t row = 0; row < 2; row++) {
    float ph = 6.28318f * u + row * 3.14159f;
    float x = (float)hat_row_center(row) + (float)hat_row_center(row) * sinf(ph);
    float vel = cosf(ph);
    uint8_t hue = (uint8_t)((int)(150 + vel * 60.0f) + row * 90);
    uint16_t base = row ? HAT_BOT_BASE : HAT_TOP_BASE;
    int xc = (int)roundf(x);
    for (int d = -w; d <= w; d++) {
      int xi = xc + d;
      if (xi < 0 || xi >= HAT_ROW_LEN) continue;
      SEGMENT.setPixelColor(base + xi, hat_palette(hue, hat_falloff(abs(d), w + 1)));
    }
  }
  hat_connector_glow((uint8_t)(t / 60), 22);
}

// ---------------------------------------------------------------------------
// Hat - Rail Weave
// The top row carries a travelling wave in one direction, the bottom row
// carries one in the opposite direction, and the rear connector blends the two
// colour worlds with a slow wave of its own - like two threads weaving
// through each other around the hat.
// ---------------------------------------------------------------------------


// ----- Effect metadata (name@sliders;colors;palette;flags;defaults) -----
static const char _data_FX_MODE_HAT_BRAKE_COMET[] PROGMEM =
  "Hat - Brake Comet@Speed,Tail;!,!;!;1;sx=128,ix=128,pal=11";






static const char _data_FX_MODE_HAT_MIRROR_BLOOM[] PROGMEM =
  "Hat - Mirror Bloom@Speed,Ring width;!,!;!;1;sx=110,ix=128,pal=11";
static const char _data_FX_MODE_HAT_DOUBLE_HELIX[] PROGMEM =
  "Hat - Double Helix@Speed,Twist;!,!;!;1;sx=120,ix=100,pal=11";

static const char _data_FX_MODE_HAT_METEOR_RAIN[] PROGMEM =
  "Hat - Meteor Rain@Speed,Spawn rate;!,!;!;1;sx=140,ix=128,pal=11";
static const char _data_FX_MODE_HAT_FIRE_CROWN[] PROGMEM =
  "Hat - Fire Crown@Cooling,Sparks;;;1;sx=120,ix=140";
static const char _data_FX_MODE_HAT_PENDULUM[] PROGMEM =
  "Hat - Pendulum@Speed,Bob size;!,!;!;1;sx=120,ix=128,pal=11";



// ============================================================================
// Z / HAT SPECTACULAR III — 15 bespoke 144-pixel physical-hat compositions
// Front arcs: top 0..50 (center 24 / LED 25), bottom 93..143 (center 116 / LED 117)
// Rear connection: 51..92 (center 70 / LED 71). Defaults are hand-authored gradients, deliberately
// INDEPENDENT of WLED's global palette. Enable "WLED palette" to override.
// Pixel rendering is bounded; dynamic effect state is per segment (SEGENV).
// ============================================================================

// Five-stop cyclic palettes: nocturnal neons, fluorescent pigments, and
// occasional near-white highlights. Stored in flash, not precious ESP8266 RAM.
static const uint8_t hat3_gradients[15][5][3] PROGMEM = {
  {{  1,  3, 34},{  0,215,255},{250,  8,180},{255,197, 18},{  1,  3, 34}}, // 0 Prism: ice / hot-pink / saffron
  {{  3,  0, 35},{122,  0,255},{255, 17,126},{255,223, 65},{  3,  0, 35}}, // 1 Hypernova: ultraviolet / rose / star gold
  {{  0, 11, 38},{  0,255,179},{ 41, 61,255},{255, 25,206},{  0, 11, 38}}, // 2 Quantum: mint / cobalt / fuchsia
  {{  1,  7, 32},{  0, 88,255},{142, 12,255},{255, 62, 90},{  1,  7, 32}}, // 3 Vortex: blue / purple / coral
  {{  6,  0, 31},{160,  4,255},{255, 28,153},{255,121,  6},{  6,  0, 31}}, // 4 Dragonfire: violet / magenta / ember
  {{  2, 17, 36},{  0,239,255},{ 61, 68,255},{227, 30,255},{  2, 17, 36}}, // 5 Diamonds: cyan / sapphire / amethyst
  {{ 18,  0, 25},{108,  0, 63},{255, 12,116},{255,111, 32},{ 18,  0, 25}}, // 6 Collider: aubergine, ruby, hot pink, molten amber; no cyan or white
  {{  0, 17, 28},{  0,191,218},{ 71,255,112},{173, 55,255},{  0, 17, 28}}, // 7 Aurora: teal / lime / orchid
  {{  3,  1, 27},{  0, 97,245},{246,  7,207},{255,168, 40},{  3,  1, 27}}, // 8 Julia: electric / violet / amber
  {{  4,  0, 31},{255, 34,185},{  0,231,255},{211,255, 88},{  4,  0, 31}}, // 9 Ink: pink / aqua / chartreuse
  {{  0, 10, 28},{  0,255,225},{255, 33, 99},{255,216, 52},{  0, 10, 28}}, // 10 Pinball: turquoise / ruby / golden
  {{  1,  6, 28},{ 18,219,255},{ 95, 38,255},{255, 42,186},{  1,  6, 28}}, // 11 Hologram: azure / violet / laser pink
  {{  0, 17, 23},{  0,236,135},{ 18,118,255},{172, 27,246},{  0, 17, 23}}, // 12 Reef: jade / cobalt / purple
  {{  2,  5, 40},{  2,233,255},{255, 27,163},{255,238,  8},{  2,  5, 40}}, // 13 Tessellation: turquoise / cherry / acid yellow
  {{  3,  0, 23},{190,  0,255},{  0,249,242},{255, 82, 35},{  3,  0, 23}}  // 14 Seraphs: ultraviolet / aqua / vermilion
};

// Portable 8-bit sine for WLED variants that do not expose FastLED hat3_sin8().
// 0..255 phase -> 0..255 output. Shared helper avoids unavailable symbol errors.
static inline uint8_t hat3_sin8(uint8_t phase) {
  const float angle = (float)phase * (6.28318530718f / 256.0f);
  return (uint8_t)(127.5f + 127.5f * sinf(angle));
}

static inline uint8_t hat3_mix8(uint8_t a, uint8_t b, uint8_t amount) {
  return (uint8_t)(((uint16_t)a * (255 - amount) + (uint16_t)b * amount + 127U) / 255U);
}
static inline uint32_t hat3_color(uint8_t scheme, uint8_t index, uint8_t brilliance = 255) {
  if (SEGMENT.check1) return color_fade(SEGMENT.color_from_palette(index, true, true, 0), brilliance);
  uint16_t p = (uint16_t)index * 4U;
  uint8_t stop = p >> 8;  // 0..3
  uint8_t f = (uint8_t)p;
  uint8_t a0 = pgm_read_byte(&hat3_gradients[scheme][stop][0]);
  uint8_t a1 = pgm_read_byte(&hat3_gradients[scheme][stop][1]);
  uint8_t a2 = pgm_read_byte(&hat3_gradients[scheme][stop][2]);
  uint8_t b0 = pgm_read_byte(&hat3_gradients[scheme][stop + 1][0]);
  uint8_t b1 = pgm_read_byte(&hat3_gradients[scheme][stop + 1][1]);
  uint8_t b2 = pgm_read_byte(&hat3_gradients[scheme][stop + 1][2]);
  uint8_t r = scale8(hat3_mix8(a0, b0, f), brilliance);
  uint8_t g = scale8(hat3_mix8(a1, b1, f), brilliance);
  uint8_t b = scale8(hat3_mix8(a2, b2, f), brilliance);
  return RGBW32(r, g, b, 0);
}
static inline void hat3_add(int16_t pixel, uint8_t scheme, uint8_t hue, uint8_t light) {
  if (pixel < 0 || pixel >= 144 || light < 3) return;
  uint32_t c = hat3_color(scheme, hue, light);
  SEGMENT.setPixelColor((uint16_t)pixel, color_add(SEGMENT.getPixelColor((uint16_t)pixel), c, false));
}
static inline uint8_t hat3_peak(int16_t dist, uint8_t radius) {
  if (dist < 0) dist = -dist;
  if (dist >= radius) return 0;
  uint16_t n = 255U - (uint32_t)dist * 255U / radius;
  return (uint8_t)((n * n) >> 8);
}
static inline uint8_t hat3_hash(uint16_t x, uint16_t y) {
  uint32_t n = (uint32_t)x * 374761393UL + (uint32_t)y * 668265263UL + 0x9E3779B9UL;
  n = (n ^ (n >> 13)) * 1274126177UL;
  return (uint8_t)(n ^ (n >> 16));
}
static inline uint8_t hat3_rowpos(uint8_t row, uint8_t x) { return row ? (93 + x) : x; }
static void hat3_spot(uint8_t row, int16_t x, uint8_t radius, uint8_t scheme, uint8_t hue, uint8_t power) {
  for (int16_t d = -(int16_t)radius + 1; d < (int16_t)radius; d++) {
    int16_t xx = x + d;
    if (xx < 0 || xx > 50) continue;
    hat3_add(hat3_rowpos(row, (uint8_t)xx), scheme, hue + (int8_t)d * 3, scale8(power, hat3_peak(d, radius)));
  }
}
static void hat3_back(uint8_t scheme, uint8_t phase, uint8_t gain) {
  // Two opposing rear beams sweep the full 42-LED connector, meet at LED 71,
  // then separate. Keep the back conspicuously alive but not a static hot spot.
  const uint8_t travel=(uint16_t)hat3_sin8(phase)*20U/255U;
  for(int16_t i=51;i<=92;i++){
    int16_t d=i-HAT_BACK_CENTER;
    uint8_t beams=qadd8(hat3_peak(d-(int16_t)travel,4),hat3_peak(d+(int16_t)travel,4));
    uint8_t edge=hat3_peak(d-(int16_t)(20-travel),3);
    uint8_t wave=hat3_sin8((uint8_t)(i*17+phase*2));
    uint8_t power=qadd8(scale8(beams,208),scale8(edge,116));
    power=qadd8(power,scale8(wave,(uint8_t)(26+gain/2)));
    uint32_t col=hat3_color(scheme,(uint8_t)(phase/3+d*8+104),power);
    uint32_t old=SEGMENT.getPixelColor((uint16_t)i);
    SEGMENT.setPixelColor((uint16_t)i,RGBW32(
      R(col)>R(old)?R(col):R(old),
      G(col)>G(old)?G(col):G(old),
      B(col)>B(old)?B(col):B(old),0));
  }
}

// 01 — PRISM GUILLOTINE. Three speed-shifting spectral blades shear across
// both front arcs; crossings generate gold-white diffraction and back glow.
static void mode_hat3_prism_guillotine(void) {
  if (SEGLEN < 144) FX_FALLBACK_STATIC;
  SEGMENT.fill(BLACK);
  const uint32_t t = strip.now;
  const uint16_t period = 3200U - (uint16_t)SEGMENT.speed * 7U;
  const uint16_t tick = t % period;
  const uint8_t phase = (uint32_t)tick * 255U / period;
  const uint8_t bend = hat3_sin8((uint8_t)(phase * 2));
  for (uint8_t row = 0; row < 2; row++) {
    for (uint8_t x = 0; x < 51; x++) {
      uint8_t brightness = 4;
      uint8_t idx = (uint8_t)(x * 8 + t / 43 + row * 74);
      for (uint8_t blade = 0; blade < 3; blade++) {
        uint8_t travel = (uint8_t)(phase + blade * 84 + row * 43);
        int16_t head = ((uint16_t)travel * 64U >> 8) - 7;
        int16_t d = (int16_t)x - head;
        uint8_t core = hat3_peak(d, 3 + (SEGMENT.intensity >> 6));
        uint8_t dispersion = hat3_peak(d - 3, 10);
        brightness = qadd8(brightness, qadd8(core, scale8(dispersion, 90)));
        if (core > 160) idx = (uint8_t)(blade * 85 + bend / 2 + row * 41);
      }
      uint8_t facets = hat3_sin8((uint8_t)(x * 25 - phase * 2 + bend / 4));
      brightness = qadd8(brightness, scale8(facets, 20));
      hat3_add(hat3_rowpos(row,x), 0, idx, brightness);
    }
  }
  hat3_spot(0,24,7,0,(uint8_t)(phase + 162),scale8(hat3_sin8(phase*3),115));
  hat3_spot(1,23,7,0,(uint8_t)(phase + 162),scale8(hat3_sin8(phase*3),115));
  hat3_back(0, phase, 43);
}

// 02 — HYPERNOVA BLOOM. Unequally spaced radial shells erupt at both center
// points with lingering chromatic shock fronts and fine star-shot particles.


// 03 — QUANTUM BRAIDS. Three counterpropagating, phase-modulated strands
// weave into knots which collapse into narrow cyan-white interference crests.


// 04 — CHROMATIC VORTEX. A front-anchored rotating lens distorts four color
// currents; a wandering dark eye reveals crystalline rims and broad caustics.


// 05 — DRAGONFIRE. A persistent heat field with front-side fuel injection,
// advection, curl-like turbulence, cooling and shifting violet/orange flames.


// 06 — DIAMOND RAIN. Persistent refractive crystals accelerate across the
// arcs; faceted halos and broken rainbow reflections follow every crossing.
struct Hat3Diamond { int16_t pos; int8_t velocity; uint8_t row,hue,life; };
static void mode_hat3_diamond_rain(void) {
  if (SEGLEN < 144) FX_FALLBACK_STATIC;
  constexpr uint8_t N=11;
  if (!SEGENV.allocateData(sizeof(Hat3Diamond)*N)) FX_FALLBACK_STATIC;
  Hat3Diamond* d=(Hat3Diamond*)SEGENV.data;
  uint32_t t=strip.now;
  if (SEGENV.call==0) { memset(d,0,sizeof(Hat3Diamond)*N); SEGENV.step=t; }
  uint32_t dt=t-SEGENV.step; if (dt>70) dt=70;
  SEGENV.step=t;
  SEGMENT.fadeToBlackBy(65);
  for (uint8_t i=0;i<N;i++) {
    if (d[i].life==0) {
      if (hw_random8() < (12 + (SEGMENT.intensity >> 3))) {
        d[i].pos = (hw_random8(2)?0:50)*16;
        d[i].velocity = d[i].pos ? -(3+(int8_t)hw_random8(5)) : (3+(int8_t)hw_random8(5));
        d[i].row=hw_random8(2);
        d[i].hue=(uint8_t)(i*33+t/63);
        d[i].life=240;
      }
      continue;
    }
    d[i].pos += ((int16_t)d[i].velocity * (int16_t)dt * (int16_t)(3+(SEGMENT.speed>>6)))/32;
    int16_t x=d[i].pos/16;
    if (x<-5 || x>55) { d[i].life=0; continue; }
    d[i].life=qsub8(d[i].life,2);
    for (int8_t j=-5;j<=5;j++) {
      int16_t xx=x+j;
      if (xx<0||xx>50) continue;
      uint8_t halo=scale8(hat3_peak(j,6),d[i].life);
      hat3_add(hat3_rowpos(d[i].row,(uint8_t)xx),5,d[i].hue+j*16,halo);
    }
    hat3_spot(d[i].row,x,2,5,187,230);
    if (abs(x-hat_row_center(d[i].row))<=3)
      hat3_spot(1-d[i].row,hat_row_center(1-d[i].row),5,5,d[i].hue+70,80);
  }
  hat3_back(5,(uint8_t)(t/47),30);
}

// 07 — SUPERCOLLIDER. Opposite packets accelerate toward front centers;
// intersections create bright multi-ring annihilation bursts and afterglow.
static void mode_hat3_supercollider(void) {
  if (SEGLEN < 144) FX_FALLBACK_STATIC;
  SEGMENT.fadeToBlackBy(105);
  uint32_t t=strip.now;
  uint16_t cycle=3100U-(uint16_t)SEGMENT.speed*6U;
  uint16_t u=t%cycle;
  uint8_t phase=(uint32_t)u*255U/cycle;
  for (uint8_t row=0;row<2;row++) {
    // mirror in time: approach -> impact -> explosion
    uint8_t motion=(phase<115)?(115-phase)*255U/115U:(phase-115)*255U/140U;
    const int16_t center=hat_row_center(row);
    int16_t a=center-(uint16_t)motion*center/255U;
    int16_t b=center+(uint16_t)motion*(50-center)/255U;
    uint8_t hue=(uint8_t)(t/47+row*59);
    hat3_spot(row,a,5,6,hue,190);
    hat3_spot(row,b,5,6,hue+116,190);
    if (phase>110 && phase<180) {
      uint8_t expand=(uint16_t)(phase-110)*38U/70U;
      uint8_t fire=(phase<145)?212:(uint16_t)(180-phase)*212U/35U;
      for (uint8_t x=0;x<51;x++) {
        uint8_t ripple=hat3_peak(abs((int)x-center)-(int16_t)expand,4);
        hat3_add(hat3_rowpos(row,x),6,(uint8_t)(phase+x*4+row*35),scale8(ripple,fire));
      }
    }
  }
  hat3_back(6,(uint8_t)(t/35),23);
}

// 08 — AURORA CATHEDRAL. Three-octave flowing noise, vertically coupled
// curtains, shifting stained-glass bands and brilliant blade-like crests.
static void mode_hat3_aurora_cathedral(void) {
  if (SEGLEN < 144) FX_FALLBACK_STATIC;
  SEGMENT.fill(BLACK);
  uint32_t t=strip.now;
  uint16_t speed=(uint16_t)(t/(18+((255-SEGMENT.speed)>>3)));
  for (uint8_t x=0;x<51;x++) {
    uint8_t n0=perlin8((uint16_t)(x*47),speed*3,923);
    uint8_t n1=perlin8((uint16_t)(x*131+1800),speed,6203);
    uint8_t n2=perlin8((uint16_t)(x*17+90),speed*5,22671);
    uint8_t curtain=qadd8(scale8(n0,140),scale8(n1,115));
    uint8_t organ=hat3_peak((int16_t)n2-180,70);
    uint8_t hue=(uint8_t)(curtain+n1/2+t/125);
    for (uint8_t row=0;row<2;row++) {
      uint8_t shimmer=hat3_sin8((uint8_t)(x*13+t/59+row*72));
      uint8_t bright=qadd8(scale8(curtain,175),qadd8(scale8(organ,95),scale8(shimmer,35)));
      if (row) bright=scale8(bright,205);
      hat3_add(hat3_rowpos(row,x),7,hue+row*55,bright);
    }
  }
  hat3_back(7,(uint8_t)(t/70),65);
}

// 09 — JULIA RIFTS. Time-warped complex Julia escape-time iteration gives
// fractured, nonrepeating color islands overlaid with a luminous fault line.
static void mode_hat3_julia_rifts(void) {
  if (SEGLEN < 144) FX_FALLBACK_STATIC;
  SEGMENT.fill(BLACK);
  uint32_t t=strip.now;
  float ti=(float)t*0.00013f*(0.50f+(float)SEGMENT.speed/255.0f);
  float cr=-0.74f+0.10f*sinf(ti*0.81f);
  float ci=0.19f+0.17f*cosf(ti*0.59f);
  for (uint8_t row=0;row<2;row++) {
    float y=(row?0.38f:-0.38f)+0.15f*sinf(ti*0.61f+row);
    for (uint8_t x=0;x<51;x++) {
      float zx=((float)x-(float)hat_row_center(row))*0.055f;
      float zy=y+0.10f*sinf((float)x*0.21f+ti*2.0f);
      uint8_t iter=0;
      for (;iter<12;iter++) {
        float zx2=zx*zx, zy2=zy*zy;
        if (zx2+zy2>4.0f) break;
        zy=2.0f*zx*zy+ci;
        zx=zx2-zy2+cr;
      }
      uint8_t inner=(iter==12)?14:(uint8_t)(80+iter*13);
      uint8_t hue=(uint8_t)(iter*23+x*3+(t/67)+row*43);
      uint8_t crack=hat3_peak((int16_t)x-(hat_row_center(row)+(int16_t)(5*sinf(ti*2.4f+row))),3);
      hat3_add(hat3_rowpos(row,x),8,hue,qadd8(inner,scale8(crack,170)));
    }
  }
  hat3_back(8,(uint8_t)(t/42),24);
}

// 10 — NEON CALLIGRAPHY. Three antialiased, time-curved light strokes trace
// elegant opposing flourishes; their overlapping brush tips ignite pinpoints.
static void mode_hat3_neon_calligraphy(void) {
  if (SEGLEN < 144) FX_FALLBACK_STATIC;
  SEGMENT.fadeToBlackBy(82);
  uint32_t t=strip.now;
  float time=(float)t*(0.00025f+SEGMENT.speed*0.000001f);
  for (uint8_t stroke=0;stroke<3;stroke++) {
    float ph=time*(0.62f+stroke*0.24f)+stroke*2.09f;
    uint8_t row=(stroke==1)?1:0;
    float x=(float)hat_row_center(row)+22.0f*sinf(ph)+4.0f*sinf(ph*2.71f+stroke);
    int16_t px=(int16_t)floorf(x);
    uint8_t frac=(uint8_t)((x-(float)px)*255.0f);
    uint8_t hue=(uint8_t)(stroke*79+t/75);
    hat3_spot(row,px,6,9,hue,scale8(220,255-frac));
    hat3_spot(row,px+1,6,9,hue+23,scale8(220,frac));
    hat3_spot(1-row,47-px,3,9,hue+91,82);
    if (fabsf(sinf(ph*1.5f))>0.92f) hat3_spot(row,px,3,9,188,175);
  }
  hat3_back(9,(uint8_t)(t/68),35);
}

// 11 — COSMIC PINBALL. Nine independent elastic luminous bodies reflect
// off edges, exchange momentum at collisions and excite the opposite arc.
struct Hat3Ball { int16_t x; int8_t v; uint8_t row,hue; };
static void mode_hat3_cosmic_pinball(void) {
  if (SEGLEN < 144) FX_FALLBACK_STATIC;
  constexpr uint8_t N=9;
  if (!SEGENV.allocateData(sizeof(Hat3Ball)*N)) FX_FALLBACK_STATIC;
  Hat3Ball* p=(Hat3Ball*)SEGENV.data;
  uint32_t t=strip.now;
  if (SEGENV.call==0) {
    SEGENV.step=t;
    for (uint8_t i=0;i<N;i++) {p[i].x=hw_random16(4,796);p[i].v=hw_random8(2)?(2+hw_random8(6)):-(2+(int8_t)hw_random8(6));p[i].row=i%2;p[i].hue=i*27;}
  }
  uint32_t dt=t-SEGENV.step; if (dt>70)dt=70; SEGENV.step=t;
  SEGMENT.fadeToBlackBy(62);
  for (uint8_t i=0;i<N;i++) {
    int16_t delta=(int16_t)p[i].v*(int16_t)dt*(int16_t)(3+(SEGMENT.speed>>6))/24;
    p[i].x+=delta;
    if (p[i].x<0) {p[i].x=-p[i].x; p[i].v=abs(p[i].v);hat3_spot(p[i].row,0,5,10,170,195);}
    if (p[i].x>800) {p[i].x=1600-p[i].x;p[i].v=-abs(p[i].v);hat3_spot(p[i].row,50,5,10,170,195);}
    for(uint8_t j=0;j<i;j++) {
      if (p[j].row==p[i].row && abs(p[j].x-p[i].x)<26) {
        int8_t v=p[i].v; p[i].v=p[j].v; p[j].v=v;
        uint8_t mix=p[i].hue;p[i].hue=p[j].hue;p[j].hue=mix;
        hat3_spot(1-p[i].row, (p[i].x+p[j].x)/32,4,10,183,140);
      }
    }
    hat3_spot(p[i].row,p[i].x/16,4,10,(uint8_t)(p[i].hue+t/90),250);
    if (p[i].x>320&&p[i].x<420) hat3_spot(1-p[i].row,hat_row_center(1-p[i].row),4,10,(uint8_t)(p[i].hue+71),50);
  }
  hat3_back(10,(uint8_t)(t/61),33);
}

// 12 — HOLOGRAM SCANNER. Three layered moving translucent scanners with
// independently drifting interference fringes and sparse micro-pixel splits.
static void mode_hat3_hologram_scanner(void) {
  if (SEGLEN < 144) FX_FALLBACK_STATIC;
  SEGMENT.fill(BLACK);
  uint32_t t=strip.now;
  uint8_t phase=(uint8_t)(t/(16+((255-SEGMENT.speed)>>4)));
  uint16_t epoch=(uint16_t)(t/960);
  for (uint8_t row=0;row<2;row++) {
    for(uint8_t x=0;x<51;x++) {
      uint8_t carrier=hat3_sin8((uint8_t)(x*29+phase+row*93));
      uint8_t upper=hat3_sin8((uint8_t)(x*7-phase*3+row*127));
      uint8_t gate=hat3_sin8((uint8_t)(x*2+phase*2+row*41));
      uint8_t cross=scale8(carrier,upper);
      uint8_t light=qadd8(scale8(gate,63),qadd8(scale8(cross,154),scale8(upper,45)));
      uint8_t h=(uint8_t)(x*6+phase+upper/3+row*27);
      if (hat3_hash(x+row*71,epoch)>248) {
        light=qadd8(light,140);
        h+=100;
      }
      uint8_t scan=hat3_peak((int16_t)x-((uint16_t)(phase+row*125)*51U/256U),4);
      hat3_add(hat3_rowpos(row,x),11,h,qadd8(light,scale8(scan,225)));
    }
  }
  hat3_back(11,phase,55);
}

// 13 — BIOLUMINESCENT REEF. A compact excitable reaction-diffusion medium:
// moving excitation fronts are followed by a slowly decaying refractory field,
// creating branched phosphorescent blooms rather than a static noise texture.
static void mode_hat3_bioluminescent_reef(void) {
  if (SEGLEN < 144) FX_FALLBACK_STATIC;
  if (!SEGENV.allocateData(408)) FX_FALLBACK_STATIC;
  uint8_t *u=SEGENV.data, *v=u+102, *nu=v+102, *nv=nu+102;
  uint32_t t=strip.now;
  if (SEGENV.call==0) {
    memset(SEGENV.data,0,408);
    u[24]=225; u[74]=225;
    for(uint8_t j=0;j<8;j++)u[hw_random8(102)]=hw_random8(110,255);
    SEGENV.step=t;
  }
  if (t-SEGENV.step>=44) {
    SEGENV.step=t;
    for(uint8_t row=0;row<2;row++) for(uint8_t x=0;x<51;x++) {
      uint8_t i=row*51+x;
      uint8_t a=u[i], b=v[i];
      uint8_t l=u[row*51+(x?x-1:0)];
      uint8_t r=u[row*51+(x<50?x+1:50)];
      int16_t lap=((int16_t)l+(int16_t)r-2*(int16_t)a)/3;
      // Excited cells spread energy until their inhibitor switches them off.
      int16_t reaction=(a>74 && b<156)?29:-11;
      int16_t next=(int16_t)a+lap+reaction-(b>>5);
      if (abs((int)x-hat_row_center(row))<3) {
        uint8_t peer=u[(1-row)*51+x];
        if (peer>135) next+=(peer-135)/8;
      }
      if (hw_random8() < (2+(SEGMENT.intensity>>5))) next+=hw_random8(140,240);
      if (next < 0) next = 0;
      if (next > 255) next = 255;
      nu[i]=(uint8_t)next;
      nv[i]=(a>125)?qadd8(b,18):qsub8(b,5);
    }
    memcpy(u,nu,102); memcpy(v,nv,102);
  }
  for(uint8_t row=0;row<2;row++)for(uint8_t x=0;x<51;x++) {
    uint8_t i=row*51+x;
    uint8_t energy=qsub8(u[i],v[i]);
    uint8_t light=qadd8(scale8(u[i],190),scale8(energy,95));
    uint8_t hue=(uint8_t)(u[i]+(t/160)+row*37+(x*3));
    hat3_add(hat3_rowpos(row,x),12,hue,light);
    if(energy>110)hat3_add(hat3_rowpos(row,x),12,172,scale8(energy,125));
  }
  hat3_back(12,(uint8_t)(t/93),30);
}

// 14 — ELECTRIC TESSELLATION. Moving triangular lattices overlap under a
// phase-shifted quasi-periodic moire; fine gold edge sparks reveal intersections.
static void mode_hat3_electric_tessellation(void) {
  if (SEGLEN < 144) FX_FALLBACK_STATIC;
  SEGMENT.fill(BLACK);
  uint32_t t=strip.now;
  uint8_t phase=(uint8_t)(t/(13+((255-SEGMENT.speed)>>4)));
  for(uint8_t row=0;row<2;row++)for(uint8_t x=0;x<51;x++) {
    uint8_t a=(uint8_t)(x*19+phase+row*61);
    uint8_t b=(uint8_t)(x*31-phase*2+row*103);
    uint8_t c=(uint8_t)(x*7+phase/3+row*19);
    uint8_t triA=(a<128)?(a*2):((255-a)*2);
    uint8_t triB=(b<128)?(b*2):((255-b)*2);
    uint8_t edgeA=qsub8(triA,170);
    uint8_t edgeB=qsub8(triB,170);
    uint8_t moire=qadd8(scale8(edgeA,205),scale8(edgeB,205));
    uint8_t planes=scale8(hat3_sin8(c),56);
    uint8_t intersection=scale8(edgeA,edgeB);
    uint8_t light=qadd8(qadd8(planes,moire),scale8(intersection,220));
    uint8_t h=(uint8_t)(a+b/2+phase+row*47);
    hat3_add(hat3_rowpos(row,x),13,h,light);
    if (intersection>58)hat3_add(hat3_rowpos(row,x),13,179,scale8(intersection,165));
  }
  hat3_back(13,phase,52);
}

// 15 — CROWN OF SERAPHS. A cinematic five-act choreography (invocation,
// ascent, eclipse, fracturing, coronation) with continuous color progression,
// mirrored arcs, transients and a prismatic rear circuit.
static void mode_hat3_crown_of_seraphs(void) {
  if (SEGLEN < 144) FX_FALLBACK_STATIC;
  SEGMENT.fill(BLACK);
  uint32_t t=strip.now;
  uint32_t duration=10000UL-(uint32_t)SEGMENT.speed*18UL;
  uint32_t local=t%duration;
  uint8_t act=(uint32_t)local*5UL/duration;
  uint32_t actLength=duration/5UL;
  uint8_t p=(uint32_t)(local%actLength)*255U/actLength;
  uint8_t scheme=14;
  for(uint8_t row=0;row<2;row++)for(uint8_t x=0;x<51;x++) {
    uint8_t d=abs((int)x-hat_row_center(row));
    uint8_t h=(uint8_t)(x*5+t/63+row*57);
    uint8_t v=0;
    if (act==0) { // Seed energy spirals inward to front
      int16_t q=25-(uint16_t)p*25U/255U;
      v=qadd8(hat3_peak((int16_t)d-q,5),scale8(hat3_sin8((uint8_t)(d*19+t/68)),46));
    } else if (act==1) { // Rising gold ribs climb toward sides
      v=qadd8(hat3_peak((int16_t)d-(int16_t)((uint16_t)p*27U/255U),5),scale8(hat3_sin8((uint8_t)(x*23-p*3)),77));
      h+=80;
    } else if(act==2) { // Dark eclipse with fluorescent diffraction rim
      int16_t eye=9+(hat3_sin8(p*2)>>5);
      v=qadd8(hat3_peak((int16_t)d-eye,4),scale8(hat3_sin8((uint8_t)(x*13+t/31)),43));
      if (d<eye-3) v=6;
      h+=143;
    } else if(act==3) { // Pattern fragments into crystalline flashes
      uint8_t n=hat3_hash(x+row*71,t/80);
      v=(n>210)?qadd8(n,40):scale8(hat3_sin8((uint8_t)(x*31+p*3)),93);
      h+=n;
    } else { // Wide cathedral crown, white-gold focal radiance
      uint8_t arch=hat3_sin8((uint8_t)(d*14+p/3));
      v=qadd8(scale8(arch,180),scale8(hat3_peak((int16_t)d-12,9),130));
      if (d<4) v=qadd8(v,scale8(hat3_sin8(p),105));
      h+=196;
    }
    // Seamlessly soften each act boundary instead of sudden all-pixel flashes.
    uint8_t envelope=(p<18)?(uint8_t)(120+p*7U):(p>236)?(uint8_t)(120+(255-p)*7U):255;
    hat3_add(hat3_rowpos(row,x),scheme,h,scale8(v,envelope));
  }
  hat3_back(scheme,(uint8_t)(t/41),42+(act==4?60:0));
}

// WLED metadata. "Use WLED palette" is OFF by default; each effect uses its
// own neon gradient until toggled. Choose intensity/speed defaults for motion.
static const char _data_FX_MODE_HAT3_PRISM_GUILLOTINE[] PROGMEM = "Hat III - Prism Guillotine@Blade speed,Width,,,,WLED palette;!,!;!;1;sx=158,ix=165,o1=0";




static const char _data_FX_MODE_HAT3_DIAMOND_RAIN[] PROGMEM = "Hat III - Diamond Rain@Velocity,Crystal density,,,,WLED palette;!,!;!;1;sx=142,ix=172,o1=0";
static const char _data_FX_MODE_HAT3_SUPERCOLLIDER[] PROGMEM = "Hat III - Supercollider@Approach speed,Burst,,,,WLED palette;!,!;!;1;sx=149,ix=165,o1=0";
static const char _data_FX_MODE_HAT3_AURORA_CATHEDRAL[] PROGMEM = "Hat III - Aurora Cathedral@Flow,Depth,,,,WLED palette;!,!;!;1;sx=112,ix=155,o1=0";
static const char _data_FX_MODE_HAT3_JULIA_RIFTS[] PROGMEM = "Hat III - Julia Rifts@Warp velocity,Fracture,,,,WLED palette;!,!;!;1;sx=110,ix=168,o1=0";
static const char _data_FX_MODE_HAT3_NEON_CALLIGRAPHY[] PROGMEM = "Hat III - Neon Calligraphy@Stroke speed,Ink,,,,WLED palette;!,!;!;1;sx=136,ix=166,o1=0";
static const char _data_FX_MODE_HAT3_COSMIC_PINBALL[] PROGMEM = "Hat III - Cosmic Pinball@Ball velocity,Energy,,,,WLED palette;!,!;!;1;sx=147,ix=175,o1=0";
static const char _data_FX_MODE_HAT3_HOLOGRAM_SCANNER[] PROGMEM = "Hat III - Hologram Scanner@Scan speed,Refraction,,,,WLED palette;!,!;!;1;sx=148,ix=180,o1=0";
static const char _data_FX_MODE_HAT3_BIOLUMINESCENT_REEF[] PROGMEM = "Hat III - Bioluminescent Reef@Diffusion,Fuel,,,,WLED palette;!,!;!;1;sx=111,ix=170,o1=0";
static const char _data_FX_MODE_HAT3_ELECTRIC_TESSELLATION[] PROGMEM = "Hat III - Electric Tessellation@Motion,Grid depth,,,,WLED palette;!,!;!;1;sx=156,ix=158,o1=0";
static const char _data_FX_MODE_HAT3_CROWN_OF_SERAPHS[] PROGMEM = "Hat III - Crown of Seraphs@Choreography,Energy,,,,WLED palette;!,!;!;1;sx=125,ix=180,o1=0";



// ============================================================================
// HAT IV: CENTER STAGE — composed for 144 physically arranged hat LEDs.
// Top arc: 0..50 (front 24), bottom arc: 93..143 (front 116).
// Back connector: 51..92 (center 70). Curated palettes default, WLED palette optional.
// These effects intentionally favor front-center theatrics over simple chases.
// ============================================================================

// Color stops in flash. Seven separate deep warm-neon stories, NO cyan/white.
static const uint8_t hat4_gradients[7][5][3] PROGMEM = {
  {{ 12,  0, 23},{122,  4, 82},{255, 18,126},{255,138, 37},{ 12,  0, 23}}, // 0 Aubergine/rose/amber
  {{ 19,  0, 20},{151,  6, 52},{255, 41,133},{245,107, 49},{ 19,  0, 20}}, // 1 Oxblood/lipstick/copper
  {{ 17,  0, 30},{194,  7, 91},{255, 72,150},{250,165, 31},{ 17,  0, 30}}, // 2 Wine/bubblegum/gold
  {{ 14,  1, 28},{115,  0, 88},{245, 18, 88},{255,115, 20},{ 14,  1, 28}}, // 3 Plum/garnet/fire
  {{  9,  0, 14},{126,  0, 27},{255,  9, 70},{246,106, 67},{  9,  0, 14}}, // 4 Black cherry/vermilion
  {{ 17,  0, 22},{123, 12, 85},{255, 30,158},{253,166, 52},{ 17,  0, 22}}, // 5 Mulberry/marigold
  {{ 10,  0, 26},{127,  0, 69},{255, 15,111},{255, 91, 20},{ 10,  0, 26}}  // 6 Velvet/ruby/ember
};
static inline uint8_t hat4_lerp(uint8_t a,uint8_t b,uint8_t w){
  return (uint8_t)(((uint16_t)a*(255U-w)+(uint16_t)b*w+127U)/255U);
}
static inline uint32_t hat4_color(uint8_t scheme,uint8_t index,uint8_t power){
  if(SEGMENT.check1)return color_fade(SEGMENT.color_from_palette(index,true,true,0),power);
  uint16_t q=(uint16_t)index*4U; uint8_t k=q>>8,f=(uint8_t)q;
  uint8_t r=hat4_lerp(pgm_read_byte(&hat4_gradients[scheme][k][0]),pgm_read_byte(&hat4_gradients[scheme][k+1][0]),f);
  uint8_t g=hat4_lerp(pgm_read_byte(&hat4_gradients[scheme][k][1]),pgm_read_byte(&hat4_gradients[scheme][k+1][1]),f);
  uint8_t b=hat4_lerp(pgm_read_byte(&hat4_gradients[scheme][k][2]),pgm_read_byte(&hat4_gradients[scheme][k+1][2]),f);
  return RGBW32(scale8(r,power),scale8(g,power),scale8(b,power),0);
}
// Max-channel optical compositing keeps overlapping saturated colors saturated
// rather than making a muddy near-white blast.
static inline void hat4_add(int16_t px,uint8_t scheme,uint8_t hue,uint8_t power){
  if(px<0||px>=144||power<3)return;
  uint32_t v=hat4_color(scheme,hue,power),old=SEGMENT.getPixelColor((uint16_t)px);
  SEGMENT.setPixelColor((uint16_t)px,RGBW32(
    R(v)>R(old)?R(v):R(old),
    G(v)>G(old)?G(v):G(old),
    B(v)>B(old)?B(v):B(old),0));
}
static inline uint8_t hat4_bell(int16_t d,uint8_t width){
  if(d<0)d=-d;
  if(width==0||d>=width)return 0;
  uint16_t k=255U-(uint16_t)d*255U/width;
  return (uint8_t)(k*k>>8);
}
static void hat4_spot(uint8_t row,int16_t x,uint8_t width,uint8_t scheme,uint8_t hue,uint8_t power){
  for(int8_t j=1-(int8_t)width;j<(int8_t)width;j++){
    int16_t xx=x+j;
    if(xx<0||xx>50)continue;
    hat4_add((row?93:0)+xx,scheme,(uint8_t)(hue+j*3),scale8(power,hat4_bell(j,width)));
  }
}

static void hat4_rear(uint8_t scheme,uint8_t phase,uint8_t gain){
  uint8_t travel=(uint16_t)hat3_sin8(phase)*20U/255U;
  for(int16_t i=51;i<=92;i++){
    int16_t d=i-HAT_BACK_CENTER;
    uint8_t toward=qadd8(hat4_bell(d-(int16_t)travel,4),
                         hat4_bell(d+(int16_t)travel,4));
    uint8_t counter=hat4_bell(d-(int16_t)(20-travel),3);
    uint8_t sweep=scale8(hat3_sin8((uint8_t)(i*23+phase*2)),(uint8_t)(35+gain));
    uint8_t light=qadd8(qadd8(scale8(toward,213),scale8(counter,105)),sweep);
    // Rear midpoint only bright when converging beams actually intersect.
    hat4_add(i,scheme,(uint8_t)(68+phase/2+d*8),light);
  }
}

// 01 SINGULARITY THEATRE. Three staggered wavefronts drawn inward on both
// bands; their energy stalls and jumps between centers, then detonates in
// colored shells and recoalesces at the front.


// 02 VELVET PORTAL. A double rim opens and closes; opposing interference
// ribs rotate inside it. The slit snaps shut onto the two front center pixels
// and blooms back outward as copper and wine-colored echoes.


// 03 SWITCHBLADE WALTZ. Four accelerating blade paths trace crossing
// figure-eights. Delayed anti-symmetric reflections bounce between the two
// front arcs. The impact lock at the middle changes color with the rhythm.
static void mode_hat4_switchblade_waltz(void){
  if(SEGLEN<144)FX_FALLBACK_STATIC;
  SEGMENT.fill(BLACK);
  uint32_t t=strip.now;
  uint8_t tick=(uint8_t)(t/(13+((255-SEGMENT.speed)>>4)));
  uint8_t p=(uint8_t)(t/31);
  for(uint8_t row=0;row<2;row++){
    for(uint8_t blade=0;blade<4;blade++){
      uint8_t phase=(uint8_t)(tick*(blade+1)+blade*64+row*18);
      int16_t disp=((int16_t)hat3_sin8(phase)-128)*(17+blade*3)/128;
      int16_t wobble=((int16_t)hat3_sin8((uint8_t)(phase*2+p+blade*41))-128)/34;
      int16_t x=hat_row_center(row)+disp+wobble;
      uint8_t power=(uint8_t)(191+(blade%2)*43);
      hat4_spot(row,x,3+(blade%3),2,(uint8_t)(72+blade*39+p/2),power);
      // Crossing accent follows the moving blade, never a constant nucleus.
      if(abs(x-hat_row_center(row))<=2)
        hat4_spot(row,hat_row_center(row),2,2,(uint8_t)(175+blade*14),195);
      hat4_spot(1-row,hat_row_center(row)+hat_row_center(1-row)-x,2,2,(uint8_t)(193+blade*21+p/3),scale8(power,104));
    }
    for(uint8_t x=0;x<51;x++){
      int16_t d=abs((int)x-hat_row_center(row));
      uint8_t inter=scale8(hat3_sin8((uint8_t)(x*19-t/48+row*103)),48);
      hat4_add((row?93:0)+x,2,(uint8_t)(p+x*8),inter);
    }
  }
  hat4_rear(2,(uint8_t)(t/40),28);
}

// 04 HOURGLASS REVERSAL. Two opposed streams visibly squeeze through the
// front and emerge on the OTHER band. After each transfer, the chronology
// reverses; prismatic ripples remain attached to the crossed focal seam.
static void mode_hat4_hourglass_reversal(void){
  if(SEGLEN<144)FX_FALLBACK_STATIC;
  SEGMENT.fill(BLACK);
  uint32_t t=strip.now;
  uint16_t cycle=4800U-(uint16_t)SEGMENT.speed*8U;
  uint8_t p=(uint32_t)(t%cycle)*255U/cycle;
  uint8_t act=p/64U,f=(uint8_t)(p%64U)*4U;
  int16_t travel=(act==0||act==2)?26-(uint16_t)f*26U/255U:(uint16_t)f*26U/255U;
  for(uint8_t row=0;row<2;row++){
    uint8_t liveRow=(act==0||act==3)?row:1-row;
    for(uint8_t d=0;d<3;d++){
      int16_t gap=travel+(int16_t)d*5;
      hat4_spot(liveRow,hat_row_center(liveRow)-gap,4,3,(uint8_t)(76+d*42+row*23),220-d*21);
      hat4_spot(liveRow,hat_row_center(liveRow)+gap,4,3,(uint8_t)(178+d*23+row*39),213-d*18);
    }
    for(uint8_t x=0;x<51;x++){
      uint8_t dd=abs((int)x-hat_row_center(row));
      uint8_t ribs=hat3_sin8((uint8_t)(dd*27+(act&1?f:-((int16_t)f))+row*64));
      hat4_add((row?93:0)+x,3,(uint8_t)(x*5+act*48),scale8(ribs,44));
    }
  }
  // Transfer flare occurs only when converging fronts meet at the seam.
  if(travel<=2){
    for(uint8_t row=0;row<2;row++){
      hat4_spot(row,hat_row_center(row),2,3,(uint8_t)(181+act*13),210);
    }
  }
  hat4_rear(3,(uint8_t)(p*2),27);
}

// 05 SCARLET ECLIPSE. An almost-black moving iris with hot cherry diffraction
// edges shutters around the center; as it closes, each front nucleus brightens
// and throws red-gold refraction teeth across the crown.


// 06 RIBBON ORIGAMI. Four continuous chapters fold two crossing strips into
// shifting triangular creases; gold-magenta folds invert through the precise
// front hinge and turn into a pair of independently timed luminous blades.
static void mode_hat4_ribbon_origami(void){
  if(SEGLEN<144)FX_FALLBACK_STATIC;
  SEGMENT.fill(BLACK);
  uint32_t t=strip.now;
  uint16_t cycle=7100U-(uint16_t)SEGMENT.speed*13U;
  uint8_t p=(uint32_t)(t%cycle)*255U/cycle,act=p/64U,q=(uint8_t)(p%64U)*4U;
  for(uint8_t row=0;row<2;row++)for(uint8_t x=0;x<51;x++){
    uint8_t d=abs((int)x-hat_row_center(row));
    uint8_t fold=(uint8_t)(d*13U+q+(act&1?row*57:row*103));
    uint8_t tri=fold<128?fold*2U:(255U-fold)*2U;
    uint8_t crease=qsub8(tri,139);
    int16_t diagonal=((int16_t)(x*7U)+(row?53:0)+(act&1?q:-((int16_t)q)))&255;
    uint8_t edge=hat4_bell(diagonal-128,12);
    uint8_t v=qadd8(scale8(crease,184),scale8(edge,173));
    if(act==2)v=scale8(v,(uint8_t)(206+(d&3)*10));
    hat4_add((row?93:0)+x,5,(uint8_t)(80+fold/2+act*34+row*18),v);
  }
  for(uint8_t row=0;row<2;row++){
    int16_t tip=hat_row_center(row)+((int16_t)hat3_sin8((uint8_t)(q+act*55+row*128))-128)/7;
    hat4_spot(row,tip,4,5,(uint8_t)(192+act*19),218);
    hat4_spot(row,hat_row_center(row)*2-tip,3,5,114,145);
    // Only a fold tip crossing the physical hinge creates a bright accent.
    if(abs(tip-hat_row_center(row))<=2)
      hat4_spot(row,hat_row_center(row),2,5,175,190);
  }
  hat4_rear(5,(uint8_t)(t/34),22);
}

// 07 MIDNIGHT REACTOR. Actual stateful heat propagation with 2x51 cells.
// Source energy enters at both front centers, is advected outward, diffuses,
// cross-couples between rows, and throws intermittent ruby-orange eruptions.
// About 208 bytes of per-segment heap, no persistent global state.





static const char _data_FX_MODE_HAT4_SWITCHBLADE_WALTZ[] PROGMEM =
  "Hat IV - Switchblade Waltz@Blade tempo,Intensity,,,,WLED palette;!,!;!;1;sx=147,ix=190,o1=0";
static const char _data_FX_MODE_HAT4_HOURGLASS_REVERSAL[] PROGMEM =
  "Hat IV - Hourglass Reversal@Cycle tempo,Focal energy,,,,WLED palette;!,!;!;1;sx=133,ix=200,o1=0";

static const char _data_FX_MODE_HAT4_RIBBON_ORIGAMI[] PROGMEM =
  "Hat IV - Ribbon Origami@Fold speed,Gleam,,,,WLED palette;!,!;!;1;sx=130,ix=192,o1=0";


class UserFxUsermod : public Usermod {
 private:
 public:
  void setup() override {
    strip.addEffect(255, &mode_diffusionfire, _data_FX_MODE_DIFFUSIONFIRE);
    strip.addEffect(255, &mode_spinning_wheel, _data_FX_MODE_SPINNINGWHEEL);
    strip.addEffect(255, &mode_2D_lavalamp, _data_FX_MODE_2D_LAVALAMP);
    strip.addEffect(255, &mode_2D_magma, _data_FX_MODE_2D_MAGMA);
    strip.addEffect(255, &mode_ants, _data_FX_MODE_ANTS);
    strip.addEffect(255, &mode_morsecode, _data_FX_MODE_MORSECODE);
    strip.addEffect(255, &mode_dissolveplus, _data_FX_MODE_DISSOLVEPLUS);
    strip.addEffect(255, &mode_nokia_snake, _data_FX_MODE_NOKIA_SNAKE);

    ////////////////////////////////////////
    //  add your effect function(s) here  //
    ////////////////////////////////////////

    // use id=255 for all custom user FX (the final id is assigned when adding the effect)

    // strip.addEffect(255, &mode_your_effect, _data_FX_MODE_YOUR_EFFECT);
    // strip.addEffect(255, &mode_your_effect2, _data_FX_MODE_YOUR_EFFECT2);
    // strip.addEffect(255, &mode_your_effect3, _data_FX_MODE_YOUR_EFFECT3);

    // Custom Chunchun: uses the 410-position virtual harness path while
    // preserving the normal 292-LED ledmap for all other effects.
    strip.addEffect(255, &mode_chunchun_harness, _data_FX_MODE_CHUNCHUN_HARNESS);

    // Z - Harness Visual Suite
    strip.addEffect(255, &mode_z_neural_pulse, _data_FX_MODE_Z_NEURAL_PULSE);
    strip.addEffect(255, &mode_z_plasma_veins, _data_FX_MODE_Z_PLASMA_VEINS);
    strip.addEffect(255, &mode_z_electric_organism, _data_FX_MODE_Z_ELECTRIC_ORGANISM);
    strip.addEffect(255, &mode_z_gravity_well, _data_FX_MODE_Z_GRAVITY_WELL);
    strip.addEffect(255, &mode_z_wormhole, _data_FX_MODE_Z_WORMHOLE);
    strip.addEffect(255, &mode_z_rainbow_fracture, _data_FX_MODE_Z_RAINBOW_FRACTURE);
    strip.addEffect(255, &mode_z_starlight, _data_FX_MODE_Z_STARLIGHT);

    // Z - Harness Visual Suite II (15 new effects)
    strip.addEffect(255, &mode_z_phoenix_flight, _data_FX_MODE_Z_PHOENIX_FLIGHT);
    strip.addEffect(255, &mode_z_comet_storm, _data_FX_MODE_Z_COMET_STORM);
    strip.addEffect(255, &mode_z_deep_current, _data_FX_MODE_Z_DEEP_CURRENT);

    // Z - Harness Visual Suite III (slow, mesmerizing, non-flashing)
strip.addEffect(255, &mode_z_tidal_interference, _data_FX_MODE_Z_TIDAL_INTERFERENCE);
    strip.addEffect(255, &mode_z_ember_drift, _data_FX_MODE_Z_EMBER_DRIFT);
    strip.addEffect(255, &mode_z_orbital_waltz, _data_FX_MODE_Z_ORBITAL_WALTZ);

    // Z - Harness Visual Suite IV (15 new slow cinematic effects)
strip.addEffect(255, &mode_z_silk_ribbon, _data_FX_MODE_Z_SILK_RIBBON);
strip.addEffect(255, &mode_z_twin_moons, _data_FX_MODE_Z_TWIN_MOONS);
    strip.addEffect(255, &mode_z_liquid_mercury, _data_FX_MODE_Z_LIQUID_MERCURY);
    strip.addEffect(255, &mode_z_echo_chamber, _data_FX_MODE_Z_ECHO_CHAMBER);
strip.addEffect(255, &mode_z_prism_cascade, _data_FX_MODE_Z_PRISM_CASCADE);
    strip.addEffect(255, &mode_z_gravity_lens, _data_FX_MODE_Z_GRAVITY_LENS);
    strip.addEffect(255, &mode_z_quiet_storm, _data_FX_MODE_Z_QUIET_STORM);
    strip.addEffect(255, &mode_z_infinite_corridor, _data_FX_MODE_Z_INFINITE_CORRIDOR);

    // Z - Hat FX Pack (144 physical LEDs; use these effects on the hat controller)


    strip.addEffect(255, &mode_hat_front_ripple, _data_FX_MODE_HAT_FRONT_RIPPLE);

    // Hat FX Pack II - five "brake" effects (fast -> slow at front center -> fast)
    strip.addEffect(255, &mode_hat_brake_comet, _data_FX_MODE_HAT_BRAKE_COMET);




    // Hat FX Pack II - nine more


    strip.addEffect(255, &mode_hat_mirror_bloom, _data_FX_MODE_HAT_MIRROR_BLOOM);
    strip.addEffect(255, &mode_hat_double_helix, _data_FX_MODE_HAT_DOUBLE_HELIX);

    strip.addEffect(255, &mode_hat_meteor_rain, _data_FX_MODE_HAT_METEOR_RAIN);
    strip.addEffect(255, &mode_hat_fire_crown, _data_FX_MODE_HAT_FIRE_CROWN);
    strip.addEffect(255, &mode_hat_pendulum, _data_FX_MODE_HAT_PENDULUM);


    // New Hat Spectacular III suite: 15 curated, multi-layer effects.
    strip.addEffect(255, &mode_hat3_prism_guillotine, _data_FX_MODE_HAT3_PRISM_GUILLOTINE);


    strip.addEffect(255, &mode_hat3_diamond_rain, _data_FX_MODE_HAT3_DIAMOND_RAIN);
    strip.addEffect(255, &mode_hat3_supercollider, _data_FX_MODE_HAT3_SUPERCOLLIDER);
    strip.addEffect(255, &mode_hat3_aurora_cathedral, _data_FX_MODE_HAT3_AURORA_CATHEDRAL);
    strip.addEffect(255, &mode_hat3_julia_rifts, _data_FX_MODE_HAT3_JULIA_RIFTS);
    strip.addEffect(255, &mode_hat3_neon_calligraphy, _data_FX_MODE_HAT3_NEON_CALLIGRAPHY);
    strip.addEffect(255, &mode_hat3_cosmic_pinball, _data_FX_MODE_HAT3_COSMIC_PINBALL);
    strip.addEffect(255, &mode_hat3_hologram_scanner, _data_FX_MODE_HAT3_HOLOGRAM_SCANNER);
    strip.addEffect(255, &mode_hat3_bioluminescent_reef, _data_FX_MODE_HAT3_BIOLUMINESCENT_REEF);
    strip.addEffect(255, &mode_hat3_electric_tessellation, _data_FX_MODE_HAT3_ELECTRIC_TESSELLATION);
    strip.addEffect(255, &mode_hat3_crown_of_seraphs, _data_FX_MODE_HAT3_CROWN_OF_SERAPHS);
    // Hat IV — Center Stage: seven new front-centered compositions

    strip.addEffect(255, &mode_hat4_switchblade_waltz, _data_FX_MODE_HAT4_SWITCHBLADE_WALTZ);
    strip.addEffect(255, &mode_hat4_hourglass_reversal, _data_FX_MODE_HAT4_HOURGLASS_REVERSAL);

    strip.addEffect(255, &mode_hat4_ribbon_origami, _data_FX_MODE_HAT4_RIBBON_ORIGAMI);


  }


  ///////////////////////////////////////////////////////////////////////////////////////////////
  //  If you want configuration options in the usermod settings page, implement these methods  //
  ///////////////////////////////////////////////////////////////////////////////////////////////

  // void addToConfig(JsonObject& root) override
  // {
  //   JsonObject top = root.createNestedObject(FPSTR("User FX"));
  //   top["myConfigValue"] = myConfigValue;
  // }
  // bool readFromConfig(JsonObject& root) override
  // {
  //   JsonObject top = root[FPSTR("User FX")];
  //   bool configComplete = !top.isNull();
  //   configComplete &= getJsonValue(top["myConfigValue"], myConfigValue);
  //   return configComplete;
  // }

  void loop() override {} // nothing to do in the loop
  uint16_t getId() override { return USERMOD_ID_USER_FX; }
};

static UserFxUsermod user_fx;
REGISTER_USERMOD(user_fx);