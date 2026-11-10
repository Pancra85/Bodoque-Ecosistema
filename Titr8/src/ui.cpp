#include "ui.h"
#include "hardwarePins.h"
#include "sequencer.h"

static void setAllSeqLeds(bool state)
{
  for (uint8_t i = 0; i < NUM_SEQ_LEDS; i++)
    digitalWrite(SEQLED_PIN[i], state ? HIGH : LOW);
}

void uiSetup()
{
  for (uint8_t i = 0; i < NUM_SEQ_LEDS; i++)
    pinMode(SEQLED_PIN[i], OUTPUT);
  for (uint8_t i = 0; i < NUM_TRACK_LEDS; i++)
    pinMode(TRACKLED_PIN[i], OUTPUT);
  pinMode(LEDESTRELLA_PIN, OUTPUT);
}

// Periodo de parpadeo del LED del track segun el banco de notas (0 = fijo)
static const uint16_t BANK_BLINK_PERIOD_MS[] = {0, 500, 166}; // fijo, ~2 Hz, ~6 Hz
#define NUM_BANK_BLINKS (sizeof(BANK_BLINK_PERIOD_MS) / sizeof(BANK_BLINK_PERIOD_MS[0]))

static uint8_t shownTrack = 0;
static uint8_t shownBank = 0;
static unsigned long blinkStartedAt = 0; // el parpadeo arranca prendido al cambiar

void uiShowTrack(uint8_t track)
{
  shownTrack = track;
  blinkStartedAt = millis();
  uiUpdateTrackLeds(blinkStartedAt);
}

void uiSetNoteBank(uint8_t bank)
{
  shownBank = bank;
  blinkStartedAt = millis();
}

void uiUpdateTrackLeds(unsigned long now)
{
  uint16_t period = (shownBank < NUM_BANK_BLINKS) ? BANK_BLINK_PERIOD_MS[shownBank] : 0;
  bool on = (period == 0) || ((now - blinkStartedAt) % period) < (period / 2);

  for (uint8_t i = 0; i < NUM_TRACK_LEDS; i++)
    digitalWrite(TRACKLED_PIN[i], (i == shownTrack && on) ? HIGH : LOW);
}

// Prende el LED de la negra actual solo en el primer paso de cada negra
void uiUpdateBeatLeds()
{
  if (sequencerFreePlay())
  {
    setAllSeqLeds(false);
    return;
  }

  uint8_t steps = sequencerSteps();
  uint8_t stepIndex = sequencerStepIndex();
  uint8_t displayStep = (stepIndex == 0) ? (steps - 1) : (stepIndex - 1); // el paso que acaba de sonar
  uint8_t beatLed = (displayStep / 4) % 4;
  bool pulseOn = (displayStep % 4) == 0;

  setAllSeqLeds(false);
  if (pulseOn)
    digitalWrite(SEQLED_PIN[beatLed], HIGH);
}

void uiSetStarLed(bool on)
{
  digitalWrite(LEDESTRELLA_PIN, on);
}
