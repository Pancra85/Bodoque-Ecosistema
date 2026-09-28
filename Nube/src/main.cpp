/*
  EL NUBE
  - Sin secuenciador.
  - Cuatro capas armónicas que se activan/desactivan.
  - Entrada y salida lenta de cada capa.
  - Tres climas tímbricos.
  - Pot1 = bruma/cutoff.
  - Pot2 = inestabilidad general.
  - pOT3 = gANANCIA LOKO
  - TEMPO por taps = velocidad de respiración del filtro.
*/


#include <Arduino.h>

#include "MozziConfigValues.h"
#define MOZZI_AUDIO_RATE 32768
#define MOZZI_CONTROL_RATE 128
#define MOZZI_ANALOG_READ MOZZI_ANALOG_READ_NONE
#if DESTRUKTOR1
// para Destruktor1:
#define MOZZI_AUDIO_PIN_1 14
#define MOZZI_AUDIO_PIN_2 15
#define MOZZI_OUTPUT_MODE MOZZI_OUTPUT_PWM

#endif

#if BODOQUEV1
////para Bodoque.v1:
#define MOZZI_AUDIO_MODE MOZZI_OUTPUT_I2S_DAC
#define MOZZI_AUDIO_CHANNELS MOZZI_STEREO
#define MOZZI_I2S_PIN_BCK 20
#define MOZZI_I2S_PIN_WS (MOZZI_I2S_PIN_BCK + 1) // CANNOT BE CHANGED, HAS TO BE NEXT TO pBCLK, i.e. default is 21
#define MOZZI_I2S_PIN_DATA 22
#define MOZZI_I2S_FORMAT MOZZI_I2S_FORMAT_LSBJ // PT8211 mode as per datasheet
#endif

#include <Mozzi.h>
#include <Oscil.h>
#include <StateVariable.h>
#include <tables/sin2048_int8.h>
#include <tables/saw2048_int8.h>
#include <tables/whitenoise8192_int8.h>
#include <mozzi_midi.h>
#include <mozzi_rand.h>
#include <EventDelay.h>
#include <AudioDelayFeedback.h>
#include "oledAnimation.h"

static const uint16_t POT_MAX = 16384;

#if DESTRUKTOR1
static const uint8_t NUM_LAYERS = 4;
static const uint8_t NUM_TRACK_LEDS = 3;
static const uint8_t NUM_POTS = 2;

static const uint8_t LAYER_LED_PIN[NUM_LAYERS] = {1, 3, 6, 8};
static const uint8_t NOTE_BUTTON_PIN[NUM_LAYERS] = {2, 4, 5, 7};
static const uint8_t TRACK_LED_PIN[NUM_TRACK_LEDS] = {11, 10, 9};

static const uint8_t TRACK_BUTTON_PIN = 0;
static const uint8_t TEMPO_BUTTON_PIN = 29;
static const uint8_t POT_PIN[NUM_POTS] = {A1, A2};

static const uint8_t JACK_SENSOR_PIN = A0;
static const uint8_t MUTE_SPEAKER_PIN = 17;
#endif

#if BODOQUEV1
static const uint8_t NUM_LAYERS = 4;
static const uint8_t NUM_TRACK_LEDS = 3;
static const uint8_t NUM_POTS = 3;

static const uint8_t LAYER_LED_PIN[NUM_LAYERS] = {1, 3, 6, 8};
static const uint8_t NOTE_BUTTON_PIN[NUM_LAYERS] = {2, 4, 5, 7};
static const uint8_t TRACK_LED_PIN[NUM_TRACK_LEDS] = {11, 10, 9};

static const uint8_t TRACK_BUTTON_PIN = 0;
static const uint8_t TEMPO_BUTTON_PIN = 24;
static const uint8_t SCALE_BUTTON_PIN = 25;
static const uint8_t POT_PIN[NUM_POTS] = {A3, A1, A0};

static const uint8_t MUTE_SPEAKER_PIN = 17;
#endif

// La raíz de la escala queda fija en Mi para que la nube siempre respire desde
// un mismo centro tonal, aunque cambien las capas, los grados y los climas.
static const uint8_t SCALE_ROOT = 52; // E3

enum ScaleMode : uint8_t
{
  SCALE_DORIAN = 0,
  SCALE_HARMONIC_MINOR = 1,
  SCALE_BYZANTINE = 2,
  SCALE_PHRYGIAN = 3,
  SCALE_LOCRIAN = 4,
  SCALE_HUNGARIAN_MINOR = 5,
  SCALE_HIJAZ = 6,
  SCALE_BAYATI = 7,
  SCALE_SABA = 8,
  SCALE_SIKAH = 9
};

static const uint8_t SCALE_MODE_COUNT = 10;

// Intervalos en centésimas de semitono. Los valores no múltiplos de 100
// permiten escalas microtonales sin cambiar la interfaz MIDI de Mozzi.
static const uint16_t SCALE_INTERVALS[SCALE_MODE_COUNT][7] = {
    {0, 200, 300, 500, 700, 900, 1000},  // Dórico
    {0, 200, 300, 500, 700, 800, 1100},  // Menor armónica
    {0, 100, 400, 500, 700, 800, 1000},  // Bizantino
    {0, 100, 300, 500, 700, 800, 1000},  // Frigio
    {0, 100, 300, 500, 600, 800, 1000},  // Locrio
    {0, 200, 300, 600, 700, 800, 1100},  // Menor húngara
    {0, 100, 400, 500, 700, 800, 1100},  // Hijaz / doble armónica
    {0, 150, 300, 500, 700, 800, 1000},  // Bayati, cuarto de tono
    {0, 150, 350, 500, 600, 800, 1000},  // Saba, cuarto de tono
    {0, 150, 300, 500, 650, 850, 1000}   // Sikah, microtonal
};

TextureMode textureMode = TEXTURE_CALM;
ScaleMode scaleMode = SCALE_DORIAN;

