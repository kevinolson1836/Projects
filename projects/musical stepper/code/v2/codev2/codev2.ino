#include <Arduino.h>
#include <IntervalTimer.h>
#include <SD.h>
#include <SPI.h>

// --- configuration ---------------------------------------------------------
const int NUM_MOTORS = 5;
const uint8_t stepPins[NUM_MOTORS] = {2, 4, 6, 8, 10};
const uint8_t dirPins[NUM_MOTORS]  = {3, 5, 7, 9, 11};

// list of MIDI files discovered on card
const int MAX_MIDI_FILES = 50;
String midiFiles[MAX_MIDI_FILES];
int midiCount = 0;

// name of the currently playing file (for logging)
String currentMidi;

// Interval timers for toggling step pins at the appropriate rate
IntervalTimer timers[NUM_MOTORS];
volatile bool motorState[NUM_MOTORS] = {0};

#define MICROSTEPPING_FACTOR 1   // 1 = full step, 2 = half, 16 = 1/16 etc.  reduce for more pulses per second
#define SPEED_MULTIPLIER 3      // scale note frequency to stepper speed (smaller -> lower pitch)
#define MIN_FREQUENCY 5         // don't step slower than this
#define MAX_FREQUENCY 30000       // cap to a reasonable maximum
typedef void (*ISRFunction)();
ISRFunction isrFunctions[NUM_MOTORS];

// explicit step ISR routines for each motor
void stepISR0() { motorState[0] = !motorState[0]; digitalWrite(stepPins[0], motorState[0]); }
void stepISR1() { motorState[1] = !motorState[1]; digitalWrite(stepPins[1], motorState[1]); }
void stepISR2() { motorState[2] = !motorState[2]; digitalWrite(stepPins[2], motorState[2]); }
void stepISR3() { motorState[3] = !motorState[3]; digitalWrite(stepPins[3], motorState[3]); }
void stepISR4() { motorState[4] = !motorState[4]; digitalWrite(stepPins[4], motorState[4]); }

File midiFile;
uint32_t currentTick = 0;     // accumulated delta time in ticks
uint8_t lastStatus = 0;       // remember most recent status byte for running status

// timing from MIDI header/tempo
uint16_t division = 480;      // ticks per quarter note (from file)
uint32_t tempo = 500000;      // microseconds per quarter note default (120 BPM)
uint32_t currentBPM = 120;

// motor assignment
int noteToMotor[128];         // which motor is playing each MIDI note (-1 = none)
// track when each note began (in ticks) for duration calculation
uint32_t noteStartTick[128];

#define MIN_DURATION_MS 5  // ignore extremely short notes (adjust as needed)

// convert MIDI note number into a frequency for the stepper
int noteToFreq(byte note) {
  float hz = 440.0 * powf(2, (note - 69) / 12.0);
  if (hz < 10) hz = 10;
  if (hz > 2000) hz = 2000;
  return (int)hz;
}

// play a motor by starting/stopping its interval timer at the given frequency
void playMotor(int motorIndex, int freq) {
  if (motorIndex < 0 || motorIndex >= NUM_MOTORS) return;
  if (freq <= 0) {
    timers[motorIndex].end();
    motorState[motorIndex] = false;
    digitalWrite(stepPins[motorIndex], LOW);
    Serial.print("motor "); Serial.print(motorIndex);
    Serial.println(" OFF");
    return;
  }
  // restart timer with new interval
  timers[motorIndex].end();
  uint32_t interval = (1000000UL * MICROSTEPPING_FACTOR) / (uint32_t)(freq * 2);
  if (interval < 10) interval = 10;
  timers[motorIndex].begin(isrFunctions[motorIndex], interval);
  Serial.print("motor "); Serial.print(motorIndex);
  Serial.print(" ON, freq="); Serial.print(freq);
  Serial.print(" interval="); Serial.println(interval);
}

// helper: read a single variable-length quantity from the file
bool readVLQ(File &f, uint32_t &value) {
  value = 0;
  uint8_t b;
  do {
    if (!f.available()) return false;
    b = f.read();
    value = (value << 7) | (b & 0x7F);
  } while (b & 0x80);
  return true;
}


