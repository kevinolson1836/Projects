#include <SD.h>

#define SD_CS BUILTIN_SDCARD
#define MOTOR_PIN_1 2
#define MOTOR_PIN_2 4
#define MOTOR_PIN_3 6
#define MOTOR_PIN_4 8
// #define MOTOR_PIN_5 10

#define dir_PIN_1 3
#define dir_PIN_2 5
#define dir_PIN_3 7
#define dir_PIN_4 9
#define dir_PIN_5 11


#define NUM_MOTORS 4
#define NUM_MOTORS_DIR 5
#define MICROSTEPPING_FACTOR 16  // Set to 1 for full-step, 2 for half-step, 16 for 1/16-step (A4988)

IntervalTimer timers[NUM_MOTORS];
volatile bool motorState[NUM_MOTORS] = {0};

File f;
uint16_t division = 480;
uint32_t tempo = 500000; // default 120 BPM (in microseconds per quarter note)
uint32_t currentBPM = 120; // calculated from tempo
byte lastStatus = 0;
 
int motorPins[NUM_MOTORS] = {MOTOR_PIN_1, MOTOR_PIN_2, MOTOR_PIN_3, MOTOR_PIN_4};
int dirPins[NUM_MOTORS_DIR] = {dir_PIN_1, dir_PIN_2, dir_PIN_3, dir_PIN_4, dir_PIN_5};
int noteToMotor[128];  // Track which motor plays each note (-1 = not playing)


// -------- ISR FUNCTIONS FOR EACH MOTOR --------
void stepISR_0() {
  motorState[0] = !motorState[0];
  digitalWrite(motorPins[0], motorState[0]);
}

void stepISR_1() {
  motorState[1] = !motorState[1];
  digitalWrite(motorPins[1], motorState[1]);
}

void stepISR_2() {
  motorState[2] = !motorState[2];
  digitalWrite(motorPins[2], motorState[2]);
}

void stepISR_3() {
  motorState[3] = !motorState[3];
  digitalWrite(motorPins[3], motorState[3]);
}

typedef void (*ISRFunction)();
ISRFunction isrFunctions[NUM_MOTORS] = {stepISR_0, stepISR_1, stepISR_2, stepISR_3};


// -------- NOTE → STEPPER FREQUENCY --------
int noteToFreq(byte note) {
  float hz = 440.0 * powf(2, (note - 69) / 12.0);
  Serial.print("  Note ");
  Serial.print(note);
  Serial.print(" = ");
  Serial.print(hz, 1);
  Serial.println(" Hz");
  
  // Clamp to safe stepper range
  if(hz < 10) hz = 10;
  if(hz > 2000) hz = 2000;
  
  return hz;
}

// -------- PLAY NOTE ON SPECIFIC MOTOR --------
void playMotor(int motorIndex, int freq) {
  if (motorIndex < 0 || motorIndex >= NUM_MOTORS) return;
  
  if(freq <= 0) {
    timers[motorIndex].end();
    Serial.print("Motor ");
    Serial.print(motorIndex + 1);
    Serial.println(" OFF");
    return;
  }
  
  // stop any previous timer on this motor before starting
  timers[motorIndex].end();
  // compute interval in microseconds, compensating for microstepping
  // With half-step mode, we need to double the interval to get correct frequencies
  uint32_t interval = (1000000UL * MICROSTEPPING_FACTOR) / (uint32_t)(freq * 2);
  if (interval < 10) interval = 10;
  timers[motorIndex].begin(isrFunctions[motorIndex], interval);
  Serial.print("Motor ");
  Serial.print(motorIndex + 1);
  Serial.print(" ON, freq=");
  Serial.println(freq);
}


// -------- READ MIDI VARIABLE LENGTH --------
uint32_t readVar() {
  uint32_t v = 0;
  byte c;
  do {
    c = f.read();
    v = (v << 7) | (c & 0x7F);
  } while(c & 0x80);
  return v;
}

// -------- SETUP --------
void setup() {
  Serial.begin(115200);
  delay(1500);
  Serial.println("START");

  for (int i = 0; i < NUM_MOTORS; i++) {
    pinMode(motorPins[i], OUTPUT);
  }
  
    pinMode(dir_PIN_1, OUTPUT);
    pinMode(dir_PIN_2, OUTPUT);
    pinMode(dir_PIN_3, OUTPUT);
    pinMode(dir_PIN_4, OUTPUT);
    pinMode(dir_PIN_5, OUTPUT);

    digitalWrite(dir_PIN_1, HIGH); // Set direction for motor 1
    digitalWrite(dir_PIN_2, HIGH); // Set direction for motor 2
    digitalWrite(dir_PIN_3, HIGH); // Set direction for motor 3 
    digitalWrite(dir_PIN_4, HIGH); // Set direction for motor 4 
    digitalWrite(dir_PIN_5, HIGH); // Set direction for motor 5 


  // initialize note->motor mapping to -1 (not playing)
  for (int i = 0; i < 128; i++) noteToMotor[i] = -1;

  if (!SD.begin(SD_CS)) {
    Serial.println("SD FAIL");
    while (1);
  }
  Serial.println("SD OK");

  f = SD.open("mario-theme.MID");
  if (!f) {
    Serial.println("NO FILE");
    while (1);
  }
  Serial.println("FILE OK");

  // -------- READ HEADER --------
  char id[5] = {0};
  f.read(id, 4);  // MThd
  uint32_t headerLen = (f.read() << 24) | (f.read() << 16) | (f.read() << 8) | f.read();
  (void)headerLen;
  f.read(); f.read(); // format
  f.read(); f.read(); // nTracks
  division = (f.read() << 8) | f.read();
  Serial.print("DIV=");
  Serial.println(division);

  // -------- FIND FIRST TRACK --------
  while (f.available()) {
    f.read(id, 4);
    uint32_t len = (f.read() << 24) | (f.read() << 16) | (f.read() << 8) | f.read();
    if (strncmp(id, "MTrk", 4) == 0) {
      Serial.println("TRACK FOUND");
      break;
    }
    if (f.position() + len < f.size()) f.seek(f.position() + len);
    else {
      Serial.println("NO TRACK FOUND");
      while (1);
    }
  }
}