// Snapshot shared from Mozzi's core 0 to the OLED animation on core 1.
volatile uint16_t oledPot1 = 0;
volatile uint16_t oledPot2 = 0;
volatile uint16_t oledPot3 = 0;
volatile uint8_t oledLayersMask = 0;
volatile uint8_t oledTextureMode = TEXTURE_CALM;
volatile bool oledAnimationReady = false;

// Raw UI snapshot produced by core 1 and consumed by Mozzi's core 0.
volatile uint16_t uiPot1 = 0;
volatile uint16_t uiPot2 = 0;
volatile uint16_t uiPot3 = 0;
volatile int uiJackAverage = 512;
volatile bool uiNotePressed[NUM_LAYERS] = {false, false, false, false};
volatile bool uiTrackPressed = false;
volatile bool uiTempoPressed = false;
volatile bool uiScalePressed = false;

// Dos osciladores por capa: una base senoidal y una voz de color.
Oscil<SIN2048_NUM_CELLS, MOZZI_AUDIO_RATE> sineOsc[NUM_LAYERS] = {
    Oscil<SIN2048_NUM_CELLS, MOZZI_AUDIO_RATE>(SIN2048_DATA),
    Oscil<SIN2048_NUM_CELLS, MOZZI_AUDIO_RATE>(SIN2048_DATA),
    Oscil<SIN2048_NUM_CELLS, MOZZI_AUDIO_RATE>(SIN2048_DATA),
    Oscil<SIN2048_NUM_CELLS, MOZZI_AUDIO_RATE>(SIN2048_DATA)};

Oscil<SAW2048_NUM_CELLS, MOZZI_AUDIO_RATE> colorOsc[NUM_LAYERS] = {
    Oscil<SAW2048_NUM_CELLS, MOZZI_AUDIO_RATE>(SAW2048_DATA),
    Oscil<SAW2048_NUM_CELLS, MOZZI_AUDIO_RATE>(SAW2048_DATA),
    Oscil<SAW2048_NUM_CELLS, MOZZI_AUDIO_RATE>(SAW2048_DATA),
    Oscil<SAW2048_NUM_CELLS, MOZZI_AUDIO_RATE>(SAW2048_DATA)};

// LFOs independientes para que las capas no respiren juntas.
Oscil<SIN2048_NUM_CELLS, MOZZI_CONTROL_RATE> driftLFO[NUM_LAYERS] = {
    Oscil<SIN2048_NUM_CELLS, MOZZI_CONTROL_RATE>(SIN2048_DATA),
    Oscil<SIN2048_NUM_CELLS, MOZZI_CONTROL_RATE>(SIN2048_DATA),
    Oscil<SIN2048_NUM_CELLS, MOZZI_CONTROL_RATE>(SIN2048_DATA),
    Oscil<SIN2048_NUM_CELLS, MOZZI_CONTROL_RATE>(SIN2048_DATA)};

Oscil<SIN2048_NUM_CELLS, MOZZI_CONTROL_RATE> filterLFO(SIN2048_DATA);
Oscil<WHITENOISE8192_NUM_CELLS, MOZZI_AUDIO_RATE> noiseOsc(WHITENOISE8192_DATA);

// Cuatro voces transitorias para el "granizo estelar".
Oscil<SIN2048_NUM_CELLS, MOZZI_AUDIO_RATE> grainOsc[NUM_LAYERS] = {
    Oscil<SIN2048_NUM_CELLS, MOZZI_AUDIO_RATE>(SIN2048_DATA),
    Oscil<SIN2048_NUM_CELLS, MOZZI_AUDIO_RATE>(SIN2048_DATA),
    Oscil<SIN2048_NUM_CELLS, MOZZI_AUDIO_RATE>(SIN2048_DATA),
    Oscil<SIN2048_NUM_CELLS, MOZZI_AUDIO_RATE>(SIN2048_DATA)};

StateVariable<BANDPASS> masterLPF;

// Nivel Q8: 0 .. 255*256.
// El suavizado exponencial evita un note-on/note-off evidente.
int32_t layerLevelQ8[NUM_LAYERS] = {0, 0, 0, 0};
int32_t layerTargetQ8[NUM_LAYERS] = {0, 0, 0, 0};

bool layerEnabled[NUM_LAYERS] = {false, false, false, false};
bool layerOctave[NUM_LAYERS] = {false, false, false, false};
float layerNoteCurrent[NUM_LAYERS] = {0.0f, 0.0f, 0.0f, 0.0f};
float layerNoteTarget[NUM_LAYERS] = {0.0f, 0.0f, 0.0f, 0.0f};
uint8_t layerDegreeIndex[NUM_LAYERS] = {0, 2, 4, 6};
float layerMicroDetune[NUM_LAYERS] = {0.0f, 0.0f, 0.0f, 0.0f};
float layerMicroDetuneTarget[NUM_LAYERS] = {0.0f, 0.0f, 0.0f, 0.0f};
unsigned long layerBreathAt[NUM_LAYERS] = {0, 0, 0, 0};
uint16_t layerBreathInterval[NUM_LAYERS] = {4800, 6400, 8200, 9800};
unsigned long layerNoteChangeAt[NUM_LAYERS] = {0, 0, 0, 0};
unsigned long lastDebugPrintAt = 0;

struct ButtonState
{
  bool stablePressed;
  bool rawPressed;
  bool holdHandled;
  unsigned long rawChangedAt;
  unsigned long pressedAt;
};

ButtonState noteButton[NUM_LAYERS] = {};
ButtonState trackButton = {};
ButtonState tempoButton = {};
ButtonState scaleButton = {};

static const unsigned long DEBOUNCE_MS = 18;
static const unsigned long HOLD_MS = 650;

uint16_t pot1 = 0;
uint16_t pot2 = 0;
uint16_t pot3 = 0;

float filterBreathHz = 0.08f;
unsigned long previousTempoTap = 0;

