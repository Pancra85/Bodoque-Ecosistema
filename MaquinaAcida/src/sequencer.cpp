#include <Arduino.h>
#include "MozziConfigValues.h"
#include "hardwarePins.h"

#define MOZZI_ANALOG_READ MOZZI_ANALOG_READ_NONE
#define MOZZI_AUDIO_RATE 32768
#define MOZZI_CONTROL_RATE 128
#include <Oscil.h>
#include <ADSR.h>
#include <StateVariable.h>
#include <tables/sin2048_int8.h>
#include <tables/square_no_alias_2048_int8.h>
#include <tables/saw2048_int8.h>
#include <tables/whitenoise8192_int8.h>
#include <mozzi_midi.h>
#include <AudioDelayFeedback.h>

#include "sequencer.h"
#include "main.h"
#include "controles.h"
#include "midiIO.h"

TrackMode mode = DRUM_MODE;
bool freePlay = false;
bool countIn = false;
uint8_t countInBeat = 0;
EventDelay randomizeClock;
EventDelay stepClock;
EventDelay countInClock;
EventDelay rollDelay;
EventDelay modeLedTimer;
EventDelay tempoTapTimer;

uint16_t stepMs = 150;
// Rango de tempo permitido (ms por paso, steps = semicorcheas)
const uint16_t SEQ_MIN_STEP_MS = 20;   // paso más rápido permitido
const uint16_t SEQ_MAX_STEP_MS = 250;  // paso más lento permitido
// Asumiendo 4 pasos por pulso, el intervalo entre taps útil es:
const unsigned long TAP_MIN_INTERVAL_MS = (unsigned long)SEQ_MIN_STEP_MS * 4UL; // 80 ms
const unsigned long TAP_MAX_INTERVAL_MS = (unsigned long)SEQ_MAX_STEP_MS * 4UL; // 1000 ms

unsigned long sequencerStepStartMillis = 0;

ADSR<CONTROL_RATE, MOZZI_AUDIO_RATE> kickEnv;
ADSR<CONTROL_RATE, MOZZI_AUDIO_RATE> snareEnv;
ADSR<CONTROL_RATE, MOZZI_AUDIO_RATE> hatEnv;
ADSR<CONTROL_RATE, MOZZI_AUDIO_RATE> snareToneEnv;
ADSR<CONTROL_RATE, MOZZI_AUDIO_RATE> bassEnv;
ADSR<CONTROL_RATE, MOZZI_AUDIO_RATE> padEnv;
ADSR<CONTROL_RATE, MOZZI_AUDIO_RATE> crashEnv;
StateVariable<LOWPASS> bassLPF;
StateVariable<LOWPASS> padLPF;
StateVariable<HIGHPASS> eqPad;
StateVariable<HIGHPASS> eqSnare;
uint16_t bassFilterCutoffBase = 800;
uint8_t bassFilterResonance = 120;
uint16_t padFilterCutoffBase = 800;
uint8_t padFilterResonance = 120;
Oscil<SIN2048_NUM_CELLS, CONTROL_RATE> bassPitchLFO(SIN2048_DATA);
Oscil<SIN2048_NUM_CELLS, CONTROL_RATE> padPitchLFO(SIN2048_DATA);
uint8_t bassEnvLevel = 0;
uint8_t padEnvLevel = 0;
uint8_t crashEnvLevel = 0;
int16_t lastHatNoise = 0;
bool bassActive = false;
bool bassOsc2Active = false;
bool padActive = false;
bool padBassDelayEnabled = false;

uint8_t steps = 16;
int16_t stepIndex = 0;
uint8_t kickSeq[MAX_STEPS] = {0};
uint8_t snareSeq[MAX_STEPS] = {0};
uint8_t hatSeq[MAX_STEPS] = {0};
uint8_t bassSeq[MAX_STEPS] = {0};
uint8_t padSeq[MAX_STEPS] = {0};

