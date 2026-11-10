#include "mozziConfig.h"
#include <MozziHeadersOnly.h>
#include <EventDelay.h>

#include "sequencer.h"
#include "tracks.h"
#include "controls.h"
#include "midiIO.h"

// Rango de tempo permitido (ms por paso, pasos = semicorcheas)
static const uint16_t SEQ_MIN_STEP_MS = 20;  // paso mas rapido permitido
static const uint16_t SEQ_MAX_STEP_MS = 250; // paso mas lento permitido
// Con 4 pasos por negra, el intervalo util entre taps es:
static const unsigned long TAP_MIN_INTERVAL_MS = (unsigned long)SEQ_MIN_STEP_MS * 4UL; // 80 ms
static const unsigned long TAP_MAX_INTERVAL_MS = (unsigned long)SEQ_MAX_STEP_MS * 4UL; // 1000 ms

static uint8_t steps = 16;
static uint8_t stepIndex = 0;
static bool freePlay = false;
static uint16_t stepMs = 150;
static unsigned long stepStartMillis = 0;

static EventDelay stepClock;
static EventDelay tempoTapTimer;
static bool tempoTapPending = false;
static unsigned long tempoFirstTapTime = 0;

void sequencerSetup(uint8_t numSteps, bool isFreePlay)
{
  steps = numSteps;
  freePlay = isFreePlay;
}

static void stepStarted()
{
  stepStartMillis = millis();
}

static void playStepAndAdvance()
{
  uint8_t step = stepIndex;
  tracksPlayDrums(step);
  controlsRecordRolls(step); // despues de los drums, asi el roll no suena dos veces
  tracksPlayMelodic(step);
  stepIndex = (stepIndex + 1) % steps;
}

void sequencerUpdate()
{
  if (freePlay)
    return;

  if (midiClockSyncActive)
    return; // el avance lo maneja midiIO.cpp mientras haya reloj MIDI

  if (stepClock.ready())
  {
    playStepAndAdvance();
    stepClock.start(stepMs);
    stepStarted();
  }
}

bool sequencerFreePlay() { return freePlay; }
uint8_t sequencerSteps() { return steps; }
uint8_t sequencerStepIndex() { return stepIndex; }
uint16_t sequencerStepMs() { return stepMs; }

uint8_t sequencerJustFiredStep()
{
  return (uint8_t)((stepIndex + steps - 1) % steps);
}

uint8_t sequencerNearestStep(unsigned long eventTime)
{
  if (stepStartMillis == 0 || stepMs == 0)
    return (uint8_t)(stepIndex % steps);

  unsigned long elapsed = eventTime - stepStartMillis;
  if (elapsed >= stepMs)
    elapsed = elapsed % stepMs;

  // Primera mitad del paso: se graba en el paso que ya sono. Segunda mitad: en el que viene.
  if (elapsed < stepMs / 2)
    return sequencerJustFiredStep();
  return (uint8_t)(stepIndex % steps);
}

void sequencerTap(unsigned long now)
{
  if (tempoTapPending)
  {
    unsigned long interval = now - tempoFirstTapTime;
    // Solo es un "segundo tap" valido si cae en el rango de tempo soportado.
    // Si no, este tap pasa a ser el primero de una nueva secuencia.
    if (interval >= TAP_MIN_INTERVAL_MS && interval <= TAP_MAX_INTERVAL_MS)
      stepMs = (uint16_t)(interval / 4u);
  }

  tempoTapPending = true;
  tempoFirstTapTime = now;
  tempoTapTimer.start(TAP_MAX_INTERVAL_MS); // la ventana coincide con el tempo mas lento soportado
}

void sequencerUpdateTapTimeout()
{
  if (tempoTapPending && tempoTapTimer.ready())
    tempoTapPending = false;
}

void sequencerMidiStep()
{
  playStepAndAdvance();
  stepStarted();
}

void sequencerMidiStart()
{
  stepIndex = 0;
  stepStarted();
}
