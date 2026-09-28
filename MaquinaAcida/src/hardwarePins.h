#pragma once
#include <Arduino.h>

#if DESTRUKTOR1

#define NUM_SEQ_LEDS 4
#define NUM_TRACKS  3
#define NUM_NOTES 4
#define NUM_POTS  2

extern const uint8_t SEQLED_PIN[NUM_SEQ_LEDS];
extern const uint8_t TRACKLED_PIN[NUM_TRACKS];
extern const uint8_t NOTEB_PIN[NUM_NOTES];

extern const uint8_t TRACKB_PIN;
extern const uint8_t TEMPOB_PIN;
extern const uint8_t POT_PIN[NUM_POTS];
 
extern const uint8_t JACKSENSOR_PIN;
extern const uint8_t MUTESPEAKER_PIN;

#define PWMAUDIO1_PIN 14
#define PWMAUDIO2_PIN 15

#elif BODOQUEV1

#define NUM_SEQ_LEDS  4
#define NUM_TRACKS  3
#define NUM_NOTES 4
#define NUM_POTS 3
 
extern const uint8_t SEQLED_PIN[NUM_SEQ_LEDS];
extern const uint8_t TRACKLED_PIN[NUM_TRACKS];
extern const uint8_t NOTEB_PIN[NUM_NOTES];
extern const uint8_t LEDESTRELLA_PIN ;

extern const uint8_t TRACKB_PIN;
extern const uint8_t TEMPOB_PIN;
extern const uint8_t DELAYBTN_PIN ;
extern const uint8_t POT_PIN[NUM_POTS];

extern const uint8_t MUTESPEAKER_PIN;

#define BCK_PIN 20
#define WS_PIN (BCK_PIN + 1) // CANNOT BE CHANGED, HAS TO BE NEXT TO pBCLK, i.e. default is 21
#define DATA_PIN 22

#endif