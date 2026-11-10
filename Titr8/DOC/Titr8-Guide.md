# Titr8: Implementation Guide

A practical guide to changing Titr8 or building a new Bodoque program from it. You only need basic programming knowledge: variables, `if`, functions, and how to build and upload a sketch.

For the full technical history (why the code looks like this, known issues, change log) see [Titr8-Refactor.md](Titr8-Refactor.md). For the board and the other programs, see [Bodoque-Ecosistema.md](Bodoque-Ecosistema.md).

> The code comments are in Spanish. Function and variable names are in English.

---

## 1. Build and upload

1. Install **VS Code** and the **PlatformIO IDE** extension.
2. Open the `Titr8` folder (the one with `platformio.ini`) in VS Code.
3. Click the **✓ Build** button in the bottom bar. The first build downloads everything and takes several minutes.
4. Connect the Bodoque by USB and click **→ Upload**.

If upload fails: short the BOOT pads while plugging in the board, and drag `.pio\build\Titr8\firmware.uf2` onto the `RPI-RP2` drive that appears.

`patch_mozzi.py` runs automatically before each build and fixes a small bug in the Mozzi library. Keep it next to `platformio.ini`.

---

## 2. How the program runs

The program has three parts that run at different speeds:

| Part | Function | How often | What it does |
|---|---|---|---|
| Setup | `setup()` in `main.cpp` | Once, at power-on | Sets up pins, reads power-on options, starts Mozzi |
| Control | `updateControl()` in `main.cpp` | **128 times per second** | Reads buttons and pots, decides what to do, moves the sequencer, updates LEDs and sound parameters |
| Audio | `updateAudio()` in `main.cpp` | **32,768 times per second** | Computes one audio sample |

**Golden rule:** the audio function runs so often that it must be very fast. Never put `Serial.print`, `delay()`, `analogRead()`, loops over arrays, or heavy float math in `voicesNextSample()`. Do that work in the control part and only use the results in the audio part.

Mozzi is the sound library. Its documentation: https://sensorium.github.io/Mozzi/

---

## 3. Project map: which file to open

All code is in `src/`.

| I want to… | Open |
|---|---|
| Change which pin a button, LED or pot uses | `hardwarePins.cpp` |
| Change what a button does | `controls.cpp` |
| Change how long a hold takes | `input.cpp` / `input.h` |
| Change or add a sound | `voices.cpp` |
| Change what a pot controls | `main.cpp` and `voices.cpp` (and `controls.cpp` for pot 3) |
| Change what the LEDs show | `ui.cpp` |
| Change how patterns are stored or played | `tracks.cpp` |
| Change tempo, steps, tap tempo | `sequencer.cpp` |
| Change power-on options | `setup()` in `main.cpp` |
| MIDI | `midiIO.cpp` |
| Mozzi settings (sample rate, output) | `mozziConfig.h` (rarely needed) |

Each `.cpp` has a matching `.h` that lists the functions other files may call.

---

## 4. Panel map

### Buttons

| Panel | Pin | Code name | Tap | Double tap | Hold |
|---|---|---|---|---|---|
| **I, II, III, IV** | 2, 4, 5, 7 | `noteButtons[0..3]` | Play and record a note (Drums: kick, snare, hat, crash) | — | 200 ms. Drums: roll. Others: note one octave up |
| **A** | 0 | `buttonA` | Transpose left (not on Drums) | Previous instrument | 500 ms: clear the current track |
| **B** | 24 | `buttonB` | Transpose right (not on Drums) | Next instrument | 500 ms: delay on/off |
| **C** | 25 | `buttonC` | Tap tempo | — | — |

**Power-on options** (hold while powering on): II = 32 steps, III = 64 steps, IV = free play (no sequencer). Default is 16 steps.

### Pots

Values go from 0 (left) to 1023 (right).

| Pot | Pin | Code | Controls |
|---|---|---|---|
| **Pot 1** | A3 | `potValue(0)` | Filter cutoff (bass and pad) and lead brightness |
| **Pot 2** | A1 | `potValue(1)` | Bass filter resonance, pad decay, lead note length |
| **Pot 3** | A0 | `potValue(2)` | Volume (first half), drive (second half) |