uint8_t buttonPins[NUM_NOTES] = {NOTEB_PIN[0], NOTEB_PIN[1], NOTEB_PIN[2], NOTEB_PIN[3]};
bool buttonState[NUM_NOTES] = {false};
bool buttonPressed[NUM_NOTES] = {false};
bool buttonReleased[NUM_NOTES] = {false};
bool buttonHeld[NUM_NOTES] = {false};
bool buttonHoldHandled[NUM_NOTES] = {false};
unsigned long buttonPressTime[NUM_NOTES] = {0};
bool rolling[NUM_NOTES] = {false};
bool recordingRoll[NUM_NOTES] = {false};
uint8_t holdStartStep[NUM_NOTES] = {0};
bool octaveHoldActive[NUM_NOTES] = {false};
bool trackButtonState = false;
bool trackButtonPressed = false;
bool trackButtonReleased = false;
bool trackButtonHeld = false;
bool trackButtonHoldHandled = false;
unsigned long trackButtonPressTime = 0;
bool trackButtonInitialState = false;
bool tempoButtonState = false;
bool tempoButtonPressed = false;
bool tempoButtonReleased = false;
unsigned long tempoButtonPressTime = 0;
bool tempoButtonInitialState = false;
uint16_t pot1 = 0;
uint16_t pot2 = 0;
uint16_t pot3 = 0;
bool tempoTapPending = false;
unsigned long tempoFirstTapTime = 0;

#if BODOQUEV1
AudioDelayFeedback<16384> padBassDelay;
#endif

// Osciladores de audio
Oscil<SIN2048_NUM_CELLS, MOZZI_AUDIO_RATE> kickOsc(SIN2048_DATA);
Oscil<SQUARE_NO_ALIAS_2048_NUM_CELLS, MOZZI_AUDIO_RATE> bassOsc(SQUARE_NO_ALIAS_2048_DATA);
Oscil<SQUARE_NO_ALIAS_2048_NUM_CELLS, MOZZI_AUDIO_RATE> bassOsc2(SQUARE_NO_ALIAS_2048_DATA);

// Osciladores PAD
Oscil<SAW2048_NUM_CELLS, MOZZI_AUDIO_RATE> padOsc1(SAW2048_DATA);
Oscil<SAW2048_NUM_CELLS, MOZZI_AUDIO_RATE> padOsc2(SAW2048_DATA);
Oscil<SAW2048_NUM_CELLS, MOZZI_AUDIO_RATE> padOsc3(SAW2048_DATA);
Oscil<SAW2048_NUM_CELLS, MOZZI_AUDIO_RATE> padOsc4(SAW2048_DATA);

// Oscilador para el tono del snare
 Oscil<SIN2048_NUM_CELLS, MOZZI_AUDIO_RATE> snareToneOsc(SIN2048_DATA);

// Osciladores de ruido blanco para snare y hi-hat
Oscil<WHITENOISE8192_NUM_CELLS, MOZZI_AUDIO_RATE> snareNoiseOsc(WHITENOISE8192_DATA);
Oscil<WHITENOISE8192_NUM_CELLS, MOZZI_AUDIO_RATE> hatNoiseOsc(WHITENOISE8192_DATA);

// Osciladores para crash cymbal
Oscil<SQUARE_NO_ALIAS_2048_NUM_CELLS, MOZZI_AUDIO_RATE> crashOsc(SQUARE_NO_ALIAS_2048_DATA);
Oscil<WHITENOISE8192_NUM_CELLS, MOZZI_AUDIO_RATE> crashNoiseOsc(WHITENOISE8192_DATA);

#if BODOQUEV1
const uint16_t PAD_BASS_DELAY_MS = 800; // no se usa mas, se calculan las cells directamente
const int8_t PAD_BASS_DELAY_FEEDBACK = 70;

// AudioDelayFeedback<PAD_BASS_DELAY_CELLS> padBassDelay;


#endif

// Chord interval sets (semitones) for 4 buttons
const int8_t chordIntervals[NUM_CHORDS][4] = {
    {0, 3, 7, 10}, // m7
    {0, 3, 7, 14}, // m9 (add9)
    {0, 5, 7, 10}, // m7sus4
    {0, 3, 6, 10}, // m7b5
    {0, -5, 0, -5},
    {0, 0, 0, 0},
    {0, 12, 12, 12},
    {0, 2, 0, 0}};

// Notas para synth: 3 notas base + 3 notas alternativas
uint8_t synthNotes[6] = {35, 36, 37, 38, 39, 40}; // C4, G4, C5, E4, B4, E5

// Definir 12 escalas diferentes (intervalos en semitonos desde la raíz) - Oscuras/Árabes
const uint8_t scales[12][6] = {
    {0, 2, 3, 5, 7, 11}, // Harmonic Minor - oscuro
    {0, 1, 3, 5, 7, 8},  // Phrygian - muy oscuro
    {0, 1, 3, 5, 6, 8},  // Locrio - ultra oscuro
    {0, 1, 4, 5, 6, 8},  // Oriental/Árabe
    {0, 2, 3, 6, 8, 11}, // Hungarian Minor - muy oscuro
    {0, 1, 3, 4, 6, 8},  // Ultralocrian - oscurísimo
    {0, 1, 4, 5, 7, 8},  // Byzantine - árabe
    {0, 3, 5, 6, 7, 10}, // Blues
    {0, 2, 4, 5, 7, 9},  // Dórico - neutral
    {0, 1, 3, 5, 6, 9},  // Super Locrian bb7 - muy oscuro
    {0, 2, 3, 5, 7, 8},  // Menor Natural
    {0, 1, 4, 6, 7, 10}  // Árabe/Maqam
};



