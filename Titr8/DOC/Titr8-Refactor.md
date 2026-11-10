# Titr8: Refactor Technical Analysis

> **Purpose:** context for anyone, including a future Claude chat, working on Titr8. It covers where Titr8 comes from, what the refactor changed, how the code is organized now, and what's still pending.
> For the ecosystem overview (board, other programs, flashing), see [Bodoque-Ecosistema.md](Bodoque-Ecosistema.md).
>
> **Last updated:** 2026-11-10

---

## 1. Background

**Titr8** is a new firmware for the **Bodoque V1** board (RP2040). The name combines *titration* and *8*, as an acid pun. It started as a copy of **MaquinaAcida**, a Mozzi-based groovebox with three tracks: **DRUM**, **SYNTH/BASS** and **PAD**. The pattern always loops, and everything you play is recorded into it (overdub is always on).

**Goal:** make it much deeper, with many more key combinations, transposing, more tracks and so on. The original code was hard to extend, so it was **refactored first with identical behavior**. New features come after that.

### Where the code lives

| Location | Role |
|---|---|
| `C:\code\pico\Bodoque-Ecosistema` | **The repo** (moved here from F: on 2026-11-10). Branch `feature/titr8`, pushed to GitHub. |
| `C:\code\pico\Bodoque-Ecosistema\Titr8` | Titr8 inside the repo. Can be built directly from here now (no brackets in the path). |
| `C:\code\pico\Bodoque-Ecosistema\Titr8\DOC` | This doc and the ecosystem overview (`Bodoque-Ecosistema.md`). Moved here from the git-ignored `Bodoque-Documentation` folder on 2026-11-10, so they are tracked now. |
| `C:\code\pico\Bodoque-Ecosistema\MaquinaAcida` | The original. Untouched. |

**History:** the repo used to live at `F:\[WORKING]\PICO\Bodoque-Ecosistema`. Drive F: stalls on disk I/O: git, file copies and PlatformIO froze there for minutes, and even Windows disk queries hung. The antivirus exclusion didn't help. That's also the most likely reason PlatformIO hung on "Configuring project" (it was first blamed on the `[WORKING]` brackets). Don't put the project back on F:.

**`C:\Titr8` is retired** (2026-11-10): its contents were moved into the repo's `Titr8` folder, which is now the only working copy. Build and upload from there.

### Build and upload setup

- VS Code + **PlatformIO IDE** extension, with the `Titr8` folder opened as the project (it must contain `platformio.ini`).
- A single environment: `[env:Titr8]` (Philhower core, `board = pico`, 250 MHz, `-DUSE_TINYUSB`).
- Libraries: Mozzi 2.0.4, Adafruit TinyUSB, MIDI Library, Adafruit SSD1306 (not used yet), PWMAudio.
- USB product name: **"Bodoque Titr8"**.
- **Upload uses picotool.** On Windows it needed **Zadig** to install **WinUSB** on **`RP2 Boot (Interface 1)`**. Never pick Interface 0: that's the `RPI-RP2` drive, and changing its driver breaks drag-and-drop.
- Fallback: short the BOOT pads and drag `.pio\build\Titr8\firmware.uf2` onto the `RPI-RP2` drive.
- The first full build takes several minutes and uses every CPU core. Later builds are incremental. **The user starts builds from VS Code; Claude doesn't run them from its shell.** Background builds started by Claude froze the PC and were killed when Claude Code crashed.

---

## 2. Hardware (Bodoque V1)

### Panel names

These are the names printed on the panel. The docs use them from here on. The code uses the same names for the function buttons (`buttonA/B/C`).

