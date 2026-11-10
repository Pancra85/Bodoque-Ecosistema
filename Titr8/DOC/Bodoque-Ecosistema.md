# Bodoque Ecosystem

**Bodoque** is a small DIY synthesizer board built around the **Raspberry Pi RP2040** (the Pico chip). The board stays the same and you change what it is by flashing different firmware. One firmware turns it into a drone machine, another into a drum machine, another into a USB MIDI controller.

This repository (`Bodoque-Ecosistema`) holds the hardware design, every firmware program, and the tools you need to build a new one.

---

## Hardware

| Part | Details |
|---|---|
| MCU | RP2040, overclocked to 250 MHz |
| Audio out | **PT8211** 16-bit stereo DAC over I2S (BCLK on GPIO 20, DATA on GPIO 22) |
| Pots | 3 potentiometers, known as *Alfa*, *Omega* and *Pi* (A0, A1, A3) |
| Buttons | 4 numbered note/step buttons (**I, II, III, IV**) and 3 function buttons (**A, B, C**) |
| LEDs | 1 LED per numbered button (**I–IV**, GPIO 1, 3, 6, 8), 4 shape LEDs (**■ square, ▲ triangle, ● circle, ✕ X**, GPIO 11, 10, 9, 15), 1 *Estrella* (star) LED (GPIO 19) and 1 power LED |
| Display | Optional SSD1306 OLED |
| USB | USB-C, used for power, flashing and USB MIDI |

The KiCad schematics and PCB are in [`_docs/schematics KiCad`](../_docs/schematics%20KiCad). Each firmware declares its own pin map in `src/hardwarePins.h`.

---

## Software stack

Every program is a separate **PlatformIO** project with the same base setup:

- **Platform and core:** `maxgerhardt/platform-raspberrypi` with the **Earle Philhower** Arduino core, board `pico`.
- **Sound engine:** each program uses one of these two.
  - **AMY**, the `shorepine/AMY Synthesizer` library. It provides additive and subtractive synthesis, the 909 drum sounds and effects. Bodoque pushes AMY's output to the PT8211 through its own routine instead of using AMY's default output.
  - **Mozzi** (`sensorium/Mozzi`) with I2S DAC output.
- **Common libraries:**
  - Adafruit SSD1306 for the OLED.
  - Adafruit TinyUSB and the MIDI Library for USB MIDI.
  - Control-Surface for the MIDI controller program.

The RP2040 has no FPU, so the code avoids float math and division in the audio path wherever it can.

---

## Programs

| Folder | Engine | What it does |
|---|---|---|
| **Ommm** | AMY | Two-layer generative synth (A/B) with an effects layer, for evolving soundscapes. |
| **Ommm - Midi (sin fx)** | AMY | Ommm with USB MIDI and no effects. |
| **Atmos** | AMY | Generative ambient synth with 4 engines: ATMOS, CRYSTAL, PULSE and TEXTURE. |
| **909pt** | AMY | Drum machine using the AMY 909 kit and a 16-step sequencer. |
| **MaquinaAcida** | Mozzi | Groovebox with DRUM, SYNTH (bass) and PAD tracks, always looping with overdub. |
| **Titr8** | Mozzi | MaquinaAcida fork being extended (Bodoque V1 only). A/B tap transposes the 4 note buttons through 12 scale degrees, A double tap = previous instrument, B double tap = next (■ Drums, ▲ Bass, ● Pads, ✕ FM Lead), hold A clears the track, hold B toggles delay, C taps tempo. See [Titr8-Refactor.md](Titr8-Refactor.md). |
| **Nube** | Mozzi | "El Nube", a slowly evolving ambient organism where the user *sows clouds* instead of playing notes. |
| **MidiControl** | — | USB MIDI controller: the buttons send notes and the pots send CCs across 3 pages (A/B/C). |

Each folder has a README with that program's controls.

---

## Repository layout

```
Bodoque-Ecosistema/
├── <Program>/          one PlatformIO project per firmware
├── _core/              shared code, drivers and DSP (in progress)
├── _docs/              KiCad hardware design
├── _drag&drop/         prebuilt .uf2 firmware, ready to flash
└── _etc/esqueleto vacio/   empty AMY template for new programs
```

---

## Flashing a program

**Without building (fastest):**

1. Connect Bodoque over USB.
2. Short the two **BOOT** pads (with a clip or tweezers) and keep them shorted while you power the board on.
3. A USB drive appears on the computer.
4. Drag the `.uf2` you want from `_drag&drop/` onto that drive.

**From source:** open the program's folder in VS Code with PlatformIO, then run **Build** and **Upload**.

---

## Making a new program

Copy `_etc/esqueleto vacio`. It is an empty AMY project already set up for Bodoque. Write the new program, then put its `README.md` and its built `.uf2` in `_drag&drop/`.
