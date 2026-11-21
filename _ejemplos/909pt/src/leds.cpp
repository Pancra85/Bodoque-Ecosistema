#include <Arduino.h>

#include "hardwarePins.h"
#include "sequence.h"

namespace
{
int lastLitBeat = -1;
}

void initLeds()
{
    for (int i = 0; i < NUM_X_LEDS; i++)
    {
        pinMode(LED_X_PIN[i], OUTPUT);
        digitalWrite(LED_X_PIN[i], LOW);
    }

    for (int i = 0; i < NUM_X_LEDS; i++)
    {
        pinMode(LED_BUTTON_PIN[i], OUTPUT);
        digitalWrite(LED_BUTTON_PIN[i], LOW);
    }
    pinMode(LED_ESTRELLA_PIN, OUTPUT);
    digitalWrite(LED_ESTRELLA_PIN, LOW);
    lastLitBeat = -1;
}

void updateLeds()
{
    const int beat = currentStep / 4;
    if (beat == lastLitBeat)
        return;

    for (int i = 0; i < NUM_NOTE_BUTTONS; i++)
        digitalWrite(LED_BUTTON_PIN[i], i == beat ? HIGH : LOW);

    lastLitBeat = beat;
}