### LEDs

| Panel | Pin | Code | Shows |
|---|---|---|---|
| **■ ▲ ● ✕** | 11, 10, 9, 15 | `TRACKLED_PIN[0..3]` | Current instrument: Drums, Bass, Pads, Lead. Steady = notes 1–4, slow blink = 5–8, fast blink = 9–12 |
| **LEDs I–IV** | 1, 3, 6, 8 | `SEQLED_PIN[0..3]` | Beat: one lights on each quarter note |
| **Star** | 19 | `LEDESTRELLA_PIN` | Delay on |

### Other pins

| Pin | Use |
|---|---|
| 17 | Speaker mute (set LOW at power-on) |
| 20, 21, 22 | Audio output to the DAC (I2S). Pin 21 must always be pin 20 + 1 |
| 23 | Set HIGH: less noise from the power supply |

---

## 5. How the `Button` class works

Every physical button is a `Button` object (`input.h`). Once per control cycle, `inputUpdate()` reads all of them. After that, each button can answer simple questions:

| Question | Method | True when |
|---|---|---|
| Is it down right now? | `isDown()` | Every cycle while it's pressed |
| Was it just pressed? | `wasPressed()` | **Only** in the cycle it went down |
| Was it just released? | `wasReleased()` | **Only** in the cycle it came up |
| Has it been held long enough? | `takeHold()` | **Once** per press, after the hold time. Calling it "uses up" the hold |
| Was the hold already used? | `holdUsed()` | After `takeHold()` returned true, or after a combo consumed the press |
| When was it pressed? | `pressTime()` | Always: time in ms of the last press |

Think of `wasPressed()` and `wasReleased()` as **events**: they're true for a single cycle (1/128 of a second), so check them every cycle or you'll miss them. `isDown()` is a **state**.

Example: light the star LED while C is down.

```cpp
digitalWrite(LEDESTRELLA_PIN, buttonC.isDown() ? HIGH : LOW);
```

Example: do something once when C is held for a while.

```cpp
if (buttonC.takeHold())
  doSomething(); // runs once, not every cycle
```

**Hold times:** note buttons and C use 200 ms (`Button::HOLD_MS` in `input.h`). A and B use 500 ms (`FUNCTION_HOLD_MS` in `input.cpp`), so a slow tap doesn't clear a track. The hold time is set per button in `inputSetup()`:

```cpp
buttonC.begin(BUTTON_C_PIN, 800); // C now holds at 800 ms
```

**Tap and double tap** (A and B) are built on top of the class in `controls.cpp`: `readFunctionButton()` turns presses into `EVENT_TAP`, `EVENT_DOUBLE_TAP` or `EVENT_HOLD`. A double tap is two taps less than 350 ms apart (`DOUBLE_TAP_MS`).

**Buttons held at power-on** (all except B) are ignored until released, so a power-on option doesn't also play a note.

---

## 6. Changing the mapping

### 6.1 Move a button, LED or pot to another pin

Edit the numbers in `hardwarePins.cpp`:

```cpp
const uint8_t NOTEB_PIN[NUM_NOTES] = {2, 4, 5, 7};          // buttons I–IV
const uint8_t TRACKLED_PIN[NUM_TRACK_LEDS] = {11, 10, 9, 15}; // ■ ▲ ● ✕
const uint8_t BUTTON_A_PIN = 0;
const uint8_t POT_PIN[NUM_POTS] = {A3, A1, A0};              // pot 1, 2, 3
```

Swapping two entries swaps what those controls do. For example, `{A1, A3, A0}` swaps pot 1 and pot 2.

### 6.2 Change what A, B or C do

Open `controls.cpp` and find `handleButtonA()` / `handleButtonB()`. Each one is a `switch` over the event:

```cpp
static void handleButtonA(unsigned long now)
{
  switch (readFunctionButton(buttonA, tapA, now))
  {
  case EVENT_TAP:
    shiftNoteBank(-1);   // transpose left
    break;
  case EVENT_DOUBLE_TAP:
    shiftTrack(-1);      // previous instrument
    break;
  case EVENT_HOLD:
    clearCurrentTrack(); // clear the track
    break;
  default:
    break;
  }
}
```

