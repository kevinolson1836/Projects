# Teensy Musical Stepper - AI Coding Guidelines

## Code Style

- **Language**: C++ (Arduino sketch format for Teensy)
- **Pin Definitions**: Use `#define` constants at top of file (e.g., `STEP_PIN`, `SD_CS`)
- **ISR Functions**: Name with `ISR` suffix, keep minimal, use `volatile` for shared state
- **Serial Debugging**: 115200 baud; include status messages for MIDI parsing (`NOTE ON`, `NOTE OFF`, `TEMPO CHANGE`)
- **Comments**: Delimiter comments `// --------` separate logical sections (SETUP, LOOP, ISR FUNCTION, etc.)

**Key file**: [code/code/code.ino](c:\Users\santa\OneDrive\Desktop\maker\musical stepper\code\code\code.ino)

## Architecture

**Interrupt-Driven MIDI Playback System**: The sketch reads MIDI files from SD card and drives a stepper motor by converting note frequencies to pulse intervals.

**Three Core Subsystems**:
1. **MIDI Parser** (`readVar()`, main loop): Parses variable-length quantities, running status, NOTE ON/OFF, meta-events (tempo)
2. **Note Processor** (`noteToFreq()`): Converts MIDI note numbers → Hz → stepper-safe frequency (30-400 Hz range)
3. **Motor Driver** (`stepISR()`, `play()`): IntervalTimer ISR toggles STEP_PIN at calculated intervals

**Data Flow**: SD card → MIDI parsing → note extraction → frequency scaling → timer configuration → motor pulse

**Stepper Frequency Scaling**:
- Formula: `hz = 440 * 2^((note-69)/12) * 0.12`
- The `0.12` factor is empirically calibrated for musical output on stepper motors
- Clamped to 30-400 Hz (slow/fast limits for stepper reliability)

## Build and Test

**Hardware Required**:
- Teensy 3.1+ with SD card reader attached to BUILTIN_SDCARD
- Stepper motor on pin 2 (STEP_PIN)
- Mario theme MIDI file as `mario-theme.MID` on SD card root

**Compilation**:
```bash
# Use Arduino IDE or Teensy Loader with Teensy boards installed
# Select: Tools > Board > Teensy 3.2+ 
# Select: Tools > USB Type > Serial
```

**Upload**: Use Teensy Loader to flash compiled `.hex` file to board

**Debugging**: 
- Open Arduino Serial Monitor at 115200 baud
- Early messages show SD initialization and MIDI file parsing
- Delta times, note frequencies, and tempo changes logged per-beat

## Project Conventions

**MIDI File Format Specifics**:
- **Fixed BPM**: 105 (hardcoded in `noteDuration` calculation; tempo meta-events parsed but ignored)
- **Division**: 480 ticks per quarter note (read from header)
- **Duration Calculation**: `noteDuration = delta * 60000 / division / 105` (ms), minimum 5ms
- **Monophonic**: Only first track parsed; tracks beyond first are skipped
- **Running Status**: Supported—status byte omitted between consecutive same-status events

**Critical Calibration Values**:
- `STEP_PIN`: Pin 2 (stepper coil pulse pin)
- `SD_CS`: BUILTIN_SDCARD (Teensy's SD chip select)
- Frequency range: 30–400 Hz (enforced in `noteToFreq()`)
- Scale factor: 0.12 (reduces concert pitch to stepper-playable range)
- Tempo: 105 BPM (fixed in current implementation)

**State Management**:
- `volatile bool state`: Toggle direction for stepper pulse
- `lastStatus`: Running status MIDI byte (persists across delta-time boundaries)
- `tempo`: Parsed but currently unused (105 BPM hardcoded)

## Integration Points

**External Dependencies**:
- **SD.h**: Arduino SD library; manages file I/O (`SD.open()`, `f.read()`, `f.seek()`)
- **IntervalTimer.h** (Teensy): Hardware timer for microsecond-precision ISR (`timer.begin()`, `timer.end()`)
- **Serial**: UART at 115200 baud for debug output

**MIDI File Interface**:
- Reads `mario-theme.MID` from SD root at startup
- Expects standard MIDI format (MThd header → MTrk track data)
- Parses Note ON (0x90), Note OFF (0x80), Tempo (0xFF 0x51) events
- Unknown events skipped silently

**Motor Timing**:
- ISR pulse period: `1000000 / (freq * 2)` microseconds
- `stepISR()` toggles STEP_PIN, creating square wave for stepper motor
- Motor turns off (`freq ≤ 0`) when note ends

## Common Tasks

**Add a new MIDI file**:
1. Place `.MID` file on SD card as `mario-theme.MID` (or update filename in code)
2. Adjust `noteDuration` BPM divider if file plays too fast/slow

**Tune stepper frequency response**:
- Modify `noteToFreq()` scale factor (0.12) to shift output pitch
- Adjust min/max frequency bounds (30–400 Hz) for motor safety

**Parse different MIDI track**:
- Modify track-selection logic after finding first `MTrk` header to read track N instead of track 1

**Change tempo**:
- Replace hardcoded `105` in `noteDuration` formula with parsed `tempo` value, scaled from microseconds to BPM