| Panel name | Code name | Function in Titr8 |
|---|---|---|
| **Button A** | `buttonA` | Tap: transpose left. Double tap: previous instrument. Hold: clear the current track |
| **Button B** | `buttonB` | Tap: transpose right. Double tap: next instrument. Hold: delay on/off |
| **Button C** | `buttonC` | Tap tempo |
| **Buttons I, II, III, IV** | `noteButtons[0..3]` | Play the notes |
| **■ Square LED** | `TRACKLED_PIN[0]` | Instrument: **Drums** (always steady: Drums don't transpose) |
| **▲ Triangle LED** | `TRACKLED_PIN[1]` | Instrument: **Bass** (blink = note bank) |
| **● Circle LED** | `TRACKLED_PIN[2]` | Instrument: **Pads** (blink = note bank) |
| **✕ X LED** | `TRACKLED_PIN[3]` | Instrument: **Lead** (FM pluck / bell) (blink = note bank) |
| **LEDs I–IV** (one per numbered button) | `SEQLED_PIN[0..3]` | Beat: each lights on its quarter note |
| **Delay LED** (star) | `LEDESTRELLA_PIN` | Delay on |
| **Power LED** | — | Board on (not driven by the firmware) |

### Pin map

From `src/hardwarePins.*`:

| Element | Pins | Use in Titr8 |
|---|---|---|
| Buttons I–IV | 2, 4, 5, 7 | Play and record (in Drums: kick / snare / hat / crash) |
| Button A (`BUTTON_A_PIN`) | 0 | See panel names above |
| Button B (`BUTTON_B_PIN`) | 24 | See panel names above |
| Button C (`BUTTON_C_PIN`) | 25 | Tap tempo |
| 3 pots | A3, A1, A0 | Pot 1: bass cutoff + lead brightness. Pot 2: resonance + pad decay + lead length. Pot 3: volume/drive |
| LEDs I–IV | 1, 3, 6, 8 | Pulse on each quarter note |
| ■ ▲ ● ✕ LEDs | 11, 10, 9, 15 | Selected instrument (Drums, Bass, Pads, Lead) |
| (✕ LED source) | 15 | Confirmed in the PCB file (`D14` sits on net `/GPIO15`, next to `D11–D13` on GPIO 9/10/11). 909pt and Atmos already drive it as the 4th LED: `LED_BUTTON_PIN = {11, 10, 9, 15}` |
| Delay LED (star) | 19 | Delay on |
| Mute speaker | 17 | Set LOW at boot |
| I2S → PT8211 | BCK 20, WS 21, DATA 22 | 16-bit stereo audio, LSBJ format |
| GPIO23 | 23 | Set HIGH: switches the Pico's power supply to PWM mode (less audio noise) |

---

## 3. Changes before the refactor

1. **Copied MaquinaAcida → Titr8**, without `.pio`/`.vscode`.
2. **Removed DESTRUKTOR1 support**, an older board with 6 buttons, 2 pots, PWM audio and a jack sensor. Titr8 is **Bodoque V1 only**:
   - Every `#if DESTRUKTOR1` / `#if BODOQUEV1` block removed, keeping the V1 code.
   - `platformio.ini`: a single `[env:Titr8]`, and the `-D BODOQUEV1` flag removed.
   - This version **built and flashed successfully**.

---

## 4. The refactor

### 4.1 Problems with the original code

- `sequencer.cpp` (~1,030 lines) held **everything**: globals, the sound engine, the sequencer, buttons, tap tempo, the randomizer, delay and gain.
- `main.cpp` (~520 lines) mixed setup, reading buttons and pots, LEDs, and all of `updateAudio()`.
- `controles.h` was only ~30 `extern` declarations, and `controles.cpp` was empty.
- Button state was spread across parallel arrays (`buttonPressed[]`, `buttonHeld[]`, `rolling[]`…), with duplicate variables for TRACK and TEMPO. Every new combination meant more flags and more `if`s.
- The tracks were hardcoded (`kickSeq`, `bassSeq`, `padSeq`, `DRUM/SYNTH/PAD_MODE`).
- Each `.cpp` declared the Mozzi configuration on its own, inconsistently (`midiIO.cpp` included Mozzi with no configuration at all).

### 4.2 Guiding rule

**Identical behavior:** same sound, same timing, same order of operations in each control cycle, and the same quirks, bugs included (see §6). Only dead code and unused variables were removed.

> §4 describes the code as it is now, including the changes made after the refactor. Those changes are listed in §8.

### 4.3 New structure (`C:\Titr8\src`)

| File | Responsibility | Main API |
|---|---|---|
| `main.cpp` | Setup, wiring of the control loop, audio callback | `setup()`, `updateControl()`, `updateAudio()`, `loop()` |
| `mozziConfig.h` | **The only** Mozzi configuration (audio 32768 Hz, control 128 Hz, I2S LSBJ 16-bit stereo). Included **first** in every `.cpp` that uses Mozzi. | — |
| `hardwarePins.h/.cpp` | Pin map. `NUM_TRACKS` was renamed to **`NUM_TRACK_LEDS`**, because the number of LEDs isn't the number of tracks. | constants |
| `input.h/.cpp` | The `Button` class and the pots. Detects events only; doesn't decide what they do. | `noteButtons[4]`, `buttonA`, `buttonB`, `buttonC`, `inputSetup()`, `inputCaptureBootState()`, `inputUpdate(now)`, `potValue(i)`, `anyNoteButtonActive()` |
| `controls.h/.cpp` | **What each button does**: note bank (transpose), tap / double tap / hold on A and B, the combo table and the power-on randomizer. New interactions go here. | `controlsUpdate(now)`, `controlsRecordRolls(step)` |
| `tracks.h/.cpp` | Recorded patterns and their encoding. | `TrackId`, `DrumId`, `currentTrack`, `tracksClear()`, `tracksMelodicEmpty()`, `tracksTriggerNote()`, `tracksRecordDrum()`, `tracksRecordNote()`, `tracksPlayDrums()`, `tracksPlayMelodic()` |
| `sequencer.h/.cpp` | Internal clock, steps, nearest step, tap tempo, MIDI clock hooks. | `sequencerSetup()`, `sequencerUpdate()`, `sequencerNearestStep()`, `sequencerJustFiredStep()`, `sequencerTap()`, `sequencerUpdateTapTimeout()`, `sequencerMidiStep()`, `sequencerMidiStart()`, getters |
| `voices.h/.cpp` | The whole Mozzi sound engine: drums, bass, pad, FM lead, harmony (scales/chords), randomization, delay, output stage and **mix**. All of its state is `static`. | `NUM_SCALE_DEGREES`, `voicesSetup()`, `voicesUpdateControl(pot1, pot2)`, `voicesNextSample()`, `triggerDrum/Bass/Pad/Lead()`, `silenceBass/Pad/Lead()`, `voicesRandomizeBass/Pad/Lead/Scale()`, `voicesSetOutputGain()`, `voicesSetDelay()` |
| `ui.h/.cpp` | LEDs. The instrument LED also shows the note bank by blinking. | `uiSetup()`, `uiShowTrack()`, `uiSetNoteBank()`, `uiUpdateTrackLeds(now)`, `uiUpdateBeatLeds()`, `uiSetStarLed()` |
| `midiIO.h/.cpp` | USB MIDI (clock sync only). Uses the sequencer API instead of touching its variables directly. | `setupMidiIO()`, `updateMidiIO()`, `midiClockSyncActive`, `midiClockPlaying` |

**Removed:** `controles.h`, `controles.cpp`, `main.h`, `delaydemo.cpp`, and dead code: count-in, `loadDemoPattern()`, `modeLedTimer`/`modeLedOn`, `octaveHoldActive[]`, and several unused globals.
**Size:** ~2,040 → ~1,570 lines.
`src/AI Instructions` is the original MaquinaAcida spec, written for DESTRUKTOR1. It isn't compiled.

### 4.4 Module dependencies

```
main ──► input, controls, sequencer, ui, voices, midiIO
controls ──► input, tracks, sequencer, voices, ui
sequencer ──► tracks, controls (controlsRecordRolls), midiIO (flags)
tracks ──► voices
midiIO ──► sequencer
ui ──► sequencer
voices ──► (Mozzi only)
```

### 4.5 Control cycle (`updateControl`, 128 Hz)

The refactor kept the original order. Since then, the delay moved from its own step (3) to Button B's handler:

1. `updateMidiIO()`
2. `inputUpdate(now)`: reads every button and the 3 pots
3. `controlsUpdate(now)`:
   1. note edges: on press, store `holdStartStep`; on release, stop the roll
   2. `voicesSetOutputGain(pot 3)`
   3. power-on randomizer
   4. **`handleCombos()`** *(the table is empty)*
   5. note holds → roll (Drums) or octave-up note (Bass/Pads/Lead)
   6. `handleButtonA()`: tap → bank left (not on Drums), double tap → previous instrument, hold → clear track
   7. `handleButtonB()`: tap → bank right (not on Drums), double tap → next instrument, hold → delay; star LED
   8. `handleButtonC()`: release → tap tempo
   9. tap timeout
   10. note presses → play and record
   11. rolls
4. `sequencerUpdate()`: when it's time, play the step (drums → record rolls → bass/pad) and advance
5. `uiUpdateBeatLeds()`
6. `uiUpdateTrackLeds(now)`: instrument LED, steady or blinking by note bank
7. `voicesUpdateControl(pot1, pot2)`: envelopes, filters, LFOs, pitches

`updateAudio()` (32768 Hz) calls `voicesNextSample()` and returns `StereoOutput::from8Bit(s >> 1, s >> 1).clip()`.

### 4.6 The `Button` class

- `update(now)` detects `wasPressed()` / `wasReleased()`, which are true only for that cycle, and the hold. The hold time is set per button in `begin(pin, holdMs)`: **200 ms** by default (`HOLD_MS`, note buttons and C) and **500 ms** for A and B (`FUNCTION_HOLD_MS` in `input.cpp`), so a slow tap doesn't clear a track.
- `takeHold()` returns `true` **only once** per press.
- `holdUsed()` tells whether that press's hold was already used, or whether a combo consumed the press.
- `consume()` cancels the press's normal action (tap, hold and release). Combos use it.
- `captureBootState(syncState)` ignores a button held at power-on until it's released. Note buttons use `syncState=false`; A and C use `true`; B isn't captured. That reproduces the original behavior exactly.

**Tap / double tap / hold (A and B):** `readFunctionButton()` in `controls.cpp` turns a button's raw events into `EVENT_TAP`, `EVENT_DOUBLE_TAP` or `EVENT_HOLD`. A tap is a release before the hold time. A double tap is a second tap released within **350 ms** (`DOUBLE_TAP_MS`) of the first one. A third tap counts as a single tap again. The single tap acts **immediately** (no waiting to see if a second tap comes); if the second tap arrives, the handler changes instrument, and changing instrument always resets the bank to 0 (notes 1–4), which also wipes the first tap's transpose.

### 4.7 Key combos (the base for the new features)

In `controls.cpp`:

```cpp
static const Combo COMBOS[] = {
    // {modifier, trigger, action}
    // Example: {&buttonC, &noteButtons[0], someAction},
    {nullptr, nullptr, nullptr} // end of table
};
```

When `modifier` is held and `trigger` is pressed, `action()` runs, and **both** presses are consumed, so neither does its normal action. Combos are evaluated before holds and taps.

**Choosing a modifier:** A and B already use tap, double tap and hold, so as modifiers they'd clash with their own hold (it fires at 500 ms). C only uses tap, so it's the best candidate.

### 4.8 Pattern encoding (`tracks.cpp`)

| Track | Array | Values |
|---|---|---|
| DRUM | `kickSeq`, `snareSeq`, `hatSeq` | `1` = hit. In `hatSeq`, `2` = **crash** |
| BASS, PAD, LEAD | `bassSeq`, `padSeq`, `leadSeq` | `0` = nothing, `degree+1` (1..12) = note, `degree+13` (13..24) = one octave up. `encodeNote()` / `decodeNote()` |

The **absolute scale degree** (0..11) is recorded, not the button. Transposing doesn't change what's already recorded. (The old PAD packed format and its legacy path were removed; patterns aren't saved anywhere, so nothing depended on them.)

`MAX_STEPS = 64`. Length is chosen at power-on: 16 (default), 32 (hold II) or 64 (hold III). Holding IV at power-on enables **free play**, with no sequencer.

### 4.9 Scale degrees and note banks

- `voices.cpp` keeps the current scale in `synthNotes[SCALE_LEN]` (6 notes, random scale and root).
- `degreeToMidi(degree)` extends it to **12 degrees** (`NUM_SCALE_DEGREES`): degrees 0–5 are the scale, 6–11 the same scale one octave up.
- `controls.cpp` keeps `noteBank` (0..2). Button I–IV plays degree `noteBank * 4 + button` (`buttonDegree()`):

| Bank | I | II | III | IV |
|---|---|---|---|---|
| 0 (power-on) | 1 | 2 | 3 | 4 |
| 1 | 5 | 6 | 7 | 8 |
| 2 | 9 | 10 | 11 | 12 |

- B moves one bank right, A one bank left. Both wrap around: A from bank 0 goes to bank 2, B from bank 2 goes to bank 0.
- Changing instrument resets the bank to 0. On Drums, A/B taps do nothing (no transpose, LED stays steady).
- **Display:** the current instrument's LED (■ ▲ ●) shows the bank:

| Bank | Notes | Instrument LED |
|---|---|---|
| 0 | 1–4 | Steady |
| 1 | 5–8 | Slow blink (500 ms period, ~2 Hz) |
| 2 | 9–12 | Fast blink (166 ms period, ~6 Hz) |

  Periods are in `BANK_BLINK_PERIOD_MS` (`ui.cpp`). The blink restarts **on** whenever the bank or instrument changes, so the change is visible at once. It also blinks on Drums, even though Drums ignore the bank, so a transpose always gives feedback and you can see where the melodic instruments will be. LEDs I–IV are not touched.

### 4.10 Lead: FM pluck / bell (`voices.cpp`)

Two-operator FM: a sine **modulator** shifts the phase of a sine **carrier** (`leadCarrier.phMod()`).

| Part | What it does |
|---|---|
| `leadEnv` (audio rate) | Volume. 1 ms attack, decays to 0. Length set by **pot 2** (80–1500 ms). |
| `leadModEnv` (control rate) | FM depth. Decays faster than the volume, so the tone gets darker as the note fades. That is what makes it sound plucked or struck. |
| `leadFmIntensity` | `modEnv × preset depth × pot 1`, computed at 128 Hz. **Pot 1** = brightness (some FM remains with pot 1 at zero). 512 = one full cycle of phase deviation. |
| `LEAD_PRESETS[]` | `{ratio, depth, modDecayMs}`. Whole-number ratios sound harmonic, the others sound inharmonic. Presets: pluck (1.0), electric piano / glass (2.0), bell (3.5), metallic (1.41), marimba (4.0). |
| `voicesRandomizeLead()` | Picks a preset. Runs with the power-on randomizer and when you clear the Lead track (hold A). |

- **Pitch:** two octaves above the bass (like the pad's root), +1 octave on hold. It follows the note bank.
- **Delay:** the lead goes into the delay together with pad and bass.
- **Mix level:** `lead = (sample × env) >> 6`, about ±500, similar to the pad. Adjust that shift after listening.
- **CPU:** float math happens only when a note is triggered (`mtof`, ratio). At audio rate it's 2 table lookups and a couple of multiplies, and nothing at all once the note has ended (`leadActive` goes false). That's far cheaper than the pad (4 saws + 2 filters).

---

## 5. Behavior (current)

- **Tap on a note (I–IV):** in the first half of a step, it plays live and is recorded on the step that just played. In the second half, it's recorded on the next step and plays when that step arrives. In Bass/Pads/Lead the note depends on the current bank (§4.9).
- **Hold on a note (200 ms):** in Drums, a roll that repeats every step and is recorded (kick, snare and hat; crash isn't recorded). In Bass/Pads/Lead, it plays and records the note one octave up.
- **Button A:**
  - Tap: transpose left (previous bank of 4 notes, wraps from 1–4 to 9–12). The instrument LED shows the bank: steady, slow blink or fast blink (§4.9). Does nothing on Drums (no transpose there, the LED stays steady).
  - Double tap: previous instrument (Drums → Lead → Pads → Bass → Drums, shown on the ■ ▲ ● ✕ LEDs).
  - Hold (500 ms): clear the current track. In Bass/Pads/Lead it also randomizes that voice's sound, and picks a new scale if all melodic tracks are empty.
- **Button B:**
  - Tap: transpose right (next bank of 4 notes, wraps from 9–12 to 1–4). Does nothing on Drums.
  - Double tap: next instrument (Drums → Bass → Pads → Lead → Drums).
  - Hold (500 ms): toggles the pad+bass delay (16384 cells, feedback 70). The Delay (star) LED shows its state.
- **Changing instrument resets the transpose:** every instrument starts on bank 0 (notes 1–4, steady LED). The first tap of a double tap transposes right away; the second one, within 350 ms, changes instrument and the bank goes back to 0. A side effect: two fast taps on B can't be used to jump two banks; tap slower than 350 ms.
- **Button C:** tap tempo. The interval between two taps sets one quarter note (4 steps). Valid range is 20–250 ms per step.
- **LEDs I–IV:** light one at a time, on the first step of each quarter note.
- **Power-on randomizer:** every 150 ms it changes the bass and pad sound and the scale, until the first note button is touched.
- **MIDI:** when MIDI Clock arrives, the internal clock stops and the sequencer follows the external clock (24 ppqn spread across the steps). Handles Start/Continue/Stop.

---

## 6. Known issues (kept on purpose, to fix later)

| # | Issue | Location | Effect |
|---|---|---|---|
| 1 | **Volume:** `mix = mix + delayedPadBass << 6;`. Because of operator precedence this is `(mix + delayed) << 6`, so it shifts the **whole mix**. | `voices.cpp`, end of `voicesNextSample()` (marked TODO) | This is how it sounds today. Fix it deliberately and check by ear. **Deferred by the user.** |
| 2 | The crash cymbal uses **float math at audio rate** (32,768 times per second) on a chip without an FPU, even while it's silent. | `voicesNextSample()` | Rough estimate: a few % of the per-sample budget (~7,600 cycles at 250 MHz); the RP2040 has fast float routines in ROM. **Not a problem yet**: the FM lead costs much less than the pad. Cheapest fix with no change in sound: skip the crash block while `crashEnvLevel == 0`. |
| 3 | The **first press** of a button held at power-on (II/III/IV) is lost and acts as an immediate hold (roll or octave note). | `Button::captureBootState` + `update` | Inherited bug, reproduced faithfully. |
| 4 | Bass detune: in 3 of 5 cases `random(0, 11) / 1000` is integer division, so it's always 0. | `voicesRandomizeBass()` | Detune is always a whole number of semitones (5, 7, 12). |
| 5 | ~~Button A clears the track after 200 ms.~~ **Changed:** A and B now hold at 500 ms (§4.6). | `input.cpp` | — |
| 6 | Inherited TODOs: avoid the recorded note sounding again on the next step when playing live; review the "nearest step" calculation (it should choose between the current step and the previous one). | header of `main.cpp` | — |
| 7 | `bassEnv.next()` is called at both audio rate and control rate, so the envelope advances extra. | `voicesNextSample()` / `voicesUpdateControl()` | Part of the current sound. Don't "fix" it without listening first. |

---

## 7. Status and next steps

- [x] Titr8 created from MaquinaAcida, Bodoque V1 only. Built and flashed.
- [x] Module refactor written in `C:\Titr8\src`.
- [x] **Refactor built, flashed and confirmed working on the board** (2026-11-09).
- [x] Synced `C:\Titr8` → repo `Bodoque-Ecosistema\Titr8` (2026-11-10, without `.pio`/`.vscode`). Committed file by file on `feature/titr8` and pushed.
- [x] Repo moved from F: to `C:\code\pico\Bodoque-Ecosistema` (2026-11-10).
- [ ] Open a PR for `feature/titr8`.
- [ ] Decide whether to retire `C:\Titr8` and build from the repo's `Titr8` folder.
- [ ] Delete the old copy on F: once the C: copy is confirmed.
- [x] New A/B layout: transpose by banks of 4 notes, double tap to change instrument, hold A to clear, hold B for delay (§8). **Built and tested** (2026-11-10).
- [ ] Decide what to do about issues #1–#4.
- [x] Show the current note bank: instrument LED steady / slow / fast blink (§4.9). **Built and tested** (2026-11-10).
- [x] 4th instrument on the ✕ LED (GPIO 15): **FM pluck / bell lead** (§4.10). **Built and tested** (2026-11-10). Check its volume against the other tracks by ear.
- [ ] More combos. C is the best modifier candidate (§4.7).

### Where to make changes for new features

- **New key combination:** a function + one line in `COMBOS[]` (`controls.cpp`).
- **New sound or voice:** `voices.cpp`. Add its state, a trigger, an update in `voicesUpdateControl`, and its share of the mix in `voicesNextSample`.
- **New track:** an entry in `TrackId` (`tracks.h`), its array and encoding in `tracks.cpp`, and its voice in `tracksTriggerNote()` (`tracks.cpp`), and its randomizer in `clearCurrentTrack()` (`controls.cpp`). All 4 shape LEDs are used now; a 5th track needs another way to display it in `uiShowTrack`.
- **Transposing:** `noteBank` and `buttonDegree()` in `controls.cpp`; degree → MIDI in `degreeToMidi()` (`voices.cpp`).
- **New tap / double tap / hold action on A or B:** the `switch` in `handleButtonA/B` (`controls.cpp`).
- **Anything that needs the step or the tempo:** use the `sequencer*()` getters. Don't add globals.

---

## 8. Changes after the refactor

### 2026-11-09: A/B layout, transpose by note banks

| Before | Now |
|---|---|
| A tap: next instrument | A tap: transpose left. A double tap: next instrument |
| A hold (200 ms): clear track | A hold (500 ms): clear track |
| B press: delay on/off | B tap: transpose right. B double tap: previous instrument. B hold (500 ms): delay on/off |
| I–IV always play scale degrees 1–4 (6-note scale) | I–IV play a bank of 4 out of 12 degrees (scale over two octaves) |

Code changes:
- `hardwarePins`, `input`: `trackButton/delayButton/tempoButton` → `buttonA/buttonB/buttonC`; pins `BUTTON_A/B/C_PIN`. `Button::begin()` takes a per-button hold time.
- `controls.cpp`: `noteBank`, `buttonDegree()`, `readFunctionButton()` (tap / double tap / hold), `handleButtonA/B/C()`. The old `handleTrackHold/Release`, `handleTempoRelease` and `handleDelayButton` are gone.
- `voices`: `NUM_SCALE_DEGREES`, `SCALE_LEN`, `degreeToMidi()`. `triggerBass/Pad()` take a degree 0..11.
- `tracks.cpp`: bass and pad share one encoding that stores the degree (§4.8).

### 2026-11-10: Note bank shown on the instrument LED

- The current instrument's LED is steady on bank 0 (notes 1–4), blinks slowly on bank 1 (5–8) and fast on bank 2 (9–12).
- `ui.cpp`: `uiShowTrack()` now stores the track; new `uiSetNoteBank()` and `uiUpdateTrackLeds(now)`, called every control cycle from `main.cpp`. `shiftNoteBank()` in `controls.cpp` calls `uiSetNoteBank()`.

### 2026-11-10: 4th instrument, FM Lead, on the ✕ LED

- New track `TRACK_LEAD` (after Pads): Drums → Bass → Pads → Lead. ✕ LED on GPIO 15 (`TRACKLED_PIN[3]`, `NUM_TRACK_LEDS = 4`).
- `voices`: `triggerLead()`, `silenceLead()`, `voicesRandomizeLead()`, FM voice and presets (§4.10). The lead is added to the mix and the delay input.
- `tracks`: `leadSeq`, same encoding as bass/pad. New `tracksTriggerNote(track, degree, octaveUp)` plays the right melodic voice, so `controls.cpp` no longer has a branch per instrument.
- `controls`: clearing Lead randomizes its preset; the power-on randomizer also randomizes the lead.

### 2026-11-10: A/B tweaks after the first hardware test

- **Instrument direction swapped:** A double tap = previous instrument, B double tap = next instrument (it was the other way round).
- **Changing instrument resets the transpose** to bank 0 (notes 1–4), and the instrument LED goes back to steady. Before, the bank carried over to the next instrument.
- **No transpose on Drums:** tapping A or B on Drums does nothing, so the ■ LED no longer starts blinking.
- `controls.cpp`: `shiftTrack()` sets `noteBank = 0` and calls `uiSetNoteBank(0)` before `uiShowTrack()`; `shiftNoteBank()` returns early on `TRACK_DRUM`; the double-tap cases no longer undo the first tap by hand (the reset covers it).
- Docs moved from `Bodoque-Documentation/` (git-ignored) to `Titr8/DOC/` (tracked). `C:\Titr8` retired; the repo's `Titr8` folder is the only working copy.
