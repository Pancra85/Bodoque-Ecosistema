#pragma once
#include <Arduino.h>

// true apenas llega el primer byte de MIDI Clock (0xF8).
// Mientras esta en true, el secuenciador NO debe avanzar por su cuenta (stepClock interno);
// el avance lo maneja midiIO llamando a advanceStep().
extern volatile bool midiClockSyncActive;

// true si el transporte MIDI esta en play (llego Start/Continue y todavia no llego Stop)
extern volatile bool midiClockPlaying;

void setupMidiIO();
void updateMidiIO(); // llamar seguido (en updateControl), lee los mensajes USB-MIDI