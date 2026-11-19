#pragma once
#include <Arduino.h>
#include "hardwarePins.h"

// Lectura de botones y potes. Aca no se decide que hace cada boton
// (eso esta en controls.cpp), solo se detectan pulsaciones, holds y sueltas.

class Button
{
public:
  static constexpr unsigned long HOLD_MS = 200;

  // holdMs: cuanto tiempo apretado cuenta como hold
  void begin(uint8_t pin, unsigned long holdMs = HOLD_MS);

  // Lee el boton al encender. Si ya estaba apretado, su proxima pulsacion se
  // ignora hasta que se suelte. syncState=true ademas toma ese estado como el
  // actual (asi se comportaban TRACK y TEMPO en MaquinaAcida, hoy A y C).
  void captureBootState(bool syncState);

  void update(unsigned long now);

  bool isDown() const { return down; }
  bool wasPressed() const { return pressed; }   // flanco de bajada, solo este ciclo
  bool wasReleased() const { return released; } // flanco de subida, solo este ciclo
  unsigned long pressTime() const { return pressedAt; }

  // true una sola vez por pulsacion, cuando el boton lleva holdMs apretado
  bool takeHold();
  // true si el hold de esta pulsacion ya se uso (o la consumio un combo)
  bool holdUsed() const { return holdHandled; }

  // La pulsacion la uso un combo: anula su accion normal (tap, hold y soltar)
  void consume();

private:
  uint8_t pin = 0;
  unsigned long holdMs = HOLD_MS;
  bool down = false;
  bool pressed = false;
  bool released = false;
  bool held = false;
  bool holdHandled = false;
  bool ignoreUntilRelease = false;
  unsigned long pressedAt = 0;
};

extern Button noteButtons[NUM_NOTES];
extern Button buttonA; // transponer a la izquierda / instrumento anterior / borrar track
extern Button buttonB; // transponer a la derecha / instrumento siguiente / delay
extern Button buttonC; // tap tempo

void inputSetup();
void inputCaptureBootState();
void inputUpdate(unsigned long now);

uint16_t potValue(uint8_t index);
bool anyNoteButtonActive();
