#include <AMY-Arduino.h>
#include <I2S.h>

#include "synth.h"
#include "controls.h"
#include "hardwarePins.h"

// SALIDA DE AUDIO
I2S myI2S(OUTPUT, I2S_BCLK, I2S_DATA);

Layer layers[NUM_LAYERS];
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
    amy_start(amy_config);
}

void setDefaultParameters()
{
    for (int i = 0; i < NUM_LAYERS; i++)
    {
        layers[i] = Layer{};
        layers[i].pan = 1.0f;
        layers[i].volume = 2.0f;
        layers[i].layerNumb = i;
    }
    audioEffects = AudioEffects{};

    layers[0].active = true;
    layers[0].baseNote = 21.0f;
    layers[0].filterType = 1;
    layers[0].filterFreq = 1000.0f;
    layers[0].resonance = 2.0f;
    layers[0].waveform = 1;
    layers[0].complexity = 400;
    layers[0].sequenceLength = 16;
    layers[0].millisPerStep = 200;

    layers[1].active = 0;
    layers[1].complexity = 300;
    layers[1].baseNote = 55.0f;
    layers[1].filterType = 1;
    layers[1].filterFreq = 1500.0f;
    layers[1].resonance = 1.0f;
    layers[1].waveform = 2;
    layers[1].sequenceLength = 32;
    layers[1].millisPerStep = 180;

    audioEffects.chorusLfoFreq = 0.1f;
    audioEffects.distDrive = 1.0f;
    audioEffects.reverbLiveness = 0.3f;
    audioEffects.chorusDepth = 1.0f;

}

