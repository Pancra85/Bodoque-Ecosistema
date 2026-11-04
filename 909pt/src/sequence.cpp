#include <Arduino.h>

#include "hardwarePins.h"
#include "sequence.h"
#include "synth.h"

namespace
{
constexpr uint16_t TEMPO_BPM = 130;
constexpr uint16_t STEP_INTERVAL_MS = 60000UL / (TEMPO_BPM * 4UL);

unsigned long lastStepAt = 0;
}

bool drumPattern[NUM_NOTE_BUTTONS][MAX_SEQUENCE_STEPS] = {};
uint8_t currentStep = 0;

void initSequence()
{
    currentStep = 0;
    lastStepAt = millis();
}

void recordDrumHit(uint8_t drumIndex)
{
    if (drumIndex >= NUM_NOTE_BUTTONS)
        return;

    drumPattern[drumIndex][currentStep] = true;
    playDrum(drumIndex);
}

void updateSequence()
{
    const unsigned long now = millis();
    if (now - lastStepAt < STEP_INTERVAL_MS)
        return;

    lastStepAt = now;
    currentStep = (currentStep + 1) % MAX_SEQUENCE_STEPS;

    for (uint8_t drum = 0; drum < NUM_NOTE_BUTTONS; drum++)
    {
        if (drumPattern[drum][currentStep])
            playDrum(drum);
    }
}