// Rangos y modulación del filtro configurables para tunear por hardware.
float filterCutoffMinHz = 500.0f;
float filterCutoffMaxHz = 10000.0f;
int32_t filterModulationMin = 0;
int32_t filterModulationMax = 220;
float filterModulationCutoffGain = 0.008f;
float filterModulationInstabilityGain = 45.0f;
float filterModulationWindScale = 2.83f;
float filterModulationCrystalScale = 2.05f;

int16_t sineMix = 240;
int16_t colorMix = 32;
int16_t noiseMix = 0;

// Deriva autónoma: destinos aleatorios en cents que se alcanzan muy lentamente.
float autoDriftCurrent[NUM_LAYERS] = {0, 0, 0, 0};
float autoDriftTarget[NUM_LAYERS] = {0, 0, 0, 0};
EventDelay autoDriftClock;

// Estado del granizo estelar.
bool grainBurstActive = false;
uint8_t grainsRemaining = 0;
uint8_t nextGrainVoice = 0;
EventDelay grainClock;
int32_t grainLevelQ8[NUM_LAYERS] = {0, 0, 0, 0};

// Delay: buffer size in samples (max delay). At 32768 Hz, 32768 cells ~= 1s
#define DELAY_CELLS 32768
AudioDelayFeedback<DELAY_CELLS> aDelay;

// User-controllable delay parameters (globals requested)
// delayTime in milliseconds, delayMix 0.0..1.0, delayFeedback -128..127
int delayTime = 800;
float delayMix = 0.9f;
int delayFeedback = 80;
float delayMixMax = 1.0f;
int delayFeedbackMax = 160;
// Base (track) targets — `setDelayParamsForTrack` updates these.
float baseDelayMix = 0.2f;
int baseDelayFeedback = 50;

// Si es true, el delay se aplica después del filtro maestro.
// Si es false, la señal pasa por delay y luego por el filtro.
bool delayAfterFilter = false;

// ============================================================
// ETAPA DE SALIDA: un pot = volumen + distorsión
//   Pot 0..512    -> volumen limpio (sin distorsión)
//   Pot 8192..16384 -> volumen full + distorsión creciente
// ============================================================

// --- Constantes que podés tocar para "afinar el sabor": -----------
static const float OUT_MAX_DRIVE = 20.0f; // Drive máximo al final del recorrido
static const int32_t OUT_KNEE_1 = 2000;   // Primer codo (fin de la zona limpia)
static const int32_t OUT_KNEE_2 = 3000;   // Segundo codo (compresión media)
static const int32_t OUT_KNEE_3 = 1600;   // Tercer codo (compresión fuerte)
// Techo = 4095 (rango del rango de salida de esta etapa)
// ------------------------------------------------------------------

// Coeficientes actualizados en updateControl() (Q10: 1024 = 1.0)
static int32_t outVolumeQ10 = 0;
static int32_t outDriveQ10 = 1024;

void updateOutputGain(uint16_t potValue)
{
  potValue = potValue >> 2;
  if (potValue < 20)
  {
    potValue = 0;
  }
  if (potValue <= 512)
  {
    // Mitad inferior: solo volumen, drive neutro (1.0)
    outVolumeQ10 = ((int32_t)potValue * 1024) / 512;
    outDriveQ10 = 1024;
  }
  else
  {
    // Mitad superior: volumen al máximo, drive sube
    outVolumeQ10 = 1024;
    uint16_t t = potValue - 512; // 0..511
    int32_t maxDriveQ10 = (int32_t)(OUT_MAX_DRIVE * 1024.0f);
    outDriveQ10 = 1024 + ((int32_t)t * (maxDriveQ10 - 1024)) / 511;
  }
}

void setDelayParamsForTrack(uint8_t track)
{
  delayTime = 800;
  baseDelayMix = 0.f;
  baseDelayFeedback = 0;
  switch (track)
  {
  case 1:
    delayTime = 300;
    // baseDelayMix = 0.4f;
    // baseDelayFeedback = 60;
    break;
  case 2:
    delayTime = 600;
    // baseDelayMix = 0.7f;
    // baseDelayFeedback = 70;
    break;
  case 0:
    delayTime = 100;
  default:
    // baseDelayMix = 0.2f;
    // baseDelayFeedback = 50;
    break;
  }
}

// -----------------------------------------------------------------------------
// Escalas, notas y debug
// -----------------------------------------------------------------------------

const char *getTextureName(TextureMode mode)
{
  switch (mode)
  {
  case TEXTURE_WIND:
    return "WIND";
  case TEXTURE_CRYSTAL:
    return "CRYSTAL";
  case TEXTURE_CALM:
  default:
    return "CALM";
  }
}

uint8_t getScaleMode()
{
  return (uint8_t)scaleMode;
}

const char *getScaleName(uint8_t mode)
{
  switch (mode)
  {
  case SCALE_SIKAH:
    return "SIKAH_MICROTONAL";
  case SCALE_SABA:
    return "SABA_MICROTONAL";
  case SCALE_BAYATI:
    return "BAYATI_QUARTER_TONE";
  case SCALE_HIJAZ:
    return "HIJAZ";
  case SCALE_HUNGARIAN_MINOR:
    return "HUNGARIAN_MINOR";
  case SCALE_LOCRIAN:
    return "LOCRIAN";
  case SCALE_PHRYGIAN:
    return "PHRYGIAN";
  case SCALE_HARMONIC_MINOR:
    return "HARMONIC_MINOR";
  case SCALE_BYZANTINE:
    return "BYZANTINE";
  case SCALE_DORIAN:
  default:
    return "DORIAN";
  }
}

float getScaleNote(uint8_t degreeIndex, uint8_t octaveOffset, uint8_t scaleMode)
{
  const uint16_t intervalCents = SCALE_INTERVALS[scaleMode][degreeIndex % 7];
  return (float)SCALE_ROOT + (float)octaveOffset * 12.0f +
         (float)intervalCents / 100.0f;
}

