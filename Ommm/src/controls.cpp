#include <Arduino.h>
#include <math.h>
#include <RP2040.h>

#include "hardwarePins.h"
#include "synth.h"
#include "controls.h"
#include "amy.h"
#include "sequence.h"
#include "oledInfo.h"
#include "synthEEPROM.h"

PotState potState[TOTAL_POTS];

LayerMenuPage currentLayerPage = PAGE_PITCH_FILTER_RES;
SelectedLayer selectedLayer = MENU_LAYER_0;
int lastComplexityLevel[NUM_LAYERS] = {-1, -1};
int currentMemorySlot = 0;
int loadMemorySlot = 0;
MemoryMenuAction memoryMenuAction = MEMORY_ACTION_FX_CONFIG;
bool effectsEnabledConfig = true;
bool effectsEnabledSelection = true;
constexpr float POT_SMOOTHING_ALPHA = 0.08f;

namespace
{
constexpr unsigned long BUTTON_DEBOUNCE_MS = 30;

struct ButtonDebouncer
{
    bool rawState = HIGH;
    bool stableState = HIGH;
    unsigned long rawChangedAt = 0;
};

bool wasButtonPressed(int pin, ButtonDebouncer &button)
{
    const unsigned long now = millis();
    const bool rawState = digitalRead(pin);

    if (rawState != button.rawState)
    {
        button.rawState = rawState;
        button.rawChangedAt = now;
    }

    if (now - button.rawChangedAt < BUTTON_DEBOUNCE_MS || rawState == button.stableState)
        return false;

    button.stableState = rawState;
    return button.stableState == LOW;
}
}

void initControls()
{
    for (int i = 0; i < TOTAL_POTS; i++)
    {
        pinMode(POT_PIN[i], INPUT);
    }
    for (int i = 0; i < NUM_NOTE_BUTTONS; i++)
    {
        pinMode(BUTTON_PIN[i], INPUT_PULLUP);
    }
    pinMode(BUTTON_A_PIN, INPUT_PULLUP);
    pinMode(BUTTON_B_PIN, INPUT_PULLUP);
    pinMode(BUTTON_C_PIN, INPUT_PULLUP);
}

// Devuelve true y llena outFraction01 (0.0-1.0) SOLO cuando corresponde actualizar.
// Mientras está "trabado", exige un movimiento > moveThreshold para destrabarse.
// Una vez destrabado, actualiza siempre (hasta que se vuelva a trabar con relockAllPots()).
bool readPotFraction(int pin, PotState &state, float &outFraction01, int moveThreshold = 5)
{
    int raw = analogRead(pin);

    if (!state.unlocked)
    {
        if (abs(raw - state.lastRaw) <= moveThreshold)
        {
            // state.lastRaw = raw; // sigue esperando el primer movimiento real
            return false;
        }
        state.unlocked = true;
    }

    state.filteredRaw += POT_SMOOTHING_ALPHA * (raw - state.filteredRaw);
    const int filtered = (int)roundf(state.filteredRaw);
    if (abs(filtered - state.lastRaw) <= moveThreshold)
        return false;

    outFraction01 = filtered / (float)ADC_MAX;
    state.lastRaw = filtered;
    return true;
}

bool readPot(int pin, PotState &state, float minValue, float maxValue, float &outMapped, int moveThreshold = 6)
{
    float frac;
    if (!readPotFraction(pin, state, frac, moveThreshold))
        return false;
    outMapped = minValue + frac * (maxValue - minValue);
    return true;
}

// Cuantiza una fracción 0-1 a un índice discreto (para elegir wave, filter type, etc.)
int quantizeFraction(float frac01, int numOptions)
{
    int idx = (int)(frac01 * numOptions);
    if (idx >= numOptions)
        idx = numOptions - 1;
    if (idx < 0)
        idx = 0;
    return idx;
}

void relockAllPots()
{
    for (int i = 0; i < TOTAL_POTS; i++)
    {
        potState[i].unlocked = false;
        potState[i].lastRaw = analogRead(POT_PIN[i]); // referencia = posición actual real
        potState[i].filteredRaw = potState[i].lastRaw;
    }
}

// Filtros y waveforms disponibles para las páginas discretas
const int NUM_FILTER_TYPES = 5; // NONE, LPF, BPF, HPF, LPF24 (confirmar orden real del enum)
const int NUM_WAVEFORMS = 4;
int waveformOptions[NUM_WAVEFORMS] = {SAW_DOWN, TRIANGLE, PULSE, NOISE};

