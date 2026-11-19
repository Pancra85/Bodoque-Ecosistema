#pragma once
#include <Arduino.h>

// Que hace cada boton: tocar, grabar, rolls, cambiar de track, tap tempo,
// delay, el randomizer del arranque y los combos de teclas.

void controlsUpdate(unsigned long now);
void controlsRecordRolls(uint8_t step); // lo llama el secuenciador en cada paso