void setLayerTargetNote(uint8_t index)
{
  const uint8_t octaveOffset = layerOctave[index] ? 1 : 0;
  const uint8_t selectedScale = getScaleMode();
  const float targetNote = getScaleNote(layerDegreeIndex[index], octaveOffset, selectedScale);

  layerNoteTarget[index] = targetNote;
  if (!layerEnabled[index])
  {
    layerNoteCurrent[index] = targetNote;
  }
}

void printDebugState()
{
  const unsigned long now = millis();
  if ((now - lastDebugPrintAt) < 1800UL)
  {
    return;
  }

  lastDebugPrintAt = now;

  Serial.print("DBG climate=");
  Serial.print(getTextureName(textureMode));
  Serial.print(" scale=");
  Serial.print(getScaleName(getScaleMode()));
  Serial.print(" pot1=");
  Serial.print(pot1);
  Serial.print(" pot2=");
  Serial.print(pot2);
  Serial.print(" pot3=");
  Serial.print(pot3);
  Serial.print(" layers=");

  for (uint8_t i = 0; i < NUM_LAYERS; ++i)
  {
    Serial.print(i);
    Serial.print(':');
    Serial.print(layerEnabled[i] ? "on" : "off");
    Serial.print("/oct");
    Serial.print(layerOctave[i] ? "1" : "0");
    Serial.print(" note=");
    Serial.print((int)layerNoteCurrent[i]);
    Serial.print("->");
    Serial.print((int)layerNoteTarget[i]);
    if (i < (NUM_LAYERS - 1))
    {
      Serial.print(" | ");
    }
  }

  Serial.println();
}

// -----------------------------------------------------------------------------
// LEDs
// -----------------------------------------------------------------------------

void setTrackLed(uint8_t index)
{
  for (uint8_t i = 0; i < NUM_TRACK_LEDS; ++i)
  {
    digitalWrite(TRACK_LED_PIN[i], i == index ? HIGH : LOW);
  }
}

void updateLayerLeds()
{
  // Capa encendida: LED fijo.
  // Capa una octava arriba: parpadeo lento.
  const bool blinkPhase = ((millis() / 350UL) & 1U) != 0;

  for (uint8_t i = 0; i < NUM_LAYERS; ++i)
  {
    bool ledOn = layerEnabled[i];
    if (ledOn && layerOctave[i])
    {
      ledOn = blinkPhase;
    }
    digitalWrite(LAYER_LED_PIN[i], ledOn ? HIGH : LOW);
  }
}

// -----------------------------------------------------------------------------
// Botones
// -----------------------------------------------------------------------------

void updateButton(ButtonState &button, bool rawPressed)
{
  const unsigned long now = millis();

  if (rawPressed != button.rawPressed)
  {
    button.rawPressed = rawPressed;
    button.rawChangedAt = now;
  }

  if ((now - button.rawChangedAt) < DEBOUNCE_MS)
  {
    return;
  }

  if (button.stablePressed != button.rawPressed)
  {
    button.stablePressed = button.rawPressed;

    if (button.stablePressed)
    {
      button.pressedAt = now;
      button.holdHandled = false;
    }
  }
}

bool buttonJustReleased(ButtonState &button, bool &previousStable)
{
  const bool released = previousStable && !button.stablePressed;
  previousStable = button.stablePressed;
  return released;
}

void toggleLayer(uint8_t index)
{
  layerEnabled[index] = !layerEnabled[index];
  layerTargetQ8[index] = layerEnabled[index] ? (255L << 8) : 0;
  setLayerTargetNote(index);

  Serial.print("DBG layer");
  Serial.print(index + 1);
  Serial.print(" toggle -> ");
  Serial.println(layerEnabled[index] ? "ON" : "OFF");
}

void activateLayerOctave(uint8_t index)
{
  // Hold no genera además el tap.
  // Activa la capa y alterna su registro.
  layerEnabled[index] = true;
  layerOctave[index] = !layerOctave[index];
  layerTargetQ8[index] = (255L << 8);
  setLayerTargetNote(index);

  Serial.print("DBG layer");
  Serial.print(index + 1);
  Serial.print(" octave -> ");
  Serial.println(layerOctave[index] ? "UP" : "BASE");
}

void resetCloud()
{
  for (uint8_t i = 0; i < NUM_LAYERS; ++i)
  {
    layerEnabled[i] = false;
    layerOctave[i] = false;
    layerTargetQ8[i] = 0;
    layerBreathAt[i] = 0;
    layerBreathInterval[i] = (uint16_t)(4800 + (i * 700));
    layerNoteChangeAt[i] = 0;
    layerMicroDetune[i] = 0.0f;
    layerMicroDetuneTarget[i] = 0.0f;
    setLayerTargetNote(i);
  }

  // Volver a una tónica tenue en vez de dejar un corte completamente vacío.
  layerEnabled[0] = true;
  layerTargetQ8[0] = (120L << 8);
  setLayerTargetNote(0);

  Serial.println("DBG reset cloud");
}

void handleNoteButtons()
{
  static bool previousStable[NUM_LAYERS] = {false, false, false, false};
  const unsigned long now = millis();

  for (uint8_t i = 0; i < NUM_LAYERS; ++i)
  {
    updateButton(noteButton[i], uiNotePressed[i]);

    if (noteButton[i].stablePressed &&
        !noteButton[i].holdHandled &&
        (now - noteButton[i].pressedAt >= HOLD_MS))
    {
      noteButton[i].holdHandled = true;
      activateLayerOctave(i);
    }

    if (buttonJustReleased(noteButton[i], previousStable[i]))
    {
      if (!noteButton[i].holdHandled)
      {
        toggleLayer(i);
      }
    }
  }
}

