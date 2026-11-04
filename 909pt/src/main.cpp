#include <Arduino.h>

#include "hardwarePins.h"
#include "controls.h"
#include "synth.h"
#include "oledInfo.h"
#include "leds.h"
#include "synthEEPROM.h"
#include "sequence.h"


void setup()
{
    initControls();
    initLeds();
    audioInit();
    initSequence();
}

void loop()
{
    updateAudioOutput();
    readButtons();
    updateSequence();
    updateLeds();
}

void setup1()
{
    // initOLED();
}

void loop1()
{

}