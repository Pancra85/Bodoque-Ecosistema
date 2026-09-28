#include "midiIO.h"
// #include "main.h"
#include "sequencer.h"
#include <Adafruit_TinyUSB.h>
#include <MIDI.h>

Adafruit_USBD_MIDI usb_midi;
MIDI_CREATE_INSTANCE(Adafruit_USBD_MIDI, usb_midi, MIDI);

volatile bool midiClockSyncActive = false;
volatile bool midiClockPlaying = false;

// Acumulador para repartir los 24 clocks/negra de MIDI entre "steps" pasos.
// steps=16 -> 6 clocks/paso, steps=32 -> 3, steps=64 -> 1.5 (promediado, ver mas abajo)
static uint16_t midiClockAccum = 0;

static void advanceMidiStep()
{
  advanceStep((uint8_t)stepIndex);
  stepIndex = (stepIndex + 1) % steps;
//   stepClock.start(stepMs);   // mantenemos el EventDelay "al dia" por si se corta el reloj MIDI a mitad de step
  sequencerStepStarted();
}

static void onMidiClock()
{
  midiClockSyncActive = true;

  if (!midiClockPlaying) return;

  midiClockAccum += steps;
  while (midiClockAccum >= 96)
  {
    midiClockAccum -= 96;
    advanceMidiStep();
  }
}

static void onMidiStart()
{
  midiClockSyncActive = true;
  midiClockPlaying = true;
  midiClockAccum = 0;
  stepIndex = 0;
//   stepClock.start(stepMs);
  sequencerStepStarted();
}

static void onMidiContinue()
{
  midiClockSyncActive = true;
  midiClockPlaying = true;
  // no tocamos stepIndex, sigue desde donde estaba
}

static void onMidiStop()
{
  midiClockPlaying = false;
  midiClockAccum = 0;
  // opcional: podrias querer silenciar todo lo que sonando (silenceBass(), etc)
}

void setupMidiIO()
{
  usb_midi.setStringDescriptor("Bodoque MIDI");
  MIDI.begin(MIDI_CHANNEL_OMNI);
  MIDI.setHandleClock(onMidiClock);
  MIDI.setHandleStart(onMidiStart);
  MIDI.setHandleContinue(onMidiContinue);
  MIDI.setHandleStop(onMidiStop);
  MIDI.turnThruOff();
}

void updateMidiIO()
{
  MIDI.read();
}