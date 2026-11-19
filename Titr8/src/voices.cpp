#include "mozziConfig.h"
#include <MozziHeadersOnly.h>
#include <Oscil.h>
#include <ADSR.h>
#include <StateVariable.h>
#include <AudioDelayFeedback.h>
#include <mozzi_midi.h>
#include <tables/sin2048_int8.h>
#include <tables/square_no_alias_2048_int8.h>
#include <tables/saw2048_int8.h>
#include <tables/whitenoise8192_int8.h>

#include "voices.h"
#include "tracks.h"

// ======================= Osciladores, envolventes y filtros =======================

// Drums
static Oscil<SIN2048_NUM_CELLS, MOZZI_AUDIO_RATE> kickOsc(SIN2048_DATA);
static Oscil<SIN2048_NUM_CELLS, MOZZI_AUDIO_RATE> snareToneOsc(SIN2048_DATA);
static Oscil<WHITENOISE8192_NUM_CELLS, MOZZI_AUDIO_RATE> snareNoiseOsc(WHITENOISE8192_DATA);
static Oscil<WHITENOISE8192_NUM_CELLS, MOZZI_AUDIO_RATE> hatNoiseOsc(WHITENOISE8192_DATA);
static Oscil<SQUARE_NO_ALIAS_2048_NUM_CELLS, MOZZI_AUDIO_RATE> crashOsc(SQUARE_NO_ALIAS_2048_DATA);
static Oscil<WHITENOISE8192_NUM_CELLS, MOZZI_AUDIO_RATE> crashNoiseOsc(WHITENOISE8192_DATA);

static ADSR<CONTROL_RATE, MOZZI_AUDIO_RATE> kickEnv;
static ADSR<CONTROL_RATE, MOZZI_AUDIO_RATE> snareEnv;
static ADSR<CONTROL_RATE, MOZZI_AUDIO_RATE> snareToneEnv;
static ADSR<CONTROL_RATE, MOZZI_AUDIO_RATE> hatEnv;
static ADSR<CONTROL_RATE, MOZZI_AUDIO_RATE> crashEnv;
static StateVariable<HIGHPASS> eqSnare;
static int16_t lastHatNoise = 0;
static uint8_t crashEnvLevel = 0;

// Bass
static Oscil<SQUARE_NO_ALIAS_2048_NUM_CELLS, MOZZI_AUDIO_RATE> bassOsc(SQUARE_NO_ALIAS_2048_DATA);
static Oscil<SQUARE_NO_ALIAS_2048_NUM_CELLS, MOZZI_AUDIO_RATE> bassOsc2(SQUARE_NO_ALIAS_2048_DATA);
static Oscil<SIN2048_NUM_CELLS, CONTROL_RATE> bassPitchLFO(SIN2048_DATA);
static ADSR<CONTROL_RATE, MOZZI_AUDIO_RATE> bassEnv;
static StateVariable<LOWPASS> bassLPF;
static uint8_t bassEnvLevel = 0;
static bool bassActive = false;
static bool bassOsc2Active = false;
static int bassBaseNote = 0;
static float bassDetune = 0.0f;
static float bassPitchLFOAmplitude = 0;
static uint16_t bassFilterCutoffBase = 800;
static uint8_t bassFilterResonance = 120;

// Pad
static Oscil<SAW2048_NUM_CELLS, MOZZI_AUDIO_RATE> padOsc1(SAW2048_DATA);
static Oscil<SAW2048_NUM_CELLS, MOZZI_AUDIO_RATE> padOsc2(SAW2048_DATA);
static Oscil<SAW2048_NUM_CELLS, MOZZI_AUDIO_RATE> padOsc3(SAW2048_DATA);
static Oscil<SAW2048_NUM_CELLS, MOZZI_AUDIO_RATE> padOsc4(SAW2048_DATA);
static Oscil<SIN2048_NUM_CELLS, CONTROL_RATE> padPitchLFO(SIN2048_DATA);
static ADSR<CONTROL_RATE, MOZZI_AUDIO_RATE> padEnv;
static StateVariable<LOWPASS> padLPF;
static StateVariable<HIGHPASS> eqPad;
static uint8_t padEnvLevel = 0;
static bool padActive = false;
static float padBaseNote = 0.0f;
static float padPitchLFOAmplitude = 0;
static float padDetune2 = 0.0f;
static float padDetune3 = 0.0f;
static uint8_t padChordType = 0; // acorde elegido por el randomizer