void applyTextureMode()
{
  switch (textureMode)
  {
  case TEXTURE_CALM:
    sineMix = 245;
    colorMix = 200;
    noiseMix = 0;
    break;

  case TEXTURE_WIND:
    sineMix = 220;
    colorMix = 180;
    noiseMix = 20;
    break;

  case TEXTURE_CRYSTAL:
    sineMix = 225;
    colorMix = 190;
    noiseMix = 10;
    break;
  }

  setTrackLed((uint8_t)textureMode);
  for (uint8_t i = 0; i < NUM_LAYERS; ++i)
  {
    setLayerTargetNote(i);
  }

  setDelayParamsForTrack((uint8_t)textureMode);

  Serial.print("DBG climate changed -> ");
  Serial.print(getTextureName(textureMode));
  Serial.print(" / scale ");
  Serial.print(getScaleName(getScaleMode()));
  Serial.print(" / delayTime=");
  Serial.print(delayTime);
  Serial.print(" delayMix=");
  Serial.print(delayMix);
  Serial.print(" delayFeedback=");
  Serial.println(delayFeedback);
}

void handleTrackButton()
{
  static bool previousStable = false;
  const unsigned long now = millis();

  updateButton(trackButton, uiTrackPressed);

  if (trackButton.stablePressed &&
      !trackButton.holdHandled &&
      (now - trackButton.pressedAt >= HOLD_MS))
  {
    trackButton.holdHandled = true;
    resetCloud();
  }

  if (buttonJustReleased(trackButton, previousStable))
  {
    if (!trackButton.holdHandled)
    {
      textureMode = (TextureMode)(((uint8_t)textureMode + 1) % NUM_TRACK_LEDS);
      applyTextureMode();
    }
  }
}

void handleScaleButton()
{
#if BODOQUEV1
  static bool previousStable = false;

  updateButton(scaleButton, uiScalePressed);

  if (buttonJustReleased(scaleButton, previousStable))
  {
    scaleMode = (ScaleMode)(((uint8_t)scaleMode + 1U) % SCALE_MODE_COUNT);

    for (uint8_t i = 0; i < NUM_LAYERS; ++i)
    {
      setLayerTargetNote(i);
    }

    Serial.print("DBG scale changed -> ");
    Serial.println(getScaleName(getScaleMode()));
  }
#endif
}

void registerTempoTap()
{
  const unsigned long now = millis();

  if (previousTempoTap != 0)
  {
    const unsigned long interval = now - previousTempoTap;

    if (interval >= 180 && interval <= 4000)
    {
      // Una respiración completa cada ocho pulsos marcados.
      filterBreathHz = 1000.0f / ((float)interval * 8.0f);
      filterBreathHz = constrain(filterBreathHz, 0.03f, 0.55f);
      filterLFO.setFreq(filterBreathHz);
    }
  }

  previousTempoTap = now;
}

void startGrainBurst()
{
  grainBurstActive = true;
  grainsRemaining = 12;
  grainClock.start(rand(40, 140));
  Serial.println("DBG spark burst start");
}

void triggerGrain()
{
  const uint8_t voice = nextGrainVoice;
  nextGrainVoice = (nextGrainVoice + 1) % NUM_LAYERS;

  const uint8_t octave = (rand(2) == 0) ? 0 : 1;
  const uint8_t degree = (uint8_t)rand(7);
  const float randomCents = ((int)rand(21) - 10) / 100.0f;
  const float note = getScaleNote(degree, octave, getScaleMode()) + randomCents;

  grainOsc[voice].setPhase(0);
  grainOsc[voice].setFreq(mtof(note));
  grainLevelQ8[voice] = ((int32_t)rand(60, 150)) << 8;
}

void updateGrainBurst()
{
  if (grainBurstActive && grainClock.ready())
  {
    triggerGrain();

    if (--grainsRemaining == 0)
    {
      grainBurstActive = false;
    }
    else
    {
      grainClock.start(rand(90, 320));
    }
  }

  for (uint8_t i = 0; i < NUM_LAYERS; ++i)
  {
    // Caída corta pero no percusiva.
    grainLevelQ8[i] -= grainLevelQ8[i] >> 6;
    if (grainLevelQ8[i] < 12)
    {
      grainLevelQ8[i] = 0;
    }
  }
}

void handleTempoButton()
{
  static bool previousStable = false;
  const unsigned long now = millis();

  updateButton(tempoButton, uiTempoPressed);

  if (tempoButton.stablePressed &&
      !tempoButton.holdHandled &&
      (now - tempoButton.pressedAt >= HOLD_MS))
  {
    tempoButton.holdHandled = true;
    startGrainBurst();
  }

  if (buttonJustReleased(tempoButton, previousStable))
  {
    if (!tempoButton.holdHandled)
    {
      registerTempoTap();
    }
  }
}

// -----------------------------------------------------------------------------
// Síntesis
// -----------------------------------------------------------------------------

void updateLayerLevels()
{
  const unsigned long now = millis();

  for (uint8_t i = 0; i < NUM_LAYERS; ++i)
  {
    if (layerEnabled[i] && (now - layerBreathAt[i]) >= layerBreathInterval[i])
    {
      layerBreathAt[i] = now;
      layerBreathInterval[i] = (uint16_t)(rand(4000, 9000));

      int32_t desiredLevel = (int32_t)rand(120, 255) << 8;
      if (layerOctave[i])
      {
        desiredLevel = (desiredLevel * 3) / 4;
      }

      if (textureMode == TEXTURE_CALM)
      {
        desiredLevel = (desiredLevel * 7) / 8;
      }
      else if (textureMode == TEXTURE_CRYSTAL)
      {
        desiredLevel = (desiredLevel * 9) / 10;
      }

      layerTargetQ8[i] = desiredLevel;
    }
    else if (!layerEnabled[i] && layerLevelQ8[i] > 1024)
    {
      layerTargetQ8[i] = 0;
    }

    const int32_t difference = layerTargetQ8[i] - layerLevelQ8[i];
    if (difference == 0)
    {
      continue;
    }

    // Aproximadamente varios segundos de entrada/salida.
    int32_t step = difference >> 9;

    if (step == 0)
    {
      step = difference > 0 ? 1 : -1;
    }

    layerLevelQ8[i] += step;
  }
}

