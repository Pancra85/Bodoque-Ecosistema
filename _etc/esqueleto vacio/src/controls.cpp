#include <Arduino.h>
#include <math.h>
#include <RP2040.h>

#include "hardwarePins.h"
#include "synth.h"
#include "controls.h"
#include "amy.h"
#include "sequence.h"
#include "oledInfo.h"
#include "synthEEPROM.h"

PotState potState[TOTAL_POTS];

LayerMenuPage currentLayerPage = PAGE_PITCH_FILTER_RES;
SelectedLayer selectedLayer = MENU_LAYER_0;
int lastComplexityLevel[NUM_LAYERS] = {-1, -1};
int currentMemorySlot = 0;
int loadMemorySlot = 0;
MemoryMenuAction memoryMenuAction = MEMORY_ACTION_FX_CONFIG;
bool effectsEnabledConfig = true;
bool effectsEnabledSelection = true;
constexpr float POT_SMOOTHING_ALPHA = 0.08f;

void initControls()
{
    for (int i = 0; i < TOTAL_POTS; i++)
    {
        pinMode(POT_PIN[i], INPUT);
    }
    for (int i = 0; i < NUM_NOTE_BUTTONS; i++)
    {
        pinMode(BUTTON_PIN[i], INPUT_PULLUP);
    }
    pinMode(BUTTON_A_PIN, INPUT_PULLUP);
    pinMode(BUTTON_B_PIN, INPUT_PULLUP);
    pinMode(BUTTON_C_PIN, INPUT_PULLUP);
}