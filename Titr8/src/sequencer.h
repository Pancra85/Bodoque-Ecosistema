#pragma once
#include <Arduino.h>

// Reloj del secuenciador: pasos, tempo (tap tempo) y avance por reloj interno o MIDI.
// 4 pasos por negra.

void sequencerSetup(uint8_t steps, bool freePlay);
void sequencerUpdate(); // avanza con el reloj interno (si no hay reloj MIDI)

bool sequencerFreePlay();
uint8_t sequencerSteps();
uint8_t sequencerStepIndex(); // el proximo paso que va a sonar
uint16_t sequencerStepMs();

uint8_t sequencerNearestStep(unsigned long eventTime); // paso donde grabar un evento
uint8_t sequencerJustFiredStep();                      // el paso que acaba de sonar

// Tap tempo
void sequencerTap(unsigned long now);
void sequencerUpdateTapTimeout();

// Reloj MIDI (llamados desde midiIO.cpp)
void sequencerMidiStep();
void sequencerMidiStart();
