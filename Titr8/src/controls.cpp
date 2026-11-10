#include "mozziConfig.h"
#include <MozziHeadersOnly.h>
#include <EventDelay.h>

#include "controls.h"
#include "input.h"
#include "tracks.h"
#include "sequencer.h"
#include "voices.h"
#include "ui.h"

// Estado de cada boton de nota
static bool rolling[NUM_NOTES] = {false};       // roll de drums sonando
static bool recordingRoll[NUM_NOTES] = {false}; // el roll se graba en cada paso
static uint8_t holdStartStep[NUM_NOTES] = {0};  // paso donde empezo la pulsacion

static EventDelay rollDelay;

// Randomizer del arranque: cambia el sonido cada 150 ms hasta que se toca una nota
static bool randomizing = true;
static EventDelay randomizeClock;

static bool delayOn = false;

// Banco de notas: los botones I-IV tocan los grados bank*4 .. bank*4+3 de los
// 12 grados de la escala. A baja de banco, B sube; ambos dan la vuelta.
#define NUM_NOTE_BANKS (NUM_SCALE_DEGREES / NUM_NOTES)
static uint8_t noteBank = 0;

// ======================= Combos de teclas =======================
// Mientras "modifier" esta apretado, apretar "trigger" ejecuta "action".
// La pulsacion de ambos queda consumida: no hacen su accion normal.
// Ojo: A y B ya usan tap, doble tap y hold. C solo usa tap, asi que es el
// mejor candidato a modificador.

struct Combo
{
  Button *modifier;
  Button *trigger;
  void (*action)();
};

static const Combo COMBOS[] = {
    // Ejemplo: {&buttonC, &noteButtons[0], someAction},
    {nullptr, nullptr, nullptr} // fin de la tabla
};

static void handleCombos()
{
  for (const Combo *c = COMBOS; c->action != nullptr; c++)
  {
    if (c->modifier->isDown() && c->trigger->wasPressed())
    {
      c->action();
      c->trigger->consume();
      c->modifier->consume();
    }
  }
}

// ======================= Helpers =======================

// Si no hay notas grabadas en BASS ni PAD, elige una escala nueva
static void randomizeScaleIfEmpty()
{
  if (tracksMelodicEmpty())
    voicesRandomizeScale();
}

// Grado de la escala (0..11) que toca un boton de nota en el banco actual
static uint8_t buttonDegree(uint8_t button)
{
  return noteBank * NUM_NOTES + button;
}

// ======================= Botones de nota =======================

static void handleNoteEdges(unsigned long now)
{
  for (uint8_t i = 0; i < NUM_NOTES; i++)
  {
    if (noteButtons[i].wasPressed())
      holdStartStep[i] = sequencerNearestStep(now);
    if (noteButtons[i].wasReleased())
    {
      recordingRoll[i] = false;
      rolling[i] = false;
    }
  }
}

// Tap: toca (si cae cerca del paso que sono) y graba en el paso mas cercano
static void playNoteButton(uint8_t button, unsigned long eventTime)
{
  bool freePlay = sequencerFreePlay();
  uint8_t nearestStep = sequencerNearestStep(eventTime);
  bool shouldPlayLive = freePlay || (nearestStep == sequencerJustFiredStep());

  if (currentTrack == TRACK_DRUM)
  {
    if (shouldPlayLive)
      triggerDrum(button);
    if (!freePlay)
      tracksRecordDrum(button, nearestStep);
  }
  else // BASS, PAD, LEAD
  {
    if (shouldPlayLive)
      tracksTriggerNote(currentTrack, buttonDegree(button), false);
    if (!freePlay)
      tracksRecordNote(currentTrack, nearestStep, buttonDegree(button), false);
  }
}

// Hold: en DRUM hace roll, en BASS/PAD/LEAD toca y graba la nota una octava arriba
static void handleNoteHold(uint8_t button)
{
  if (currentTrack == TRACK_DRUM)
  {
    rolling[button] = true;
    recordingRoll[button] = true;
    triggerDrum(button); // suena apenas empieza el roll
    return;
  }

  uint8_t degree = buttonDegree(button);
  if (!sequencerFreePlay())
    tracksRecordNote(currentTrack, holdStartStep[button], degree, true);

  tracksTriggerNote(currentTrack, degree, true);
}

static void handleNoteHolds()
{
  for (uint8_t i = 0; i < NUM_NOTES; i++)
  {
    if (noteButtons[i].takeHold())
      handleNoteHold(i);
  }
}

static void handleNotePresses()
{
  unsigned long now = millis();
  for (uint8_t i = 0; i < NUM_NOTES; i++)
  {
    if (noteButtons[i].wasPressed())
      playNoteButton(i, now);
  }
}

static void handleRolls()
{
  for (uint8_t i = 0; i < NUM_NOTES; i++)
  {
    if (!rolling[i])
      continue;
    if (rollDelay.ready())
    {
      if (currentTrack == TRACK_DRUM)
        triggerDrum(i);
      rollDelay.start(sequencerStepMs());
    }
  }
}