// Nota base del bajo
int bassBaseNote = 0;
float bassDetune = 0.0f;

// Nota base del PAD
float padBaseNote = 0.0f;




// Estado de LFO del PAD
int16_t currentPadCutoffLFO = 0;



// Output control: the first half of the pot is volume, the second half adds drive.
const int32_t OUT_MAX_DRIVE_Q10 = 12 * 1024;
const int32_t OUT_MAX_VOLUME_Q10 = 1088;
const int32_t OUT_SOFT_KNEE = 384;
const int32_t OUT_CEILING = 512;
int32_t outVolumeQ10 = 0;
int32_t outDriveQ10 = 1024;

// Variables para el LFO del cutoff
int16_t currentBassLFO = 0;

// Variables para randomización del bass
float bassPitchLFOFreq = 0.5f;
bool bassOsc3Active = false;

// Generic amplitude control variables (used by randomizer)
float bassPitchMod;
float bassPitchLFOAmplitude;
float padPitchMod;
float padPitchLFOAmplitude;

// Variables para randomización del PAD
float padLFOFreq = 0.8f;
float padPitchLFOFreq = 0.5f;
float padDetune2 = 0.0f;
float padDetune3 = 0.0f;
// Current pad chord type selected by randomizer (0..3)
uint8_t padChordType = 0;

void initDelay()
{
#if BODOQUEV1
    padBassDelay.setDelayTimeCells((uint16_t)PAD_BASS_DELAY_CELLS); //lo desactivo porque está fijo
    padBassDelay.setFeedbackLevel(0);
#endif
}

void updatePadBassDelay()
{
#if BODOQUEV1
    static bool buttonToggle = false;
    static bool previousButtonState = false;
    bool buttonPressed = !digitalRead(DELAYBTN_PIN);

    if (buttonPressed == 1 && previousButtonState == 0)
    {
        if (buttonToggle == 1)
        {
            buttonToggle = 0;
            padBassDelay.setFeedbackLevel(0);
            padBassDelayEnabled = 0;
        }
        else
        {
            buttonToggle = 1;
            padBassDelay.setFeedbackLevel(PAD_BASS_DELAY_FEEDBACK);
            padBassDelayEnabled = 1;
        }
    }
    digitalWrite(LEDESTRELLA_PIN, buttonToggle);
    previousButtonState = buttonPressed;
#endif
}

void updateOutputGain(uint16_t potValue)
{
    potValue = constrain(potValue, (uint16_t)0, (uint16_t)1023);

    if (potValue <= 512)
    {
        outVolumeQ10 = ((int32_t)potValue * 1024) / 512;
        outDriveQ10 = 1024;
        return;
    }

    uint16_t drivePosition = potValue - 512;
    outVolumeQ10 = 1024 + ((int32_t)drivePosition * (OUT_MAX_VOLUME_Q10 - 1024)) / 511;
    outDriveQ10 = 1024 + ((int32_t)drivePosition * (OUT_MAX_DRIVE_Q10 - 1024)) / 511;
}

int16_t applyOutputGain(int16_t sample)
{
    int32_t output = ((int32_t)sample * outVolumeQ10) / 1024;
    if (outDriveQ10 == 1024)
        return (int16_t)constrain(output, -OUT_CEILING, OUT_CEILING);

    int32_t driven = (output * outDriveQ10) / 1024;
    int32_t magnitude = abs(driven);
    if (magnitude > OUT_SOFT_KNEE)
    {
        int32_t excess = magnitude - OUT_SOFT_KNEE;
        magnitude = OUT_SOFT_KNEE + (excess * (OUT_CEILING - OUT_SOFT_KNEE)) /
                                        ((OUT_CEILING - OUT_SOFT_KNEE) + excess);
    }

    #if BODOQUEV1
    return (int16_t)(driven < 0 ? -magnitude : magnitude);
    #elif DESTRUKTOR1
    return OUT_MAX_VOLUME_Q10;
    #endif
}

