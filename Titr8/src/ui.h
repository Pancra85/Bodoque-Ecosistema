#pragma once
#include <Arduino.h>

// LEDs: pulso de negra en los LEDs de secuencia, track seleccionado y LED estrella (delay).
// El LED del track muestra tambien el banco de notas: fijo, parpadeo lento o rapido.

void uiSetup();
void uiShowTrack(uint8_t track);
void uiSetNoteBank(uint8_t bank);
void uiUpdateTrackLeds(unsigned long now); // llamar en cada ciclo de control
void uiUpdateBeatLeds();
void uiSetStarLed(bool on);