void selectLayer(SelectedLayer layerIndex)
{
    introActive = 0;
    if (selectedLayer == layerIndex)
        return;
    selectedLayer = layerIndex;
    relockAllPots();
}

void selectPage(LayerMenuPage page)
{
    introActive = 0;
    if (currentLayerPage == page)
        return;
    currentLayerPage = page;
    if (page == PAGE_MEMORY)
    {
        memoryMenuAction = MEMORY_ACTION_FX_CONFIG;
        effectsEnabledSelection = effectsEnabledConfig;
        loadMemorySlot = currentMemorySlot;
    }
    relockAllPots();
}

void confirmMemoryOperation()
{
    switch (memoryMenuAction)
    {
    case MEMORY_ACTION_FX_CONFIG:
        if (effectsEnabledSelection != effectsEnabledConfig && saveEffectsEnabledConfig(effectsEnabledSelection))
            rp2040.reboot();
        break;
    case MEMORY_ACTION_LOAD:
        if (loadMemorySlot == -1)
        {
            setDefaultParameters();
            for (int i = 0; i < NUM_LAYERS; i++)
            {
                setLayerComplexity(layers[i], layers[i].complexity);
                assignHarmonyNotes(layers[i]);
                layers[i].layerNeedsRebuild = true;
                layers[i].harmonyNeedsRebuild = layers[i].harmonyMode == HARMONY_SEQUENCE;
                layers[i].sequenceSounding = false;
            }
            applyEffects();
        }
        else if (loadMemory(loadMemorySlot))
            currentMemorySlot = loadMemorySlot;
        break;
    case MEMORY_ACTION_SAVE:
        saveMemory(currentMemorySlot);
        break;
    }
}

void readButtons()
{
    static ButtonDebouncer buttonA;
    static ButtonDebouncer buttonB;
    static ButtonDebouncer buttonC;
    static ButtonDebouncer pageButtons[NUM_NOTE_BUTTONS];

    static LayerMenuPage lastSelectedLayerPage = PAGE_PITCH_FILTER_RES;
    static LayerMenuPage lastSelectedEffectsPage = PAGE_DISTORTION;
    const bool pressedA = wasButtonPressed(BUTTON_A_PIN, buttonA);
    const bool pressedB = wasButtonPressed(BUTTON_B_PIN, buttonB);
    const bool pressedC = wasButtonPressed(BUTTON_C_PIN, buttonC);
    bool pressedPage[NUM_NOTE_BUTTONS];
    for (int i = 0; i < NUM_NOTE_BUTTONS; i++)
        pressedPage[i] = wasButtonPressed(BUTTON_PIN[i], pageButtons[i]);

    // desactiva capas:
    if (pressedA)
    {
        if (selectedLayer == MENU_LAYER_0)
        {
            layers[0].active = !layers[0].active;
            layers[0].layerNeedsRebuild = true; // para que se aplique el cambio de estado
        }
    }
    if (pressedB)
    {
        if (selectedLayer == MENU_LAYER_1)
        {
            layers[1].active = !layers[1].active;
            layers[1].layerNeedsRebuild = true; // para que se aplique el cambio de estado
        }
    }

    // elije la capa y recuerda la página anterior de cada menu para volver a ella
    if (pressedA)
    {
        if (selectedLayer == MENU_EFFECTS)
        { // Si vengo de EFFECTS, recuerdo su página
            lastSelectedEffectsPage = currentLayerPage;
            selectPage(lastSelectedLayerPage);
        }
        // Serial.println("lastSelectedEffectsPage: " + String(lastSelectedEffectsPage));
        selectLayer(MENU_LAYER_0);
    }
    if (pressedB)
    {
        if (selectedLayer == MENU_EFFECTS)
        { // Si vengo de EFFECTS, recuerdo su página
            lastSelectedEffectsPage = currentLayerPage;
            selectPage(lastSelectedLayerPage);
        }
        selectLayer(MENU_LAYER_1);
    }
    if (pressedC)
    {
        if (selectedLayer != MENU_EFFECTS) // Si vengo de otro menu, recuerdo su página
            lastSelectedLayerPage = currentLayerPage;
        selectLayer(MENU_EFFECTS);
        selectPage(lastSelectedEffectsPage);
    }

    if (selectedLayer == MENU_EFFECTS)
    {
        if (pressedPage[0])
            selectPage(PAGE_DISTORTION);
        if (pressedPage[1])
            selectPage(PAGE_CHORUS);
        if (pressedPage[2])
            selectPage(PAGE_REVERB);
        if (pressedPage[3])
        {
            if (currentLayerPage == PAGE_MEMORY)
                confirmMemoryOperation();
            else
                selectPage(PAGE_MEMORY);
        }
    }
    else
    {
        if (pressedPage[0])
            selectPage(PAGE_PITCH_FILTER_RES);
        if (pressedPage[1])
            selectPage(PAGE_TYPE_WAVE_COMPLEX);
        if (pressedPage[2])
            selectPage(PAGE_VOL_HARMONY_SCALE);
        if (pressedPage[3])
            selectPage(PAGE_LENGTH_MELODY_TIEMPO);
    }
}

