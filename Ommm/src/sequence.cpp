#include <Arduino.h>
#include <AMY-Arduino.h>

#include "sequence.h"
#include "synth.h"
#include "controls.h"

const ChordDef CHORDS[NUM_CHORD_TYPES] = {
    {"Mayor", {0, 4, 7, 0}, 3},
    {"Menor", {0, 3, 7, 0}, 3},
    {"Disminuido", {0, 3, 6, 0}, 3},
    {"Sus4", {0, 5, 7, 0}, 3},
};

const ScaleDef SCALES[] = {
    {"Mayor", {0, 2, 4, 5, 7, 9, 11, 0}, 7},
    {"MenorNat", {0, 2, 3, 5, 7, 8, 10, 0}, 7},
    {"Dorico", {0, 2, 3, 5, 7, 9, 10, 0}, 7},
    // --- Oscuras / raras (temperamento normal) ---
    {"Locria", {0, 1, 3, 5, 6, 8, 10, 0}, 7},      // la más "inestable" de las modales clásicas
    {"HungaraMen", {0, 2, 3, 6, 7, 8, 11, 0}, 7},  // menor húngara, sonido "gitano"
    {"Enigmatica", {0, 1, 4, 6, 8, 10, 11, 0}, 7}, // de las escalas más raras que existen
    // --- Árabes / microtonales (cuartos de tono, semitonos .5) ---
    {"Hijaz", {0, 1, 4, 5, 7, 8, 10, 0}, 7},      // árabe, temperamento normal
    {"Bayati", {0, 1.5, 3, 5, 7, 8.5, 10, 0}, 7}, // maqam con 2da y 6ta "neutras"
    {"Rast", {0, 2, 3.5, 5, 7, 9, 10.5, 0}, 7},
    {"Saba", {0, 1.5, 3, 4, 6, 8, 10, 0}, 7}, // muy oscura, característica del maqam Saba
};

uint32_t nextRandom(uint32_t &state)
{
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
}

void generateSequence(Layer &l)
{
    const ScaleDef &scale = SCALES[l.harmonyParam % NUM_SCALE_TYPES];
    int lastStep=MAX_SEQUENCE_STEPS;

    uint32_t baseSeedMix = l.seed ^ ((uint32_t)l.harmonyParam *l.density * 2654435761u);
    uint32_t rngNotes = baseSeedMix ^ 0x1u;
    uint32_t rngGate = baseSeedMix ^ 0x9E3779B1u;
    uint32_t rngSecondary = baseSeedMix ^ 0x85EBCA6Bu;
    if (rngNotes == 0)
        rngNotes = 1;
    if (rngGate == 0)
        rngGate = 1;
    if (rngSecondary == 0)
        rngSecondary = 1;

    // --- 1) MATRIZ DE DENSIDAD por bloques (rítmica, no pareja) ---
    int repeatStepChoice = nextRandom(rngGate) % 3;
    int repeatStep = (repeatStepChoice == 0) ? 4 : (repeatStepChoice == 1) ? 8
                                                                           : 16;
    if (repeatStep > lastStep)
        repeatStep = lastStep;

    uint16_t blockThreshold[16];
    for (int b = 0; b < repeatStep; b++)
        blockThreshold[b] = nextRandom(rngGate) % (DENSITY_MAX + 1);

    uint16_t stepOverride[MAX_SEQUENCE_STEPS];
    for (int i = 0; i < lastStep; i++)
        stepOverride[i] = 0xFFFF; // sin excepción

    int numOverrides = nextRandom(rngGate) % (lastStep / 2 + 1);
    for (int k = 0; k < numOverrides; k++)
    {
        int pos = nextRandom(rngGate) % lastStep;
        stepOverride[pos] = nextRandom(rngGate) % (DENSITY_MAX + 1);
    }

    for (int i = 0; i < lastStep; i++)
    {
        uint16_t threshold = (stepOverride[i] != 0xFFFF) ? stepOverride[i] : blockThreshold[i % repeatStep];
        l.stepActive[i] = (threshold < l.density) ? 1 : 0;
    }

    // --- 2) MELODÍA PRINCIPAL: 70% de repetir nota ya usada, salteando silencios ---
    float usedNotes[MAX_SEQUENCE_STEPS];
    int notesUsed = 0;
    float lastActiveNote = 0;

    for (int i = 0; i < lastStep; i++)
    {
        if (!l.stepActive[i])
        {
            l.sequenceNotes[i] = lastActiveNote; // valor de referencia; no se va a escuchar (rest corta)
            continue;
        }

        uint32_t r1 = nextRandom(rngNotes);
        bool repeatNote = (notesUsed > 1) && ((r1 % 100) < 70);

        float note;
        if (repeatNote)
        {
            note = usedNotes[nextRandom(rngNotes) % notesUsed];
        }
        else
        {
            uint32_t r2 = nextRandom(rngNotes);
            int degreeIndex = r2 % scale.numNotes;
            int octaveShift = (r2 / scale.numNotes) % 2;
            note = scale.notes[degreeIndex] + octaveShift * 12;
        }

        l.sequenceNotes[i] = note;
        // Serial.println(note);
        lastActiveNote = note;
        if (notesUsed < MAX_SEQUENCE_STEPS)
            usedNotes[notesUsed++] = note;
    }

    // --- 3) MELODÍA SECUNDARIA: a veces se mezcla encima de la principal ---
    bool secondaryEnabled = (nextRandom(rngSecondary) % 100) < 50; // 50% de las regeneraciones
    if (secondaryEnabled)
    {
        int divisor = 1 + (nextRandom(rngSecondary) % 8);
        int secLength = lastStep / divisor;
        if (secLength < 1)
            secLength = 1;
        if (secLength > MAX_SEQUENCE_STEPS)
            secLength = MAX_SEQUENCE_STEPS;

        float octaveShift2 = 12.0f * (nextRandom(rngSecondary) % 3); // 0, +1 u +2 octavas
        float secPool[MAX_SEQUENCE_STEPS];
        float secNotes[MAX_SEQUENCE_STEPS];
        int secUsed = 0;

        for (int s = 0; s < secLength; s++)
        {
            uint32_t r1 = nextRandom(rngSecondary);
            bool rep = (secUsed > 1) && ((r1 % 100) < 70);
            float n;
            if (rep)
                n = secPool[nextRandom(rngSecondary) % secUsed];
            else
            {
                uint32_t r2 = nextRandom(rngSecondary);
                n = scale.notes[r2 % scale.numNotes];
            }
            secNotes[s] = n + octaveShift2;
            if (secUsed < MAX_SEQUENCE_STEPS)
                secPool[secUsed++] = n;
        }

        int mixPercent = 30 + (nextRandom(rngSecondary) % 40); // 30-70% de las notas activas
        for (int i = 0; i < lastStep; i++)
        {
            if (!l.stepActive[i])
                continue; // no pisamos silencios
            uint32_t r = nextRandom(rngSecondary);
            if ((int)(r % 100) < mixPercent)
                l.sequenceNotes[i] = secNotes[i % secLength];
        }
    }

    if (l.currentStep >= lastStep)
        l.currentStep = 0;
}