void handleSequencer()
{
    if (freePlay)
    {
        return;
    }

    if (midiClockSyncActive)
    {
        return; // el avance de step lo maneja midiIO.cpp mientras haya reloj MIDI
    }

    if (stepClock.ready())
    {
        advanceStep((uint8_t)stepIndex);
        stepIndex = (stepIndex + 1) % steps;
        stepClock.start(stepMs);
        sequencerStepStarted();
    }
}

void randomizeSynthNotes()
{
    // Elegir una escala al azar (0-11)
    uint8_t scaleIndex = random(0, 12);
    // Elegir una raíz al azar (de C3=48 a C5=72, pero ajustado para que quepa)
    uint8_t root = random(30, 42); // De C3 a C5 (36-60), para que las notas no excedan MIDI range
    // Serial.print("-  root:");
    // Serial.print(root);
    for (uint8_t i = 0; i < 6; i++)
    {
        synthNotes[i] = root + scales[scaleIndex][i];
        // Serial.print(" - > ");
        // Serial.print(synthNotes[i]);
    }
    // Serial.println();
}

void randomizeBassConfigValues()
{
    bassPitchLFOFreq = random(10, 100) / 100.0f; // de 0.10 a 2.00
    bassPitchLFO.setFreq(bassPitchLFOFreq);
    if (random(0, 10) <= 4)
    {
        bassPitchLFOAmplitude = random(1, 20); // de 0.01 a 1.00
    }
    else
    {
        bassPitchLFOAmplitude = 0;
    }

    //  Randomizar detune del segundo oscilador del bajo
    int detuneType = random(0, 100);
    if (detuneType < 20)
    {
        bassOsc2Active = false;
    }
    else if (detuneType < 50)
    {
        bassOsc2Active = true;
        bassDetune = random(1, 11) / 1000.0f; // 0.01 to 0.10 notas
    }
    else if (detuneType < 70)
    {
        bassOsc2Active = true;
        bassDetune = 5.0f + random(0, 11) / 1000;
    }
    else if (detuneType < 85)
    {
        bassOsc2Active = true;
        bassDetune = 12.0f + random(0, 11) / 1000;
    }
    else
    {
        bassOsc2Active = true;
        bassDetune = 7.0f + random(0, 11) / 1000;
    }
    // Serial.print(bassPitchLFOAmplitude);
    // Serial.print("-");
    // Serial.println(bassDetune);
}

void randomizePadConfigValues()
{
    padPitchLFOFreq = random(10, 401) / 100.0f;
    padPitchLFO.setFreq(padPitchLFOFreq);
    if (random(0, 10) <= 4)
    {
        padPitchLFOAmplitude = random(1, 10); // de 0.01 a 1.00
    }
    else
    {
        padPitchLFOAmplitude = 0;
    }
    int padDetType = random(0, 100);
    if (padDetType < 30)
    {
        padDetune2 = 0.0f;
        padDetune3 = 0.0f;
    }
    else if (padDetType < 65)
    {
        padDetune2 = random(1, 10) / 100.;
        padDetune3 = random(1, 10) / 100.;
    }
    else
    {
        padDetune2 = random(1, 30) / 100.;
        padDetune3 = random(7, 30) / 100.;
    }

    // randomize chord type for pad voicings (0..3)
    padChordType = random(0, NUM_CHORDS);
    // Serial.print(padChordType);
    // Serial.print(" - ");
    // Serial.print(padPitchLFOAmplitude);
    // Serial.print(" - ");
    // Serial.println(padDetune2);
}

void updateSynthNotesIfNeeded()
{
    // Check if any notes are recorded in bass or pad sequences
    bool bassHasNotes = false;
    bool padHasNotes = false;

    for (int i = 0; i < MAX_STEPS; i++)
    {
        if (bassSeq[i] != 0)
            bassHasNotes = true;
        if (padSeq[i] != 0)
            padHasNotes = true;
    }

    // If neither track has notes, randomize synth notes
    if (!bassHasNotes && !padHasNotes)
    {
        randomizeSynthNotes();
    }
}

void randomizeConfigValues()
{
    if (!randomizeClock.ready())
    {
        return;
    }

    randomizeBassConfigValues();
    randomizePadConfigValues();
    updateSynthNotesIfNeeded();

    randomizeClock.start(150);
}

void clearBassTrack()
{
    memset(bassSeq, 0, sizeof(bassSeq));
    bassActive = false;
    bassBaseNote = 0;
    bassOsc.setFreq(0.0f);
    bassOsc2.setFreq(0.0f);
}

