#pragma once

#define TOTAL_POTS 3
#define NUM_X_LEDS 4
#define NUM_NOTE_BUTTONS 4

#define I2S_BCLK 20
#define I2S_DATA 22
const short POT_PIN[TOTAL_POTS] = {A0,A1,A3};


// Botones de Notas MIDI
const short BUTTON_PIN[NUM_NOTE_BUTTONS] = {2, 4, 5, 7};

const short BUTTON_A_PIN = 0; 
const short BUTTON_B_PIN = 24;
const short BUTTON_C_PIN = 25;

// LEDs
const short LED_BUTTON_PIN[NUM_NOTE_BUTTONS] = {11,10,9,15};
const short LED_X_PIN[NUM_X_LEDS] = {1,3,6,8};
const short LED_ESTRELLA_PIN = 19;
