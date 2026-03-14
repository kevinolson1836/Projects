#include <Arduino.h>
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
#define BASE_CONSTANT_SPEED 250  // pulses per second when idle

// step ISRs
void stepISR0(){ motorState[0]=!motorState[0]; digitalWrite(stepPins[0], motorState[0]); }
void stepISR1(){ motorState[1]=!motorState[1]; digitalWrite(stepPins[1], motorState[1]); }
void stepISR2(){ motorState[2]=!motorState[2]; digitalWrite(stepPins[2], motorState[2]); }
void stepISR3(){ motorState[3]=!motorState[3]; digitalWrite(stepPins[3], motorState[3]); }

typedef void (*ISRFunction)();
ISRFunction isrFunctions[NUM_MOTORS] = {stepISR0, stepISR1, stepISR2, stepISR3};

// MIDI stuff
const int MAX_MIDI_FILES = 50;
String midiFiles[MAX_MIDI_FILES];
int midiCount = 0;
String currentMidi;
File midiFile;

volatile uint32_t currentTick = 0;
volatile uint16_t division = 480;
volatile uint32_t tempo = 500000;  // microseconds per quarter note
const int MIN_DURATION_MS = 5;

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

long noteToFreq(byte note) {
  if (note < 12 || note > 127) return 0;
  float freq = 440.0 * pow(2.0, (note - 69.0) / 12.0);
  freq = constrain(freq, 10, 2000);
  return (long)freq;
}

uint32_t readVLQ() {
  uint32_t value = 0;
  uint8_t c;
  do {
    if (!midiFile.available()) return 0;
    c = midiFile.read();
    value = (value << 7) | (c & 0x7F);
  } while (c & 0x80);
  return value;
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("Constant-Speed 4-Motor Stepper Controller");

  // init motors
  for (int i = 0; i < NUM_MOTORS; i++) {
    pinMode(stepPins[i], OUTPUT);
    pinMode(dirPins[i], OUTPUT);
    digitalWrite(dirPins[i], HIGH);
  }

  // init SD
  if (!SD.begin(BUILTIN_SDCARD)) {
    Serial.println("SD init failed!");
    while (1);
  }

  // scan for MIDI files
  File root = SD.open("/");
  while (true) {
    File entry = root.openNextFile();
    if (!entry) break;
    if (!entry.isDirectory()) {
      String name = entry.name();
      String upper = name;
      upper.toUpperCase();
      if (upper.endsWith(".MID")) {
        if (midiCount < MAX_MIDI_FILES) midiFiles[midiCount++] = name;
      }
    }
    entry.close();
  }
  root.close();

  if (midiCount == 0) {
    Serial.println("No MIDI files found!");
    while (1);
  }

  randomSeed(analogRead(A0));
  Serial.print("Found "); Serial.print(midiCount); Serial.println(" MIDI files");

  // start first file
  int idx = random(midiCount);
  currentMidi = midiFiles[idx];
  Serial.print("Playing: "); Serial.println(currentMidi);
  midiFile = SD.open(currentMidi.c_str());

  // parse header
  byte headerByte[4];
  for (int i = 0; i < 4; i++) headerByte[i] = midiFile.read();
  midiFile.seek(12);
  division = (midiFile.read() << 8) | midiFile.read();

  // start all motors at constant speed
  for (int i = 0; i < NUM_MOTORS; i++) {
    playMotor(i, BASE_CONSTANT_SPEED);
  }

  Serial.println("Motors running at constant speed. MIDI controls note dynamics.");
}

void loop() {
  if (!midiFile.available()) {
    // pick random next file
    int idx = random(midiCount);
    currentMidi = midiFiles[idx];
    Serial.print("Next: "); Serial.println(currentMidi);
    midiFile = SD.open(currentMidi.c_str());
    midiFile.seek(14);  // skip header, go to track
    currentTick = 0;
    return;
  }

  uint32_t delta = readVLQ();
  currentTick += delta;

  uint8_t status = midiFile.read();
  uint8_t channel = status & 0x0F;

  if ((status & 0xF0) == 0x90) {  // note on
    uint8_t note = midiFile.read();
    uint8_t velocity = midiFile.read();

    if (velocity == 0) {
      // note off-style
      return;
    }

    int motor = note % NUM_MOTORS;
    int freq = noteToFreq(note);
    digitalWrite(dirPins[motor], (note >= 60) ? HIGH : LOW);

    // play note but at current base speed
    if (freq > 0) {
      // just change direction, keep speed constant
      Serial.print("Note "); Serial.print(note); Serial.print(" -> Motor ");
      Serial.print(motor); Serial.print(" Dir: "); Serial.println((note >= 60) ? "FWD" : "REV");
    }
  } 
  else if ((status & 0xF0) == 0x80) {  // note off
    uint8_t note = midiFile.read();
    uint8_t velocity = midiFile.read();
    // motors keep spinning
  }
  else if (status == 0xFF) {  // meta event
    uint8_t metaType = midiFile.read();
    uint32_t length = readVLQ();

    if (metaType == 0x51) {  // set tempo
      tempo = 0;
      for (uint32_t i = 0; i < length; i++) {
        tempo = (tempo << 8) | midiFile.read();
      }
    } else {
      for (uint32_t i = 0; i < length; i++) midiFile.read();
    }
  }
  else {
    // skip unknown events
    uint32_t skip = readVLQ();
    midiFile.seek(midiFile.position() + skip);
  }

  delay(1);  // small delay to prevent loop spam
}