void clearPadTrack()
{
    memset(padSeq, 0, sizeof(padSeq));
    padActive = false;
    padBaseNote = 0;
    padOsc1.setFreq(0.0f);
    padOsc2.setFreq(0.0f);
    padOsc3.setFreq(0.0f);
    padOsc4.setFreq(0.0f);
}



bool anyInstrumentButtonActive()
{
    for (int i = 0; i < NUM_NOTES; i++)
    {
        if (buttonState[i] || buttonPressed[i])
        {
            return true;
        }
    }
    return false;
}

void loadDemoPattern()
{
    // demo acid / techno oscuro
    uint8_t demoKick[16] = {1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0};
    uint8_t demoSnare[16] = {0, 0, 1, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0};
    uint8_t demoHat[16] = {1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0};
    uint8_t demoSynth[16] = {1, 0, 2, 0, 3, 0, 4, 0, 5, 0, 6, 0, 0, 0, 0, 0};

    clearDrumTracks();
    clearBassTrack();
    clearPadTrack();

    for (uint8_t i = 0; i < steps; i++)
    {
        kickSeq[i] = demoKick[i % 16];
        snareSeq[i] = demoSnare[i % 16];
        hatSeq[i] = demoHat[i % 16];
        bassSeq[i] = demoSynth[i % 16];
    }
}

uint32_t beatDurationMs()
{
    return (uint32_t)stepMs * 4u;
}

uint8_t getSequencerNearestStep(unsigned long eventTime)
{
    if (stepIndex < 0)
        return 0;

    if (sequencerStepStartMillis == 0 || stepMs == 0)
        return (uint8_t)(stepIndex % steps);

    unsigned long elapsed = eventTime - sequencerStepStartMillis;
    if (elapsed >= stepMs)
        elapsed = elapsed % stepMs;

    unsigned long halfStep = stepMs / 2;
    uint8_t upcomingStep  = (uint8_t)(stepIndex % steps);                 // el que está por sonar
    uint8_t justFiredStep = (uint8_t)((stepIndex + steps - 1) % steps);   // el que ya sonó

    if (elapsed < halfStep)
        return justFiredStep;
    return upcomingStep;
}

void sequencerStepStarted()
{
    sequencerStepStartMillis = millis();
}

void clearDrumTracks()
{
    memset(kickSeq, 0, sizeof(kickSeq));
    memset(snareSeq, 0, sizeof(snareSeq));
    memset(hatSeq, 0, sizeof(hatSeq));
}

uint8_t noteIndexForButton(int buttonIndex)
{
    return buttonIndex;
}

void playDrumButton(int buttonIndex)
{
    if (buttonIndex == 0)
    {
        triggerKick();
    }
    else if (buttonIndex == 1)
    {
        triggerSnare();
    }
    else if (buttonIndex == 2)
    {
        triggerHat();
    }
    else if (buttonIndex == 3)
    {
        triggerCrash();
    }
}

void recordDrumButton(int buttonIndex, uint8_t stepToRecord)
{
    if (buttonIndex == 0)
        kickSeq[stepToRecord] = 1;
    else if (buttonIndex == 1)
        snareSeq[stepToRecord] = 1;
    else if (buttonIndex == 2)
        hatSeq[stepToRecord] = 1;
    else if (buttonIndex == 3)
        hatSeq[stepToRecord] = 2; // Use value 2 to distinguish crash from regular hat
}

void playNoteButton(int buttonIndex, bool octaveUp, unsigned long eventTime)
{
    uint8_t index = noteIndexForButton(buttonIndex);
    uint8_t nearestStep = getSequencerNearestStep(eventTime);
    uint8_t justFiredStep = (uint8_t)((stepIndex + steps - 1) % steps);
    bool shouldPlayLive = freePlay || (nearestStep == justFiredStep);

    if (mode == DRUM_MODE)
    {
        if (shouldPlayLive)
            playDrumButton(buttonIndex);
        if (!freePlay)
            recordDrumButton(buttonIndex, nearestStep);
    }
    else if (mode == SYNTH_MODE)
    {
        if (shouldPlayLive)
            triggerSynth(index, octaveUp);
        if (!freePlay)
            bassSeq[nearestStep] = index + 1;
    }
    else if (mode == PAD_MODE)
    {
        if (shouldPlayLive)
            triggerPad(index, octaveUp);
        if (!freePlay)
            padSeq[nearestStep] = PADSEQ_PACK(index, (octaveUp ? 1 : 0), 0) + 1;
    }
}

