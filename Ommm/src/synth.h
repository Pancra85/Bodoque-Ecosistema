#pragma once
#include "sequence.h"

// --- Límites de compilación (presupuesto de osciladores) ---
#define NUM_LAYERS 2
#define MAX_OSCS_PER_LAYER_NOFX 3 // máximo de osciladores audibles por capa
#define MAX_LFOS_PER_LAYER_NOFX 3  // máximo de LFOs de pitch por capa
#define MAX_OSCS_PER_LAYER_WITHFX 2 // Con FX desabilitados: máximo de osciladores audibles por capa
#define MAX_LFOS_PER_LAYER_WITHFX 3  //Con FX desabilitados: máximo de LFOs de pitch por capa
#define COMPLEXITY_MAX 1023

// Rango de del oscilador
#define MIDI_NOTE_MIN 0.0f  // C1, ~33 Hz
#define MIDI_NOTE_MAX 84.0f  

#define FILTER_FREQ_MIN 1
#define FILTER_FREQ_MAX 4000

#define LFO_FREQ_MIN 0
#define LFO_FREQ_MAX 10



enum LfoTarget
{
    LFO_TARGET_PITCH,
    LFO_TARGET_FILTER,
};

struct Oscillator
{
    float frequency;     // Hz (para audibles) o velocidad (para LFOs)
    float detune;        // proporción, ej: -0.01 .. +0.01
    int waveform;        // SINE, SAW_DOWN, etc.
    float modDepth;      // cuánto te modula el LFO asignado (0 = sin modulación)
    LfoTarget lfoTarget; // a qué parámetro afecta su LFO asignado
    bool active;         // si está sonando o silenciado
};

// mod_target bits (de AMY)- Indica lo que modifica el LFO
#define MOD_TARGET_FREQ 4
#define MOD_TARGET_FILTER 8

#define NUM_HARMONY_MODE 4
enum HarmonyMode
{
    HARMONY_NONE,
    HARMONY_INTERVAL,
    HARMONY_CHORD,
    HARMONY_SEQUENCE
};

struct Layer
{
    int layerNumb; // número de capa (0..NUM_LAYERS-1)
    // --- "Complejidad" actual (cuántos slots están en uso) ---
    int numOscOnLayer;
    int numLFOsOnLayer;

    // --- Slots físicos (tamaño fijo = máximo posible) ---
    Oscillator oscs[MAX_OSCS_PER_LAYER_NOFX];
    Oscillator lfos[MAX_LFOS_PER_LAYER_NOFX];

    // qué LFO (índice LOCAL, 0..numLFOsOnLayer-1) modula a cada oscilador.
    //  -1 = ese oscilador no tiene modulación de pitch.
    int lfoAssignment[MAX_OSCS_PER_LAYER_NOFX][2]; // [i][0]=slot pitch, [i][1]=slot filtro
    // --- Índices globales asignados en AMY ---
    int oscBase; // índice del primer oscilador audible de esta capa (siempre 0?)
    int lfoBase; // índice del primer LFO de esta capa

    // --- Parámetros compartidos por toda la capa ---
    float baseNote; // raíz de la capa (la mueve un pote)
    int waveform;   // forma de onda de todos los osciladores de la capa
    int filterType;
    float filterFreq;
    float resonance;
    int complexity;
    float pan;    // L:0 a R:1.0
    float volume; // 0.0 a 10.0, para subir el volumen de la capa sin tocar los osciladores individuales

    bool layerNeedsRebuild; // --- Flag para saber si hay que re-armar la capa ---
    bool active;       // si la capa está activa o silenciada

    HarmonyMode harmonyMode;
    int harmonyParam; // índice dentro de INTERVAL_SEMITONES/CHORDS/SCALES, según el modo

    int millisPerStep; // duración de cada paso
    int lastStepMillis;
    int currentStep;

    bool harmonyNeedsRebuild;                // flag para saber si hay que re-armar la armonía (cuando cambian los parámetros)
    float harmonyIntervalSemitones;          // 100% libre, controlado a mano por un pote
    float noteOffsets[MAX_OSCS_PER_LAYER_NOFX];   // semitonos por oscilador (interval/chord)
    float sequenceNotes[MAX_SEQUENCE_STEPS]; // ahora float, para microtonal // semitonos por paso (sequence)
    uint16_t sequenceBaseTag;                // tags reservados para esta capa
    bool sequenceSounding;                   // para saber si hay que cortar o no

    uint32_t seed;                          // semilla para generar la secuencia
    int sequenceLength;                     // pasos activos (2..MAX_SEQUENCE_STEPS)
    int density;                            // qué tan seguido hay nota nueva
    uint8_t stepActive[MAX_SEQUENCE_STEPS]; // 1 = nota nueva ese step, 0 = sostiene la anterior
};
extern Layer layers[NUM_LAYERS];
extern bool effectsEnabledConfig;

struct AudioEffects
{
    float chorusLevel;
    float chorusDepth;
    float chorusLfoFreq;
    float chorusMaxDelay;

    float reverbLevel;
    float reverbLiveness;
    float reverbDamping;
    float reverbXoverHz;

    bool distType;   // fold o clip
    float distDrive; // 1.0 = sin drive extra, >1 = más saturación
    float distMix;   // 0 = dry, 1 = wet total

    float distCrush;
    float distBits; // ej: 4-8 bits (16 = "sin crush" de profundidad)
    float distRate; // reducción de sample rate
};
extern AudioEffects audioEffects;

void applyLayerParams(Layer &l);
void audioInit();
void updateAudioOutput();
void assignOscIndex();
void processLayerRebuilds(Layer &l);
void rebuildLayer(Layer &l);
void setLayerComplexity(Layer &l, int complexity);
void applyEffects();
void setDefaultParameters();
void assignHarmonyNotes(Layer &l);
float freqFromSemitones(float rootFreq, float semitones);
float midiNoteToFreq(float note);