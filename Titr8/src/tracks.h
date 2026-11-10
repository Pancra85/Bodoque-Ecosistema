#pragma once
#include <Arduino.h>

// Patrones grabados de cada track.

#define MAX_STEPS 64

enum TrackId : uint8_t
{
  TRACK_DRUM = 0,
  TRACK_BASS,
  TRACK_PAD,
  TRACK_LEAD, // FM pluck / campana (LED X)
  TRACK_COUNT
};

enum DrumId : uint8_t
{
  DRUM_KICK = 0,
  DRUM_SNARE,
  DRUM_HAT,
  DRUM_CRASH
};

extern TrackId currentTrack;

void tracksClear(TrackId track); // borra el patron (y silencia la voz si es melodica)
bool tracksMelodicEmpty();       // true si BASS, PAD y LEAD no tienen ninguna nota
void tracksTriggerNote(TrackId track, uint8_t degree, bool octaveUp); // toca la voz melodica del track

void tracksRecordDrum(uint8_t drum, uint8_t step);
void tracksRecordNote(TrackId track, uint8_t step, uint8_t degree, bool octaveUp); // degree: 0..11

void tracksPlayDrums(uint8_t step);
void tracksPlayMelodic(uint8_t step);