void updateAutonomousDrift()
{
  const unsigned long now = millis();

  if (autoDriftClock.ready())
  {
    for (uint8_t i = 0; i < NUM_LAYERS; ++i)
    {
      autoDriftTarget[i] = ((int)rand(141) - 70) / 1000.0f;
      layerMicroDetuneTarget[i] = ((float)((int)rand(25) - 12)) / 200.0f;
    }
    autoDriftClock.start(rand(15000, 32000));
  }

  for (uint8_t i = 0; i < NUM_LAYERS; ++i)
  {
    autoDriftCurrent[i] +=
        (autoDriftTarget[i] - autoDriftCurrent[i]) * 0.0005f;

    layerMicroDetune[i] +=
        (layerMicroDetuneTarget[i] - layerMicroDetune[i]) * 0.001f;

    if ((now - layerNoteChangeAt[i]) > (7000UL + (uint32_t)(i * 1200UL)))
    {
      layerNoteChangeAt[i] = now;
      layerDegreeIndex[i] = (uint8_t)((layerDegreeIndex[i] + (uint8_t)rand(1, 3)) % 7);
      setLayerTargetNote(i);
    }
  }
}

void updateFrequenciesAndFilter()
{
  const float rawPot2 = (float)pot2 / POT_MAX;
  const float instability = constrain(rawPot2 * 0.7f + rawPot2 * rawPot2 * 0.3f, 0.0f, 1.0f);

  for (uint8_t i = 0; i < NUM_LAYERS; ++i)
  {
    const int16_t driftSample = driftLFO[i].next();

    float driftDepthSemitones = 0.001f + instability * 0.012f;
    float autonomousDepth = 0.0f;

    if (textureMode == TEXTURE_WIND)
    {
      driftDepthSemitones += 0.012f;
      autonomousDepth = 0.06f + instability * 0.08f;
    }
    else if (textureMode == TEXTURE_CRYSTAL)
    {
      driftDepthSemitones *= 0.6f;
      autonomousDepth = 0.03f;
    }
    else
    {
      autonomousDepth = 0.035f + instability * 0.03f;
    }

    const float drift = ((float)driftSample / 128.0f) * driftDepthSemitones;
    const float octave = layerOctave[i] ? 12.0f : 0.0f;

    layerNoteCurrent[i] += (layerNoteTarget[i] - layerNoteCurrent[i]) * (0.002f + instability * 0.002f);

    const float autonomous = autoDriftCurrent[i] * autonomousDepth;
    const float microDetune = layerMicroDetune[i];
    const float note = layerNoteCurrent[i] + octave + drift + autonomous + microDetune;

    const float baseFrequency = mtof(note);
    // Serial.println(baseFrequency);

    float colorInterval = 0.0f;
    if (textureMode == TEXTURE_CRYSTAL)
    {
      colorInterval = 12.0f;
    }

    const float direction = (i & 1U) ? 1.0f : -1.0f;
    const float detune = direction * instability * 0.06f;

    sineOsc[i].setFreq(baseFrequency);
    colorOsc[i].setFreq(mtof(note + colorInterval + detune));
  }

  const int16_t lfo = filterLFO.next();

  const float rawPot1 = (float)pot1 / POT_MAX;
  const float cutoffCurve = rawPot1 * rawPot1; // hace la curva exponencial
  const uint16_t cutoffBase =
      (uint16_t)constrain(filterCutoffMinHz + cutoffCurve * (filterCutoffMaxHz - filterCutoffMinHz),
                          filterCutoffMinHz,
                          filterCutoffMaxHz);

  const float cutoffRatio = (cutoffBase - filterCutoffMinHz) /
                            (filterCutoffMaxHz - filterCutoffMinHz);
  int32_t modulationDepth = (int32_t)constrain(
      filterModulationMin +
          (int32_t)(cutoffRatio * (float)(filterModulationMax - filterModulationMin)) +
          (int32_t)(instability * filterModulationInstabilityGain),
      filterModulationMin,
      filterModulationMax);

  if (textureMode == TEXTURE_WIND)
  {
    modulationDepth = (int32_t)((float)modulationDepth * filterModulationWindScale);
  }
  else if (textureMode == TEXTURE_CRYSTAL)
  {
    modulationDepth = (int32_t)((float)modulationDepth * filterModulationCrystalScale);
  }

  int32_t cutoff =
      (int32_t)cutoffBase + ((int32_t)lfo * modulationDepth) / 128;

  cutoff = constrain(cutoff, filterCutoffMinHz, filterCutoffMaxHz);
  masterLPF.setCentreFreq((uint16_t)cutoff);

  const uint8_t resonance =
      (uint8_t)constrain(70 + (int)(instability * 35.0f), 70, 115);
  masterLPF.setResonance((Q0n8)resonance);

//---update gain
#if BODOQUEV1
  //  int outputDrive = (float)map(pot3, 0, 16304, 0, 200)/100;
  updateOutputGain(pot3);
  // Serial.println(outputDrive);
#endif
}

void readPots()
{
  // ADC reads happen on core 1. Core 0 only copies the latest snapshot.
  pot1 = uiPot1;
  pot2 = uiPot2;
  pot3 = uiPot3;
}

#if DESTRUKTOR1
// una truchada para mutear el audio si no hay jack conectado
void detectOutputJack()
{
  const int average = uiJackAverage;
  const int NO_JACK_HIGH = 300;
  const int NO_JACK_LOW = 100;
  const unsigned long MUTE_MS = 5000UL;

  static unsigned long muteExpire = 0;
  const unsigned long now = millis();

  const bool readingNoJack =
      average >= NO_JACK_HIGH || average <= NO_JACK_LOW;

  if (readingNoJack)
  {
    muteExpire = now + MUTE_MS;
  }

  const bool muted = muteExpire != 0 && now < muteExpire;
  digitalWrite(MUTE_SPEAKER_PIN, muted ? LOW : HIGH);
}
#endif
// -----------------------------------------------------------------------------
// Arduino / Mozzi
// -----------------------------------------------------------------------------

