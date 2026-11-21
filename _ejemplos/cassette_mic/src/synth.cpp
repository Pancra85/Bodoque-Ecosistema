#include <Arduino.h>
#include <I2S.h>
#include <math.h>

#include "hardwarePins.h"
#include "synth.h"

float playbackSpeedMin = 0.1f;
float playbackSpeedMax = 100.0f;
float playbackSpeedDeadbandMin = 0.45f;
float playbackSpeedDeadbandMax = 0.55f;
float playbackRateCorrection = 0.5f;
float playbackSpeed = 1.0f;
int8_t playbackDirection = 1;
float playbackVolumeMin = 0.0f;
float playbackVolumeMax = 16.0f;
float playbackVolume = 1.0f;
float outputSampleRateMin = 1.0f;
float outputSampleRateMax = 32000.0f;
float outputSampleRateCurve = 2.0f;
uint32_t outputSampleRate = 32000;
uint16_t recordingStartDelayMs = 50; 
uint16_t playbackSpeedUpdateIntervalMs = 20;
uint16_t playbackCrossfadeMs = 70;
uint8_t LOOP_RECORDING = 1; // 1: bucle; 0: una pasada para diagnosticar el pop.

namespace
{
constexpr uint32_t INPUT_SAMPLE_RATE = 32000;
constexpr uint32_t STORED_SAMPLE_RATE = 32000;
constexpr uint32_t MIN_I2S_OUTPUT_SAMPLE_RATE = 120;
constexpr size_t AUDIO_BUFFER_SIZE = 160 * 1024;
constexpr uint8_t INPUT_DOWNSAMPLE = INPUT_SAMPLE_RATE / STORED_SAMPLE_RATE;
constexpr uint8_t PCM_SHIFT = 21;
constexpr uint8_t BUTTON_RELEASE_DEBOUNCE_MS = 20;
constexpr float POT_ADC_MAX_VALUE = 1023.0f;
constexpr uint8_t PCM8_MAX_AMPLITUDE = 127;
constexpr uint32_t VOLUME_METER_DECAY_INTERVAL_US = 1500;
constexpr int32_t OUTPUT_SOFT_KNEE = 28000;
constexpr int32_t OUTPUT_CEILING = 32767;
constexpr uint32_t SPEED_LED_STEP_US = 150000;

int8_t audioBuffer[AUDIO_BUFFER_SIZE];
size_t recordedSamples = 0;
size_t playbackPosition = 0;
uint32_t inputFrames = 0;
int8_t recordButton = -1;
unsigned long releaseStartedAt = 0;
unsigned long recordingStartAt = 0;
unsigned long lastControlUpdateAt = 0;
uint16_t playbackFraction = 0;
uint32_t playbackStepQ16 = 0;
uint32_t configuredDacSampleRate = 0;
uint32_t playbackSpeedLedLastStepUs = 0;
uint8_t playbackSpeedLedStep = 0;
int32_t dcEstimate = 0;
bool dcPrimed = false;
bool waitingForAllButtonsUp = false;
uint8_t shownSeqLedMask = 0xFF;
uint8_t shownVolumeLedMask = 0xFF;
bool microphoneActive = false;
uint8_t volumeMeterPeak = 0;
uint32_t volumeMeterLastDecayUs = 0;

I2S microphoneI2S(INPUT);
I2S dacI2S(OUTPUT, I2S_BCLK, I2S_DATA);

enum class AudioState : uint8_t
{
    Waiting,
    Starting,
    Recording,
    Full,
    Playing
};

AudioState audioState = AudioState::Waiting;

void writeSeqLedMask(uint8_t mask)
{
    if (mask == shownSeqLedMask)
    {
        return;
    }

    for (uint8_t i = 0; i < NUM_SEQ_LEDS; ++i)
    {
        digitalWrite(LED_BUTTON_PIN[i], mask & (1 << i) ? HIGH : LOW);
    }
    shownSeqLedMask = mask;
}

void writeVolumeLedMask(uint8_t mask)
{
    if (mask == shownVolumeLedMask)
    {
        return;
    }

    for (uint8_t i = 0; i < NUM_X_LEDS; ++i)
    {
        digitalWrite(LED_X_PIN[i], mask & (1 << i) ? HIGH : LOW);
    }
    shownVolumeLedMask = mask;
}

void setSeqLedProgress(uint8_t count, int8_t direction)
{
    uint8_t mask = 0;
    for (uint8_t i = 0; i < NUM_SEQ_LEDS; ++i)
    {
        bool ledOn = direction < 0 ? i >= NUM_SEQ_LEDS - count : i < count;
        if (ledOn)
        {
            mask |= 1 << i;
        }
    }
    writeSeqLedMask(mask);
}

void setSeqLedPlaybackProgress(uint8_t count, int8_t direction)
{
    uint8_t positionMask = 0;
    for (uint8_t i = 0; i < NUM_SEQ_LEDS; ++i)
    {
        bool ledOn = direction < 0 ? i >= NUM_SEQ_LEDS - count : i < count;
        if (ledOn)
        {
            positionMask |= 1 << i;
        }
    }

    uint32_t stepIntervalUs =
        static_cast<uint32_t>(SPEED_LED_STEP_US / playbackSpeed);
    if (stepIntervalUs == 0)
    {
        stepIntervalUs = 1;
    }
    uint32_t nowUs = micros();
    uint32_t elapsedUs = nowUs - playbackSpeedLedLastStepUs;
    if (elapsedUs >= stepIntervalUs)
    {
        uint32_t elapsedSteps = elapsedUs / stepIntervalUs;
        playbackSpeedLedStep =
            (playbackSpeedLedStep + elapsedSteps) % (NUM_SEQ_LEDS * 2);
        playbackSpeedLedLastStepUs += elapsedSteps * stepIntervalUs;
    }

    uint8_t speedMask = 0;
    if ((playbackSpeedLedStep % 2) == 0)
    {
        speedMask = 1 << (playbackSpeedLedStep / 2);
    }
    // XOR cancela los LEDs compartidos entre posicion y velocidad.
    writeSeqLedMask(positionMask ^ speedMask);
}

void setSeqLedCount(uint8_t count)
{
    setSeqLedProgress(count, 1);
}

void setVolumeLedCount(uint8_t count)
{
    uint8_t mask = 0;
    for (uint8_t i = 0; i < NUM_X_LEDS; ++i)
    {
        if (i >= NUM_X_LEDS - count)
        {
            mask |= 1 << i;
        }
    }
    writeVolumeLedMask(mask);
}

bool allNoteButtonsReleased()
{
    for (uint8_t i = 0; i < NUM_NOTES; ++i)
    {
        if (digitalRead(BUTTON_PIN[i]) == LOW)
        {
            return false;
        }
    }
    return true;
}

int8_t pressedNoteButton()
{
    for (uint8_t i = 0; i < NUM_NOTES; ++i)
    {
        if (digitalRead(BUTTON_PIN[i]) == LOW)
        {
            return static_cast<int8_t>(i);
        }
    }
    return -1;
}

int8_t toPcm8(int64_t sample)
{
    int64_t scaled = sample >> PCM_SHIFT;
    if (scaled > 127)
    {
        scaled = 127;
    }
    else if (scaled < -128)
    {
        scaled = -128;
    }
    return static_cast<int8_t>(scaled);
}

void stopOnAudioError(const char *message)
{
    Serial.println(message);
    while (true)
    {
        delay(1000);
    }
}

void stopMicrophone()
{
    if (microphoneActive)
    {
        microphoneI2S.end();
        microphoneActive = false;
    }
}

void writeDacSilenceFrame()
{
    // Mantiene activo el flujo I2S mientras no hay audio grabado para reproducir.
    int16_t silence = 0;
    dacI2S.write(silence);
    dacI2S.write(silence);
}

void startRecording(int8_t button)
{
    recordButton = button;
    releaseStartedAt = 0;
    recordingStartAt = millis();

    if (!microphoneI2S.setBCLK(MIC_I2S_BCLK) ||
        !microphoneI2S.setDATA(MIC_I2S_DATA) ||
        !microphoneI2S.setBitsPerSample(16) ||
        !microphoneI2S.begin(INPUT_SAMPLE_RATE))
    {
        stopOnAudioError("Error al iniciar el I2S del microfono");
    }

    dcPrimed = false;
    microphoneActive = true;
    audioState = AudioState::Starting;
    setSeqLedCount(0);
    setVolumeLedCount(0);
}

void startCapture()
{
    recordedSamples = 0;
    playbackPosition = 0;
    playbackFraction = 0;
    inputFrames = 0;
    volumeMeterPeak = 0;
    volumeMeterLastDecayUs = micros();
    audioState = AudioState::Recording;
    setVolumeLedCount(0);
    Serial.println("Grabando mientras mantienes la nota...");
}

void updatePlaybackControls();

void trimRecordingTail()
{
    // Diagnostico del pop: recorta al final lo mismo que se descarta al inicio.
    size_t tailSamples = static_cast<size_t>(
        (static_cast<uint64_t>(STORED_SAMPLE_RATE) * recordingStartDelayMs) /
        1000ULL);
    recordedSamples =
        tailSamples < recordedSamples ? recordedSamples - tailSamples : 0;
}

void startPlayback()
{
    stopMicrophone();
    recordButton = -1;

    if (recordedSamples == 0)
    {
        audioState = AudioState::Waiting;
        setSeqLedCount(0);
        setVolumeLedCount(0);
        return;
    }

    uint32_t physicalSampleRate =
        outputSampleRate < MIN_I2S_OUTPUT_SAMPLE_RATE
            ? MIN_I2S_OUTPUT_SAMPLE_RATE
            : outputSampleRate;
    if (physicalSampleRate != configuredDacSampleRate &&
        !dacI2S.setFrequency(physicalSampleRate))
    {
        stopOnAudioError("Error al cambiar la frecuencia del PT8211");
    }
    configuredDacSampleRate = physicalSampleRate;

    lastControlUpdateAt = 0;
    updatePlaybackControls();
    playbackPosition = playbackDirection < 0 ? recordedSamples - 1 : 0;
    playbackFraction = 0;
    playbackSpeedLedStep = 0;
    playbackSpeedLedLastStepUs = micros();
    audioState = AudioState::Playing;
    setSeqLedCount(0);
    setVolumeLedCount(0);
    Serial.print("Reproduciendo ");
    Serial.print((recordedSamples * 1000UL) / STORED_SAMPLE_RATE);
    Serial.println(LOOP_RECORDING ? " ms en bucle." : " ms, una pasada.");
}

void cancelRecordingStart()
{
    recordButton = -1;
    if (recordedSamples > 0)
    {
        startPlayback();
    }
    else
    {
        stopMicrophone();
        audioState = AudioState::Waiting;
        setSeqLedCount(0);
        setVolumeLedCount(0);
    }
}

void updatePlaybackControls()
{
    unsigned long now = millis();
    if (now - lastControlUpdateAt < playbackSpeedUpdateIntervalMs)
    {
        return;
    }
    lastControlUpdateAt = now;

    if (playbackSpeedMin <= 0.0f || playbackSpeedMax <= playbackSpeedMin ||
        playbackSpeedDeadbandMin <= 0.0f ||
        playbackSpeedDeadbandMax >= 1.0f ||
        playbackSpeedDeadbandMin >= playbackSpeedDeadbandMax ||
        playbackRateCorrection <= 0.0f || playbackVolumeMin < 0.0f ||
        playbackVolumeMin > 1.0f || playbackVolumeMax < 1.0f ||
        playbackVolumeMax > 1024.0f ||
        outputSampleRateMin < 1.0f ||
        outputSampleRateMax <= outputSampleRateMin ||
        outputSampleRateMax > 32000.0f || outputSampleRateCurve <= 0.0f)
    {
        stopOnAudioError("Configuracion de audio invalida");
    }

    float speedPotPosition = analogRead(POT_PIN[1]) / POT_ADC_MAX_VALUE;
    uint16_t volumePotValue = analogRead(POT_PIN[0]);
    float sampleRatePotPosition = analogRead(POT_PIN[2]) / POT_ADC_MAX_VALUE;
    if (speedPotPosition > 1.0f) speedPotPosition = 1.0f;
    if (sampleRatePotPosition > 1.0f) sampleRatePotPosition = 1.0f;
    if (volumePotValue > 1023)
    {
        volumePotValue = 1023;
    }

    if (speedPotPosition < playbackSpeedDeadbandMin)
    {
        playbackDirection = -1;
        float speedPosition =
            (playbackSpeedDeadbandMin - speedPotPosition) / playbackSpeedDeadbandMin;
        playbackSpeed = expf(logf(playbackSpeedMin) +
                             speedPosition * (logf(playbackSpeedMax) -
                                              logf(playbackSpeedMin)));
    }
    else if (speedPotPosition > playbackSpeedDeadbandMax)
    {
        playbackDirection = 1;
        float speedPosition =
            (speedPotPosition - playbackSpeedDeadbandMax) /
            (1.0f - playbackSpeedDeadbandMax);
        playbackSpeed = expf(logf(playbackSpeedMin) +
                             speedPosition * (logf(playbackSpeedMax) -
                                              logf(playbackSpeedMin)));
    }
    else
    {
        playbackDirection = 1;
        playbackSpeed = playbackSpeedMin;
    }

    if (volumePotValue <= 512)
    {
        playbackVolume =
            playbackVolumeMin +
            (volumePotValue / 512.0f) * (1.0f - playbackVolumeMin);
    }
    else
    {
        float volumeBoostPosition = (volumePotValue - 512) / 511.0f;
        playbackVolume =
            1.0f + volumeBoostPosition * (playbackVolumeMax - 1.0f);
    }

    float curvedRatePosition = powf(sampleRatePotPosition, outputSampleRateCurve);
    outputSampleRate = static_cast<uint32_t>(
        outputSampleRateMin +
        curvedRatePosition * (outputSampleRateMax - outputSampleRateMin) + 0.5f);
    if (outputSampleRate < 1)
    {
        outputSampleRate = 1;
    }

    uint32_t physicalSampleRate =
        outputSampleRate < MIN_I2S_OUTPUT_SAMPLE_RATE
            ? MIN_I2S_OUTPUT_SAMPLE_RATE
            : outputSampleRate;
    if (physicalSampleRate != configuredDacSampleRate)
    {
        if (!dacI2S.setFrequency(physicalSampleRate))
        {
            stopOnAudioError("Error al cambiar la frecuencia del PT8211");
        }
        configuredDacSampleRate = physicalSampleRate;
    }

    float sourceSamplesPerOutputFrame =
        (STORED_SAMPLE_RATE / static_cast<float>(configuredDacSampleRate)) *
        playbackRateCorrection * playbackSpeed;
    playbackStepQ16 = static_cast<uint32_t>(sourceSamplesPerOutputFrame * 65536.0f);
    if (playbackStepQ16 == 0)
    {
        playbackStepQ16 = 1;
    }
}

void captureSample(bool store)
{
    int32_t sampleLeft = 0;
    int32_t sampleRight = 0;
    if (microphoneI2S.read32(&sampleLeft, &sampleRight))
    {
        if (!dcPrimed)
        {
            dcEstimate = sampleLeft;
            dcPrimed = true;
        }

        int64_t difference = static_cast<int64_t>(sampleLeft) - dcEstimate;
        dcEstimate += static_cast<int32_t>(difference >> 10);
        int64_t centeredSample = difference;

        if (store)
        {
            int32_t pcmSample = toPcm8(centeredSample);
            uint8_t amplitude = pcmSample < 0 ? -pcmSample : pcmSample;
            if (amplitude > PCM8_MAX_AMPLITUDE)
            {
                amplitude = PCM8_MAX_AMPLITUDE;
            }

            uint32_t nowUs = micros();
            if (amplitude > volumeMeterPeak)
            {
                volumeMeterPeak = amplitude;
                volumeMeterLastDecayUs = nowUs;
            }
            else if (volumeMeterPeak > 0 &&
                     nowUs - volumeMeterLastDecayUs >=
                         VOLUME_METER_DECAY_INTERVAL_US)
            {
                --volumeMeterPeak;
                volumeMeterLastDecayUs = nowUs;
            }

            uint8_t litLeds =
                (volumeMeterPeak * NUM_X_LEDS) / PCM8_MAX_AMPLITUDE;
            setVolumeLedCount(litLeds);

            if (inputFrames % INPUT_DOWNSAMPLE == 0)
            {
                audioBuffer[recordedSamples++] = static_cast<int8_t>(pcmSample);
            }
        }
        ++inputFrames;
    }
}

void stopPlayback()
{
    // Una pasada permite ubicar si el pop ocurre al inicio o al final.
    audioState = AudioState::Waiting;
    setSeqLedCount(0);
    setVolumeLedCount(0);
    Serial.println("Reproduccion finalizada.");
}

bool playOutputFrame()
{
    size_t nextPosition = playbackPosition + 1;
    if (nextPosition == recordedSamples)
    {
        nextPosition = 0;
    }

    int32_t first = audioBuffer[playbackPosition];
    int32_t next = audioBuffer[nextPosition];
    int32_t interpolatedQ16 =
        first * (65536 - playbackFraction) + next * playbackFraction;
    int32_t outputSampleQ16 = interpolatedQ16;
    size_t crossfadeSamples = 0;
    if (LOOP_RECORDING && playbackCrossfadeMs > 0)
    {
        uint64_t requestedSamples =
            (static_cast<uint64_t>(STORED_SAMPLE_RATE) * playbackCrossfadeMs) /
            1000ULL;
        if (requestedSamples > recordedSamples / 2)
        {
            requestedSamples = recordedSamples / 2;
        }
        crossfadeSamples = static_cast<size_t>(requestedSamples);
    }

    if (crossfadeSamples > 0)
    {
        bool inCrossfade =
            playbackDirection < 0
                ? playbackPosition < crossfadeSamples
                : playbackPosition >= recordedSamples - crossfadeSamples;
        if (inCrossfade)
        {
            size_t crossfadeStart = recordedSamples - crossfadeSamples;
            size_t blendSamplePosition =
                playbackDirection < 0
                    ? crossfadeStart + playbackPosition
                    : playbackPosition - crossfadeStart;
            size_t blendNextPosition =
                blendSamplePosition + 1 < recordedSamples
                    ? blendSamplePosition + 1
                    : blendSamplePosition;
            int32_t blendFirst = audioBuffer[blendSamplePosition];
            int32_t blendNext = audioBuffer[blendNextPosition];
            int32_t blendSampleQ16 =
                blendFirst * (65536 - playbackFraction) +
                blendNext * playbackFraction;

            float fractionalPosition = playbackFraction / 65536.0f;
            float blend;
            if (playbackDirection < 0)
            {
                blend = crossfadeSamples == 1
                            ? 1.0f
                            : (crossfadeSamples - 1 - playbackPosition -
                               fractionalPosition) /
                                  (crossfadeSamples - 1);
            }
            else
            {
                blend = crossfadeSamples == 1
                            ? 1.0f
                            : ((playbackPosition - crossfadeStart) +
                               fractionalPosition) /
                                  (crossfadeSamples - 1);
            }
            if (blend < 0.0f)
            {
                blend = 0.0f;
            }
            else if (blend > 1.0f)
            {
                blend = 1.0f;
            }

            outputSampleQ16 = static_cast<int32_t>(
                interpolatedQ16 * (1.0f - blend) + blendSampleQ16 * blend);
        }
    }

    // Q10 con int64 evita overflow; la curva suave reduce el clipping abrupto.
    int64_t amplifiedSample =
        (static_cast<int64_t>(outputSampleQ16) *
         static_cast<int64_t>(playbackVolume * 1024.0f + 0.5f)) /
        (256LL * 1024LL);
    int64_t magnitude = amplifiedSample < 0 ? -amplifiedSample : amplifiedSample;
    if (magnitude > OUTPUT_SOFT_KNEE)
    {
        int64_t excess = magnitude - OUTPUT_SOFT_KNEE;
        magnitude =
            OUTPUT_SOFT_KNEE +
            (excess * (OUTPUT_CEILING - OUTPUT_SOFT_KNEE)) /
                ((OUTPUT_CEILING - OUTPUT_SOFT_KNEE) + excess);
    }
    if (magnitude > OUTPUT_CEILING)
    {
        magnitude = OUTPUT_CEILING;
    }
    int16_t sample =
        static_cast<int16_t>(amplifiedSample < 0 ? -magnitude : magnitude);
    dacI2S.write(sample);
    dacI2S.write(sample);

    int64_t phaseQ16 = static_cast<int64_t>(playbackFraction) +
                       static_cast<int64_t>(playbackDirection) * playbackStepQ16;
    int64_t wholeDelta = phaseQ16 / 65536;
    int64_t newFraction = phaseQ16 % 65536;
    if (newFraction < 0)
    {
        newFraction += 65536;
        --wholeDelta;
    }
    int64_t newPosition = static_cast<int64_t>(playbackPosition) + wholeDelta;
    if (newPosition < 0 || newPosition >= static_cast<int64_t>(recordedSamples))
    {
        if (LOOP_RECORDING)
        {
            int64_t loopLength =
                static_cast<int64_t>(recordedSamples - crossfadeSamples);
            if (newPosition < 0)
            {
                newPosition %= loopLength;
            }
            else
            {
                newPosition =
                    crossfadeSamples + (newPosition - recordedSamples) % loopLength;
            }
            if (newPosition < 0)
            {
                newPosition += loopLength;
            }
        }
        else
        {
            stopPlayback();
            return false;
        }
    }
    playbackPosition = static_cast<size_t>(newPosition);
    playbackFraction = static_cast<uint16_t>(newFraction);
    return true;
}

void playSample()
{
    updatePlaybackControls();
    for (uint8_t frame = 0; frame < 2; ++frame)
    {
        if (!playOutputFrame())
        {
            return;
        }
    }

    uint64_t progressQ16;
    if (playbackDirection < 0)
    {
        int64_t reverseProgress =
            (static_cast<int64_t>(recordedSamples - 1 - playbackPosition) << 16) -
            playbackFraction;
        progressQ16 = reverseProgress > 0 ? reverseProgress : 0;
    }
    else
    {
        progressQ16 = (static_cast<uint64_t>(playbackPosition) << 16) +
                      playbackFraction;
    }
    uint64_t totalQ16 = static_cast<uint64_t>(recordedSamples) << 16;
    uint8_t litLeds = (progressQ16 * (NUM_SEQ_LEDS + 1)) / totalQ16;
    if (litLeds > NUM_SEQ_LEDS)
    {
        litLeds = NUM_SEQ_LEDS;
    }
    setSeqLedPlaybackProgress(litLeds, playbackDirection);
}
}