// Lead FM (pluck / campana): una portadora senoidal modulada en fase por otra senoidal.
// - leadEnv: volumen (audio rate), decae a 0.
// - leadModEnv: profundidad de FM (control rate), decae mas rapido: el brillo se apaga
//   antes que el volumen, que es lo que da el caracter de pluck/campana.
static Oscil<SIN2048_NUM_CELLS, MOZZI_AUDIO_RATE> leadCarrier(SIN2048_DATA);
static Oscil<SIN2048_NUM_CELLS, MOZZI_AUDIO_RATE> leadModulator(SIN2048_DATA);
static ADSR<CONTROL_RATE, MOZZI_AUDIO_RATE> leadEnv;
static ADSR<CONTROL_RATE, CONTROL_RATE> leadModEnv;
static bool leadActive = false;
static int32_t leadFmIntensity = 0; // control rate -> audio rate. 512 = 1 ciclo de desvio de fase

struct LeadPreset
{
  float ratio;         // frecuencia moduladora / portadora
  uint16_t depth;      // profundidad maxima de FM (con el pote 1 al maximo)
  uint16_t modDecayMs; // cuanto tarda en apagarse el brillo
};

// Ratios enteros = armonico (pluck), no enteros = inarmonico (campana, metal)
static const LeadPreset LEAD_PRESETS[] = {
    {1.0f, 400, 90},   // pluck
    {2.0f, 260, 250},  // piano electrico / vidrio
    {3.5f, 300, 700},  // campana
    {1.41f, 450, 200}, // metalico
    {4.0f, 200, 120},  // marimba
};
#define NUM_LEAD_PRESETS (sizeof(LEAD_PRESETS) / sizeof(LEAD_PRESETS[0]))
static uint8_t leadPreset = 0;

// Delay de pad + bass
#define PAD_BASS_DELAY_CELLS 16384
static const int8_t PAD_BASS_DELAY_FEEDBACK = 70;
static AudioDelayFeedback<PAD_BASS_DELAY_CELLS> padBassDelay;
static bool padBassDelayEnabled = false;

// Etapa de salida: la 1ra mitad del pote es volumen, la 2da agrega drive.
static const int32_t OUT_MAX_DRIVE_Q10 = 12 * 1024;
static const int32_t OUT_MAX_VOLUME_Q10 = 1088;
static const int32_t OUT_SOFT_KNEE = 384;
static const int32_t OUT_CEILING = 512;
static int32_t outVolumeQ10 = 0;
static int32_t outDriveQ10 = 1024;

// ======================= Armonia =======================

// Intervalos (semitonos) de los acordes del PAD
#define NUM_CHORDS 8
static const int8_t chordIntervals[NUM_CHORDS][4] = {
    {0, 3, 7, 10}, // m7
    {0, 3, 7, 14}, // m9 (add9)
    {0, 5, 7, 10}, // m7sus4
    {0, 3, 6, 10}, // m7b5
    {0, -5, 0, -5},
    {0, 0, 0, 0},
    {0, 12, 12, 12},
    {0, 2, 0, 0}};

// Notas MIDI de la escala actual (una octava)
#define SCALE_LEN 6
static uint8_t synthNotes[SCALE_LEN] = {35, 36, 37, 38, 39, 40};

// Grado 0..11 -> nota MIDI. Los grados 6..11 son la misma escala una octava arriba.
static uint8_t degreeToMidi(uint8_t degree)
{
  return synthNotes[degree % SCALE_LEN] + 12 * (degree / SCALE_LEN);
}

