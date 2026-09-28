#pragma once

#include <Arduino.h>

extern bool introActive;

bool initOLED();
void updateOLED();
void initEyeAnimation();
void drawEyeAnimation();