#pragma once

#include <Arduino.h>
#include <MozziHeadersOnly.h>

#include <EventDelay.h>
#include <Oscil.h>
#include <ADSR.h>
#include <tables/sin2048_int8.h>
#include <tables/square_no_alias_2048_int8.h>
#include <tables/saw2048_int8.h>
#include <tables/whitenoise8192_int8.h>
#include <StateVariable.h>
#include <AudioDelayFeedback.h>

// Osciladores de audio
extern Oscil<SIN2048_NUM_CELLS, MOZZI_AUDIO_RATE> kickOsc;
extern Oscil<SQUARE_NO_ALIAS_2048_NUM_CELLS, MOZZI_AUDIO_RATE> bassOsc;
extern Oscil<SQUARE_NO_ALIAS_2048_NUM_CELLS, MOZZI_AUDIO_RATE> bassOsc2;

// Osciladores PAD
extern Oscil<SAW2048_NUM_CELLS, MOZZI_AUDIO_RATE> padOsc1;
extern Oscil<SAW2048_NUM_CELLS, MOZZI_AUDIO_RATE> padOsc2;
extern Oscil<SAW2048_NUM_CELLS, MOZZI_AUDIO_RATE> padOsc3;
extern Oscil<SAW2048_NUM_CELLS, MOZZI_AUDIO_RATE> padOsc4;

// Oscilador para el tono del snare
extern Oscil<SIN2048_NUM_CELLS, MOZZI_AUDIO_RATE> snareToneOsc;

// Osciladores de ruido blanco para snare y hi-hat
extern Oscil<WHITENOISE8192_NUM_CELLS, MOZZI_AUDIO_RATE> snareNoiseOsc;
extern Oscil<WHITENOISE8192_NUM_CELLS, MOZZI_AUDIO_RATE> hatNoiseOsc;

// Osciladores para crash cymbal
extern Oscil<SQUARE_NO_ALIAS_2048_NUM_CELLS, MOZZI_AUDIO_RATE> crashOsc;
extern Oscil<WHITENOISE8192_NUM_CELLS, MOZZI_AUDIO_RATE> crashNoiseOsc;

// Filtros pasa bajos
extern StateVariable<LOWPASS> bassLPF;
extern StateVariable<LOWPASS> padLPF;

// LFOs para BASS
extern Oscil<SIN2048_NUM_CELLS, CONTROL_RATE> bassPitchLFO;

// LFOs para PAD
extern Oscil<SIN2048_NUM_CELLS, CONTROL_RATE> padPitchLFO;

// Filtro EQ para el PAD y snare
extern StateVariable<HIGHPASS> eqPad;
extern StateVariable<HIGHPASS> eqSnare;

// Envolventes ADSR
extern ADSR<CONTROL_RATE, MOZZI_AUDIO_RATE> kickEnv;
extern ADSR<CONTROL_RATE, MOZZI_AUDIO_RATE> snareEnv;
extern ADSR<CONTROL_RATE, MOZZI_AUDIO_RATE> hatEnv;

// Envolvente para el tono del snare
extern  ADSR<CONTROL_RATE, MOZZI_AUDIO_RATE> snareToneEnv;

// Envolvente de volumen del bajo
extern ADSR<CONTROL_RATE, MOZZI_AUDIO_RATE> bassEnv;

// Envolvente del PAD
extern ADSR<CONTROL_RATE, MOZZI_AUDIO_RATE> padEnv;
extern uint8_t padEnvLevel;

// Envolvente del Crash
extern ADSR<CONTROL_RATE, MOZZI_AUDIO_RATE> crashEnv;
extern uint8_t crashEnvLevel;

// Estado para el ruido del hi-hat
extern int16_t lastHatNoise;

// Nota base del bajo
extern bool bassActive;
extern bool bassOsc2Active;

// Nota base del PAD
extern bool padActive;

//delay
extern const uint16_t PAD_BASS_DELAY_MS; //no se usa mas, se calculan las cells directamente
extern const int8_t PAD_BASS_DELAY_FEEDBACK;
#define PAD_BASS_DELAY_CELLS 16384 //(uint32_t)PAD_BASS_DELAY_MS * (uint32_t)MOZZI_AUDIO_RATE / 1000U;
//   float(((unsigned long)MOZZI_AUDIO_RATE * PAD_BASS_DELAY_MS) / 1000);
extern bool padBassDelayEnabled;
extern AudioDelayFeedback<PAD_BASS_DELAY_CELLS> padBassDelay;

extern EventDelay randomizeClock;

// Variables para el secuenciador
#define MAX_STEPS 64
extern uint8_t steps;
extern int16_t stepIndex; // current sequencer step
extern uint8_t kickSeq[MAX_STEPS];
extern uint8_t snareSeq[MAX_STEPS];
extern uint8_t hatSeq[MAX_STEPS];

// Separate sequences for bass (synth) and pad so playback can be per-track
extern uint8_t bassSeq[MAX_STEPS]; // 0=none, 1..6=base/alt notes, 7..12=octave up
extern uint8_t padSeq[MAX_STEPS];

// Temporizadores
extern EventDelay stepClock;
extern EventDelay countInClock;
extern EventDelay rollDelay;
extern EventDelay modeLedTimer;
extern EventDelay tempoTapTimer;
extern uint16_t stepMs;
extern unsigned long sequencerStepStartMillis;

//Modos y estados
enum TrackMode
{
  DRUM_MODE = 0,
  SYNTH_MODE = 1,
  PAD_MODE = 2
};
extern TrackMode mode;
extern bool freePlay;

// Chord interval sets (semitones) for 4 buttons
#define NUM_CHORDS 8

extern const int8_t chordIntervals[NUM_CHORDS][4];

// Notas para synth: 3 notas base + 3 notas alternativas
extern uint8_t synthNotes[6];

// Definir 12 escalas diferentes (intervalos en semitonos desde la raíz) - Oscuras/Árabes
extern const uint8_t scales[12][6];

//sonido
extern uint8_t bassEnvLevel; 
extern uint8_t padEnvLevel;

// Variables de la envolvente de filtro del bass
extern uint16_t bassFilterCutoffBase;
extern uint8_t bassFilterResonance;

// Variables de la envolvente de filtro del PAD
extern uint16_t padFilterCutoffBase;
extern uint8_t padFilterResonance;

void clearDrumTracks();
void sequencerStepStarted();
uint32_t beatDurationMs();
uint8_t getSequencerNearestStep(unsigned long eventTime);
void handleButtonHolds();
void handleTrackButtonHold();
void handleTrackButtonRelease();
void handleTempoButtonRelease();
void handlePendingTempoTap();
void handleNotePresses();
void handleRolls();
void handleSequencer();
void updateOutputGain(uint16_t potValue);
void  updatePadBassDelay();
void randomizeConfigValues();
void updateEnvelopesAndModulation();
void advanceStep(uint8_t currentStep);
void silenceBass();
void triggerPad(uint8_t noteIndex, bool octaveUp);
void triggerSynth(uint8_t noteIndex, bool octaveUp);
void triggerKick();
void triggerSnare();
void triggerHat();
void triggerCrash();
void triggerSynth(uint8_t noteIndex, bool octaveUp);
void triggerPad(uint8_t noteIndex, bool octaveUp);
int16_t applyOutputGain(int16_t sample);
bool anyInstrumentButtonActive();
void stopRandomization();
void setupEnvelopes();
void initDelay();