// 12 escalas (intervalos en semitonos desde la raiz) - oscuras/arabes
static const uint8_t scales[12][6] = {
    {0, 2, 3, 5, 7, 11}, // Harmonic Minor - oscuro
    {0, 1, 3, 5, 7, 8},  // Phrygian - muy oscuro
    {0, 1, 3, 5, 6, 8},  // Locrio - ultra oscuro
    {0, 1, 4, 5, 6, 8},  // Oriental/Arabe
    {0, 2, 3, 6, 8, 11}, // Hungarian Minor - muy oscuro
    {0, 1, 3, 4, 6, 8},  // Ultralocrian - oscurisimo
    {0, 1, 4, 5, 7, 8},  // Byzantine - arabe
    {0, 3, 5, 6, 7, 10}, // Blues
    {0, 2, 4, 5, 7, 9},  // Dorico - neutral
    {0, 1, 3, 5, 6, 9},  // Super Locrian bb7 - muy oscuro
    {0, 2, 3, 5, 7, 8},  // Menor Natural
    {0, 1, 4, 6, 7, 10}  // Arabe/Maqam
};

// ======================= Setup =======================

static void setupEnvelopes()
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

  padEnv.setADLevels(255, 200);
  padEnv.setTimes(8, 200, 180, 120);

  leadEnv.setADLevels(255, 0);
  leadEnv.setTimes(1, 600, 0, 0); // el decay lo ajusta el pote 2 en voicesUpdateControl()

  leadModEnv.setADLevels(255, 0);
  leadModEnv.setTimes(1, LEAD_PRESETS[0].modDecayMs, 0, 0);
}

void voicesSetup()
{
  padBassDelay.setDelayTimeCells((uint16_t)PAD_BASS_DELAY_CELLS);
  padBassDelay.setFeedbackLevel(0);

  setupEnvelopes();

  bassLPF.setResonance(bassFilterResonance);
  bassLPF.setCentreFreq(bassFilterCutoffBase);
  padLPF.setResonance(120);
  padLPF.setCentreFreq(800);
  bassPitchLFO.setFreq(0.5f);
  padPitchLFO.setFreq(0.5f);
  snareNoiseOsc.setFreq(2.0f);
  hatNoiseOsc.setFreq(2.0f);
  hatNoiseOsc.setPhase(WHITENOISE8192_NUM_CELLS / 2);
  crashNoiseOsc.setFreq(2.0f);
  crashOsc.setFreq(5000);

  // EQ de pad y snare
  eqPad.setResonance(100);
  eqPad.setCentreFreq(200);
  eqSnare.setResonance(100);
  eqSnare.setCentreFreq(150);
}

// ======================= Disparos =======================

static void triggerKick()
{
  kickOsc.setPhase(0);
  kickOsc.setFreq(45);
  kickEnv.noteOn();
}

static void triggerSnare()
{
  snareToneOsc.setPhase(0);
  snareEnv.noteOn();
  snareToneEnv.noteOn();
  snareToneOsc.setFreq(220.0f);
}

static void triggerHat()
{
  hatEnv.noteOn();
  crashEnv.noteOff(); // el hat corta al crash
}

static void triggerCrash()
{
  crashOsc.setPhase(0);
  crashOsc.setFreq(8000.0f); // onda cuadrada muy aguda, timbre metalico
  crashNoiseOsc.setPhase(0);
  crashNoiseOsc.setFreq(3.0f); // ruido para modular el pitch y mezclar
  crashEnv.noteOn();
}

void triggerDrum(uint8_t drum)
{
  if (drum == DRUM_KICK)
    triggerKick();
  else if (drum == DRUM_SNARE)
    triggerSnare();
  else if (drum == DRUM_HAT)
    triggerHat();
  else if (drum == DRUM_CRASH)
    triggerCrash();
}

void triggerBass(uint8_t degree, bool octaveUp)
{
  uint8_t midiNote = degreeToMidi(degree);
  if (octaveUp)
    midiNote += 12;
  bassBaseNote = midiNote; // la frecuencia se actualiza en voicesUpdateControl()
  bassEnv.noteOn();
  bassActive = true;
}

