#pragma once

#include <Arduino.h>
#include "main.h"
#include "hardwarePins.h"

extern uint8_t buttonPins[NUM_NOTES];
extern bool buttonState[NUM_NOTES];
extern bool buttonPressed[NUM_NOTES];
extern bool buttonReleased[NUM_NOTES];
extern bool buttonHeld[NUM_NOTES];
extern bool buttonHoldHandled[NUM_NOTES];
extern unsigned long buttonPressTime[NUM_NOTES];
extern bool rolling[NUM_NOTES];
extern bool recordingRoll[NUM_NOTES];
extern uint8_t holdStartStep[NUM_NOTES];
extern bool octaveHoldActive[NUM_NOTES];
extern bool trackButtonState;
extern bool trackButtonPressed;
extern bool trackButtonReleased;
extern bool trackButtonHeld;
extern bool trackButtonHoldHandled;
extern unsigned long trackButtonPressTime;
extern bool trackButtonInitialState;
extern bool tempoButtonState;
extern bool tempoButtonPressed;
extern bool tempoButtonReleased;
extern unsigned long tempoButtonPressTime;
extern bool tempoButtonInitialState;
extern uint16_t pot1;
extern uint16_t pot2;
extern uint16_t pot3;
extern bool tempoTapPending;
extern unsigned long tempoFirstTapTime;