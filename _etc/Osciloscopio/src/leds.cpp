#include <Arduino.h>
#include <math.h>

#include "hardwarePins.h"
#include "controls.h"
#include "synth.h"

namespace
{
constexpr unsigned long LED_UPDATE_INTERVAL_MS = 20;
constexpr float LED_TWO_PI = 6.28318530718f;

float ledPhase[NUM_X_LEDS] = {};
unsigned long lastLedUpdate = 0;
}

void initLeds()
{
    for (int i = 0; i < NUM_NOTE_BUTTONS; i++)
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
}