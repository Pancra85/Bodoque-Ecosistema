// Titr8 - groovebox acida para el Bodoque V1 (basada en MaquinaAcida).
//
// Modulos:
//   input      botones (Button) y potes
//   controls   que hace cada boton, combos de teclas, randomizer del arranque
//   tracks     patrones grabados (DRUM, BASS, PAD)
//   sequencer  reloj, pasos y tap tempo
//   voices     motor de sonido (Mozzi) y mezcla
//   ui         LEDs
//   midiIO     USB MIDI (sync de reloj)
//
// TODO (heredados de MaquinaAcida):
// - Al tocar en vivo, evitar que la nota grabada vuelva a sonar en el paso siguiente.
//   Hoy no se toca en vivo si la nota cae mas cerca del paso siguiente: se graba y suena ahi.
// - Revisar el calculo de "paso mas cercano": deberia decidir entre el paso actual y el anterior.

#include "mozziConfig.h"
#include <Mozzi.h>

#include "hardwarePins.h"
#include "input.h"
#include "controls.h"
#include "tracks.h"
#include "sequencer.h"
#include "voices.h"
#include "ui.h"
#include "midiIO.h"

void setup()
{
  // GPIO23 en HIGH pone la fuente del Pico en modo PWM: menos ruido en el audio
  pinMode(23, OUTPUT);
  digitalWrite(23, HIGH);

  Serial.begin(115200);

  uiSetup();
  inputSetup();
  pinMode(MUTESPEAKER_PIN, OUTPUT);
  digitalWrite(MUTESPEAKER_PIN, LOW);

  // Botones apretados al encender: no cuentan como pulsacion
  inputCaptureBootState();

  // Opciones de arranque:
  //   boton 4 -> free play (sin secuenciador)
  //   boton 3 -> 64 pasos, boton 2 -> 32 pasos, si no 16
  bool freePlay = (digitalRead(NOTEB_PIN[3]) == LOW);
  uint8_t steps = 16;
  if (digitalRead(NOTEB_PIN[2]) == LOW)
    steps = 64;
  else if (digitalRead(NOTEB_PIN[1]) == LOW)
    steps = 32;
  sequencerSetup(steps, freePlay);

  voicesSetup();
  setupMidiIO();
  uiShowTrack(currentTrack);

  // Esperar a que se suelten los botones de nota antes de arrancar
  bool allReleased;
  do
  {
    allReleased = digitalRead(NOTEB_PIN[0]) && digitalRead(NOTEB_PIN[1]) &&
                  digitalRead(NOTEB_PIN[2]) && digitalRead(NOTEB_PIN[3]);
  } while (!allReleased);

  startMozzi(CONTROL_RATE);
}

void updateControl()
{
  updateMidiIO();

  unsigned long now = millis();
  inputUpdate(now);
  controlsUpdate(now);
  sequencerUpdate();
  uiUpdateBeatLeds();
  uiUpdateTrackLeds(now);
  voicesUpdateControl(potValue(0), potValue(1));
}

AudioOutput updateAudio()
{
  int16_t sample = voicesNextSample();
  return StereoOutput::from8Bit(sample >> 1, sample >> 1).clip();
}

void loop()
{
  audioHook();
}
