#pragma once

// Configuracion de Mozzi para Titr8 (Bodoque V1: PT8211 por I2S).
// Incluir este header ANTES que cualquier header de Mozzi en cada .cpp,
// asi todos los archivos usan la misma tasa de audio y de control.

#include <MozziConfigValues.h>
#include "hardwarePins.h"

#define MOZZI_ANALOG_READ MOZZI_ANALOG_READ_NONE
#define MOZZI_AUDIO_RATE 32768
#define MOZZI_CONTROL_RATE 128

#define MOZZI_I2S_PIN_BCK BCK_PIN
#define MOZZI_I2S_PIN_WS WS_PIN
#define MOZZI_I2S_PIN_DATA DATA_PIN
#define MOZZI_AUDIO_MODE MOZZI_OUTPUT_I2S_DAC
#define MOZZI_I2S_FORMAT MOZZI_I2S_FORMAT_LSBJ
#define MOZZI_AUDIO_BITS 16
#define MOZZI_AUDIO_CHANNELS MOZZI_STEREO
