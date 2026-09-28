#include <Arduino.h>
#include "hardwarePins.h"

// La configuracion de salida de audio depende de la placa.
// DESTRUKTOR1: PWM por GPIO 14 (mono).
// BODOQUEV1: PT8211 por I2S, formato LSBJ.
#if DESTRUKTOR1

const uint8_t SEQLED_PIN[NUM_SEQ_LEDS] = {1, 3, 6, 8};
const uint8_t TRACKLED_PIN[NUM_TRACKS] = {11, 10, 9};
const uint8_t NOTEB_PIN[NUM_NOTES] = {2, 4, 5, 7};
const uint8_t TRACKB_PIN = 0;
const uint8_t TEMPOB_PIN = 29;
const uint8_t POT_PIN[NUM_POTS] = {A1, A2};

const uint8_t JACKSENSOR_PIN = A0;
const uint8_t MUTESPEAKER_PIN = 17;
#endif


#if BODOQUEV1

const uint8_t SEQLED_PIN[NUM_SEQ_LEDS] = {1, 3, 6, 8};
const uint8_t TRACKLED_PIN[NUM_TRACKS] = {11, 10, 9};
const uint8_t NOTEB_PIN[NUM_NOTES] = {2, 4, 5, 7};
const uint8_t LEDESTRELLA_PIN = 19;
const uint8_t TRACKB_PIN = 0;
const uint8_t TEMPOB_PIN = 25;
const uint8_t DELAYBTN_PIN = 24;
const uint8_t POT_PIN[NUM_POTS] = {A3, A1, A0};
const uint8_t MUTESPEAKER_PIN = 17;

#endif