void setup()
{
  Serial.begin(115200);
  pinMode(23, OUTPUT);
  digitalWrite(23, HIGH);

  for (uint8_t i = 0; i < NUM_LAYERS; ++i)
  {
    pinMode(LAYER_LED_PIN[i], OUTPUT);
    pinMode(NOTE_BUTTON_PIN[i], INPUT_PULLUP);
  }

  for (uint8_t i = 0; i < NUM_TRACK_LEDS; ++i)
  {
    pinMode(TRACK_LED_PIN[i], OUTPUT);
  }

  pinMode(TRACK_BUTTON_PIN, INPUT_PULLUP);
  pinMode(TEMPO_BUTTON_PIN, INPUT_PULLUP);
#if BODOQUEV1
  pinMode(SCALE_BUTTON_PIN, INPUT_PULLUP);
#endif

  pinMode(POT_PIN[0], INPUT);
  pinMode(POT_PIN[1], INPUT);
#if DESTRUKTOR1
  pinMode(JACK_SENSOR_PIN, INPUT);
#endif

  pinMode(MUTE_SPEAKER_PIN, OUTPUT);

  randSeed(analogRead(A0) + analogRead(A1) + analogRead(A2));
  scaleMode = (ScaleMode)rand(SCALE_MODE_COUNT);

  // Velocidades distintas y deliberadamente no relacionadas.
  driftLFO[0].setFreq(0.031f);
  driftLFO[1].setFreq(0.043f);
  driftLFO[2].setFreq(0.057f);
  driftLFO[3].setFreq(0.071f);

  filterLFO.setFreq(filterBreathHz);
  noiseOsc.setFreq(2.0f);
  autoDriftClock.start(4000);

  masterLPF.setCentreFreq(700);
  masterLPF.setResonance((Q0n8)85);

  applyTextureMode();
  resetCloud();

  startMozzi(MOZZI_CONTROL_RATE);

  // Initialize delay module with current global parameters
  {
    uint32_t cells = (uint32_t)delayTime * (uint32_t)MOZZI_AUDIO_RATE / 1000U;
    if (cells < 1U)
      cells = 1U;
    if (cells > DELAY_CELLS)
      cells = DELAY_CELLS;
    aDelay.setDelayTimeCells((uint16_t)cells);
    aDelay.setFeedbackLevel((int8_t)delayFeedback);
  }
}

int16_t applyOutputGainAndSaturation(int16_t sample)
{
  // 1) Drive: ganancia de entrada a la etapa
  int32_t x = ((int32_t)sample * outDriveQ10) >> 10;

  // 2) Soft-clip por tramos (tipo clipper de diodos).
  //    Cuanto más entra, más se comprime la pendiente.
  bool neg = false;
  if (x < 0)
  {
    x = -x;
    neg = true;
  }

  if (x < OUT_KNEE_1)
  {
    // Zona limpia: pasa tal cual
  }
  else if (x < OUT_KNEE_2)
  {
    // Pendiente 1/2
    x = OUT_KNEE_1 + ((x - OUT_KNEE_1) >> 1);
  }
  else if (x < OUT_KNEE_3)
  {
    // Pendiente 1/4
    x = OUT_KNEE_1 + ((OUT_KNEE_2 - OUT_KNEE_1) >> 1) + ((x - OUT_KNEE_2) >> 2);
  }
  else
  {
    // Pendiente 1/8 + techo duro
    int32_t v = OUT_KNEE_1 + ((OUT_KNEE_2 - OUT_KNEE_1) >> 1) + ((OUT_KNEE_3 - OUT_KNEE_2) >> 2) + ((x - OUT_KNEE_3) >> 3);
    if (v > 4095)
      v = 4095;
    x = v;
  }

  if (neg)
    x = -x;

  // 3) Volumen de salida
  int32_t out = (x * outVolumeQ10) >> 10;
  // 4) Clamp de seguridad
  return (int16_t)constrain(out, -4095, 4095);
}

void updateControl()
{
  readPots();

  handleNoteButtons();
  handleTrackButton();
  handleTempoButton();
  handleScaleButton();

  updateLayerLevels();
  updateAutonomousDrift();
  updateGrainBurst();
  updateFrequenciesAndFilter();
  updateLayerLeds();

  oledPot1 = pot1;
  oledPot2 = pot2;
  oledPot3 = pot3;
  oledLayersMask = (layerEnabled[0] ? 1U : 0U) |
                   (layerEnabled[1] ? 2U : 0U) |
                   (layerEnabled[2] ? 4U : 0U) |
                   (layerEnabled[3] ? 8U : 0U);
  oledTextureMode = (uint8_t)textureMode;

  // Apply live delay parameter changes (globals can be tweaked elsewhere)
  {
    // Map pot2 (0..16384) to [0..1] and scale delay mix/feedback between
    // base (per-track) and configured maxima.
    const float pot2norm = (float)pot2 / POT_MAX;

    // Compute effective mix and feedback from base -> max using pot2
    float effectiveMix = baseDelayMix + (delayMixMax - baseDelayMix) * pot2norm;
    effectiveMix = constrain(effectiveMix, 0.0f, delayMixMax);

    int effectiveFeedback = baseDelayFeedback + (int)((float)(delayFeedbackMax - baseDelayFeedback) * pot2norm + 0.5f);
    // Clamp to signed 8-bit range expected by AudioDelayFeedback
    effectiveFeedback = constrain(effectiveFeedback, -128, 127);

    // Update globals used elsewhere
    delayMix = effectiveMix;
    delayFeedback = effectiveFeedback;

    uint32_t cells = (uint32_t)delayTime * (uint32_t)MOZZI_AUDIO_RATE / 1000U;
    if (cells < 1U)
      cells = 1U;
    if (cells > DELAY_CELLS)
      cells = DELAY_CELLS;
    aDelay.setDelayTimeCells((uint16_t)cells);
    aDelay.setFeedbackLevel((int8_t)delayFeedback);
  }

#if DESTRUKTOR1
  detectOutputJack();
#endif

#if BODOQUEV1
  digitalWrite(MUTE_SPEAKER_PIN, LOW);
#endif
}

