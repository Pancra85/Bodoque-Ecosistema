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

void updateLeds()
{
    const unsigned long now = millis();
    const unsigned long elapsedMs = now - lastLedUpdate;
    if (elapsedMs < LED_UPDATE_INTERVAL_MS)
        return;
    lastLedUpdate = now;

    int selectedLed = (int)currentLayerPage;
    if (selectedLayer == MENU_EFFECTS)
        selectedLed -= (int)PAGE_DISTORTION;
    if (selectedLed < 0 || selectedLed >= NUM_NOTE_BUTTONS)
        selectedLed = 0;

    for (int i = 0; i < NUM_NOTE_BUTTONS; i++)
        digitalWrite(LED_X_PIN[i], i == selectedLed ? HIGH : LOW);

    int visualLayerIndex = 0;
    if (selectedLayer == MENU_LAYER_0 || selectedLayer == MENU_LAYER_1)
    {
        visualLayerIndex = (int)selectedLayer;
    }
    else if (layers[1].active && (!layers[0].active || layers[1].complexity > layers[0].complexity))
    {
        visualLayerIndex = 1;
    }

    Layer &visualLayer = layers[visualLayerIndex];
    const int complexity = constrain(visualLayer.complexity, 0, COMPLEXITY_MAX);
    int animatedLedCount = 0;
    if (visualLayer.active && complexity > 0)
    {
        animatedLedCount = (complexity * NUM_X_LEDS) / COMPLEXITY_MAX;
        if (animatedLedCount == 0)
            animatedLedCount = 1;
    }

    const float elapsedSeconds = elapsedMs / 1000.0f;
    for (int i = 0; i < NUM_X_LEDS; i++)
    {
        bool ledOn = false;
        if (i < animatedLedCount)
        {
            float frequency;
            if (visualLayer.numLFOsOnLayer > 0)
            {
                const int lfoIndex = i % visualLayer.numLFOsOnLayer;
                frequency = visualLayer.lfos[lfoIndex].frequency;
            }
            else
            {
                frequency = 0.25f + 3.0f * complexity / COMPLEXITY_MAX;
            }

            if (frequency < 0.0f)
                frequency = 0.0f;
            ledPhase[i] += LED_TWO_PI * frequency * elapsedSeconds;
            if (ledPhase[i] >= LED_TWO_PI)
                ledPhase[i] = fmodf(ledPhase[i], LED_TWO_PI);

            const float phaseOffset = LED_TWO_PI * i / NUM_X_LEDS;
            ledOn = sinf(ledPhase[i] + phaseOffset) > 0.0f;
        }

        digitalWrite(LED_BUTTON_PIN[i], ledOn ? HIGH : LOW);
        digitalWrite(LED_ESTRELLA_PIN, layers[selectedLayer].active ? HIGH : LOW);
    }
}