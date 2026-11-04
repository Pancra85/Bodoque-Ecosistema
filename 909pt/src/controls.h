#pragma once
#define ADC_MAX 1023.0 // 10 bits por default en el core earlephilhower
#include "hardwarePins.h"

struct PotState
{
    int lastRaw;
    float filteredRaw;
    bool unlocked;
};

extern PotState potState[TOTAL_POTS];

void initControls();
void relockAllPots();
void readPots();
void readButtons();