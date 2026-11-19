#include <Arduino.h>

#include "synth.h"

void setup()
{
    Serial.begin(115200);
    audioInit();
}

void loop()
{
    updateAudioOutput();
}