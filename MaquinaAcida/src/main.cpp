// TODO: hay que hacer que cuando suena nextStep aca, mutee el proxima nota de secuencer asi no suena dos veces
//   Ahora lo que hice es mutear cuando apretas el boton, no toca en live, acomoda la nota y suena. Para que no repita. Antes hacia: cuando tocas PUM y al instante cuando llega el step donde grabó, PUM de nuevo
// CORREGIR!!!: Tendria que ver bien de calcular si cae en el paso actual o en el paso que sigue, eso esta hecho por IA y medio mal: Ahora que lo pienso le dije a la ia que la idea es ver si cae mas cerca del paso actual o el proximo, pero deberia haber sido si cae en el paso actual o ne el anterior.
// para el destruktor1: Agregar control de volumen manteniendo apretado el boton de arriba de TEMPO y girando pote1, pote2 tambien deberia cambiar TEMPO, y sacar el tap tempo?

#include "MozziConfigValues.h" // for named option values
#include "hardwarePins.h"

#define MOZZI_ANALOG_READ MOZZI_ANALOG_READ_NONE
#define MOZZI_AUDIO_RATE 32768
#define MOZZI_CONTROL_RATE 128

#if DESTRUKTOR1
#define MOZZI_AUDIO_MODE MOZZI_OUTPUT_PWM
#define MOZZI_AUDIO_PIN_1 PWMAUDIO1_PIN
#define MOZZI_AUDIO_PIN_2 PWMAUDIO2_PIN
#elif BODOQUEV1
#define MOZZI_I2S_PIN_BCK BCK_PIN
#define MOZZI_I2S_PIN_WS WS_PIN
#define MOZZI_I2S_PIN_DATA DATA_PIN
#define MOZZI_AUDIO_MODE MOZZI_OUTPUT_I2S_DAC
#define MOZZI_I2S_FORMAT MOZZI_I2S_FORMAT_LSBJ
#define MOZZI_AUDIO_BITS 16
#define MOZZI_AUDIO_CHANNELS MOZZI_STEREO
#endif

#include <Mozzi.h>
#include <Oscil.h>
#include <ADSR.h>
// #include <StateVariable.h>

// #include <mozzi_rand.h>
// #include <mozzi_midi.h>
// #include <EventDelay.h>
// #include <AudioDelayFeedback.h>

#include "main.h"
#include "sequencer.h"
#include "controles.h"
#include "midiIO.h"

// Variables para el LED de modo
bool modeLedOn = false;

// Variables para ignorar botones iniciales
bool initialButtonStates[NUM_NOTES] = {false};
bool initialButtonCheckDone = false;

bool randomizing = true;

void setAllLeds(bool state)
{
  for (int i = 0; i < NUM_SEQ_LEDS; i++)
  {
    digitalWrite(SEQLED_PIN[i], state ? HIGH : LOW);
  }
}

void setActiveLed(uint8_t ledIndex)
{
  setAllLeds(false);
  if (ledIndex < NUM_SEQ_LEDS)
  {
    digitalWrite(SEQLED_PIN[ledIndex], HIGH);
  }
}

void setTrackLed(uint8_t trackIndex)
{
  for (uint8_t i = 0; i < NUM_TRACKS; i++)
  {
    digitalWrite(TRACKLED_PIN[i], (i == trackIndex) ? HIGH : LOW);
  }
}

void setupButtons()
{
  for (int i = 0; i < NUM_NOTES; i++)
  {
    pinMode(buttonPins[i], INPUT_PULLUP);
  }
  pinMode(TRACKB_PIN, INPUT_PULLUP);
  pinMode(TEMPOB_PIN, INPUT_PULLUP);
#if BODOQUEV1
  pinMode(DELAYBTN_PIN, INPUT_PULLUP);
#endif
}

