#pragma once
#include "synth.h"

#define NUM_MEMORY_SLOTS 20

struct LayerPresetData
{
    float baseNote;
    float filterFreq;
    float resonance;
    float pan;
    float volume;
    float harmonyIntervalSemitones;
    uint32_t seed;
    int32_t complexity;
    int32_t waveform;
    int32_t filterType;
    int32_t harmonyParam;
    int32_t millisPerStep;
    int32_t sequenceLength;
    int32_t density;
    uint8_t active;
    uint8_t harmonyMode;
};

struct PresetData
{
    uint32_t magic;
    uint8_t version;

    LayerPresetData layers[NUM_LAYERS];
    AudioEffects audioEffects;
};

void initEEPROM();
bool saveMemory(int slot);
bool loadMemory(int slot);
bool isSlotUsed(int slot);
bool loadEffectsEnabledConfig();
bool saveEffectsEnabledConfig(bool enabled);