#include <AMY-Arduino.h>
#include <I2S.h>
#include <stdint.h>

#include "synth.h"
#include "controls.h"
#include "hardwarePins.h"

// SALIDA DE AUDIO
I2S myI2S(OUTPUT, I2S_BCLK, I2S_DATA);

AudioEffects audioEffects;

float midiNoteToFreq(float note)
{
    return 440.0f * powf(2.0f, (note - 69.0f) / 12.0f); // A4 = nota 69 = 440Hz
}

void audioInit()
{
    // I2S manual, para el PT8211
    myI2S.setBitsPerSample(16);
    myI2S.setFrequency(44100);
    myI2S.setLSBJFormat();

    if (!myI2S.begin())
    {
        Serial.println("I2S init failed!");
        while (1)
            ;
    }

    // AMY: le decimos que NO toque el I2S, que nosotros sacamos el audio a mano
    amy_config_t amy_config = amy_default_config();
    amy_config.platform.multicore = 0; // para poder usar el core1 para el OLED, igual no parece cambiar el rendimiento
    amy_config.audio = AMY_AUDIO_IS_NONE;
    amy_config.features.default_synths = 1;
    amy_start(amy_config);
}

void playDrum(uint8_t drumIndex)
{
    static constexpr uint8_t DRUM_NOTES[] = {35, 37, 42, 46};
    if (drumIndex >= sizeof(DRUM_NOTES))
        return;

    amy_event event = amy_default_event();
    event.synth = 10;
    event.midi_note = DRUM_NOTES[drumIndex];
    event.velocity = 1.0f;
    amy_add_event(&event);
}

void updateAudioOutput()
{
    int16_t *audio = amy_update();

    for (int i = 0; i < AMY_BLOCK_SIZE; i++)
    {
        myI2S.write(audio[i * 2 + 0]); // L
        myI2S.write(audio[i * 2 + 1]); // R
    }
}

float freqFromSemitones(float rootFreq, float semitones)
{
    return rootFreq * powf(2.0f, semitones / 12.0f);
}

void applyEffects()
{
    // audioEffects.chorusLevel = 1.0;         // Nivel de mezcla (0.0 a 1.0)
    // audioEffects.chorusDepth = 1.5;         // Profundidad del LFO
    // audioEffects.chorusLfoFreq = 0.2;      // Velocidad del LFO en Hz
    // audioEffects.chorusMaxDelay = 512;      // Tiempo máximo de delay en samples (Máx: 512)

    // audioEffects.reverbLevel = 1.0;         // Cantidad de reverb en la mezcla final
    // audioEffects.reverbLiveness = 0.7;      // Tamaño/duración de la cola de la reverb
    // audioEffects.reverbDamping = 0.2;       // Absorción de altas frecuencias
    // audioEffects.reverbXoverHz = 2000;     // Frecuencia de cruce del filtro de reverb

    amy_event e = amy_default_event();

    // e.osc = 0;              // El oscilador que genera el sonido
    // 2. Configurar CHORUS

    // 3. Configurar ECHO / DELAY (AMY usa el término 'echo')
    // NO ANDA PARECE???
    // e.echo_level = 1;           // Volumen de las repeticiones (0.0 a 1.0)
    // e.echo_delay_ms = 300;        // Tiempo entre repeticiones en milisegundos
    // e.echo_max_delay_ms = 1000;   // Límite máximo del búfer en milisegundos
    // e.echo_feedback = 0.99;        // Cuántas repeticiones (0.0 a 0.99)
    // e.echo_filter_coef = 0.2;     // Filtro para las repeticiones (LPF suave)

    // 4. Configurar REVERB
    e.reverb_level =  audioEffects.reverbLevel;
    e.reverb_liveness =  audioEffects.reverbLiveness;
    e.reverb_damping = audioEffects.reverbDamping;
    audioEffects.reverbXoverHz = 3000;
    e.reverb_xover_hz = audioEffects.reverbXoverHz;

    e.chorus_level = audioEffects.chorusLevel;
    e.chorus_depth = audioEffects.chorusDepth;
    e.chorus_lfo_freq = audioEffects.chorusLfoFreq;
    audioEffects.chorusMaxDelay = 200;
    e.chorus_max_delay = audioEffects.chorusMaxDelay;

    e.dist_clip = audioEffects.distType == 0 ? 1 : 0;
    e.dist_fold = audioEffects.distType == 1 ? 1 : 0;
    e.dist_drive_coefs[COEF_CONST] = audioEffects.distDrive; // 1.0 = sin drive extra, >1 = más saturación
    e.dist_mix_coefs[COEF_CONST] = audioEffects.distMix;     // 0 = dry, 1 = wet total

    // e.dist_crush = audioEffects.distCrush;
    // e.dist_bits = audioEffects.distBits; // ej: 4-8 bits (16 = "sin crush" de profundidad)
    // e.dist_rate = audioEffects.distRate; // reducción de sample rate

    // 5. Enviar todo al motor global de AMY
    amy_add_event(&e);
}
