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

// list of WAV files
const int MAX_AUDIO_FILES = 50;
String audioFiles[MAX_AUDIO_FILES];
int audioCount = 0;
String currentAudio;

// audio objects
AudioOutputI2S        i2sOut;
AudioPlaySdWav        wavplayer;
AudioConnection       patchWav_0(wavplayer, 0, i2sOut, 0);
AudioConnection       patchWav_1(wavplayer, 1, i2sOut, 1);
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
  AudioMemory(25);
  delay(200);
  sgtl5000_1.enable();
  delay(100);
  sgtl5000_1.volume(0.95);
  delay(100);
  Serial.println("Audio codec initialized");

  // init SD
  if (!SD.begin(BUILTIN_SDCARD)) {
    Serial.println("SD init failed!");
    while (1);
  }

  // scan for WAV files
  File root = SD.open("/");
  while (true) {
    File entry = root.openNextFile();
    if (!entry) break;
    if (!entry.isDirectory()) {
      String name = entry.name();
      String upper = name;
      upper.toUpperCase();
      if (upper.endsWith(".WAV")) {
        if (audioCount < MAX_AUDIO_FILES) audioFiles[audioCount++] = name;
      }
    }
    entry.close();
  }
  root.close();

  if (audioCount == 0) {
    Serial.println("No WAV files found!");
    while (1);
  }

  randomSeed(analogRead(A0));
  Serial.print("Found "); Serial.print(audioCount); Serial.println(" WAV files");
  
  for (int i = 0; i < audioCount && i < 5; i++) {
    Serial.print("  File "); Serial.print(i); Serial.print(": "); Serial.println(audioFiles[i]);
  }

  // start first file
  int idx = random(audioCount);
  currentAudio = audioFiles[idx];
  Serial.print("Starting initial: "); Serial.println(currentAudio);
  File testFile = SD.open(currentAudio.c_str());
  if (testFile) {
    Serial.print("File size: "); Serial.println(testFile.size());
    testFile.close();
  } else {
    Serial.println("ERROR: Could not open file!");
  }
  wavplayer.play(currentAudio.c_str());
  delay(200);

  // start motors at constant speed
  for (int i = 0; i < NUM_MOTORS; i++) {
    playMotor(i, CONSTANT_SPEED);
  }
}

void loop() {
  // check if current file finished
  bool playing = wavplayer.isPlaying();
  static unsigned long lastCheck = 0;
  if (millis() - lastCheck > 2000) {
    Serial.print("Playing: "); Serial.print(playing); 
    Serial.print(" | Offset: "); Serial.print(wavplayer.positionMillis());
    Serial.print(" | File: "); Serial.println(currentAudio);
    lastCheck = millis();
  }
  
  if (!playing && millis() > 5000) {
    // pick another random file
    int idx = random(audioCount);
    currentAudio = audioFiles[idx];
    Serial.print("Starting: "); Serial.println(currentAudio);
    wavplayer.play(currentAudio.c_str());
  }

  delay(100);
}