void readPots()
{
    float mapped, frac;
    bool changed = false;
    bool changedFX = false;
    if (selectedLayer == MENU_EFFECTS)
    {
        // Serial.println(currentLayerPage);
        switch (currentLayerPage)
        {
        case PAGE_DISTORTION:
        {
            if (readPotFraction(POT_PIN[0], potState[0], frac))
            {
                audioEffects.distType = quantizeFraction(frac, 2);
                changedFX = true;
            }
            if (readPot(POT_PIN[1], potState[1], 0.0f, 1.0f, mapped))
            {
                audioEffects.distMix = mapped; // 0 = dry, 1 = wet total
                changedFX = true;
            }
            if (readPot(POT_PIN[2], potState[2], 1.0f, 20.0f, mapped))
            {
                audioEffects.distDrive = mapped; // 1.0 = sin drive extra, >1 = más saturación
                changedFX = true;
            }
            break;
        }
        case PAGE_MEMORY:
        {
            if (readPotFraction(POT_PIN[0], potState[0], frac))
            {
                effectsEnabledSelection = quantizeFraction(frac, 2) != 0;
                memoryMenuAction = MEMORY_ACTION_FX_CONFIG;
            }
            if (readPotFraction(POT_PIN[1], potState[1], frac))
            {
                currentMemorySlot = quantizeFraction(frac, NUM_MEMORY_SLOTS);
                memoryMenuAction = MEMORY_ACTION_SAVE;
            }
            if (readPotFraction(POT_PIN[2], potState[2], frac))
            {
                loadMemorySlot = quantizeFraction(frac, NUM_MEMORY_SLOTS + 1) - 1;
                memoryMenuAction = MEMORY_ACTION_LOAD;
            }

            break;
        }
        case PAGE_CHORUS:
        {
            if (!effectsEnabledConfig)
                break;
            if (readPot(POT_PIN[0], potState[0], 0.0f, 1.0f, mapped))
            {
                audioEffects.chorusLevel = mapped; // Nivel de mezcla (0.0 a 1.0)
                changedFX = true;
            }
            if (readPot(POT_PIN[1], potState[1], 0.01f, 5.0f, mapped))
            {
                audioEffects.chorusLfoFreq = mapped; // Velocidad del LFO en Hz
                changedFX = true;
            }
            if (readPot(POT_PIN[2], potState[2], 0.0f, 1.0f, mapped))
            {
                audioEffects.chorusDepth = mapped; // Profundidad del LFO
                changedFX = true;
            }
            break;
        }
        case PAGE_REVERB:
        {
            if (!effectsEnabledConfig)
                break;
            if (readPot(POT_PIN[0], potState[0], 0.0f, 1.0f, mapped))
            {
                audioEffects.reverbLevel = mapped; // Cantidad de reverb en la mezcla final
                changedFX = true;
            }
            if (readPot(POT_PIN[1], potState[1], 0.0f, 1.0f, mapped))
            {
                audioEffects.reverbDamping = mapped; // Absorción de altas frecuencias
                changedFX = true;
            }
            if (readPot(POT_PIN[2], potState[2], 0.0f, 1.0f, mapped))
            {
                audioEffects.reverbLiveness = mapped; // Tamaño/duración de la cola de la reverb
                changedFX = true;
            }

            break;
        }
        }
        if (changedFX)
            applyEffects();
    }
    else
    {
        Layer &l = layers[selectedLayer];

        switch (currentLayerPage)
        {
        case PAGE_PITCH_FILTER_RES:
        {
            if (readPot(POT_PIN[0], potState[0], 0.5f, 10.0f, mapped))
            {
                l.resonance = mapped;
                changed = true;
            } // rango de Q según la doc de AMY
            if (readPot(POT_PIN[1], potState[1], FILTER_FREQ_MIN, FILTER_FREQ_MAX, mapped))
            {
                mapped = ((long)mapped * mapped) / (2048);
                l.filterFreq = mapped;
                changed = true;
            }
            if (readPot(POT_PIN[2], potState[2], MIDI_NOTE_MIN, MIDI_NOTE_MAX, mapped))
            {
                l.baseNote = mapped;

                if (l.harmonyMode == HARMONY_SEQUENCE)
                {
                    l.harmonyNeedsRebuild = true;
                }
                else
                {
                    changed = true;
                }
            }
            break;
        }

        case PAGE_TYPE_WAVE_COMPLEX:
        {
            if (readPotFraction(POT_PIN[0], potState[0], frac))
            {
                l.filterType = quantizeFraction(frac, NUM_FILTER_TYPES);
                changed = true;
                if (l.filterType == 0)
                {
                    setLayerComplexity(l, l.complexity); // porque hay qeu reasignar los LFOS al pitch
                }
            }
            if (readPot(POT_PIN[1], potState[1], 0.0f, COMPLEXITY_MAX, mapped))
            {
                int newLevel = (int)round(mapped);
                l.complexity = newLevel;
                setLayerComplexity(l, newLevel);
            }
            if (readPotFraction(POT_PIN[2], potState[2], frac))
            {
                int newWave = waveformOptions[quantizeFraction(frac, NUM_WAVEFORMS)];
                l.waveform = newWave;
                changed = true;
            }

            break;
        }

        case PAGE_VOL_HARMONY_SCALE:
        {
            if (readPot(POT_PIN[0], potState[0], 0, 10.0f, mapped)) // umbral más grande
            {
                l.volume = mapped;
                changed = true;
            }
            int harmonyParamMax =  (l.harmonyMode == HARMONY_CHORD)  ? NUM_CHORD_TYPES
                                                                                         : (l.harmonyMode == HARMONY_SEQUENCE) ? NUM_SCALE_TYPES
                                                                                                                               : 0;
            if (l.harmonyMode == HARMONY_INTERVAL)
            {
                float mapped;
                if (readPot(POT_PIN[1], potState[1], -24.0f, 24.0f, mapped))
                {
                    l.harmonyIntervalSemitones = mapped;
                    assignHarmonyNotes(l);
                    generateSequence(l); // regenera melodía completa con la nueva densidad
                    changed = 1;
                }
            }
            else
            {
                if (readPotFraction(POT_PIN[1], potState[1], frac))
                {
                    int newHarmonyParamIndex = quantizeFraction(frac, harmonyParamMax);
                    if (newHarmonyParamIndex != l.harmonyParam)
                    {
                        l.harmonyParam = newHarmonyParamIndex;
                        l.harmonyNeedsRebuild = true;
                    }
                }
            }
            if (readPotFraction(POT_PIN[2], potState[2], frac))
            {
                int newHarmonyMode = quantizeFraction(frac, NUM_HARMONY_MODE);
                if (newHarmonyMode != l.harmonyMode)
                {
                    l.harmonyMode = (HarmonyMode)newHarmonyMode;
                    l.harmonyNeedsRebuild = true;
                }
            }
            break;
        }
        case PAGE_LENGTH_MELODY_TIEMPO:
        {
            if (readPot(POT_PIN[0], potState[0], 0.0f, MAX_SEQUENCE_STEPS, mapped))
            {
                l.sequenceLength = mapped;
                // changed = true;
            }
            if (readPot(POT_PIN[1], potState[1], 1, 1024, mapped))
            {
                l.millisPerStep = mapped;
            }
            if (readPot(POT_PIN[2], potState[2], 0, 1024, mapped))
            {
                l.density = mapped;
                generateSequence(l); // regenera melodía completa con la nueva densidad
            }
        }
        break;
        }
        if (changed && l.active == true)
        {
            applyLayerParams(l);
            introActive = 0; // desactiva la intro
        }
    }
}