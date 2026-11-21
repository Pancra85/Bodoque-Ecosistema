#pragma once
#include "sequence.h"

struct AudioEffects
{
    float chorusLevel;
    float chorusDepth;
    float chorusLfoFreq;
    float chorusMaxDelay;

    float reverbLevel;
    float reverbLiveness;
    float reverbDamping;
    float reverbXoverHz;

    bool distType;   // fold o clip
    float distDrive; // 1.0 = sin drive extra, >1 = más saturación
    float distMix;   // 0 = dry, 1 = wet total

    float distCrush;
    float distBits; // ej: 4-8 bits (16 = "sin crush" de profundidad)
    float distRate; // reducción de sample rate
};
extern AudioEffects audioEffects;

void updateAudioOutput();
void audioInit();
void playDrum(uint8_t drumIndex);
void applyEffects();
float freqFromSemitones(float rootFreq, float semitones);
float midiNoteToFreq(float note);