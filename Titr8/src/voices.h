#pragma once
#include <Arduino.h>

// Motor de sonido: drums, bass y pad, delay y etapa de salida.

void voicesSetup();
void voicesUpdateControl(uint16_t pot1, uint16_t pot2); // envolventes, filtros y LFOs (control rate)
int16_t voicesNextSample();                             // mezcla final (audio rate)

void triggerDrum(uint8_t drum); // DrumId
// Grados tocables: la escala de 6 notas en dos octavas (0..5 y 6..11)
#define NUM_SCALE_DEGREES 12

void triggerBass(uint8_t degree, bool octaveUp); // degree: 0..NUM_SCALE_DEGREES-1
void triggerPad(uint8_t degree, bool octaveUp);
void triggerLead(uint8_t degree, bool octaveUp); // FM pluck / campana
void silenceBass();
void silencePad();
void silenceLead();

// Randomizacion del sonido
void voicesRandomizeBass();
void voicesRandomizePad();
void voicesRandomizeLead(); // elige un preset de FM (pluck, campana, metal...)
void voicesRandomizeScale(); // escala y raiz nuevas para las notas de los botones

// Salida
void voicesSetOutputGain(uint16_t potValue); // 1ra mitad del pote: volumen, 2da: drive
void voicesSetDelay(bool enabled);