void readButtons()
{
  unsigned long now = millis();

  for (int i = 0; i < NUM_NOTES; i++)
  {
    bool current = (digitalRead(buttonPins[i]) == LOW);
    buttonPressed[i] = false;
    buttonReleased[i] = false;

    if (current != buttonState[i])
    {
      buttonState[i] = current;

      if (current && !initialButtonStates[i])
      {
        // Button pressed (and not initial state)
        buttonPressed[i] = true;
        buttonPressTime[i] = now;
        buttonHeld[i] = false;
        buttonHoldHandled[i] = false;
        holdStartStep[i] = getSequencerNearestStep(now);
        octaveHoldActive[i] = false;
      }
      else if (!current)
      {
        // Button released
        buttonReleased[i] = true;
        buttonHeld[i] = false;
        recordingRoll[i] = false; // Dejar de grabar roll cuando se suelta
        rolling[i] = false;       // Detener roll cuando se suelta
        octaveHoldActive[i] = false;
        if (initialButtonStates[i])
        {
          initialButtonStates[i] = false;
        }
      }
    }
    else if (current && !buttonHeld[i])
    {
      if (now - buttonPressTime[i] >= 200)
      {
        buttonHeld[i] = true;
      }
    }
  }

  bool currentTrack = (digitalRead(TRACKB_PIN) == LOW);
  trackButtonPressed = false;
  trackButtonReleased = false;
  if (currentTrack != trackButtonState)
  {
    if (currentTrack)
    {
      if (!trackButtonInitialState)
      {
        trackButtonState = true;
        trackButtonPressed = true;
        trackButtonPressTime = now;
        trackButtonHeld = false;
        trackButtonHoldHandled = false;
      }
    }
    else
    {
      trackButtonState = false;
      trackButtonReleased = true;
      trackButtonHeld = false;
      if (trackButtonInitialState)
      {
        trackButtonInitialState = false;
      }
    }
  }
  else if (currentTrack && !trackButtonHeld)
  {
    if (now - trackButtonPressTime >= 200)
    {
      trackButtonHeld = true;
    }
  }

  bool currentTempo = (digitalRead(TEMPOB_PIN) == LOW);
  tempoButtonPressed = false;
  tempoButtonReleased = false;
  if (currentTempo != tempoButtonState)
  {
    if (currentTempo)
    {
      if (!tempoButtonInitialState)
      {
        tempoButtonState = true;
        tempoButtonPressed = true;
        tempoButtonPressTime = now;
      }
    }
    else
    {
      tempoButtonState = false;
      tempoButtonReleased = true;
      if (tempoButtonInitialState)
      {
        tempoButtonInitialState = false;
      }
    }
  }
}

void readPots()
{
  pot1 = analogRead(POT_PIN[0]);
  pot2 = analogRead(POT_PIN[1]);
#if BODOQUEV1
  pot3 = analogRead(POT_PIN[2]);
#endif
}

void updateLed()
{
  if (modeLedOn)
  {
    if (modeLedTimer.ready())
    {
      modeLedOn = false;
      setAllLeds(false);
    }
    return;
  }

  if (!freePlay)
  {
    // uint8_t beatLed = (stepIndex / 4) % 4;
    // bool pulseOn = (stepIndex % 4) == 0;
    uint8_t displayStep = (stepIndex == 0) ? (steps - 1) : (stepIndex - 1);
    uint8_t beatLed = (displayStep / 4) % 4;
    bool pulseOn = (displayStep % 4) == 0;
    // Serial.println(beatLed);
    if (pulseOn)
    {
      setActiveLed(beatLed);
    }
    else
    {
      setAllLeds(false);
    }
  }
  else
  {
    setAllLeds(false);
  }
}

#if DESTRUKTOR1
// Detecta si hay jack conectado al output usando el sensor analógico.
// Regla: si la lectura sube por encima de NO_JACK_HIGH (ej. 300)
// o baja por debajo de NO_JACK_LOW (ej. 100) se considera "no jack".
// Cuando se detecta "no jack" se mantiene mute (LOW) durante MUTE_MS (10s).
void detectOutputJack()
{
  const int samples = 6;
  long sum = 0;
  for (int i = 0; i < samples; i++)
  {
    sum += analogRead(JACKSENSOR_PIN);
  }
  int avg = (int)(sum / samples);

  const int NO_JACK_HIGH = 300;
  const int NO_JACK_LOW = 100;
  const unsigned long MUTE_MS = 5000UL; // en ms

  static unsigned long muteExpire = 0;
  unsigned long now = millis();

  bool readingNoJack = (avg >= NO_JACK_HIGH) || (avg <= NO_JACK_LOW);

  if (readingNoJack)
  {
    // extender/activar temporizador de mute
    muteExpire = now + MUTE_MS;
  }
  else
  {
    // lectura dentro del rango: considerar jack presente y cancelar temporizador
    // muteExpire = 0;
  }

  bool muted = (muteExpire != 0 && now < muteExpire);
  // cuando NO hay jack -> MUTESPEAKER_PIN LOW
  digitalWrite(MUTESPEAKER_PIN, muted ? LOW : HIGH);
}
#endif