void handleButtonHold(int buttonIndex)
{
    if (buttonHoldHandled[buttonIndex])
    {
        return;
    }
    buttonHoldHandled[buttonIndex] = true;

    if (buttonIndex < NUM_NOTES)
    {
        if (mode == DRUM_MODE)
        {
            rolling[buttonIndex] = true;
            recordingRoll[buttonIndex] = true;
            playDrumButton(buttonIndex); // Sonar inmediatamente al iniciar roll
        }
        else
        {
            uint8_t index = noteIndexForButton(buttonIndex);
            if (!freePlay)
            {
                if (mode == SYNTH_MODE)
                    bassSeq[holdStartStep[buttonIndex]] = index + 7;
                else if (mode == PAD_MODE)
                    padSeq[holdStartStep[buttonIndex]] = PADSEQ_PACK(index, 1, 0) + 1;
            }
            // Hold triggers only the selected track for live play
            if (mode == SYNTH_MODE)
                triggerSynth(index, true);
            else if (mode == PAD_MODE)
                triggerPad(index, true);
            octaveHoldActive[buttonIndex] = true;
        }
    }
}

void handleTrackButtonHold()
{
    if (!trackButtonHeld || trackButtonHoldHandled)
    {
        return;
    }
    trackButtonHoldHandled = true;
    if (mode == DRUM_MODE)
    {
        clearDrumTracks();
    }
    else if (mode == SYNTH_MODE)
    {
        clearBassTrack();
        randomizeBassConfigValues();
        updateSynthNotesIfNeeded();
    }
    else if (mode == PAD_MODE)
    {
        clearPadTrack();
        randomizePadConfigValues();
        updateSynthNotesIfNeeded();
    }
}

void handleTrackButtonRelease()
{
    if (!trackButtonReleased || trackButtonHoldHandled)
    {
        return;
    }

    unsigned long now = millis();
    unsigned long duration = now - trackButtonPressTime;
    if (duration >= 1000)
    {
        return;
    }

    mode = (TrackMode)((mode + 1) % NUM_TRACKS);
    setTrackLed((uint8_t)mode);
}

void handleTempoButtonRelease()
{
    if (!tempoButtonReleased)
        return;

    unsigned long now = millis();
    unsigned long duration = now - tempoButtonPressTime;
    if (duration >= 1000)
        return; // esto fue un hold, no un tap

    if (tempoTapPending)
    {
        unsigned long interval = now - tempoFirstTapTime;
        // Solo es un "segundo tap" válido si cae en el rango de tempo soportado.
        // Si fue demasiado lento o demasiado rápido, no tocamos stepMs:
        // este tap pasa a ser el nuevo "primer tap" de una posible nueva secuencia.
        if (interval >= TAP_MIN_INTERVAL_MS && interval <= TAP_MAX_INTERVAL_MS)
        {
            stepMs = (uint16_t)(interval / 4u);
        }
    }

    tempoTapPending = true;
    tempoFirstTapTime = now;
    tempoTapTimer.start(TAP_MAX_INTERVAL_MS); // la ventana ahora coincide con el tempo más lento soportado
}

void handlePendingTempoTap()
{
    if (tempoTapPending && tempoTapTimer.ready())
    {
        tempoTapPending = false;
    }
}

void handleNotePresses()
{
    unsigned long now = millis();
    for (int i = 0; i < NUM_NOTES; i++)
    {
        if (!buttonPressed[i])
        {
            continue;
        }

        playNoteButton(i, false, now);
    }
}

void handleButtonHolds()
{
    for (int i = 0; i < NUM_NOTES; i++)
    {
        if (buttonHeld[i] && !buttonHoldHandled[i])
        {
            handleButtonHold(i);
        }
    }
}

void handleRolls()
{
    for (int i = 0; i < NUM_NOTES; i++)
    {
        if (!rolling[i])
        {
            continue;
        }
        if (rollDelay.ready())
        {
            if (mode == DRUM_MODE)
            {
                playDrumButton(i);
            }
            else
            {
                // No reproducir roll en synth/pad mode; octave hold se maneja con handleButtonHold
            }
            rollDelay.start(stepMs);
        }
    }
}

void triggerKick()
{
    kickOsc.setPhase(0);
    kickOsc.setFreq(45);
    kickEnv.noteOn();
}

void triggerSnare()
{
    snareToneOsc.setPhase(0);
    snareEnv.noteOn();
    snareToneEnv.noteOn();
    snareToneOsc.setFreq(220.0f);
}

void triggerHat()
{
    hatEnv.noteOn();
    crashEnv.noteOff();
}