Replace the function called in a `case` to change the action. For example, to make hold A toggle the delay instead of clearing, put `toggleDelay();` under `EVENT_HOLD`. (Also call `uiSetStarLed(delayOn);` so the star LED updates; `handleButtonB()` already does it.)

`handleButtonC()` only handles a tap (it calls `sequencerTap()`).

### 6.3 Change what the note buttons do

In `controls.cpp`:
- `playNoteButton()`: what a tap does (play and record, per instrument).
- `handleNoteHold()`: what a hold does (roll on Drums, octave up on the others).

### 6.4 Add a key combination

A combo is "hold one button, press another". Combos live in the `COMBOS[]` table in `controls.cpp`. When a combo fires, both presses are **consumed**: neither button does its normal action.

Example: hold C and press I to pick a new random scale.

```cpp
static void comboNewScale()
{
  voicesRandomizeScale();
}

static const Combo COMBOS[] = {
    {&buttonC, &noteButtons[0], comboNewScale}, // C + I
    {nullptr, nullptr, nullptr}                 // end of table (keep it last)
};
```

The function must be written **above** the table. C is the best modifier: A and B already use their own hold, which would fire while you're holding them for a combo.

### 6.5 Change what a pot controls

The pots are read in `inputUpdate()` and handed out in two places:
- `main.cpp`: `voicesUpdateControl(potValue(0), potValue(1));` passes pots 1 and 2 to the sound engine.
- `controls.cpp`: `voicesSetOutputGain(potValue(2));` uses pot 3 for volume.

Inside `voicesUpdateControl()` (`voices.cpp`), `map()` turns the pot value into a useful range:

```cpp
// Pot 1 → bass cutoff between 80 and 2000 Hz
bassFilterCutoffBase = constrain(map(pot1, 0, 1024, 80, 2000), 80, 2000);
```

To change the range, change the last two numbers of `map()` and of `constrain()`. To make the pot turn the other way, swap them (`map(pot1, 0, 1024, 2000, 80)`).

To make a pot control something new, use `pot1` or `pot2` in `voicesUpdateControl()`, or call `potValue(n)` anywhere in the control part (never in the audio part).

---

## 7. Sounds

All sound code is in `voices.cpp`, in this order:

| Section | Contains |
|---|---|
| Oscillators, envelopes and filters | Declaration of every sound part |
| Harmony | Scales, chords, `degreeToMidi()` |
| Setup | `setupEnvelopes()`, `voicesSetup()`: initial values |
| Triggers | `triggerKick()`, `triggerBass()`, … start a sound |
| Randomization | `voicesRandomize…()`: new random timbres |
| Control rate | `voicesUpdateControl()`: pots, envelopes, pitch (128 Hz) |
| Output | Volume and drive |
| Audio rate | `voicesNextSample()`: computes each voice and the **mix** |

### 7.1 The building blocks (Mozzi)

| Block | Example | What it is |
|---|---|---|
| `Oscil` | `kickOsc` | Plays a wave table (sine, square, saw, noise) at a frequency. `setFreq(hz)`, `next()` gives the next sample (−128..127) |
| `ADSR` | `kickEnv` | Envelope: volume shape over time. `noteOn()` starts it, `next()` gives 0..255 |
| `StateVariable` | `bassLPF` | Filter: `LOWPASS` (darker) or `HIGHPASS` (thinner). `setCentreFreq(hz)`, `setResonance(0..255, lower = more resonant)` |
| `AudioDelayFeedback` | `padBassDelay` | Echo |

An envelope is set with:

```cpp
kickEnv.setADLevels(255, 0);       // level after attack, level after decay (0..255)
kickEnv.setTimes(2, 120, 0, 0);    // attack, decay, sustain, release in ms
```

Envelopes declared as `ADSR<CONTROL_RATE, MOZZI_AUDIO_RATE>` need `update()` in `voicesUpdateControl()` and `next()` in `voicesNextSample()`.

