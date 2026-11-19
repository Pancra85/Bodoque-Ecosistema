#include <Arduino.h>
#include "hardwarePins.h"

// Bodoque V1: audio por PT8211 (I2S, formato LSBJ).

const uint8_t SEQLED_PIN[NUM_SEQ_LEDS] = {1, 3, 6, 8};
const uint8_t TRACKLED_PIN[NUM_TRACK_LEDS] = {11, 10, 9, 15}; // cuadrado, triangulo, circulo, X
const uint8_t NOTEB_PIN[NUM_NOTES] = {2, 4, 5, 7};
const uint8_t LEDESTRELLA_PIN = 19;
const uint8_t BUTTON_A_PIN = 0;
const uint8_t BUTTON_B_PIN = 24;
const uint8_t BUTTON_C_PIN = 25;
const uint8_t POT_PIN[NUM_POTS] = {A3, A1, A0};
const uint8_t MUTESPEAKER_PIN = 17;
