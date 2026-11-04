#include <Arduino.h>
#include <Adafruit_TinyUSB.h>
#include <MIDI.h>

#include "synth.h"
#include "midiIO.h"

namespace
{
constexpr uint8_t MIDI_CLOCKS_PER_STEP = 6;

Adafruit_USBD_MIDI usbMidi;
MIDI_CREATE_INSTANCE(Adafruit_USBD_MIDI, usbMidi, MIDI);

struct ActiveMidiNote
{
    uint8_t note;
    bool active;
};

ActiveMidiNote activeNotes[NUM_LAYERS][MAX_OSCS_PER_LAYER_NOFX] = {};
volatile bool clockSynchronized = false;
volatile bool clockTransportRunning = false;
volatile uint8_t clocksIntoStep = 0;
volatile uint32_t clockStepCount = 0;
uint32_t lastClockStepByLayer[NUM_LAYERS] = {};

void handleClock()
{
    clockSynchronized = true;
    clockTransportRunning = true;
    clocksIntoStep++;
    if (clocksIntoStep >= MIDI_CLOCKS_PER_STEP)
    {
        clocksIntoStep = 0;
        clockStepCount++;
    }
}

void handleStart()
{
    clockSynchronized = true;
    clockTransportRunning = true;
    clocksIntoStep = 0;
    clockStepCount++;
}

void handleContinue()
{
    clockSynchronized = true;
    clockTransportRunning = true;
    clocksIntoStep = 0;
}

void handleStop()
{
    clockSynchronized = true;
    clockTransportRunning = false;
    clocksIntoStep = 0;
}

bool validVoice(uint8_t layerIndex, uint8_t voiceIndex)
{
    return layerIndex < NUM_LAYERS && voiceIndex < MAX_OSCS_PER_LAYER_NOFX;
}
}

void midiIOBegin()
{
    if (!TinyUSBDevice.isInitialized())
        TinyUSBDevice.begin(0);

    usbMidi.setStringDescriptor("Ommm MIDI");
    MIDI.begin(MIDI_CHANNEL_OMNI);
    MIDI.turnThruOff();
    MIDI.setHandleClock(handleClock);
    MIDI.setHandleStart(handleStart);
    MIDI.setHandleContinue(handleContinue);
    MIDI.setHandleStop(handleStop);

    if (TinyUSBDevice.mounted())
    {
        TinyUSBDevice.detach();
        delay(10);
        TinyUSBDevice.attach();
    }
}

void midiIOLoop()
{
#ifdef TINYUSB_NEED_POLLING_TASK
    TinyUSBDevice.task();
#endif

    while (MIDI.read())
    {
    }
}

bool midiIOClockSynchronized()
{
    return clockSynchronized;
}

bool midiIOClockRunning()
{
    return clockTransportRunning;
}

bool midiIOClockStepDue(uint8_t layerIndex)
{
    if (layerIndex >= NUM_LAYERS || !clockTransportRunning)
        return false;

    const uint32_t stepCount = clockStepCount;
    if (stepCount == lastClockStepByLayer[layerIndex])
        return false;

    lastClockStepByLayer[layerIndex] = stepCount;
    return true;
}

void midiIONoteOff(uint8_t layerIndex, uint8_t voiceIndex)
{
    if (!validVoice(layerIndex, voiceIndex))
        return;

    ActiveMidiNote &activeNote = activeNotes[layerIndex][voiceIndex];
    if (activeNote.active && TinyUSBDevice.mounted())
        MIDI.sendNoteOff(activeNote.note, 0, layerIndex + 1);
    activeNote.active = false;
}

void midiIONoteOn(uint8_t layerIndex, uint8_t voiceIndex, uint8_t note, uint8_t velocity)
{
    if (!validVoice(layerIndex, voiceIndex) || note > 127 || velocity == 0)
        return;

    ActiveMidiNote &activeNote = activeNotes[layerIndex][voiceIndex];
    if (activeNote.active && activeNote.note == note)
        return;

    midiIONoteOff(layerIndex, voiceIndex);
    if (TinyUSBDevice.mounted())
        MIDI.sendNoteOn(note, velocity, layerIndex + 1);
    activeNote.note = note;
    activeNote.active = true;
}

void midiIOStopLayerNotes(uint8_t layerIndex)
{
    if (layerIndex >= NUM_LAYERS)
        return;

    for (uint8_t voice = 0; voice < MAX_OSCS_PER_LAYER_NOFX; voice++)
        midiIONoteOff(layerIndex, voice);
}