void stopRandomization()
{
  randomizing = false;
}

void setup()
{
  // para el ruido ¿?
  pinMode(23, OUTPUT);
  digitalWrite(23, HIGH);

  Serial.begin(115200);
  for (int i = 0; i < NUM_SEQ_LEDS; i++)
  {
    pinMode(SEQLED_PIN[i], OUTPUT);
  }
  for (int i = 0; i < NUM_TRACKS; i++)
  {
    pinMode(TRACKLED_PIN[i], OUTPUT);
  }
  for (int i = 0; i < NUM_POTS; i++)
  {
    pinMode(POT_PIN[i], INPUT);
  }
#if BODOQUEV1
  pinMode(LEDESTRELLA_PIN, OUTPUT);
#endif

#if DESTRUKTOR1
  pinMode(MUTESPEAKER_PIN, OUTPUT);
#elif BODOQUEV1
  pinMode(MUTESPEAKER_PIN, OUTPUT);
  pinMode(DELAYBTN_PIN, INPUT_PULLUP);
  digitalWrite(MUTESPEAKER_PIN, LOW);
#endif
  setupButtons();

  // Capturar estados iniciales de botones para ignorar presiones iniciales
  for (int i = 0; i < NUM_NOTES; i++)
  {
    initialButtonStates[i] = (digitalRead(buttonPins[i]) == LOW);
  }
  trackButtonInitialState = (digitalRead(TRACKB_PIN) == LOW);
  tempoButtonInitialState = (digitalRead(TEMPOB_PIN) == LOW);
  trackButtonState = trackButtonInitialState;
  tempoButtonState = tempoButtonInitialState;

  if (digitalRead(NOTEB_PIN[3]) == LOW)
  {
    freePlay = true;
  }
  if (digitalRead(NOTEB_PIN[2]) == LOW)
  {
    steps = 64;
  }
  else if (digitalRead(NOTEB_PIN[1]) == LOW)
  {
    steps = 32;
  }
  else
  {
    steps = 16;
  }

  initDelay();
  setupMidiIO();

  setupEnvelopes();
  setTrackLed((uint8_t)mode);
  bassLPF.setResonance((Q0n8)bassFilterResonance);
  bassLPF.setCentreFreq(bassFilterCutoffBase);
  padLPF.setResonance((Q0n8)padFilterResonance);
  padLPF.setCentreFreq(padFilterCutoffBase);
  bassPitchLFO.setFreq(0.5f);
  padPitchLFO.setFreq(0.5f);
  snareNoiseOsc.setFreq(2.0f);
  hatNoiseOsc.setFreq(2.0f);
  hatNoiseOsc.setPhase(WHITENOISE8192_NUM_CELLS / 2);
  crashNoiseOsc.setFreq(2.0f);
  crashOsc.setFreq(5000);

  // Configuración de EQ para pad y snare
  eqPad.setResonance(100);
  eqPad.setCentreFreq(200);
  eqSnare.setResonance(100);
  eqSnare.setCentreFreq(150);

  if (!freePlay)
  {
    // startCountIn();
  }

  bool anyButton;
  do
  {
    anyButton = digitalRead(NOTEB_PIN[0]) && digitalRead(NOTEB_PIN[1]) && digitalRead(NOTEB_PIN[2]) && digitalRead(NOTEB_PIN[3]);
    // Serial.println("solta topu");
  } while (anyButton == false);
  startMozzi(CONTROL_RATE);
}

void updateControl()
{
  updateMidiIO();
  // Serial.println(analogRead(JACKSENSOR_PIN));
  // digitalWrite(MUTESPEAKER_PIN,0);
  readButtons();
  readPots();
  updateOutputGain(pot3);
  updatePadBassDelay();
  if (randomizing && anyInstrumentButtonActive())
  {
    stopRandomization();
  }
  if (randomizing)
  {
    randomizeConfigValues();
  }
  handleButtonHolds();
  handleTrackButtonHold();
  handleTrackButtonRelease();
  handleTempoButtonRelease();
  handlePendingTempoTap();
  handleNotePresses();
  handleRolls();
  handleSequencer();
  updateLed();
  updateEnvelopesAndModulation();
#if DESTRUKTOR1
  detectOutputJack();
#endif
}

