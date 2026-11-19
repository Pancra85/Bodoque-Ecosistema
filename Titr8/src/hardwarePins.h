#pragma once
#include <Arduino.h>

// Pines del Bodoque V1.

#define NUM_SEQ_LEDS   4
#define NUM_TRACK_LEDS 4
#define NUM_NOTES      4
#define NUM_POTS       3

extern const uint8_t SEQLED_PIN[NUM_SEQ_LEDS];
extern const uint8_t TRACKLED_PIN[NUM_TRACK_LEDS];
extern const uint8_t NOTEB_PIN[NUM_NOTES];
extern const uint8_t LEDESTRELLA_PIN;

extern const uint8_t BUTTON_A_PIN;
extern const uint8_t BUTTON_B_PIN;
extern const uint8_t BUTTON_C_PIN;
extern const uint8_t POT_PIN[NUM_POTS];

extern const uint8_t MUTESPEAKER_PIN;

// I2S hacia el DAC PT8211
#define BCK_PIN 20
#define WS_PIN (BCK_PIN + 1) // no se puede cambiar: tiene que ser BCK + 1 (21)
#define DATA_PIN 22
