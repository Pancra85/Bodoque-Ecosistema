#include <Arduino.h>

#include "hardwarePins.h"
#include "controls.h"
#include "synth.h"
#include "oledInfo.h"
#include "leds.h"
#include "synthEEPROM.h"

namespace
{
enum StartupPhase
{
    STARTUP_LOAD_PRESET,
    STARTUP_WAIT_REVERB,
    STARTUP_WAIT_CHORUS,
    STARTUP_COMPLETE
};

constexpr unsigned long EFFECT_WARMUP_MS = 500;
StartupPhase startupPhase = STARTUP_LOAD_PRESET;
unsigned long startupPhaseStartedAt = 0;

void serviceStartupSequence()
{
    const unsigned long now = millis();

    if (startupPhase == STARTUP_WAIT_REVERB && now - startupPhaseStartedAt >= EFFECT_WARMUP_MS)
    {
        audioEffects.chorusLevel = 1.0f;
        applyEffects();
        startupPhase = STARTUP_WAIT_CHORUS;
        startupPhaseStartedAt = now;
    }
    else if (startupPhase == STARTUP_WAIT_CHORUS && now - startupPhaseStartedAt >= EFFECT_WARMUP_MS)
    {
        
        loadMemory(0);
        applyEffects();
        startupPhase = STARTUP_COMPLETE;
    }
}
}

void setup()
{
    Serial.begin(115200);

    audioInit();
    initEEPROM();
    loadEffectsEnabledConfig();
    assignOscIndex();
    setDefaultParameters();


    initControls();
    relockAllPots();
    initLeds();
    introActive = 1;

    if (effectsEnabledConfig)
    {
        audioEffects.reverbLevel = 1.0f;
        applyEffects();
        startupPhase = STARTUP_WAIT_REVERB;
        startupPhaseStartedAt = millis();
    }
    else
    {
        loadMemory(0);
        applyEffects();
        startupPhase = STARTUP_COMPLETE;
    }

}

void loop()
{
    serviceStartupSequence();

    // hacer que la escala tambien sea parte del seed de generacion de melodia

    for (int i = 0; i < NUM_LAYERS; i++)
        updateSequenceStep(layers[i]);

    static long potsLastRefresh;
    // const long potsRefreshTime = 20;
    // if (millis() - potsLastRefresh >= potsRefreshTime)
    // {
        readPots();
    // }

    for (int i = 0; i < NUM_LAYERS; i++)
    {
        processLayerRebuilds(layers[i]);
    }

    for (int i = 0; i < NUM_LAYERS; i++) // reconstruye la armonia si es necesario
    {
        if (layers[i].harmonyNeedsRebuild)
        {
            layers[i].harmonyNeedsRebuild = false;
            rebuildHarmonyStructure(layers[i]);
        }
    }

    updateAudioOutput();
}

void setup1()
{
    initOLED();
}

void loop1()
{
    updateLeds();

    static long controlsLastRefresh;
    const long controlsRefreshTime = 20;
    if (millis() - controlsLastRefresh >= controlsRefreshTime)
    {
        readButtons();
    }

    static long oledLastRefresh;
    const long oledRefreshTime = 40;
    if (1 && (millis() - oledLastRefresh >= oledRefreshTime))
    {
        if (introActive)
        {
            drawEyeAnimation();
        }
        else
        {
            updateOLED();
            oledLastRefresh = millis();
        }
    }
}