void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 2000);
  Serial.println("Teensy stepper MIDI player starting");

  // initialise GPIO
  for (int i = 0; i < NUM_MOTORS; i++) {
    pinMode(stepPins[i], OUTPUT);
    pinMode(dirPins[i], OUTPUT);
    digitalWrite(dirPins[i], HIGH);
    // prepare the ISR function pointers
  }

  // fill ISR table (use named functions defined below)
  isrFunctions[0] = stepISR0;
  isrFunctions[1] = stepISR1;
  isrFunctions[2] = stepISR2;
  isrFunctions[3] = stepISR3;
  isrFunctions[4] = stepISR4;

  // initialise note mapping and start times
  for (int i = 0; i < 128; i++) {
    noteToMotor[i] = -1;
    noteStartTick[i] = 0;
  }

  // initialise SD
  if (!SD.begin(BUILTIN_SDCARD)) {
    Serial.println("SD init failed!");
    while (1);
  }

  // scan for MID files on root
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
    Serial.println("No MIDI files found on SD");
    while (1);
  }
  randomSeed(analogRead(A0));
  int idx = random(midiCount);
  currentMidi = midiFiles[idx];
  Serial.print("Opening "); Serial.println(currentMidi);
  midiFile = SD.open(currentMidi.c_str());
  if (!midiFile) {
    Serial.print("Cannot open ");
    Serial.println(currentMidi);
    while (1);
  }

  // parse MIDI header to get division and move to first track
  char id[5] = {0};
  midiFile.read(id, 4); // should be "MThd"
  uint32_t headerLen = (midiFile.read() << 24) | (midiFile.read() << 16) | (midiFile.read() << 8) | midiFile.read();
  (void)headerLen;
  midiFile.read(); midiFile.read(); // format
  midiFile.read(); midiFile.read(); // nTracks
  division = (midiFile.read() << 8) | midiFile.read();
  Serial.print("DIV="); Serial.println(division);

  // locate first track chunk
  while (midiFile.available()) {
    midiFile.read(id, 4);
    uint32_t len = (midiFile.read() << 24) | (midiFile.read() << 16) | (midiFile.read() << 8) | midiFile.read();
    if (strncmp(id, "MTrk", 4) == 0) {
      Serial.println("TRACK FOUND");
      break;
    }
    if (midiFile.position() + len < midiFile.size()) midiFile.seek(midiFile.position() + len);
    else {
      Serial.println("NO TRACK FOUND");
      while (1);
    }
  }

  Serial.println("Ready to play");
}