void triggerCrash()
{
    // 1. Oscilador principal: onda cuadrada en frecuencia muy aguda
    crashOsc.setPhase(0);
    crashOsc.setFreq(8000.0f); // Frecuencia muy aguda para timbre metálico

    // 2. Ruido para modulación FM del pitch y mezcla
    crashNoiseOsc.setPhase(0);
    crashNoiseOsc.setFreq(3.0f); // Frecuencia del ruido para modulación

    // 3. Iniciar envolvente
    crashEnv.noteOn();
}

void triggerSynth(uint8_t noteIndex, bool octaveUp = false)
{
    uint8_t midiNote = synthNotes[noteIndex];
    if (octaveUp)
    {
        midiNote += 12;
    }
    bassBaseNote = midiNote;
    // bassOsc.setFreq((float)bassBaseNote);
    // if (bassOsc2Active)
    // {
    //   float detunedFreq = bassBaseNote * pow(2.0f, bassDetune / 12.0f);
    //   bassOsc2.setFreq(detunedFreq);
    // }
    // bassOsc.setPhase(0);
    // bassOsc2.setPhase(0);
    bassEnv.noteOn();
    bassActive = true;
}

void triggerPad(uint8_t noteIndex, bool octaveUp = false)
{
    // PAD: play a 4-note chord block (House-style) two octaves above bass
    // noteIndex here is index into synthNotes[] (0..5)
    uint8_t rootMidi = synthNotes[noteIndex];
    int baseShift = 24; // two octaves above bass
    if (octaveUp)
        baseShift += 12;

    // chordType determined by current randomizer (do not record chord type)
    // Build and start four oscillators for the chord
    int8_t i0 = chordIntervals[padChordType][0];

    int n0 = rootMidi + baseShift + i0; // creo que i0 es 0 siempre

    // prueba detune
    //  float detunedFreq = pBaseFreq * pow(2.0f, bassDetune / 12.0f);
    //  bassOsc2.setFreq(detunedFreq);
    padBaseNote = n0;

    // Serial.print("PAD chord: ");
    // Serial.print(n0);
    // Serial.print(", ");
    // Serial.print(n1);
    // Serial.print(", ");
    // Serial.print(n2);
    // Serial.print(", ");
    // Serial.print(n3);
    // Serial.println(", ");

    padEnv.noteOn();
    padActive = true;
}

void silenceBass()
{
    bassActive = false;
    bassBaseNote = 0.0f;
    bassOsc.setFreq(0.0f);
    bassOsc2.setFreq(0.0f);
}

void advanceStep(uint8_t currentStep)
{
    // Serial.print(currentStep);
    // Serial.print(": ");
    if (kickSeq[currentStep])
    {
        triggerKick();
        // Serial.print("KICK");
    }
    if (snareSeq[currentStep])
        triggerSnare();
    if (hatSeq[currentStep] == 1)
        triggerHat();
    else if (hatSeq[currentStep] == 2)
        triggerCrash();

    // Grabación de roll en drums: agregar golpe en el paso actual si está en recording roll
    if (recordingRoll[0])
        kickSeq[currentStep] = 1;
    if (recordingRoll[1])
        snareSeq[currentStep] = 1;
    if (recordingRoll[2])
        hatSeq[currentStep] = 1;

    // Serial.println();
    // Play both melodic sequences (bass and pad) regardless of selected track.
    if (bassSeq[currentStep])
    {
        uint8_t seqValue = bassSeq[currentStep];
        bool octaveUp = (seqValue >= 7);
        uint8_t noteIndex = octaveUp ? (seqValue - 7) : (seqValue - 1);
        triggerSynth(noteIndex, octaveUp);
    }
    if (padSeq[currentStep])
    {
        uint8_t seqValue = padSeq[currentStep];
        uint8_t noteIndex = 0;
        bool octaveUp = false;
        // Support legacy values (1..6 => index+1, 7..12 => index+7)
        if (seqValue > 0 && seqValue <= 12)
        {
            if (seqValue >= 7)
            {
                octaveUp = true;
                noteIndex = seqValue - 7;
            }
            else
            {
                octaveUp = false;
                noteIndex = seqValue - 1;
            }
        }
        else
        {
            // packed format: subtract 1 offset, then unpack
            seqValue = seqValue - 1;
            noteIndex = PADSEQ_NOTEINDEX(seqValue);
            octaveUp = (PADSEQ_OCTAVEFLAG(seqValue) != 0);
        }
        triggerPad(noteIndex, octaveUp);
    }
}