// Reserva el rango MÁXIMO posible por capa. Los índices quedan fijos
// para siempre una vez llamada — no hace falta volver a llamarla nunca más.
void assignOscIndex()
{
    const int maxOscsPerLayer = effectsEnabledConfig ? MAX_OSCS_PER_LAYER_NOFX : MAX_OSCS_PER_LAYER;
    int nextOsc = 0;
    for (int c = 0; c < NUM_LAYERS; c++)
    {
        layers[c].oscBase = nextOsc;
        nextOsc += maxOscsPerLayer;
        layers[c].lfoBase = nextOsc;
        nextOsc += MAX_LFOS_PER_LAYER;
    }
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

// Arma el evento completo del oscilador i de la capa l (asume i < numOscOnLayer)
amy_event buildActiveOscEvent(Layer &l, int i)
{
    amy_event e = amy_default_event();
    e.osc = l.oscBase + i;

    e.amp_coefs[COEF_CONST] = l.volume;
    e.wave = l.waveform;
    e.velocity = 1;
    e.filter_type = l.filterType;
    e.filter_freq_coefs[COEF_CONST] = l.filterFreq;
    e.resonance = l.resonance;
    e.pan_coefs[COEF_CONST] = l.pan;

    if (l.harmonyMode != HARMONY_SEQUENCE)
    {
        float rootFreq = midiNoteToFreq(l.baseNote);
        float noteFreq = freqFromSemitones(rootFreq, l.noteOffsets[i]);
        e.freq_coefs[COEF_CONST] = noteFreq * (1.0f + l.oscs[i].detune);
    }

    int slot0Lfo = l.lfoAssignment[i][0];
    int slot1Lfo = l.lfoAssignment[i][1];
    bool noFilter = (l.filterType == FILTER_NONE);

    // Slot 0: siempre pitch
    if (slot0Lfo >= 0)
    {
        e.freq_coefs[COEF_MOD0] = l.oscs[i].modDepth;
        e.mod_source[0] = l.lfoBase + slot0Lfo;
    }
    else
    {
        e.freq_coefs[COEF_MOD0] = 0.0f;
    }

    // Slot 1: filtro normalmente. Si NO hay filtro, también apunta a pitch
    // (con su propio coeficiente/fuente) en vez de desperdiciarse en la nada.
    e.freq_coefs[COEF_MOD1] = 0.0f;
    e.filter_freq_coefs[COEF_MOD1] = 0.0f;

    if (slot1Lfo >= 0)
    {
        e.mod_source[1] = l.lfoBase + slot1Lfo;
        if (noFilter)
            e.freq_coefs[COEF_MOD1] = l.oscs[i].modDepth;
        else
            e.filter_freq_coefs[COEF_MOD1] = l.oscs[i].modDepth;
    }

    return e;
}

// Reenvía el estado completo de todos los osciladores YA ACTIVOS.
// No toca índices ni prende/apaga slots (eso es trabajo de rebuildLayer).
void applyLayerParams(Layer &l)
{
    if (!l.active)
    {
        rebuildLayer(l);
        return;
    }

    for (int i = 0; i < l.numOscOnLayer; i++)
    {
        amy_event e = buildActiveOscEvent(l, i);
        amy_add_event(&e);
    }
}

// Decide qué LFO le toca a cada oscilador. Acá es donde probás distintas
// estrategias (1 a 1, round-robin, todos al mismo, etc.)
void assignLfoRouting(Layer &l)
{
    for (int i = 0; i < MAX_OSCS_PER_LAYER; i++)
    {
        l.lfoAssignment[i][0] = -1;
        l.lfoAssignment[i][1] = -1;
    }

    if (l.numLFOsOnLayer == 0 || l.numOscOnLayer == 0)
        return;

    for (int lfoIdx = 0; lfoIdx < l.numLFOsOnLayer; lfoIdx++)
    {
        int oscIdx = lfoIdx % l.numOscOnLayer;
        int slot = (lfoIdx / l.numOscOnLayer) % 2;

        if (l.lfoAssignment[oscIdx][slot] == -1)
            l.lfoAssignment[oscIdx][slot] = lfoIdx;
    }
}

void rebuildLayer(Layer &l)
{
    assignLfoRouting(l);

    const int maxOscsPerLayer = effectsEnabledConfig ? MAX_OSCS_PER_LAYER_NOFX : MAX_OSCS_PER_LAYER;
    for (int i = 0; i < maxOscsPerLayer; i++)
    {
        if (i < l.numOscOnLayer && l.active == 1)
        {
            amy_event e = buildActiveOscEvent(l, i);
            amy_add_event(&e);
        }
        else
        {
            amy_event e = amy_default_event();
            e.osc = l.oscBase + i;
            e.velocity = 0;
            amy_add_event(&e);
        }
    }

    // LFOs sin cambios respecto a lo que ya tenías
    for (int i = 0; i < MAX_LFOS_PER_LAYER; i++)
    {
        amy_event e = amy_default_event();
        e.osc = l.lfoBase + i;
        if (l.active && i < l.numLFOsOnLayer)
        {
            e.wave = SINE;
            e.freq_coefs[COEF_CONST] = l.lfos[i].frequency;
            e.velocity = 1;
        }
        else
        {
            e.velocity = 0;
        }
        amy_add_event(&e);
    }
}

void processLayerRebuilds(Layer &l)
{
    if (l.layerNeedsRebuild)
    {
        assignOscIndex();
        rebuildLayer(l);
        // rebuildHarmonyStructure(l);
        l.layerNeedsRebuild = false;
    }
}

void setLayerComplexity(Layer &l, int complexity)
{
    const float MAX_PITCH_MOD_DEPTH = 0.08f; // profundidad máxima del LFO sobre pitch
    const float MAX_FILTER_MOD_DEPTH = 0.4f; // profundidad máxima del LFO sobre filtro
    const float MAX_DETUNE_STEP = 0.2f;      // desafinación máxima entre osciladores (en realidad hace tope a un tercio de esto)

    if (complexity < 0)
        complexity = 0;
    if (complexity > COMPLEXITY_MAX)
        complexity = COMPLEXITY_MAX;

    l.complexity = complexity;
    float complexityFrac = complexity / (float)COMPLEXITY_MAX; // 0.0 .. 1.0, para escalar todo
    const float lfoDepthStart = ADC_MAX / 3.0f;
    float lfoDepthFrac = (complexity - lfoDepthStart) / (COMPLEXITY_MAX - lfoDepthStart);
    if (lfoDepthFrac < 0.0f)
        lfoDepthFrac = 0.0f;
    if (lfoDepthFrac > 1.0f)
        lfoDepthFrac = 1.0f;
    const int maxOscsPerLayer = effectsEnabledConfig ? MAX_OSCS_PER_LAYER_NOFX : MAX_OSCS_PER_LAYER;

    // --- Cantidad de osciladores ---
    int numOsc;
    if (complexity < COMPLEXITY_MAX / 4)
    {
        // Sube de 1 al máximo habilitado dentro de este primer tramo.
        float frac = complexity / (float)(COMPLEXITY_MAX / 3);
        numOsc = 1 + (int)(frac * (maxOscsPerLayer - 1));
    }
    else
    {
        numOsc = maxOscsPerLayer;
    }

    // --- Cantidad de LFOs ---
    int numLfo;
    if (complexity < ADC_MAX / 3)
    {
        numLfo = 0;
    }
    else if (complexity < ADC_MAX / 2)
    {
        // Sube de 1 a MAX_LFOS_PER_LAYER entre ADC_MAX/3 y ADC_MAX/2
        float frac = (complexity - ADC_MAX / 3) / (float)(ADC_MAX / 2 - ADC_MAX / 3);
        numLfo = 1 + (int)(frac * (MAX_LFOS_PER_LAYER - 1));
    }
    else
    {
        numLfo = MAX_LFOS_PER_LAYER;
    }

    // Clamps de seguridad (por redondeos en los cálculos de arriba)
    if (numOsc > maxOscsPerLayer)
        numOsc = maxOscsPerLayer;
    if (numOsc < 0)
        numOsc = 0;
    if (numLfo > MAX_LFOS_PER_LAYER)
        numLfo = MAX_LFOS_PER_LAYER;
    if (numLfo < 0)
        numLfo = 0;

    l.numOscOnLayer = numOsc;
    l.numLFOsOnLayer = numLfo;

    // Serial.println("Layer complexity set to: " + String(numOsc) + " oscs, " + String(numLfo) + " LFOs");

    if (l.filterType == FILTER_NONE)
    {
        for (int i = 0; i < MAX_OSCS_PER_LAYER; i++)
            l.oscs[i].lfoTarget = LFO_TARGET_PITCH;
    }
    else
    {
        for (int i = 0; i < MAX_OSCS_PER_LAYER; i++)
            l.oscs[i].lfoTarget = (i % 2 == 0) ? LFO_TARGET_PITCH : LFO_TARGET_FILTER;
    }
    // El detune crece con la complejidad: cuanto más "compleja" la capa, más desafinada
    float detuneStep = complexityFrac * MAX_DETUNE_STEP;
    detuneStep = min(detuneStep, MAX_DETUNE_STEP / 3); // para que en un tercio para de desafinar, que es cuando se activan los lfos
    // Serial.println("Detune step: " + String(detuneStep));
    for (int i = 0; i < MAX_OSCS_PER_LAYER; i++)
    {
        if (i < numOsc)
        {
            if (i == 0)
            {
                l.oscs[i].detune = 0;
            }
            else
            {
                if (i % 2 == 0)
                {
                    l.oscs[i].detune = (i / 2) * detuneStep; // Multiplicador para pares: 2->1, 4->2, 6->3...
                }
                else
                {
                    l.oscs[i].detune = -((i + 1) / 2) * detuneStep; // Multiplicador para impares: 1-> -1, 3-> -2, 5-> -3...
                }
            }
            // La profundidad del LFO también escala con la complejidad
            if (l.oscs[i].lfoTarget == LFO_TARGET_PITCH)
                l.oscs[i].modDepth = lfoDepthFrac * MAX_PITCH_MOD_DEPTH;
            else
                l.oscs[i].modDepth = lfoDepthFrac * MAX_FILTER_MOD_DEPTH;

            l.oscs[i].active = true;
        }
        else
        {
            l.oscs[i].active = false;
        }
    }

    for (int i = 0; i < MAX_LFOS_PER_LAYER; i++)
    {
        if (i < numLfo)
        {
            int globalLfoSlot = l.layerNumb * MAX_LFOS_PER_LAYER + i;
            l.lfos[i].frequency = 0.1f + globalLfoSlot * 0.137f; // paso "raro" a propósito, evita relaciones simples entre LFOs
            l.lfos[i].active = true;
        }
        else
        {
            l.lfos[i].active = false;
        }
    }

    l.layerNeedsRebuild = true;
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
    e.reverb_level = effectsEnabledConfig ? audioEffects.reverbLevel : 0.0f;
    e.reverb_liveness = effectsEnabledConfig ? audioEffects.reverbLiveness : 0.0f;
    e.reverb_damping = effectsEnabledConfig ? audioEffects.reverbDamping : 0.0f;
    audioEffects.reverbXoverHz = 3000;
    e.reverb_xover_hz = effectsEnabledConfig ? audioEffects.reverbXoverHz : 0.0f;

    e.chorus_level = effectsEnabledConfig ? audioEffects.chorusLevel : 0.0f;
    e.chorus_depth = effectsEnabledConfig ? audioEffects.chorusDepth : 0.0f;
    e.chorus_lfo_freq = effectsEnabledConfig ? audioEffects.chorusLfoFreq : 0.0f;
    audioEffects.chorusMaxDelay = 40;
    e.chorus_max_delay = effectsEnabledConfig ? audioEffects.chorusMaxDelay : 0.0f;

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
