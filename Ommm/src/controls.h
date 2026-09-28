#pragma once
#define ADC_MAX 1023.0 // 10 bits por default en el core earlephilhower
#include "hardwarePins.h"

enum LayerMenuPage
{
    PAGE_PITCH_FILTER_RES,         // BUTTON_PIN[0]
    PAGE_TYPE_WAVE_COMPLEX,            // BUTTON_PIN[1]
    PAGE_VOL_HARMONY_SCALE,  // BUTTON_PIN[2]
    PAGE_LENGTH_MELODY_TIEMPO, // BUTTON_PIN[3]
    //--paginas de efectos--
    PAGE_DISTORTION,               // BUTTON_PIN[0]
    PAGE_CHORUS,                   // BUTTON_PIN[1]
    PAGE_REVERB,                    // BUTTON_PIN[2]
    PAGE_MEMORY                   // BUTTON_PIN[3]
};

enum SelectedLayer
{
    MENU_LAYER_0,
    MENU_LAYER_1,
    MENU_EFFECTS
};

extern LayerMenuPage currentLayerPage;
extern SelectedLayer selectedLayer;
extern int currentMemorySlot;
extern int loadMemorySlot;

enum MemoryMenuAction
{
    MEMORY_ACTION_FX_CONFIG,
    MEMORY_ACTION_LOAD,
    MEMORY_ACTION_SAVE
};

extern MemoryMenuAction memoryMenuAction;
extern bool effectsEnabledSelection;

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