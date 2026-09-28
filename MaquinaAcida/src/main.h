#pragma once




//-------notas musicales
// Pad chord encoding helpers:
#define PADSEQ_PACK(noteIndex, octaveFlag, chordType) ((uint8_t)(((chordType & 0x03) << 6) | ((octaveFlag & 0x01) << 5) | (noteIndex & 0x1F)))
#define PADSEQ_CHORDTYPE(v) (((v) >> 6) & 0x03)
#define PADSEQ_OCTAVEFLAG(v) (((v) >> 5) & 0x01)
#define PADSEQ_NOTEINDEX(v) ((v) & 0x1F)

 
void setTrackLed(uint8_t trackIndex);