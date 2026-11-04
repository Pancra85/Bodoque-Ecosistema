#pragma once

#define MAX_CHORD_NOTES 4
#define MAX_SCALE_NOTES 8
#define MAX_SEQUENCE_STEPS 128
#define DENSITY_MAX 1023

struct Layer;

struct ChordDef
{
    const char *name;
    int8_t notes[MAX_CHORD_NOTES];
    int8_t numNotes;
};

struct ScaleDef
{
    const char *name;
    float notes[MAX_SCALE_NOTES];
    int8_t numNotes;
};


#define NUM_CHORD_TYPES 4
extern const ChordDef CHORDS[NUM_CHORD_TYPES];

#define NUM_SCALE_TYPES 10
extern const ScaleDef SCALES[NUM_SCALE_TYPES];

void rebuildHarmonyStructure(Layer &l);
void generateSequence(Layer &l);
void updateSequenceStep(Layer &l);