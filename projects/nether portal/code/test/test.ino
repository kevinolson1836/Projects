/*
 * Minecraft Nether Portal LED Effect
 * ESP32 + 103x WS2812B LEDs on DATA_PIN 27
 *
 * Effect layers (all blended together):
 *   1. BASE GLOW     – slow breathing purple/violet ambient
 *   2. PORTAL WAVE   – a luminous shimmer that sweeps around the frame
 *   3. SPARK TWINKLE – random bright flashes mimicking the in-game particles
 *   4. EDGE FLICKER  – slight random brightness variation per-LED for texture
 *
 * Tunable constants are at the top of the file.
 */

#include <FastLED.h>

// ─── Hardware ──────────────────────────────────────────────────────────────────
#define DATA_PIN        27
#define NUM_LEDS        103
#define LED_TYPE        WS2812B
#define COLOR_ORDER     GRB

// ─── Tweak These ───────────────────────────────────────────────────────────────
#define MASTER_BRIGHTNESS  255   // 0-255  (overall brightness cap)

// Base glow
#define GLOW_MIN_VAL       10    // darkest point of breathing cycle
#define GLOW_MAX_VAL       150   // brightest point of breathing cycle
#define GLOW_SPEED         0.3     // higher = faster breathing (~1 breath per 3s)

// Portal wave
#define WAVE_SPEED         10000  // ms for one full revolution (10 seconds)
#define WAVE_WIDTH         80    // tail length in LEDs — almost the full strip
#define WAVE_BRIGHTNESS    150   // peak brightness of the wave crest

// Spark twinkles
#define SPARK_CHANCE       18   // 1-in-N chance per LED per frame to spark
#define SPARK_DECAY        4    // how fast sparks fade (higher = shorter flash)
#define SPARK_MIN_BRIGHT   140   // minimum brightness for a new spark

// Edge flicker
#define FLICKER_AMOUNT     10    // ±random offset added to each LED per frame

// Portal colour palette  (Nether purple/magenta hues)
//   hue is in FastLED HSV space 0-255 (purple ≈ 192, violet ≈ 170, magenta ≈ 210)
#define HUE_CENTER         195   // dominant hue
#define HUE_RANGE          22    // ±hue wander around HUE_CENTER for variety

// ─── Globals ───────────────────────────────────────────────────────────────────
CRGB leds[NUM_LEDS];

uint8_t sparkBrightness[NUM_LEDS];  // per-LED spark intensity (decays each frame)

// ─── Helpers ───────────────────────────────────────────────────────────────────

// A nice S-curve for the wave falloff
uint8_t waveFalloff(int dist, int halfWidth) {
  if (dist >= halfWidth) return 0;
  // cosine ease: bright at centre, black at edges
  float t = (float)dist / (float)halfWidth;
  float cos_val = cosf(t * (float)M_PI / 2.0f);
  return (uint8_t)(cos_val * cos_val * 255.0f);
}

// Soft-light style blend: result is brighter than either input but not clipped harshly
uint8_t softAdd(uint8_t a, uint8_t b) {
  uint16_t sum = (uint16_t)a + b;
  return (uint8_t)(sum > 255 ? 255 : sum);
}

// ─── Setup ─────────────────────────────────────────────────────────────────────
void setup() {
  delay(300); // power-up safety
  FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS)
         .setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(MASTER_BRIGHTNESS);
  memset(sparkBrightness, 0, sizeof(sparkBrightness));
}

// ─── Main Loop ─────────────────────────────────────────────────────────────────
void loop() {
  uint32_t now = millis();

  // ── 1. BASE GLOW (breathing) ──────────────────────────────────────────────
  // beatsin8 produces a smooth 0-255 sine wave at ~GLOW_SPEED BPM
  uint8_t breathVal = beatsin8(GLOW_SPEED, GLOW_MIN_VAL, GLOW_MAX_VAL);

  // ── 2. ADVANCE WAVE POSITION (smooth float, millis-based) ────────────────
  //    wavePos is now a float 0..NUM_LEDS derived from millis() so the sweep
  //    is perfectly smooth and takes WAVE_SPEED ms per full revolution.
  float wavePosF = fmod((float)(now % (uint32_t)WAVE_SPEED) / (float)WAVE_SPEED
                        * (float)NUM_LEDS, (float)NUM_LEDS);

  // ── 3. RENDER EACH LED ────────────────────────────────────────────────────
  for (int i = 0; i < NUM_LEDS; i++) {

    // --- Hue: vary slightly per LED + slow drift for colour texture ---
    // Each LED gets its own slowly-drifting hue offset based on position + time
    uint8_t hue = HUE_CENTER
                + (uint8_t)((sin8((uint8_t)(i * 13 + now / 60)) - 128) * HUE_RANGE / 128);

    // --- Base glow layer ---
    uint8_t val = breathVal;

    // --- Wave layer ---
    // Distance from this LED to the wave front (circular, float-based for smoothness)
    float dist = (float)i - wavePosF;
    if (dist < 0) dist += (float)NUM_LEDS;
    // Long tail: wave illuminates almost the whole strip; only a small notch is dark
    int tailDist = (dist <= (float)NUM_LEDS / 2.0f) ? (int)dist : (int)(NUM_LEDS - dist);
    uint8_t waveContrib = waveFalloff(tailDist, WAVE_WIDTH);
    // Scale wave contribution
    waveContrib = scale8(waveContrib, WAVE_BRIGHTNESS);
    val = softAdd(val, waveContrib);

    // --- Spark / twinkle layer ---
    // Chance to spawn a new spark on this LED
    if (sparkBrightness[i] == 0 && random8() < (255 / SPARK_CHANCE)) {
      sparkBrightness[i] = random8(SPARK_MIN_BRIGHT, 255);
    }
    if (sparkBrightness[i] > 0) {
      val = softAdd(val, sparkBrightness[i]);
      // Decay
      uint8_t decay = SPARK_DECAY + random8(0, 8); // slight randomness in decay
      sparkBrightness[i] = (sparkBrightness[i] > decay) ? sparkBrightness[i] - decay : 0;
    }

    // --- Edge flicker ---
    int8_t flicker = random8(0, FLICKER_AMOUNT) - (FLICKER_AMOUNT / 2);
    int16_t vFinal = (int16_t)val + flicker;
    vFinal = constrain(vFinal, 0, 255);

    // --- Write colour ---
    // Slightly boost saturation for the wave crest (whiter = more magic-portal feel)
    uint8_t sat = 220 - scale8(waveContrib, 90); // wave crest desaturates slightly toward white
    leds[i] = CHSV(hue, sat, (uint8_t)vFinal);
  }

  // ── 4. SHOW ───────────────────────────────────────────────────────────────
  FastLED.show();

  // ~60 fps cap
  FastLED.delay(16);
}
