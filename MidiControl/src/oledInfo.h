#pragma once

#include <Arduino.h>

bool initOLED();
void updateOLED(uint8_t currentLayerPage, const uint8_t potValues[3][3],
				bool bpmReceiving, uint16_t currentBPM);