#include <Arduino.h>
#include <EEPROM.h>

#include "synth.h"
#include "controls.h"
#include "synthEEPROM.h"

#define PRESET_MAGIC 0x53594E32         // "SYN2", formato compacto sin arrays de secuencia
#define EFFECTS_CONFIG_MAGIC 0x46584331 // "FXC1", configuración global independiente de presets
#define EEPROM_STORAGE_SIZE 4096
#define PRESET_VERSION 2

struct EffectsConfigData
{
    uint32_t magic;
    uint8_t enabled;
};

static_assert(sizeof(PresetData) * NUM_MEMORY_SLOTS + sizeof(EffectsConfigData) <= EEPROM_STORAGE_SIZE,
              "Preset slots exceed RP2040 EEPROM capacity");

int effectsConfigAddress()
{
    return sizeof(PresetData) * NUM_MEMORY_SLOTS;
}

extern AudioEffects audioEffects; // ajustá el nombre si tu variable global se llama distinto

LayerPresetData saveLayerParameters(const Layer &layer)
{
    LayerPresetData data{};
    data.baseNote = layer.baseNote;
    data.filterFreq = layer.filterFreq;
    data.resonance = layer.resonance;
    data.pan = layer.pan;
    data.volume = layer.volume;
    data.harmonyIntervalSemitones = layer.harmonyIntervalSemitones;
    data.seed = layer.seed;
    data.complexity = layer.complexity;
    data.waveform = layer.waveform;
    data.filterType = layer.filterType;
    data.harmonyParam = layer.harmonyParam;
    data.millisPerStep = layer.millisPerStep;
    data.sequenceLength = layer.sequenceLength;
    data.density = layer.density;
    data.active = layer.active ? 1 : 0;
    data.harmonyMode = (uint8_t)layer.harmonyMode;
    return data;
}

void loadLayerParameters(Layer &layer, const LayerPresetData &data, int layerIndex)
{
    const int previousStep = layer.currentStep;
    layer = Layer{};
    layer.layerNumb = layerIndex;
    layer.baseNote = data.baseNote;
    layer.filterFreq = data.filterFreq;
    layer.resonance = data.resonance;
    layer.pan = data.pan;
    layer.volume = data.volume;
    layer.harmonyIntervalSemitones = data.harmonyIntervalSemitones;
    layer.seed = data.seed;
    layer.complexity = data.complexity;
    layer.waveform = data.waveform;
    layer.filterType = data.filterType;
    layer.harmonyParam = data.harmonyParam;
    layer.millisPerStep = data.millisPerStep;
    layer.sequenceLength = constrain(data.sequenceLength, 1, MAX_SEQUENCE_STEPS);
    layer.density = constrain(data.density, 0, DENSITY_MAX);
    layer.active = data.active != 0;
    layer.harmonyMode = data.harmonyMode < NUM_HARMONY_MODE ? (HarmonyMode)data.harmonyMode : HARMONY_NONE;
    layer.currentStep = previousStep % layer.sequenceLength;
    layer.sequenceSounding = false;

    setLayerComplexity(layer, layer.complexity);
    layer.layerNeedsRebuild = true;
    layer.harmonyNeedsRebuild = true;
}

void initEEPROM()
{
    // El RP2040 emula EEPROM sobre un sector de flash: hay que reservar
    // el tamaño total UNA vez, al arrancar, antes de leer o escribir nada.
    EEPROM.begin(sizeof(PresetData) * NUM_MEMORY_SLOTS + sizeof(EffectsConfigData));
}

int slotAddress(int slot)
{
    return slot * sizeof(PresetData);
}

bool saveMemory(int slot)
{
    if (slot < 0 || slot >= NUM_MEMORY_SLOTS)
        return false;

    PresetData data{};
    data.magic = PRESET_MAGIC;
    data.version = PRESET_VERSION;

    for (int i = 0; i < NUM_LAYERS; i++)
        data.layers[i] = saveLayerParameters(layers[i]);

    data.audioEffects = audioEffects;

    EEPROM.put(slotAddress(slot), data);
    return EEPROM.commit(); // acá recién se escribe físicamente a flash
}

bool loadMemory(int slot)
{
    if (slot < 0 || slot >= NUM_MEMORY_SLOTS)
        return false;

    PresetData data;
    EEPROM.get(slotAddress(slot), data);

    if (data.magic != PRESET_MAGIC || data.version != PRESET_VERSION)
        return false; // slot vacío o corrupto: no tocamos nada

    for (int i = 0; i < NUM_LAYERS; i++)
    {
        loadLayerParameters(layers[i], data.layers[i], i);
        processLayerRebuilds(layers[i]);
    }

    audioEffects = data.audioEffects;
    applyEffects();

    return true;
}

bool isSlotUsed(int slot)
{
    if (slot < 0 || slot >= NUM_MEMORY_SLOTS)
        return false;

    uint32_t magic;
    EEPROM.get(slotAddress(slot), magic); // magic es el primer campo del struct, alineado al inicio
    if (magic != PRESET_MAGIC)
        return false;

    uint8_t version;
    EEPROM.get(slotAddress(slot) + sizeof(uint32_t), version);
    return version == PRESET_VERSION;
}

bool loadEffectsEnabledConfig()
{
    EffectsConfigData data;
    EEPROM.get(effectsConfigAddress(), data);
    if (data.magic != EFFECTS_CONFIG_MAGIC)
        return false;

    effectsEnabledConfig = data.enabled != 0;
    effectsEnabledSelection = effectsEnabledConfig;
    return true;
}

bool saveEffectsEnabledConfig(bool enabled)
{
    EffectsConfigData data = {EFFECTS_CONFIG_MAGIC, (uint8_t)(enabled ? 1 : 0)};
    EEPROM.put(effectsConfigAddress(), data);
    if (!EEPROM.commit())
        return false;

    effectsEnabledConfig = enabled;
    effectsEnabledSelection = enabled;
    return true;
}