void controlsRecordRolls(uint8_t step)
{
  // El crash (boton 4) no se graba en roll
  for (uint8_t drum = DRUM_KICK; drum <= DRUM_HAT; drum++)
  {
    if (recordingRoll[drum])
      tracksRecordDrum(drum, step);
  }
}

// ======================= Botones A, B y C =======================
// A: tap = transponer a la izquierda, doble tap = instrumento siguiente, hold = borrar track
// B: tap = transponer a la derecha,   doble tap = instrumento anterior,  hold = delay
// C: tap tempo
//
// El tap transpone apenas se suelta el boton (sin esperar a ver si viene un
// segundo tap). Si llega el segundo tap, se deshace esa transposicion y se
// cambia de instrumento.

static const unsigned long DOUBLE_TAP_MS = 350; // entre la 1ra y la 2da soltada

enum FunctionEvent : uint8_t
{
  EVENT_NONE,
  EVENT_TAP,
  EVENT_DOUBLE_TAP,
  EVENT_HOLD
};

struct TapState
{
  bool armed = false; // hubo un tap simple que puede volverse doble
  unsigned long lastTapAt = 0;
};

static TapState tapA, tapB;

static FunctionEvent readFunctionButton(Button &button, TapState &tap, unsigned long now)
{
  if (button.takeHold())
  {
    tap.armed = false; // un hold corta la secuencia de taps
    return EVENT_HOLD;
  }
  if (!button.wasReleased() || button.holdUsed())
    return EVENT_NONE;
  if (now - button.pressTime() >= 1000)
    return EVENT_NONE; // soltada de un boton que ya estaba apretado al encender

  if (tap.armed && now - tap.lastTapAt < DOUBLE_TAP_MS)
  {
    tap.armed = false; // un tercer tap vuelve a ser simple
    return EVENT_DOUBLE_TAP;
  }
  tap.armed = true;
  tap.lastTapAt = now;
  return EVENT_TAP;
}

static void shiftNoteBank(int8_t direction)
{
  noteBank = (noteBank + NUM_NOTE_BANKS + direction) % NUM_NOTE_BANKS;
  uiSetNoteBank(noteBank);
}

static void shiftTrack(int8_t direction)
{
  currentTrack = (TrackId)((currentTrack + TRACK_COUNT + direction) % TRACK_COUNT);
  uiShowTrack(currentTrack);
}

// Hold de A: borra el track actual (en BASS/PAD/LEAD tambien randomiza su sonido)
static void clearCurrentTrack()
{
  tracksClear(currentTrack);
  if (currentTrack == TRACK_BASS)
  {
    voicesRandomizeBass();
    randomizeScaleIfEmpty();
  }
  else if (currentTrack == TRACK_PAD)
  {
    voicesRandomizePad();
    randomizeScaleIfEmpty();
  }
  else if (currentTrack == TRACK_LEAD)
  {
    voicesRandomizeLead();
    randomizeScaleIfEmpty();
  }
}

static void toggleDelay()
{
  delayOn = !delayOn;
  voicesSetDelay(delayOn);
}

static void handleButtonA(unsigned long now)
{
  switch (readFunctionButton(buttonA, tapA, now))
  {
  case EVENT_TAP:
    shiftNoteBank(-1);
    break;
  case EVENT_DOUBLE_TAP:
    shiftNoteBank(+1); // deshace la transposicion del primer tap
    shiftTrack(+1);
    break;
  case EVENT_HOLD:
    clearCurrentTrack();
    break;
  default:
    break;
  }
}

static void handleButtonB(unsigned long now)
{
  switch (readFunctionButton(buttonB, tapB, now))
  {
  case EVENT_TAP:
    shiftNoteBank(+1);
    break;
  case EVENT_DOUBLE_TAP:
    shiftNoteBank(-1); // deshace la transposicion del primer tap
    shiftTrack(-1);
    break;
  case EVENT_HOLD:
    toggleDelay();
    break;
  default:
    break;
  }
  uiSetStarLed(delayOn);
}

static void handleButtonC()
{
  if (!buttonC.wasReleased())
    return;

  unsigned long now = millis();
  if (now - buttonC.pressTime() >= 1000)
    return; // fue un hold, no un tap

  sequencerTap(now);
}

// ======================= Randomizer del arranque =======================

static void updateRandomizer()
{
  if (randomizing && anyNoteButtonActive())
    randomizing = false;

  if (!randomizing || !randomizeClock.ready())
    return;

  voicesRandomizeBass();
  voicesRandomizePad();
  voicesRandomizeLead();
  randomizeScaleIfEmpty();
  randomizeClock.start(150);
}

// ======================= Loop de control =======================

void controlsUpdate(unsigned long now)
{
  handleNoteEdges(now);
  voicesSetOutputGain(potValue(2));
  updateRandomizer();
  handleCombos();
  handleNoteHolds();
  handleButtonA(now);
  handleButtonB(now);
  handleButtonC();
  sequencerUpdateTapTimeout();
  handleNotePresses();
  handleRolls();
}
