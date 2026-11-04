#pragma once

#include <stdint.h>

void midiIOBegin();
void midiIOLoop();
bool midiIOClockSynchronized();
bool midiIOClockRunning();
bool midiIOClockStepDue(uint8_t layerIndex);
void midiIONoteOn(uint8_t layerIndex, uint8_t voiceIndex, uint8_t note, uint8_t velocity);
void midiIONoteOff(uint8_t layerIndex, uint8_t voiceIndex);
void midiIOStopLayerNotes(uint8_t layerIndex);