#include <Arduino.h>

#include "hardwarePins.h"
#include "controls.h"
#include "sequence.h"

PotState potState[TOTAL_POTS];

namespace
{
constexpr unsigned long BUTTON_DEBOUNCE_MS = 20;

bool buttonReading[NUM_NOTE_BUTTONS] = {};
bool buttonPressed[NUM_NOTE_BUTTONS] = {};
unsigned long buttonChangedAt[NUM_NOTE_BUTTONS] = {};
}

void initControls()
{
    for (int i = 0; i < TOTAL_POTS; i++)
    {
        pinMode(POT_PIN[i], INPUT);
    }
    for (int i = 0; i < NUM_NOTE_BUTTONS; i++)
    {
        pinMode(BUTTON_PIN[i], INPUT_PULLUP);
        buttonReading[i] = digitalRead(BUTTON_PIN[i]) == LOW;
        buttonPressed[i] = buttonReading[i];
        buttonChangedAt[i] = millis();
    }
    pinMode(BUTTON_A_PIN, INPUT_PULLUP);
    pinMode(BUTTON_B_PIN, INPUT_PULLUP);
    pinMode(BUTTON_C_PIN, INPUT_PULLUP);
}

void readButtons()
{
    const unsigned long now = millis();

    for (int i = 0; i < NUM_NOTE_BUTTONS; i++)
    {
        const bool reading = digitalRead(BUTTON_PIN[i]) == LOW;
        if (reading != buttonReading[i])
        {
            buttonReading[i] = reading;
            buttonChangedAt[i] = now;
        }

        if (reading != buttonPressed[i] &&
            now - buttonChangedAt[i] >= BUTTON_DEBOUNCE_MS)
        {
            buttonPressed[i] = reading;
            if (buttonPressed[i])
                recordDrumHit(i);
        }
    }
}