### 7.2 Tweak an existing sound

Most sounds can be changed with a few numbers:

| Sound | Where | Try |
|---|---|---|
| Kick pitch | `triggerKick()`: `setFreq(45)`; `voicesNextSample()`: `35 + (kickEnvLevel >> 1)` | Lower = deeper |
| Kick length | `setupEnvelopes()`: `kickEnv.setTimes(2, 120, 0, 0)` | Bigger decay (120) = longer boom |
| Snare tone | `triggerSnare()`: `setFreq(220.0f)` | |
| Hat length | `hatEnv.setTimes(1, 35, 0, 0)` | |
| Lead timbres | `LEAD_PRESETS[]` | `{ratio, depth, modDecayMs}`. Whole ratios sound harmonic, others like bells |
| Pad chords | `chordIntervals[][]` | Semitones from the root, 4 notes per chord |
| Scales | `scales[12][6]` | Semitones from the root, 6 notes per scale |
| Delay | `PAD_BASS_DELAY_CELLS`, `PAD_BASS_DELAY_FEEDBACK` | Cells = delay time (16384 cells = 0.5 s), feedback 0..127 |

### 7.3 Change a voice's volume

At the end of `voicesNextSample()` each voice is added to the mix with a **shift**. Each `<< 1` doubles the volume:

```cpp
mix = ((int32_t)kick << 5);              // kick: loud
mix = mix + ((int32_t)snare << 1);
mix = mix + ((int32_t)hat << 0);         // hat: as is
```

`<< 1` = ×2, `<< 2` = ×4. For less, shift the voice right where it's computed (`>> 1` = half). Use shifts instead of `*` or `/` in audio code: they're much faster.

> The last line, `mix = mix + delayedPadBass << 6;`, has a known precedence bug that is kept on purpose (see Titr8-Refactor.md §6). Don't "fix" it without checking the volume by ear.

### 7.4 Add a new sound: example, replace the crash with a tom

Five steps, all in `voices.cpp`. Replacing the crash means button IV on Drums plays the tom, and recording already works.

**1. Declare it** (with the other drums at the top):

```cpp
static Oscil<SIN2048_NUM_CELLS, MOZZI_AUDIO_RATE> tomOsc(SIN2048_DATA);
static ADSR<CONTROL_RATE, MOZZI_AUDIO_RATE> tomEnv;
```

**2. Set it up** (in `setupEnvelopes()`):

```cpp
tomEnv.setADLevels(255, 0);
tomEnv.setTimes(1, 250, 0, 0);
```

**3. Trigger it** (write `triggerTom()` next to `triggerCrash()`, and call it from `triggerDrum()`):

```cpp
static void triggerTom()
{
  tomOsc.setPhase(0);
  tomOsc.setFreq(110);
  tomEnv.noteOn();
}

// in triggerDrum():
  else if (drum == DRUM_CRASH)
    triggerTom();
```

**4. Update its envelope** (in `voicesUpdateControl()`, next to `crashEnv.update();`):

```cpp
tomEnv.update();
```

**5. Compute it and add it to the mix** (in `voicesNextSample()`):

```cpp
// with the other drums
uint8_t tomEnvLevel = tomEnv.next();
int16_t tom = (tomOsc.next() * tomEnvLevel) >> 8;

// with the other mix lines
mix = mix + ((int32_t)tom << 4); // adjust by ear
```

Then remove the crash code you no longer use (its block in `voicesNextSample()` costs CPU even when silent).

### 7.5 Add a whole new instrument (track)

This is bigger. Follow an existing melodic one, the **Lead**, step by step:

1. `voices.h/.cpp`: the voice (`triggerX()`, `silenceX()`, maybe `voicesRandomizeX()`), its update in `voicesUpdateControl()`, its sample and mix line in `voicesNextSample()`.
2. `tracks.h`: add `TRACK_X` to `TrackId`, **before** `TRACK_COUNT`.
3. `tracks.cpp`: a pattern array `xSeq[MAX_STEPS]`, and handle it in `tracksClear()`, `tracksMelodicEmpty()`, `tracksTriggerNote()`, `tracksRecordNote()` and `tracksPlayMelodic()`. Search for `leadSeq` and `TRACK_LEAD` to find every place.
4. `controls.cpp`: its randomizer in `clearCurrentTrack()` and `updateRandomizer()`.
5. LEDs: all 4 instrument LEDs are already used, so a 5th instrument needs a new way to show it in `ui.cpp`.

