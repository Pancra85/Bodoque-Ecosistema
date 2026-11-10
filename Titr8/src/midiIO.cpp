#include "midiIO.h"
#include "sequencer.h"
#include <Adafruit_TinyUSB.h>
#include <MIDI.h>

Adafruit_USBD_MIDI usb_midi;
MIDI_CREATE_INSTANCE(Adafruit_USBD_MIDI, usb_midi, MIDI);

volatile bool midiClockSyncActive = false;
volatile bool midiClockPlaying = false;

// Acumulador para repartir los 24 clocks/negra de MIDI entre los pasos.
// 16 pasos -> 6 clocks/paso, 32 -> 3, 64 -> 1.5 (promediado)
static uint16_t midiClockAccum = 0;

static void onMidiClock()
{
  midiClockSyncActive = true;

  if (!midiClockPlaying)
    return;

  midiClockAccum += sequencerSteps();
  while (midiClockAccum >= 96)
  {
    midiClockAccum -= 96;
    sequencerMidiStep();
  }
}

static void onMidiStart()
{
  midiClockSyncActive = true;
  midiClockPlaying = true;
  midiClockAccum = 0;
  sequencerMidiStart();
}

static void onMidiContinue()
{
  midiClockSyncActive = true;
  midiClockPlaying = true;
  // no tocamos el paso, sigue desde donde estaba
}

static void onMidiStop()
{
  midiClockPlaying = false;
  midiClockAccum = 0;
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
