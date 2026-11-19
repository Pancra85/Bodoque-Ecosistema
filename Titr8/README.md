# Titr8

Acid groovebox firmware for the **Bodoque V1** board (RP2040), built with PlatformIO and [Mozzi](https://sensorium.github.io/Mozzi/). It started as a fork of MaquinaAcida.

Four instruments (■ Drums, ▲ Bass, ● Pads, ✕ FM Lead) record into one pattern that always loops. Everything you play is overdubbed.

## Documentation

| Document | Read it to… |
|---|---|
| [Implementation guide](DOC/Titr8-Guide.md) | Change Titr8 or build a new program from it: panel map, the `Button` class, remapping controls, changing and adding sounds, LEDs, pitfalls. Start here if you want to modify the code. |
| [Refactor technical analysis](DOC/Titr8-Refactor.md) | Understand where the code comes from, how it's organized, the current behavior, known issues and the change log. |
| [Bodoque ecosystem](DOC/Bodoque-Ecosistema.md) | Get the big picture: the board, the other firmware programs in this repo, and how to flash them. |

## Controls at a glance

| Control | Tap | Double tap | Hold |
|---|---|---|---|
| **I–IV** | Play and record a note | — | Drums: roll. Others: octave up |
| **A** | Transpose left | Previous instrument | Clear the track |
| **B** | Transpose right | Next instrument | Delay on/off |
| **C** | Tap tempo | — | — |

**Pots:** 1 = filter cutoff / lead brightness, 2 = resonance / pad decay / lead length, 3 = volume, then drive.

**Power-on options** (hold while powering on): II = 32 steps, III = 64 steps, IV = free play.

## Build

Open this folder in VS Code with the PlatformIO extension, then click **Build** and **Upload**. Details in the [guide](DOC/Titr8-Guide.md#1-build-and-upload).