---

## 8. LEDs

LEDs are driven only from `ui.cpp`:

| Function | Called | Does |
|---|---|---|
| `uiShowTrack(track)` | When the instrument changes | Selects which instrument LED is on |
| `uiSetNoteBank(bank)` | When transposing | Sets the blink speed |
| `uiUpdateTrackLeds(now)` | Every control cycle | Draws the instrument LED (steady or blinking) |
| `uiUpdateBeatLeds()` | Every control cycle | Draws LEDs I–IV (beat) |
| `uiSetStarLed(on)` | From button B | Star LED |

**Careful:** `uiUpdateTrackLeds()` and `uiUpdateBeatLeds()` redraw their LEDs **every cycle**. If you `digitalWrite()` one of those LEDs from somewhere else, it gets overwritten 1/128 s later. To show something new on them, change those functions instead. The star LED is free: only `uiSetStarLed()` touches it.

Blink speeds are in `BANK_BLINK_PERIOD_MS[]` (`ui.cpp`), in ms per blink. `0` = steady.

---

## 9. Patterns and the sequencer (short version)

- A step is a 16th note: 4 steps per beat. Patterns have 16, 32 or 64 steps (`MAX_STEPS = 64`).
- `tracks.cpp` keeps one array per sound: `kickSeq`, `snareSeq`, `hatSeq` (2 = crash), `bassSeq`, `padSeq`, `leadSeq`. One slot per step; 0 = nothing.
- Melodic tracks store the **scale degree** (which of the 12 notes), not the button, so transposing later doesn't change what's recorded.
- `sequencer.cpp` moves forward one step at a time and calls `tracksPlayDrums()` and `tracksPlayMelodic()`. Use its getters (`sequencerStepIndex()`, `sequencerStepMs()`, …) if you need the step or the tempo.
- With MIDI clock connected, the sequencer follows the external clock instead of its own.

---

## 10. Making a new program from Titr8

1. Copy the `Titr8` folder next to it, with a new name (for example `MyProgram`). **Don't copy `.pio/` or `.vscode/`**: they are build files (16 MB+) and get recreated.
2. In `platformio.ini`, rename `[env:Titr8]` to `[env:MyProgram]` and change `usb_product = Bodoque Titr8` to your name. That's the name the computer shows for the USB MIDI device.
3. Update `CLAUDE.md` and the docs (or delete them) so they describe your program.
4. Open the **new folder** in VS Code and build.
5. Keep `mozziConfig.h` as the first include in every `.cpp` that uses Mozzi, and keep `patch_mozzi.py`.

Start small: change one thing, build, upload, listen. Then the next.

---

## 11. Pitfalls and tips

| Problem | Cause / fix |
|---|---|
| Clicks, glitches or a frozen board | Too much work in the audio part. Move it to `voicesUpdateControl()` |
| "multiple definition" link errors from Mozzi | `patch_mozzi.py` missing, or `#include <Mozzi.h>` in more than one file. Only `main.cpp` includes `<Mozzi.h>`; other files use `<MozziHeadersOnly.h>` |
| Strange sample rate or silence | A `.cpp` includes a Mozzi header before `mozziConfig.h` |
| A new sound is silent | Missing `noteOn()`, `update()` in control rate, `next()` in audio rate, or the mix line |
| A new sound is distorted | Too loud in the mix: lower its shift |
| A button event is missed | `wasPressed()` / `wasReleased()` are true for one cycle only; check them every cycle |
| An LED doesn't stay on | `ui.cpp` redraws it every cycle (see §8) |
| Variables shared between files | Avoid new globals. Add a small function to the module's `.h` instead, like `sequencerStepMs()` |
| Debugging | `Serial.println()` in the **control** part only, then open the PlatformIO serial monitor (115200 baud) |