void updateEnvelopesAndModulation()
{
    kickEnv.update();
    snareEnv.update();
    hatEnv.update();
    snareToneEnv.update();
    bassEnv.update();
    padEnv.update();
    crashEnv.update();

    // filtro bajo
    bassFilterCutoffBase = constrain(map(pot1, 0, 1024, 80, 2000), 80, 2000);

    int bassFilterResonanceMin = 20;
    bassFilterResonance = (uint8_t)constrain(map(pot2, 0, 1024, 200, bassFilterResonanceMin), bassFilterResonanceMin, 200);
    u_int16_t bassFilterDecay = 220; // constrain(map(pot2, 0, 1024, 20, 800), 20, 800);
    // Serial.println(bassFilterResonance);
    bassLPF.setResonance(bassFilterResonance);

    bassEnvLevel = bassEnv.next();
    uint16_t envCutoff = constrain(bassFilterCutoffBase + (uint16_t)(bassEnvLevel * 2), 80, 2200);
    bassLPF.setCentreFreq(envCutoff);

    bassEnv.setADLevels(255, 20);
    bassEnv.setTimes(5, bassFilterDecay, 1, 100);

    // filtro pad
    padLPF.setResonance(100);
    padLPF.setCentreFreq(envCutoff);
    // Update PAD ADSR parameters only when pot changes to avoid reinitializing the envelope
    static uint16_t lastPot2 = 0;

    if (pot2 != lastPot2) // no sirve de mucho pq varia siempre la lectura del pote
    {
        const u_int16_t padFilterDecayMax = 1600;
        u_int16_t padFilterDecay = (uint16_t)(((uint32_t)pot2 * pot2) / padFilterDecayMax);
        padFilterDecay = constrain(map(pot2, 0, 1024, 1, padFilterDecayMax), 1, padFilterDecayMax); // antilog
        // padFilterDecay = (padFilterDecay * padFilterDecay) >> 10; // curve to make it more sensitive at low values

        // Set attack peak to full, sustain level small so decay isn't abrupt.
        // Use same padFilterDecay for attack, decay and release so times match.
        padEnv.setADLevels(255, 10);
        // attack_ms, decay_ms, sustain_ms, release_ms
        padEnv.setTimes(1, padFilterDecay, 10, 100);
        lastPot2 = pot2;
    }
    // pitch bajo
    if (bassBaseNote > 0)
    {
        bassPitchMod = bassPitchLFO.next() * bassPitchLFOAmplitude / 4000.0f;
        bassOsc.setFreq(mtof(bassBaseNote + bassPitchMod));
        if (bassOsc2Active)
        {
            float detunedFreq = (mtof(bassBaseNote + bassPitchMod + bassDetune));
            bassOsc2.setFreq(detunedFreq);
        }
    }
    // pitch pad
    if (padBaseNote > 0)
    {
        padPitchMod = padPitchLFO.next() * padPitchLFOAmplitude / 4000.0f;
        int8_t i0 = chordIntervals[padChordType][0];
        int8_t i1 = chordIntervals[padChordType][1];
        int8_t i2 = chordIntervals[padChordType][2];
        int8_t i3 = chordIntervals[padChordType][3];
        float n0 = mtof(padBaseNote + i0 + padPitchMod);
        float n1 = mtof(padBaseNote + i1 + padDetune2 + padPitchMod);
        float n2 = mtof(padBaseNote + i2 + padDetune3 + padPitchMod);
        float n3 = mtof(padBaseNote + i3 - padDetune2 + padPitchMod);
        padOsc1.setFreq(n0);
        padOsc2.setFreq(n1);
        padOsc3.setFreq(n2);
        padOsc4.setFreq(n3);
    }
    // Serial.println(padPitchMod);
}

void setupEnvelopes()
{
    kickEnv.setADLevels(255, 0);
    kickEnv.setTimes(2, 120, 0, 0);

    snareEnv.setADLevels(200, 0);
    snareEnv.setTimes(1, 80, 0, 0);

    snareToneEnv.setADLevels(180, 0);
    snareToneEnv.setTimes(1, 80, 0, 0);

    hatEnv.setADLevels(160, 0);
    hatEnv.setTimes(1, 35, 0, 0);

    crashEnv.setADLevels(255, 0);
    crashEnv.setTimes(0, 800, 0, 0);

    bassEnv.setADLevels(255, 40);
    bassEnv.setTimes(20, 200, 20, 40);

    // PAD envelope: use similar shape but with decay ~400ms as requested
    padEnv.setADLevels(255, 200);
    padEnv.setTimes(8, 200, 180, 120);
}