AudioOutput updateAudio()
{
  // --- Actualizar envolvente de volumen del bajo (audio rate) ---
  bassEnvLevel = bassEnv.next();

  // --- Generar audio ---
  // Bombo
  uint8_t kickEnvLevel = kickEnv.next();
  int kickFreq = 35 + (kickEnvLevel >> 1);
  kickOsc.setFreq(kickFreq);
  int16_t kick = (kickOsc.next() * kickEnvLevel) >> 8;

  // Snare (ruido blanco)
  int16_t snNoise = snareNoiseOsc.next();
  int16_t snare = eqSnare.next((snNoise * snareEnv.next())) >> 7;
  // Tono del snare (onda senoidal con su propia envolvente)
  int16_t snareTone = eqSnare.next((snareToneOsc.next() * snareToneEnv.next())) >> 8;

  // Hi-Hat (ruido + diferenciador)
  int16_t hatNoise = hatNoiseOsc.next();
  int16_t hatHp = hatNoise - lastHatNoise;
  lastHatNoise = hatNoise;
  int16_t hat = (hatHp * hatEnv.next()) >> 7;

  // Crash Cymbal: High-pitched square wave modulated by noise for metallic timbre
  crashEnvLevel = crashEnv.next();
  // Obtener ruido para modulación de frecuencia
  int16_t noiseMod = crashNoiseOsc.next();
  // Modulación FM sutil: el ruido modula la frecuencia del oscilador
  // Convertir ruido (-128 a 127) a modulación de frecuencia (±1000 Hz)
  float freqMod = (noiseMod / 128.0f) * 800.0f;
  float currentFreq = 8000.0f + freqMod;
  // Asegurar que la frecuencia no sea negativa
  if (currentFreq < 100.0f)
    currentFreq = 100.0f;
  crashOsc.setFreq(currentFreq);
  // Generar sonido principal (onda cuadrada)
  int16_t crashSquare = crashOsc.next();
  // Generar ruido blanco para mezclar (wash del plato)
  int16_t crashNoise = crashNoiseOsc.next();
  // Mezclar: 70% onda cuadrada, 30% ruido para el carácter metálico
  int16_t crash = ((crashSquare * 7 + crashNoise * 3) / 10);
  // Aplicar envolvente
  crash = (crash * crashEnvLevel) >> 8;

  int16_t bass = 0;
  if (bassActive)
  {
    int16_t bassOscSample = bassOsc.next();
    int16_t bassOsc2Sample = bassOsc2Active ? bassOsc2.next() : 0;
    int16_t bassRaw = (bassOscSample + bassOsc2Sample); // Mezclar ambos osciladores
    bass = (bassLPF.next(bassRaw) * bassEnvLevel) >> 6;
  }

  // PAD mixing
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

#if BODOQUEV1
  int32_t delayedPadBass = 0;
  int32_t padBassInput = padBassDelayEnabled ? (((int32_t)(bass >> 2) + (pad >> 1)) >> 2) : 0;
  delayedPadBass = padBassDelay.next(padBassInput);
#endif

  int32_t mix;
  mix = ((int32_t)kick << 5);
  mix = mix + ((int32_t)snare << 1);
  mix = mix + ((int32_t)snareTone << 0);
  mix = mix + ((int32_t)hat << 0);
  mix = mix + ((int32_t)crash << 0);
  mix = mix + ((int32_t)bass << 0);
  mix = mix + ((int32_t)pad << 0);
#if BODOQUEV1
  mix = mix + delayedPadBass << 6;
#endif

// Escala final. DESTRUKTOR1 entrega PWM mono; BODOQUEV1 entrega I2S estéreo al PT8211.
#if BODOQUEV1
  int16_t finalSample = applyOutputGain((int16_t)(mix >> 5));
#elif DESTRUKTOR1
  int16_t finalSample = mix>>2;
#endif

#if DESTRUKTOR1
  return MonoOutput::from8Bit(finalSample >> 1).clip();
#endif

#if BODOQUEV1
  return StereoOutput::from8Bit(finalSample >> 1, finalSample >> 1).clip();
#endif
}

void loop()
{
  audioHook();
}