AudioOutput updateAudio()
{
  int32_t tonalMix = 0;
  int32_t totalLevel = 0;

  for (uint8_t i = 0; i < NUM_LAYERS; ++i)
  {
    const int16_t level = (int16_t)(layerLevelQ8[i] >> 8);

    if (level <= 0)
    {
      continue;
    }

    const int16_t sineSample = sineOsc[i].next();
    const int16_t colorSample = colorOsc[i].next();

    int32_t voice =
        ((int32_t)sineSample * sineMix +
         (int32_t)colorSample * colorMix) >>
        8;

    tonalMix += (voice * level) >> 8;
    totalLevel += level;
  }

  // Compensación suave: conserva presencia al sumar capas, sin bajar tanto
  // como la normalización anterior.
  if (totalLevel > 380)
  {
    tonalMix = (tonalMix * 380L) / totalLevel;
  }
  tonalMix <<= 1;

  // El ruido queda deliberadamente muy por debajo de las notas.
  const int16_t noise =
      (int16_t)(((int32_t)noiseOsc.next() * noiseMix *
                 (32 + (pot2 >> 3))) >>
                17);

  int32_t grainMix = 0;
  for (uint8_t i = 0; i < NUM_LAYERS; ++i)
  {
    const int16_t level = grainLevelQ8[i] >> 8;
    grainMix += ((int32_t)grainOsc[i].next() * level) >> 8;
  }

  int32_t raw = tonalMix + noise + grainMix;
  raw = constrain(raw, -4095, 4095);

  float dm_f = delayMix;
  if (dm_f < 0.0f)
    dm_f = 0.0f;
  if (dm_f > 1.0f)
    dm_f = 1.0f;
  int32_t scale = (int32_t)(dm_f * 256.0f + 0.5f); // 0..256

  int16_t filtered;
  int32_t mixedClamped;

  if (delayAfterFilter)
  {
    // Primero aplicar el filtro maestro, luego el delay.
    filtered = masterLPF.next((int16_t)raw);
    int32_t delayInput = ((int32_t)filtered) >> 1;
    int32_t delayed = aDelay.next(delayInput);
    int32_t delayedScaled = (delayed * scale) >> 8;
    int32_t mixed = (int32_t)filtered + delayedScaled;
    mixedClamped = constrain(mixed, -4095, 4095);
  }
  else
  {
    // Primero delay, luego filtro sobre la mezcla.
    int32_t delayInput = raw >> 1;
    int32_t delayed = aDelay.next(delayInput);
    int32_t delayedScaled = (delayed * scale) >> 8;
    int32_t mixed = raw + delayedScaled;
    int32_t mixedLimited = constrain(mixed, -4095, 4095);
    filtered = masterLPF.next((int16_t)mixedLimited);
    mixedClamped = filtered;
  }

  int16_t finalSample = applyOutputGainAndSaturation((int16_t)mixedClamped);

#if DESTRUKTOR1
  return MonoOutput::from8Bit((int16_t)(finalSample >> 1)).clip();
#endif
#if BODOQUEV1
  return StereoOutput::from8Bit(finalSample >> 6, finalSample >> 6); // return an int signal centred around 0
#endif
}

void loop()
{
  audioHook();
}

// RP2040 core 1: the OLED must never be touched by Mozzi's core 0.
void sampleUiOnCore1()
{
  // Raw ADC values are represented in the historical 0..16384 control range.
  uiPot1 = (uint16_t)analogRead(POT_PIN[0]) << 4;
  uiPot2 = (uint16_t)analogRead(POT_PIN[1]) << 4;
#if BODOQUEV1
  uiPot3 = (uint16_t)analogRead(POT_PIN[2]) << 4;
#endif

  for (uint8_t i = 0; i < NUM_LAYERS; ++i)
  {
    uiNotePressed[i] = digitalRead(NOTE_BUTTON_PIN[i]) == LOW;
  }
  uiTrackPressed = digitalRead(TRACK_BUTTON_PIN) == LOW;
  uiTempoPressed = digitalRead(TEMPO_BUTTON_PIN) == LOW;
#if BODOQUEV1
  uiScalePressed = digitalRead(SCALE_BUTTON_PIN) == LOW;
#endif

#if DESTRUKTOR1
  long sum = 0;
  for (uint8_t i = 0; i < 6; ++i)
  {
    sum += analogRead(JACK_SENSOR_PIN);
  }
  uiJackAverage = (int)(sum / 6);
#endif
}

void setup1()
{
#if BODOQUEV1
  oledAnimationReady = initOLEDAnimation();
#endif
}

void loop1()
{
  static unsigned long lastOLEDUpdate = 0;
  static unsigned long lastDebugUpdate = 0;
  const unsigned long now = millis();

  sampleUiOnCore1();

#if BODOQUEV1
  if (!oledAnimationReady || (now - lastOLEDUpdate < 50UL))
  {
    // delay(1);
    return;
  }

  lastOLEDUpdate = now;
#endif

  if (now - lastDebugUpdate >= 1800UL)
  {
    lastDebugUpdate = now;
    printDebugState();
  }

  const uint16_t p1 = oledPot1;
  const uint16_t p2 = oledPot2;
  const uint16_t p3 = oledPot3;
  const uint8_t layersMask = oledLayersMask;
  const TextureMode mode = (TextureMode)oledTextureMode;
  const bool enabledLayers[4] = {
      (layersMask & 1U) != 0,
      (layersMask & 2U) != 0,
      (layersMask & 4U) != 0,
      (layersMask & 8U) != 0};

  renderSynthAnimation(p1, p2, p3, enabledLayers, mode);
}