void audioInit()
{
    analogReadResolution(10);
    for (uint8_t i = 0; i < NUM_NOTES; ++i)
    {
        pinMode(BUTTON_PIN[i], INPUT_PULLUP);
    }
    for (uint8_t i = 0; i < NUM_SEQ_LEDS; ++i)
    {
        pinMode(LED_BUTTON_PIN[i], OUTPUT);
        digitalWrite(LED_BUTTON_PIN[i], LOW);
    }
    for (uint8_t i = 0; i < NUM_X_LEDS; ++i)
    {
        pinMode(LED_X_PIN[i], OUTPUT);
        digitalWrite(LED_X_PIN[i], LOW);
    }
    shownSeqLedMask = 0;
    shownVolumeLedMask = 0;

    // Inicia el DAC al arranque para no habilitarlo al soltar el boton.
    uint32_t physicalSampleRate =
        outputSampleRate < MIN_I2S_OUTPUT_SAMPLE_RATE
            ? MIN_I2S_OUTPUT_SAMPLE_RATE
            : outputSampleRate;
    if (!dacI2S.setBitsPerSample(16) ||
        !dacI2S.setFrequency(physicalSampleRate) ||
        !dacI2S.setLSBJFormat() ||
        !dacI2S.begin())
    {
        stopOnAudioError("Error al iniciar el I2S del PT8211");
    }
    configuredDacSampleRate = physicalSampleRate;
    for (uint8_t frame = 0; frame < 4; ++frame)
    {
        writeDacSilenceFrame();
    }
}