// -------- LOOP --------
void loop() {
  while (f.available()) {
    uint32_t delta = readVar();
    
    // -------- CONVERT DELTA TICKS TO MILLISECONDS AND WAIT --------
    // Use tempo from MIDI file to calculate proper timing
    uint32_t waitMs = (delta * 60000UL) / division / currentBPM;
    if (waitMs > 0 && waitMs < 10000) { // Skip absurdly long waits
      delay(waitMs);
    }

    byte status = f.read();

    // -------- HANDLE RUNNING STATUS --------
    if (status < 0x80) {
      f.seek(f.position() - 1);
      status = lastStatus;
      Serial.print("Using running status: 0x");
      Serial.println(status, HEX);
    } else {
      lastStatus = status;
    }

    // -------- NOTE ON (0x90-0x9F) --------
    if ((status & 0xF0) == 0x90) {
      byte note = f.read();
      byte vel = f.read();
      Serial.print("NOTE ON: ");
      Serial.print(note);
      Serial.print(" vel=");
      Serial.println(vel);

      if (vel == 0) {
        // Note ON with velocity 0 is treated as NOTE OFF
        if (noteToMotor[note] >= 0) {
          playMotor(noteToMotor[note], 0);
          noteToMotor[note] = -1;
        }
      } else {
        int freq = noteToFreq(note);
        
        // Find next available motor
        int motorToUse = -1;
        for (int i = 0; i < NUM_MOTORS; i++) {
          int isInUse = 0;
          for (int j = 0; j < 128; j++) {
            if (noteToMotor[j] == i) {
              isInUse = 1;
              break;
            }
          }
          if (!isInUse) {
            motorToUse = i;
            break;
          }
        }
        
        // If no free motor, reuse the oldest one
        if (motorToUse == -1) {
          motorToUse = 0;
        }
        
        noteToMotor[note] = motorToUse;
        Serial.print("  -> Motor ");
        Serial.println(motorToUse + 1);
        playMotor(motorToUse, freq);
      }
    }
    // -------- NOTE OFF (0x80-0x8F) --------
    else if ((status & 0xF0) == 0x80) {
      byte note = f.read();
      f.read(); // velocity
      Serial.print("NOTE OFF: ");
      Serial.println(note);
      
      // Stop the motor playing this specific note
      if (noteToMotor[note] >= 0) {
        int motor = noteToMotor[note];
        Serial.print("  -> Stop Motor ");
        Serial.println(motor + 1);
        playMotor(motor, 0);
        noteToMotor[note] = -1;
      }
    }
    // -------- CONTROL CHANGE (0xB0-0xBF) --------
    else if ((status & 0xF0) == 0xB0) {
      f.read(); // controller
      f.read(); // value
      Serial.println("Control Change (ignored)");
    }
    // -------- PROGRAM CHANGE (0xC0-0xCF) - 1 data byte --------
    else if ((status & 0xF0) == 0xC0) {
      f.read(); // program
      Serial.println("Program Change (ignored)");
    }
    // -------- CHANNEL PRESSURE (0xD0-0xDF) - 1 data byte --------
    else if ((status & 0xF0) == 0xD0) {
      f.read(); // pressure
      Serial.println("Channel Pressure (ignored)");
    }
    // -------- PITCH BEND (0xE0-0xEF) --------
    else if ((status & 0xF0) == 0xE0) {
      f.read(); // LSB
      f.read(); // MSB
      Serial.println("Pitch Bend (ignored)");
    }
    // -------- META EVENTS (0xFF) --------
    else if (status == 0xFF) {
      byte type = f.read();
      uint32_t len = readVar();
      if (type == 0x51 && len == 3) {
        tempo = (f.read() << 16) | (f.read() << 8) | f.read();
        if (tempo == 0) tempo = 500000; // guard against division by zero
        currentBPM = 60000000UL / tempo;  // Convert microseconds per beat to BPM
        Serial.print("TEMPO CHANGE: ");
        Serial.print(tempo);
        Serial.print(" us/beat = ");
        Serial.print(currentBPM);
        Serial.println(" BPM");
      } else {
        while (len--) f.read();
      }
    }
    else {
      Serial.print("Unknown status: 0x");
      Serial.println(status, HEX);
      // skip unknown message safely
      f.read();
    }
  }

  Serial.println("DONE");
  while (1);
}
