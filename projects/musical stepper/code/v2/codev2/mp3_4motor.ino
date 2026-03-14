#include <Arduino.h>
#include <Audio.h>
#include <Wire.h>
#include <SD.h>
#include <SPI.h>
#include <IntervalTimer.h>

// 4 motors
const int NUM_MOTORS = 4;
const uint8_t stepPins[NUM_MOTORS] = {2, 4, 6, 8};
const uint8_t dirPins[NUM_MOTORS]  = {3, 5, 7, 9};

IntervalTimer timers[NUM_MOTORS];
volatile bool motorState[NUM_MOTORS] = {0};

#define MICROSTEPPING_FACTOR 4
#define CONSTANT_SPEED 500  // pulses per second for all motors

// step ISRs
void stepISR0(){ motorState[0]=!motorState[0]; digitalWrite(stepPins[0], motorState[0]); }
void stepISR1(){ motorState[1]=!motorState[1]; digitalWrite(stepPins[1], motorState[1]); }
void stepISR2(){ motorState[2]=!motorState[2]; digitalWrite(stepPins[2], motorState[2]); }
void stepISR3(){ motorState[3]=!motorState[3]; digitalWrite(stepPins[3], motorState[3]); }

typedef void (*ISRFunction)();
ISRFunction isrFunctions[NUM_MOTORS] = {stepISR0, stepISR1, stepISR2, stepISR3};

// list of MP3 files
const int MAX_MP3_FILES = 50;
String mp3Files[MAX_MP3_FILES];
int mp3Count = 0;
String currentMp3;

// audio objects
AudioInputI2S         i2sIn;
AudioOutputI2S        i2sOut;
AudioPlaySdMp3        mp3player;
AudioConnection       patchIn0(i2sIn, 0, i2sOut, 0);
AudioConnection       patchIn1(i2sIn, 1, i2sOut, 1);
AudioConnection       patchMp3_0(mp3player, 0, i2sOut, 0);
AudioConnection       patchMp3_1(mp3player, 1, i2sOut, 1);
AudioControlSGTL5000  sgtl5000_1;

void playMotor(int m, int freq) {
  if (m < 0 || m >= NUM_MOTORS) return;
  if (freq <= 0) {
    timers[m].end();
    digitalWrite(stepPins[m], LOW);
    return;
  }
  timers[m].end();
  uint32_t interval = (1000000UL * MICROSTEPPING_FACTOR) / (uint32_t)(freq * 2);
  if (interval < 10) interval = 10;
  timers[m].begin(isrFunctions[m], interval);
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("MP3 Motor Controller Starting");

  // init motors
  for (int i = 0; i < NUM_MOTORS; i++) {
    pinMode(stepPins[i], OUTPUT);
    pinMode(dirPins[i], OUTPUT);
    digitalWrite(dirPins[i], HIGH);
  }

  // init audio
  AudioMemory(10);
  sgtl5000_1.enable();
  sgtl5000_1.volume(0.5);

  // init SD
  if (!SD.begin(BUILTIN_SDCARD)) {
    Serial.println("SD init failed!");
    while (1);
  }

  // scan for MP3 files
  File root = SD.open("/");
  while (true) {
    File entry = root.openNextFile();
    if (!entry) break;
    if (!entry.isDirectory()) {
      String name = entry.name();
      String upper = name;
      upper.toUpperCase();
      if (upper.endsWith(".MP3")) {
        if (mp3Count < MAX_MP3_FILES) mp3Files[mp3Count++] = name;
      }
    }
    entry.close();
  }
  root.close();

  if (mp3Count == 0) {
    Serial.println("No MP3 files found!");
    while (1);
  }

  randomSeed(analogRead(A0));
  Serial.print("Found "); Serial.print(mp3Count); Serial.println(" MP3 files");

  // start first file
  int idx = random(mp3Count);
  currentMp3 = mp3Files[idx];
  Serial.print("Playing: "); Serial.println(currentMp3);
  mp3player.play(currentMp3.c_str());

  // start motors at constant speed
  for (int i = 0; i < NUM_MOTORS; i++) {
    playMotor(i, CONSTANT_SPEED);
  }
}

void loop() {
  // check if current file finished
  if (!mp3player.isPlaying()) {
    // pick another random file
    int idx = random(mp3Count);
    currentMp3 = mp3Files[idx];
    Serial.print("Next: "); Serial.println(currentMp3);
    mp3player.play(currentMp3.c_str());
  }

  delay(100);
}