void loop() {
  if (!midiFile) return;

  while (midiFile.available()) {
    uint32_t delta;
    if (!readVLQ(midiFile, delta)) break;

    // advance tick counter so note durations are tracked
    currentTick += delta;

    // convert delta ticks to milliseconds using current tempo
    uint32_t waitMs = (delta * 60000UL) / division / currentBPM;
    Serial.print("delta=" ); Serial.print(delta);
    Serial.print(" ticks -> wait=" ); Serial.print(waitMs);
    Serial.println(" ms");
    if (waitMs > 0 && waitMs < 10000) {
      delay(waitMs);
    }

    uint8_t status = midiFile.read();
    // running status handling
    if (status < 0x80) {
      midiFile.seek(midiFile.position() - 1);
      status = lastStatus;
    } else {
      lastStatus = status;
    }

    if ((status & 0xF0) == 0x90) {
      // note on
      uint8_t note = midiFile.read();
      uint8_t vel = midiFile.read();
      Serial.print("NOTE ON: "); Serial.print(note);
      Serial.print(" vel="); Serial.println(vel);
      if (vel == 0) {
        // note ON with velocity 0 -> treat as note off
        if (noteToMotor[note] >= 0) {
          playMotor(noteToMotor[note], 0);
          noteToMotor[note] = -1;
        }
      } else {
        // record start tick for duration measurement
        noteStartTick[note] = currentTick;
        int freq = noteToFreq(note);
        // scale by velocity and multiplier
        freq = (freq * vel) / 127;
        // adjust for microstepping: more microsteps per full-step increases pulse rate
        long pulses = (long)freq * SPEED_MULTIPLIER * MICROSTEPPING_FACTOR;
        if (pulses < MIN_FREQUENCY) pulses = MIN_FREQUENCY;
        if (pulses > MAX_FREQUENCY) pulses = MAX_FREQUENCY;
        Serial.print("  -> base freq "); Serial.print(freq);
        Serial.print(" pulses/sec " ); Serial.println(pulses);
        freq = (int)pulses;  // reuse variable for interval calculation
        // find a free motor
        int motorToUse = -1;
        for (int i = 0; i < NUM_MOTORS; i++) {
          bool inUse = false;
          for (int j = 0; j < 128; j++) {
            if (noteToMotor[j] == i) { inUse = true; break; }
          }
          if (!inUse) { motorToUse = i; break; }
        }
        if (motorToUse == -1) motorToUse = 0;
        Serial.print("  -> assign motor "); Serial.println(motorToUse);
        noteToMotor[note] = motorToUse;
        // set direction: notes >= middle C (60) -> forward, below -> reverse
        if (note >= 60) digitalWrite(dirPins[motorToUse], HIGH);
        else digitalWrite(dirPins[motorToUse], LOW);
        playMotor(motorToUse, freq);
      }
    } else if ((status & 0xF0) == 0x80) {
      // note off
      uint8_t note = midiFile.read();
      midiFile.read();
      Serial.print("NOTE OFF: "); Serial.println(note);
      // compute duration
      uint32_t lenTicks = currentTick - noteStartTick[note];
      uint32_t lenMs = (lenTicks * 60000UL) / division / currentBPM;
      Serial.print("  duration "); Serial.print(lenMs); Serial.println(" ms");
      if (lenMs < MIN_DURATION_MS) {
        Serial.println("  too short, skipping motor");
        noteToMotor[note] = -1;
      } else if (noteToMotor[note] >= 0) {
        playMotor(noteToMotor[note], 0);
        noteToMotor[note] = -1;
      }
    } else if ((status & 0xF0) == 0xB0) {
      midiFile.read(); midiFile.read();
    } else if ((status & 0xF0) == 0xC0) {
      midiFile.read();
    } else if ((status & 0xF0) == 0xD0) {
      midiFile.read();
    } else if ((status & 0xF0) == 0xE0) {
      midiFile.read(); midiFile.read();
    } else if (status == 0xFF) {
      uint8_t type = midiFile.read();
      uint32_t len;
      readVLQ(midiFile, len);
      if (type == 0x51 && len == 3) {
        tempo = (midiFile.read() << 16) | (midiFile.read() << 8) | midiFile.read();
        if (tempo == 0) tempo = 500000;
        currentBPM = 60000000UL / tempo;
      } else {
        while (len-- && midiFile.available()) midiFile.read();
      }
    } else {
      // unknown/status; skip one data byte safely
      midiFile.read();
    }

    // nothing to run here; interval timers handle stepping automatically
  }

  // reached end of file: close and choose another file at random
  midiFile.close();
  Serial.println("Finished reading MIDI");
  int idx = random(midiCount);
  currentMidi = midiFiles[idx];
  Serial.print("Next file: "); Serial.println(currentMidi);
  midiFile = SD.open(currentMidi.c_str());
  if (!midiFile) {
    Serial.print("Cannot open "); Serial.println(currentMidi);
    while (1);
  }
  // parse header and locate first track again (same as setup)
  char id[5] = {0};
  midiFile.read(id, 4); // MThd
  uint32_t headerLen = (midiFile.read() << 24) | (midiFile.read() << 16) | (midiFile.read() << 8) | midiFile.read();
  (void)headerLen;
  midiFile.read(); midiFile.read(); // format
  midiFile.read(); midiFile.read(); // nTracks
  division = (midiFile.read() << 8) | midiFile.read();
  Serial.print("DIV="); Serial.println(division);
  while (midiFile.available()) {
    midiFile.read(id, 4);
    uint32_t len = (midiFile.read() << 24) | (midiFile.read() << 16) | (midiFile.read() << 8) | midiFile.read();
    if (strncmp(id, "MTrk", 4) == 0) {
      Serial.println("TRACK FOUND");
      break;
    }
    if (midiFile.position() + len < midiFile.size()) midiFile.seek(midiFile.position() + len);
    else {
      Serial.println("NO TRACK FOUND");
      while (1);
    }
  }
}
