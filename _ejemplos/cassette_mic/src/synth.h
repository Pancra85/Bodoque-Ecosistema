#pragma once

#include <Arduino.h>

extern float playbackSpeedMin;
extern float playbackSpeedMax;
extern float playbackSpeedDeadbandMin;
extern float playbackSpeedDeadbandMax;
extern float playbackRateCorrection;
extern float playbackSpeed;
extern int8_t playbackDirection;
extern float playbackVolumeMin;
extern float playbackVolumeMax;
extern float playbackVolume;
extern float outputSampleRateMin;
extern float outputSampleRateMax;
extern float outputSampleRateCurve;
extern uint32_t outputSampleRate;
extern uint16_t recordingStartDelayMs;
extern uint16_t playbackSpeedUpdateIntervalMs;
extern uint16_t playbackCrossfadeMs;
extern uint8_t LOOP_RECORDING;

void updateAudioOutput();
void audioInit();