void updateAudioOutput()
{
    // Durante espera y grabacion se mantiene el PT8211 recibiendo silencio.
    if (audioState != AudioState::Playing)
    {
        writeDacSilenceFrame();
    }

    if (audioState == AudioState::Starting)
    {
        captureSample(false);

        if (digitalRead(BUTTON_PIN[recordButton]) == LOW)
        {
            releaseStartedAt = 0;
        }
        else if (releaseStartedAt == 0)
        {
            releaseStartedAt = millis();
        }
        else if (millis() - releaseStartedAt >= BUTTON_RELEASE_DEBOUNCE_MS)
        {
            cancelRecordingStart();
            return;
        }

        if (millis() - recordingStartAt >= recordingStartDelayMs)
        {
            startCapture();
        }
        return;
    }

    if (audioState == AudioState::Recording)
    {
        captureSample(true);

        if (recordedSamples == AUDIO_BUFFER_SIZE)
        {
            stopMicrophone();
            audioState = AudioState::Full;
            releaseStartedAt = 0;
            setVolumeLedCount(0);
            Serial.println("Memoria llena; esperando que sueltes la nota.");
            return;
        }

        if (digitalRead(BUTTON_PIN[recordButton]) == LOW)
        {
            releaseStartedAt = 0;
        }
        else if (releaseStartedAt == 0)
        {
            releaseStartedAt = millis();
        }
        else if (millis() - releaseStartedAt >= BUTTON_RELEASE_DEBOUNCE_MS)
        {
            trimRecordingTail();
            startPlayback();
        }
        return;
    }

    if (audioState == AudioState::Full)
    {
        if (digitalRead(BUTTON_PIN[recordButton]) == LOW)
        {
            releaseStartedAt = 0;
        }
        else if (releaseStartedAt == 0)
        {
            releaseStartedAt = millis();
        }
        else if (millis() - releaseStartedAt >= BUTTON_RELEASE_DEBOUNCE_MS)
        {
            trimRecordingTail();
            startPlayback();
        }
        return;
    }

    if (!allNoteButtonsReleased())
    {
        if (waitingForAllButtonsUp)
        {
            return;
        }
        int8_t button = pressedNoteButton();
        if (button >= 0)
        {
            waitingForAllButtonsUp = true;
            startRecording(button);
            return;
        }
    }
    else
    {
        waitingForAllButtonsUp = false;
    }

    if (audioState == AudioState::Playing)
    {
        playSample();
    }
}
