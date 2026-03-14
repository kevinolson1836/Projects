#include <FastLED.h>

#define LED_PIN     3
#define NUM_LEDS    30
#define AUDIO_PIN   A3
#define LED_TYPE    WS2812B

#define SAMPLES       200
#define SMOOTH_ALPHA  0.4
#define FPS           256

#define MIN_BRIGHTNESS_FLOOR  0.15  // 0.0–1.0  (try 0.08–0.18)

CRGB leds[NUM_LEDS];
float smoothedAverage = 0;
float ledValues[NUM_LEDS];

// -------- Dynamic range tracking --------
float dynamicMax = 1;
float dynamicMin = 1023;

float attackSpeed  = 0.05;
float releaseSpeed = 0.03;


float MINattackSpeed  = 0.03;
float MINreleaseSpeed = 0.05;

// Green → Yellow → Orange → Red
uint8_t colorStops[5] = {96, 72, 32, 16, 0};
float currentHue = 96;

void setup() {
  Serial.begin(115200);
  FastLED.addLeds<LED_TYPE, LED_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(200);
  FastLED.clear();
  FastLED.show();

  for (int i = 0; i < NUM_LEDS; i++) ledValues[i] = 0;
}

void loop() {
  static unsigned long lastFrame = 0;
  if (millis() - lastFrame < 1000 / FPS) return;
  lastFrame = millis();

  // -------- Read & smooth audio --------
  float sum = 0;
  for (int i = 0; i < SAMPLES; i++) {
    sum += analogRead(AUDIO_PIN);
  }
  float average = sum / SAMPLES;

  smoothedAverage =
    smoothedAverage * (1.0 - SMOOTH_ALPHA) +
    average * SMOOTH_ALPHA;

  // -------- Dynamic MAX --------
  if (smoothedAverage > dynamicMax) {
    dynamicMax += (smoothedAverage - dynamicMax) * attackSpeed;
  } else {
    dynamicMax -= (dynamicMax - smoothedAverage) * releaseSpeed;
  }

  // -------- Dynamic MIN --------
  if (smoothedAverage < dynamicMin) {
    dynamicMin += (smoothedAverage - dynamicMin) * MINattackSpeed;
  } else {
    dynamicMin -= (dynamicMin - smoothedAverage) * MINreleaseSpeed;
  }

  // Prevent collapse
  if (dynamicMax - dynamicMin < 10) {
    dynamicMax = dynamicMin + 10;
  }

  // -------- Normalize --------
  float normalized =
    constrain((smoothedAverage - dynamicMin) /
              (dynamicMax - dynamicMin), 0.0, 1.0);

  // -------- Bias toward green --------
  float biased = pow(normalized, 1.1);

  // -------- Apply brightness floor (NEW) --------
  float visibleLevel =
    MIN_BRIGHTNESS_FLOOR +
    biased * (1.0 - MIN_BRIGHTNESS_FLOOR);

  // -------- Map to gradient --------
  float scaled = biased * 4.0;
  int idx = int(scaled);
  float frac = scaled - idx;

  uint8_t targetHue;
  if (idx >= 4) targetHue = colorStops[4];
  else targetHue =
    colorStops[idx] * (1 - frac) +
    colorStops[idx + 1] * frac;

  // -------- Smooth hue --------
  if (targetHue < currentHue) {
    currentHue -= (currentHue - targetHue) * releaseSpeed;
  } else {
    currentHue += (targetHue - currentHue) * (attackSpeed * 2);
  }

  // -------- LED trail --------
  for (int i = NUM_LEDS - 1; i > 0; i--) {
    ledValues[i] = max(ledValues[i] * 0.95, ledValues[i - 1]);
  }
  ledValues[0] = visibleLevel;

  // -------- Apply LEDs --------
  for (int i = 0; i < NUM_LEDS; i++) {
    float trailFade = 1.0 - (float)i / NUM_LEDS;
    uint8_t brightness = uint8_t(ledValues[i] * 255 * trailFade);
    leds[i] = CHSV(uint8_t(currentHue), 255, brightness);
  }

  FastLED.show();
}
