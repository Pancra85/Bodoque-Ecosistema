#include "input.h"

Button noteButtons[NUM_NOTES];
Button buttonA;
Button buttonB;
Button buttonC;

// A y B tienen tap, doble tap y hold. Su hold es mas largo que el de las
// notas para que un tap lento no borre el track.
static const unsigned long FUNCTION_HOLD_MS = 500;

static uint16_t pots[NUM_POTS] = {0};

// ---------------- Button ----------------

void Button::begin(uint8_t buttonPin, unsigned long buttonHoldMs)
{
  pin = buttonPin;
  holdMs = buttonHoldMs;
  pinMode(pin, INPUT_PULLUP);
}

void Button::captureBootState(bool syncState)
{
  bool bootDown = (digitalRead(pin) == LOW);
  ignoreUntilRelease = bootDown;
  if (syncState)
    down = bootDown;
}

void Button::update(unsigned long now)
{
  bool current = (digitalRead(pin) == LOW);
  pressed = false;
  released = false;

  if (current != down)
  {
    down = current;
    if (current)
    {
      if (!ignoreUntilRelease)
      {
        pressed = true;
        pressedAt = now;
        held = false;
        holdHandled = false;
      }
    }
    else
    {
      released = true;
      held = false;
      ignoreUntilRelease = false;
    }
  }
  else if (current && !held && now - pressedAt >= holdMs)
  {
    held = true;
  }
}

bool Button::takeHold()
{
  if (!held || holdHandled)
    return false;
  holdHandled = true;
  return true;
}

void Button::consume()
{
  pressed = false;
  holdHandled = true;
}

// ---------------- Botones y potes del Bodoque ----------------

void inputSetup()
{
  for (uint8_t i = 0; i < NUM_POTS; i++)
    pinMode(POT_PIN[i], INPUT);

  for (uint8_t i = 0; i < NUM_NOTES; i++)
    noteButtons[i].begin(NOTEB_PIN[i]);
  buttonA.begin(BUTTON_A_PIN, FUNCTION_HOLD_MS);
  buttonB.begin(BUTTON_B_PIN, FUNCTION_HOLD_MS);
  buttonC.begin(BUTTON_C_PIN);
}

void inputCaptureBootState()
{
  for (uint8_t i = 0; i < NUM_NOTES; i++)
    noteButtons[i].captureBootState(false);
  buttonA.captureBootState(true);
  buttonC.captureBootState(true);
}

void inputUpdate(unsigned long now)
{
  for (uint8_t i = 0; i < NUM_NOTES; i++)
    noteButtons[i].update(now);
  buttonA.update(now);
  buttonB.update(now);
  buttonC.update(now);

  for (uint8_t i = 0; i < NUM_POTS; i++)
    pots[i] = analogRead(POT_PIN[i]);
}

uint16_t potValue(uint8_t index)
{
  return pots[index];
}

bool anyNoteButtonActive()
{
  for (uint8_t i = 0; i < NUM_NOTES; i++)
  {
    if (noteButtons[i].isDown() || noteButtons[i].wasPressed())
      return true;
  }
  return false;
}
