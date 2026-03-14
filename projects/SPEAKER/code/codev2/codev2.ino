#include <FastLED.h>

#define LED_PIN     3
#define NUM_LEDS    30
#define AUDIO_PIN   A3
#define LED_TYPE    WS2812B
#define FPS         200
#define SAMPLES     150

CRGB leds[NUM_LEDS];

// Audio
float smoothedAudio = 0;
float lastAudio = 0;
float energy = 0;

// Trail
float ledValues[NUM_LEDS];   // intensity 0-1
float currentHue[NUM_LEDS];  // hue per LED

// Rolling calibration
float dynamicMin = 1023;
float dynamicMax = 0;
const float CALIB_ATTACK = 0.01;
const float CALIB_RELEASE = 0.002;

// Color stops green->yellow->red
uint8_t colorStops[3] = {96, 64, 16}; // green → yellow → red

// Sensitivity factor
float SENSITIVITY = 1.6;

// How fast hue fades toward green along the trail
float TRAIL_FADE = 0.002;

// Audio smoothing factor (higher = smoother, 0–1)
float AUDIO_SMOOTH = 0.65;

// Trail smoothing factor (higher = smoother, 0–1)
float TRAIL_SMOOTH = 0.18;  // increased from 0.42 for smoother trail

void setup() {
  Serial.begin(115200);
  FastLED.addLeds<LED_TYPE, LED_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(220);
  FastLED.clear();
  FastLED.show();

  for (int i = 0; i < NUM_LEDS; i++) {
    ledValues[i] = 0;
    currentHue[i] = 96; // start green
  }
}

void loop() {
  static unsigned long lastFrame = 0;
  if (millis() - lastFrame < 1000 / FPS) return;
  lastFrame = millis();

  // -------- Read audio ----------
  float sum = 0;
  for (int i = 0; i < SAMPLES; i++) sum += analogRead(AUDIO_PIN);
  float sample = sum / SAMPLES;

  // -------- Rolling Calibration ----------
  if (sample > dynamicMax) dynamicMax += (sample - dynamicMax) * CALIB_ATTACK;
  else dynamicMax -= (dynamicMax - sample) * CALIB_RELEASE;

  if (sample < dynamicMin) dynamicMin += (sample - dynamicMin) * CALIB_ATTACK;
  else dynamicMin -= (dynamicMin - sample) * CALIB_RELEASE;

  if (dynamicMax - dynamicMin < 50) dynamicMax = dynamicMin + 50;

  // -------- Normalize & sensitivity with audio smoothing ----------
  smoothedAudio = smoothedAudio * AUDIO_SMOOTH + sample * (1.0 - AUDIO_SMOOTH);
  float normalized = constrain((smoothedAudio - dynamicMin) / (dynamicMax - dynamicMin), 0.0, 1.0);
  normalized = pow(normalized, 1.0 / SENSITIVITY);

  // -------- Energy detection ----------
  energy = abs(normalized - lastAudio);
  lastAudio = normalized;

  // -------- Spike intensity for red --------
  float spike = 0;
  if (energy > 0.004) {
    spike = constrain((energy - 0.03) * 15.0, 0.0, 1.0);
  }

  float visual = max(normalized, spike);
  visual = constrain(visual, 0.05, 1.0);

  // -------- Shift trail down with smooth fading ----------
  for (int i = NUM_LEDS - 1; i > 0; i--) {
    // Smoothly blend previous LED into current
    ledValues[i] = ledValues[i] * TRAIL_SMOOTH + ledValues[i - 1] * (1.0 - TRAIL_SMOOTH);
    // Smooth hue toward green along trail
    currentHue[i] = currentHue[i] + (currentHue[i - 1] - currentHue[i]) * (1.0 - TRAIL_SMOOTH);
    currentHue[i] = currentHue[i] + (48 - currentHue[i]) * TRAIL_FADE;
  }

  // -------- Head LED --------
  ledValues[0] = visual;

  // Map visual intensity to color stops: green -> yellow -> orange -> red
  uint8_t headHue;
  if (visual < 0.33) {          // green -> yellow
      float f = visual / 0.33;
      headHue = colorStops[0] * (1 - f) + colorStops[1] * f;
  } else if (visual < 0.37) {   // yellow -> orange
      float f = (visual - 0.33) / 0.33;
      headHue = colorStops[1] * (1 - f) + 32 * f;  // 32 = orange hue
  } else {                       // orange -> red
      float f = (visual - 0.33) / 0.34;
      headHue = 32 * (1 - f) + colorStops[2] * f; // colorStops[2] = red
  }

  currentHue[0] = headHue;

  // -------- Output LEDs ----------
  for (int i = 0; i < NUM_LEDS; i++) {
    uint8_t brightness = constrain(ledValues[i] * 255, 25, 255);
    leds[i] = CHSV(currentHue[i], 255, brightness);
  }

  FastLED.show();
}