void rebuildHarmonyStructure(Layer &l)
{
    if (l.harmonyMode == HARMONY_SEQUENCE)
    {
        generateSequence(l);
    }
    else
    {
        assignHarmonyNotes(l);
        applyLayerParams(l);
    }
}

void updateLayerRoot(Layer &l, float newNote)
{
    l.baseNote = newNote;

    if (l.harmonyMode == HARMONY_SEQUENCE)
    {
        if (!l.sequenceSounding)
            return;
        float rootFreq = midiNoteToFreq(l.baseNote);
        float noteFreq = freqFromSemitones(rootFreq, l.sequenceNotes[l.currentStep]);
        for (int i = 0; i < l.numOscOnLayer; i++)
        {
            amy_event e = amy_default_event();
            e.osc = l.oscBase + i;
            e.freq_coefs[COEF_CONST] = noteFreq * (1.0f + l.oscs[i].detune);
            amy_add_event(&e);
        }
    }
    else
    {
        applyLayerParams(l);
    }
}

void assignHarmonyNotes(Layer &l)
{
    if (l.harmonyMode == HARMONY_INTERVAL)
    {
        for (int i = 0; i < MAX_OSCS_PER_LAYER; i++)
            l.noteOffsets[i] = (i % 2 == 0) ? 0.0f : l.harmonyIntervalSemitones;
    }
    else if (l.harmonyMode == HARMONY_CHORD)
    {
        const ChordDef &chord = CHORDS[l.harmonyParam % NUM_CHORD_TYPES];
        for (int i = 0; i < MAX_OSCS_PER_LAYER; i++)
            l.noteOffsets[i] = (float)chord.notes[i % chord.numNotes];
    }
    else
    {
        for (int i = 0; i < MAX_OSCS_PER_LAYER; i++)
            l.noteOffsets[i] = 0.0f;
    }
}

void updateSequenceStep(Layer &l)
{
    if (l.harmonyMode != HARMONY_SEQUENCE || l.numOscOnLayer == 0)
        return;

    if (!l.active)
    {
        if (l.sequenceSounding)
        {
            for (int i = 0; i < l.numOscOnLayer; i++)
            {
                amy_event e = amy_default_event();
                e.osc = l.oscBase + i;
                e.velocity = 0;
                amy_add_event(&e);
            }
            l.sequenceSounding = false;
        }
        return;
    }

    if (millis() - l.lastStepMillis < l.millisPerStep)
        return;

    l.lastStepMillis = millis();
    l.currentStep = (l.currentStep + 1) % l.sequenceLength;

    if (!l.stepActive[l.currentStep])
    {
        if (l.sequenceSounding) // recién ahora que se apaga, cortamos
        {
            for (int i = 0; i < l.numOscOnLayer; i++)
            {
                amy_event e = amy_default_event();
                e.osc = l.oscBase + i;
                e.velocity = 0;
                amy_add_event(&e);
            }
            l.sequenceSounding = false;
        }
        return;
    }

    float rootFreq = midiNoteToFreq(l.baseNote);
    float noteFreq = freqFromSemitones(rootFreq, l.sequenceNotes[l.currentStep]);
    for (int i = 0; i < l.numOscOnLayer; i++)
    {
        amy_event e = amy_default_event();
        e.osc = l.oscBase + i;
        e.freq_coefs[COEF_CONST] = noteFreq * (1.0f + l.oscs[i].detune);
        e.velocity = 1;
        amy_add_event(&e);
    }
    l.sequenceSounding = true;
}