void triggerPad(uint8_t degree, bool octaveUp)
{
  // Acorde de 4 notas, dos octavas arriba del bass
  uint8_t rootMidi = degreeToMidi(degree);
  int baseShift = 24;
  if (octaveUp)
    baseShift += 12;

  int8_t i0 = chordIntervals[padChordType][0];
  padBaseNote = rootMidi + baseShift + i0; // las frecuencias se actualizan en voicesUpdateControl()

  padEnv.noteOn();
  padActive = true;
}

void triggerLead(uint8_t degree, bool octaveUp)
{
  // Dos octavas arriba del bass, como la raiz del pad
  uint8_t midiNote = degreeToMidi(degree) + 24;
  if (octaveUp)
    midiNote += 12;

  // float solo al disparar la nota, no a audio rate
  float carrierFreq = mtof((float)midiNote);
  leadCarrier.setFreq(carrierFreq);
  leadModulator.setFreq(carrierFreq * LEAD_PRESETS[leadPreset].ratio);
  leadCarrier.setPhase(0);
  leadModulator.setPhase(0);

  leadEnv.noteOn();
  leadModEnv.noteOn();
  leadActive = true;
}

void silenceLead()
{
  leadActive = false;
  leadEnv.noteOff();
}

void silenceBass()
{
  bassActive = false;
  bassBaseNote = 0;
  bassOsc.setFreq(0.0f);
  bassOsc2.setFreq(0.0f);
}

void silencePad()
{
  padActive = false;
  padBaseNote = 0;
  padOsc1.setFreq(0.0f);
  padOsc2.setFreq(0.0f);
  padOsc3.setFreq(0.0f);
  padOsc4.setFreq(0.0f);
}

// ======================= Randomizacion =======================

void voicesRandomizeScale()
{
  uint8_t scaleIndex = random(0, 12);
  uint8_t root = random(30, 42);
  for (uint8_t i = 0; i < SCALE_LEN; i++)
    synthNotes[i] = root + scales[scaleIndex][i];
}

