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
uint16_t recordingStartDelayMs = 200;
uint16_t playbackSpeedUpdateIntervalMs = 20;

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

int8_t toPcm8(int32_t sample)
{
    int32_t scaled = sample >> PCM_SHIFT;
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

void startRecording(int8_t button)
{
    if (audioState == AudioState::Playing)
    {
        dacI2S.end();
    }

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

void startPlayback()
{
    stopMicrophone();
    recordButton = -1;

    if (recordedSamples == 0)
    {
        audioState = AudioState::Waiting;
        setSeqLedCount(0);
        return;
    }

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

    lastControlUpdateAt = 0;
    updatePlaybackControls();
    playbackPosition = playbackDirection < 0 ? recordedSamples - 1 : 0;
    playbackFraction = 0;
    audioState = AudioState::Playing;
    setSeqLedCount(0);
    setVolumeLedCount(0);
    Serial.print("Reproduciendo ");
    Serial.print((recordedSamples * 1000UL) / STORED_SAMPLE_RATE);
    Serial.println(" ms en bucle.");
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
        playbackVolumeMax < playbackVolumeMin || outputSampleRateMin < 1.0f ||
        outputSampleRateMax <= outputSampleRateMin ||
        outputSampleRateMax > 32000.0f || outputSampleRateCurve <= 0.0f)
    {
        stopOnAudioError("Configuracion de audio invalida");
    }

    float speedPotPosition = analogRead(POT_PIN[1]) / POT_ADC_MAX_VALUE;
    float volumePotPosition = analogRead(POT_PIN[0]) / POT_ADC_MAX_VALUE;
    float sampleRatePotPosition = analogRead(POT_PIN[2]) / POT_ADC_MAX_VALUE;
    if (speedPotPosition > 1.0f) speedPotPosition = 1.0f;
    if (volumePotPosition > 1.0f) volumePotPosition = 1.0f;
    if (sampleRatePotPosition > 1.0f) sampleRatePotPosition = 1.0f;

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

    playbackVolume = playbackVolumeMin +
                     volumePotPosition * (playbackVolumeMax - playbackVolumeMin);

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
        if (store)
        {
            int32_t pcmSample = toPcm8(sampleLeft);
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

void playOutputFrame()
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
    float amplifiedSample = (interpolatedQ16 / 256.0f) * playbackVolume;
    if (amplifiedSample > 32767.0f)
    {
        amplifiedSample = 32767.0f;
    }
    else if (amplifiedSample < -32768.0f)
    {
        amplifiedSample = -32768.0f;
    }

    int16_t sample = static_cast<int16_t>(amplifiedSample);
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
    newPosition %= static_cast<int64_t>(recordedSamples);
    if (newPosition < 0)
    {
        newPosition += recordedSamples;
    }
    playbackPosition = static_cast<size_t>(newPosition);
    playbackFraction = static_cast<uint16_t>(newFraction);
}

void playSample()
{
    updatePlaybackControls();
    for (uint8_t frame = 0; frame < 2; ++frame)
    {
        playOutputFrame();
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
    setSeqLedProgress(litLeds, playbackDirection);
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
}

void updateAudioOutput()
{
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
