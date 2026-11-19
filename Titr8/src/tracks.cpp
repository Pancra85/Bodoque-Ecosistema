#include "tracks.h"
#include "voices.h"

TrackId currentTrack = TRACK_DRUM;

// DRUM: 1 = golpe. En hatSeq, 2 = crash.
static uint8_t kickSeq[MAX_STEPS] = {0};
static uint8_t snareSeq[MAX_STEPS] = {0};
static uint8_t hatSeq[MAX_STEPS] = {0};

// BASS y PAD: 0 = nada, si no encodeNote(grado, octava). Se graba el grado
// absoluto (0..11), asi transponer no cambia lo que ya esta grabado.
static uint8_t bassSeq[MAX_STEPS] = {0};
static uint8_t padSeq[MAX_STEPS] = {0};
static uint8_t leadSeq[MAX_STEPS] = {0};

// 1..12 = grado + 1, 13..24 = grado + 13 (octava arriba)
static uint8_t encodeNote(uint8_t degree, bool octaveUp)
{
  return 1 + degree + (octaveUp ? NUM_SCALE_DEGREES : 0);
}

static void decodeNote(uint8_t value, uint8_t &degree, bool &octaveUp)
{
  value -= 1;
  octaveUp = (value >= NUM_SCALE_DEGREES);
  degree = octaveUp ? value - NUM_SCALE_DEGREES : value;
}

void tracksClear(TrackId track)
{
  if (track == TRACK_DRUM)
  {
    memset(kickSeq, 0, sizeof(kickSeq));
    memset(snareSeq, 0, sizeof(snareSeq));
    memset(hatSeq, 0, sizeof(hatSeq));
  }
  else if (track == TRACK_BASS)
  {
    memset(bassSeq, 0, sizeof(bassSeq));
    silenceBass();
  }
  else if (track == TRACK_PAD)
  {
    memset(padSeq, 0, sizeof(padSeq));
    silencePad();
  }
  else if (track == TRACK_LEAD)
  {
    memset(leadSeq, 0, sizeof(leadSeq));
    silenceLead();
  }
}

bool tracksMelodicEmpty()
{
  for (int i = 0; i < MAX_STEPS; i++)
  {
    if (bassSeq[i] != 0 || padSeq[i] != 0 || leadSeq[i] != 0)
      return false;
  }
  return true;
}

void tracksTriggerNote(TrackId track, uint8_t degree, bool octaveUp)
{
  if (track == TRACK_BASS)
    triggerBass(degree, octaveUp);
  else if (track == TRACK_PAD)
    triggerPad(degree, octaveUp);
  else if (track == TRACK_LEAD)
    triggerLead(degree, octaveUp);
}

void tracksRecordDrum(uint8_t drum, uint8_t step)
{
  if (drum == DRUM_KICK)
    kickSeq[step] = 1;
  else if (drum == DRUM_SNARE)
    snareSeq[step] = 1;
  else if (drum == DRUM_HAT)
    hatSeq[step] = 1;
  else if (drum == DRUM_CRASH)
    hatSeq[step] = 2;
}

void tracksRecordNote(TrackId track, uint8_t step, uint8_t degree, bool octaveUp)
{
  if (track == TRACK_BASS)
    bassSeq[step] = encodeNote(degree, octaveUp);
  else if (track == TRACK_PAD)
    padSeq[step] = encodeNote(degree, octaveUp);
  else if (track == TRACK_LEAD)
    leadSeq[step] = encodeNote(degree, octaveUp);
}

void tracksPlayDrums(uint8_t step)
{
  if (kickSeq[step])
    triggerDrum(DRUM_KICK);
  if (snareSeq[step])
    triggerDrum(DRUM_SNARE);
  if (hatSeq[step] == 1)
    triggerDrum(DRUM_HAT);
  else if (hatSeq[step] == 2)
    triggerDrum(DRUM_CRASH);
}

void tracksPlayMelodic(uint8_t step)
{
  // BASS, PAD y LEAD suenan siempre, sin importar que track este seleccionado.
  uint8_t degree;
  bool octaveUp;

  if (bassSeq[step])
  {
    decodeNote(bassSeq[step], degree, octaveUp);
    triggerBass(degree, octaveUp);
  }

  if (padSeq[step])
  {
    decodeNote(padSeq[step], degree, octaveUp);
    triggerPad(degree, octaveUp);
  }

  if (leadSeq[step])
  {
    decodeNote(leadSeq[step], degree, octaveUp);
    triggerLead(degree, octaveUp);
  }
}