void voicesRandomizeBass()
{
  float bassPitchLFOFreq = random(10, 100) / 100.0f;
  bassPitchLFO.setFreq(bassPitchLFOFreq);
  if (random(0, 10) <= 4)
    bassPitchLFOAmplitude = random(1, 20);
  else
    bassPitchLFOAmplitude = 0;

  // Detune del segundo oscilador.
  // Nota: en los ultimos 3 casos "random(0, 11) / 1000" es division entera (siempre 0).
  int detuneType = random(0, 100);
  if (detuneType < 20)
  {
    bassOsc2Active = false;
  }
  else if (detuneType < 50)
  {
    bassOsc2Active = true;
    bassDetune = random(1, 11) / 1000.0f;
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
}

void voicesRandomizePad()
{
  float padPitchLFOFreq = random(10, 401) / 100.0f;
  padPitchLFO.setFreq(padPitchLFOFreq);
  if (random(0, 10) <= 4)
    padPitchLFOAmplitude = random(1, 10);
  else
    padPitchLFOAmplitude = 0;

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

  padChordType = random(0, NUM_CHORDS);
}

void voicesRandomizeLead()
{
  leadPreset = random(0, NUM_LEAD_PRESETS);
  leadModEnv.setTimes(1, LEAD_PRESETS[leadPreset].modDecayMs, 0, 0);
}

// ======================= Control rate =======================

void voicesUpdateControl(uint16_t pot1, uint16_t pot2)
{
  kickEnv.update();
  snareEnv.update();
  hatEnv.update();
  snareToneEnv.update();
  bassEnv.update();
  padEnv.update();
  crashEnv.update();

  // Filtro del bass: pote 1 = cutoff, pote 2 = resonancia
  bassFilterCutoffBase = constrain(map(pot1, 0, 1024, 80, 2000), 80, 2000);

  int bassFilterResonanceMin = 20;
  bassFilterResonance = (uint8_t)constrain(map(pot2, 0, 1024, 200, bassFilterResonanceMin), bassFilterResonanceMin, 200);
  uint16_t bassFilterDecay = 220;
  bassLPF.setResonance(bassFilterResonance);

  bassEnvLevel = bassEnv.next();
  uint16_t envCutoff = constrain(bassFilterCutoffBase + (uint16_t)(bassEnvLevel * 2), 80, 2200);
  bassLPF.setCentreFreq(envCutoff);

  bassEnv.setADLevels(255, 20);
  bassEnv.setTimes(5, bassFilterDecay, 1, 100);

  // Filtro del pad: sigue al cutoff del bass; pote 2 = decay de la envolvente
  padLPF.setResonance(100);
  padLPF.setCentreFreq(envCutoff);

  static uint16_t lastPot2 = 0;
  if (pot2 != lastPot2) // casi siempre cambia, la lectura del pote tiene ruido
  {
    const uint16_t padFilterDecayMax = 1600;
    uint16_t padFilterDecay = constrain(map(pot2, 0, 1024, 1, padFilterDecayMax), 1, padFilterDecayMax);
    padEnv.setADLevels(255, 10);
    padEnv.setTimes(1, padFilterDecay, 10, 100); // attack, decay, sustain, release (ms)
    lastPot2 = pot2;
  }

  // Lead FM: pote 1 = brillo (profundidad de FM), pote 2 = largo de la nota
  leadEnv.update();
  leadModEnv.update();
  if (leadActive)
  {
    uint16_t leadDecay = constrain(map(pot2, 0, 1024, 80, 1500), 80, 1500);
    leadEnv.setTimes(1, leadDecay, 0, 0);

    uint8_t modLevel = leadModEnv.next();
    uint32_t brightness = constrain(pot1, (uint16_t)0, (uint16_t)1023); // 0..1023
    leadFmIntensity = ((int32_t)modLevel * LEAD_PRESETS[leadPreset].depth * (int32_t)(brightness + 128)) >> 18;

    if (!leadEnv.playing())
      leadActive = false; // la nota termino: deja de calcularse a audio rate
  }

  // Pitch del bass
  if (bassBaseNote > 0)
  {
    float bassPitchMod = bassPitchLFO.next() * bassPitchLFOAmplitude / 4000.0f;
    bassOsc.setFreq(mtof(bassBaseNote + bassPitchMod));
    if (bassOsc2Active)
    {
      float detunedFreq = (mtof(bassBaseNote + bassPitchMod + bassDetune));
      bassOsc2.setFreq(detunedFreq);
    }
  }

  // Pitch del pad
  if (padBaseNote > 0)
  {
    float padPitchMod = padPitchLFO.next() * padPitchLFOAmplitude / 4000.0f;
    int8_t i0 = chordIntervals[padChordType][0];
    int8_t i1 = chordIntervals[padChordType][1];
    int8_t i2 = chordIntervals[padChordType][2];
    int8_t i3 = chordIntervals[padChordType][3];
    padOsc1.setFreq(mtof(padBaseNote + i0 + padPitchMod));
    padOsc2.setFreq(mtof(padBaseNote + i1 + padDetune2 + padPitchMod));
    padOsc3.setFreq(mtof(padBaseNote + i2 + padDetune3 + padPitchMod));
    padOsc4.setFreq(mtof(padBaseNote + i3 - padDetune2 + padPitchMod));
  }
}

// ======================= Salida =======================

void voicesSetOutputGain(uint16_t potValue)
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

static int16_t applyOutputGain(int16_t sample)
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

  return (int16_t)(driven < 0 ? -magnitude : magnitude);
}

void voicesSetDelay(bool enabled)
{
  padBassDelay.setFeedbackLevel(enabled ? PAD_BASS_DELAY_FEEDBACK : 0);
  padBassDelayEnabled = enabled;
}

// ======================= Audio rate =======================

