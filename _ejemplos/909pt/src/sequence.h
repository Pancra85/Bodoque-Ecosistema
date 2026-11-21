#pragma once

#include <stdint.h>

#include "hardwarePins.h"

#define MAX_SEQUENCE_STEPS 16

extern bool drumPattern[NUM_NOTE_BUTTONS][MAX_SEQUENCE_STEPS];
extern uint8_t currentStep;

void initSequence();
void updateSequence();
void recordDrumHit(uint8_t drumIndex);