int16_t voicesNextSample()
{
  bassEnvLevel = bassEnv.next();

  // Kick
  uint8_t kickEnvLevel = kickEnv.next();
  int kickFreq = 35 + (kickEnvLevel >> 1);
  kickOsc.setFreq(kickFreq);
  int16_t kick = (kickOsc.next() * kickEnvLevel) >> 8;

  // Snare: ruido blanco + tono senoidal con su propia envolvente
  int16_t snNoise = snareNoiseOsc.next();
  int16_t snare = eqSnare.next((snNoise * snareEnv.next())) >> 7;
  int16_t snareTone = eqSnare.next((snareToneOsc.next() * snareToneEnv.next())) >> 8;

  // Hi-hat: ruido + diferenciador
  int16_t hatNoise = hatNoiseOsc.next();
  int16_t hatHp = hatNoise - lastHatNoise;
  lastHatNoise = hatNoise;
  int16_t hat = (hatHp * hatEnv.next()) >> 7;

  // Crash: onda cuadrada aguda con FM de ruido + ruido mezclado
  // (usa float a audio rate: candidato a optimizar)
  crashEnvLevel = crashEnv.next();
  int16_t noiseMod = crashNoiseOsc.next();
  float freqMod = (noiseMod / 128.0f) * 800.0f;
  float currentFreq = 8000.0f + freqMod;
  if (currentFreq < 100.0f)
    currentFreq = 100.0f;
  crashOsc.setFreq(currentFreq);
  int16_t crashSquare = crashOsc.next();
  int16_t crashNoise = crashNoiseOsc.next();
  int16_t crash = ((crashSquare * 7 + crashNoise * 3) / 10); // 70% cuadrada, 30% ruido
  crash = (crash * crashEnvLevel) >> 8;

  // Bass
  int16_t bass = 0;
  if (bassActive)
  {
    int16_t bassOscSample = bassOsc.next();
    int16_t bassOsc2Sample = bassOsc2Active ? bassOsc2.next() : 0;
    int16_t bassRaw = (bassOscSample + bassOsc2Sample);
    bass = (bassLPF.next(bassRaw) * bassEnvLevel) >> 6;
  }

  // Pad
  int16_t pad = 0;
  if (padActive)
  {
    int16_t p1 = padOsc1.next();
    int16_t p2 = padOsc2.next();
    int16_t p3 = padOsc3.next();
    int16_t p4 = padOsc4.next();
    int32_t padRaw = (int32_t)p1 + (int32_t)p2 + (int32_t)p3 + (int32_t)p4;
    int16_t padFiltered = eqPad.next(padLPF.next((int16_t)constrain(padRaw, -32767, 32767)));
    padEnvLevel = padEnv.next();
    pad = (padFiltered * padEnvLevel) >> 8;
  }

  // Lead FM: la moduladora corre la fase de la portadora
  int16_t lead = 0;
  if (leadActive)
  {
    Q15n16 modulation = (Q15n16)leadFmIntensity * leadModulator.next();
    int16_t leadSample = leadCarrier.phMod(modulation);
    lead = (leadSample * leadEnv.next()) >> 6; // ~ +-500, similar al pad
  }

  // Delay de pad + bass + lead
  int32_t padBassInput = padBassDelayEnabled ? (((int32_t)(bass >> 2) + (pad >> 1) + (lead >> 1)) >> 2) : 0;
  int32_t delayedPadBass = padBassDelay.next(padBassInput);

  int32_t mix;
  mix = ((int32_t)kick << 5);
  mix = mix + ((int32_t)snare << 1);
  mix = mix + ((int32_t)snareTone << 0);
  mix = mix + ((int32_t)hat << 0);
  mix = mix + ((int32_t)crash << 0);
  mix = mix + ((int32_t)bass << 0);
  mix = mix + ((int32_t)pad << 0);
  mix = mix + ((int32_t)lead << 0);
  // TODO (volumen, pendiente): por precedencia esto es (mix + delayedPadBass) << 6,
  // o sea que desplaza TODA la mezcla, no solo el delay. Se deja asi a proposito
  // hasta revisarlo de oido.
  mix = mix + delayedPadBass << 6;

  return applyOutputGain((int16_t)(mix >> 5));
}
