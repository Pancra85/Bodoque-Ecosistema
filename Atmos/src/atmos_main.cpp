#include <Arduino.h>
#include <AMY-Arduino.h>
#include <I2S.h>
#include <math.h>
#include "hardwarePins.h"

// BODOQUE ATMOS V0.8.2 - ATMOS / CRYSTAL / PULSE / TEXTURE - RP2040 + AMY + PT8211
// Basado en la V0.6 que ya funciona en el dispositivo.
// Cuatro motores: ATMOS, CRYSTAL, PULSE y TEXTURE.
// A corto = ambiente; A largo + potes = PITCH / RING / FX POR MOTOR.
// A+B durante un segundo cambia el motor.
// TEXTURE granular y golpe metalico especial en PULSE.

I2S audioOutput(OUTPUT, I2S_BCLK, I2S_DATA);

constexpr uint8_t MUTE_PIN = 17;
constexpr uint32_t SAMPLE_RATE = 44100;
constexpr uint8_t NUM_LAYERS = 4;
constexpr uint8_t NUM_SPARKS = 2;
// Osciladores AMY: 0-3 capas, 4-5 destellos originales,
// 6 fantasma, 7 pulso grave, 8-9 campana inarmonica.
constexpr uint8_t GHOST_OSC = 6;
constexpr uint8_t PULSE_OSC = 7;
constexpr uint8_t BELL_A_OSC = 8;
constexpr uint8_t BELL_B_OSC = 9;
constexpr uint32_t CONTROL_MS = 40;
constexpr uint32_t DEBOUNCE_MS = 45;
constexpr uint32_t LONG_PRESS_MS = 750;
constexpr float TWO_PI_F = 6.28318530718f;
constexpr int32_t OUTPUT_GAIN = 6;  // Ganancia confirmada en V0.6
constexpr uint32_t ENGINE_CHORD_MS = 1000;
constexpr uint32_t CHORD_WINDOW_MS = 400;

// Se reutilizan los 10 osciladores de V0.6. No se reservan nuevos buffers.
enum SoundEngine : uint8_t {
    ENGINE_ATMOS = 0, ENGINE_CRYSTAL = 1,
    ENGINE_PULSE = 2, ENGINE_TEXTURE = 3
};
const char* engineNames[4] = {"ATMOS", "CRYSTAL", "PULSE", "TEXTURE"};
SoundEngine activeEngine = ENGINE_ATMOS;
float crystalBlend = 0.0f;    // 0 = ATMOS, 1 = CRYSTAL
// Transicion en dos fases: atenuar 1.2 s, cambiar motor, subir 2 s.
// Se conservan las colas del efecto, pero no se mezclan nuevas notas de motores distintos.
SoundEngine requestedEngine = ENGINE_ATMOS;
bool engineSwitching = false;
bool engineSwitchDown = false;
float engineGate = 1.0f;
uint32_t engineSwitchAt = 0;

bool pulseEnabled[4] = {true, true, true, true};
bool textureEnabled[4] = {true, true, true, true};
float pulseEnv[4] = {};
float textureEnv[4] = {};
float pulseFreq[4] = {55.0f, 110.0f, 164.81f, 220.0f};
uint32_t nextPulseStepAt = 0;
uint32_t nextTextureEventAt = 0;
uint8_t pulseStep = 7;
uint8_t pulsePattern = 0;
float pulseAccent = 0.0f;
float textureNoiseLevel = 0.0f; // legado, ahora sin ruido blanco continuo
float pulseClickEnv = 0.0f;
// Granulador ligero: 4 granos cortos de ruido filtrado y ondas inarmonicas con
// ventana de amplitud suave. Sin buffers de muestras ni osciladores AMY nuevos.
struct TextureGrain {
    // Variables enteras para no ejecutar aritmetica flotante por muestra.
    uint32_t samplesLeft = 0, samplesTotal = 1;
    uint32_t phase = 0, phaseStep = 0;
    int32_t filtered = 0;
    uint16_t filterQ15 = 2000, gainQ15 = 0, panQ15 = 16384;
    uint16_t envQ15 = 0, envStep = 1;
    uint8_t kind = 0;
};
TextureGrain grains[4];
uint32_t nextGrainAt = 0;
uint8_t nextGrainIndex = 0;
float textureGrainGain = 0.0f;
uint8_t textureFlavor = 0;
// 0: velos de aire; 1: metal oscuro; 2: resonancia; 3: bruma.

bool chordArmed = false;
bool chordTriggered = false;
uint32_t chordStartedAt = 0;

// Controles independientes para los cuatro tipos de eventos CRYSTAL:
// 0 cristal, 1 gota, 2 campana, 3 eco fantasma.
bool crystalEnabled[4] = {true, true, true, true};
bool crystalSaved[4] = {true, true, true, true};
bool crystalSoloActive = false;
uint8_t crystalSoloIndex = 0;
bool crystalPaused = false;
uint32_t crystalNextEventAt = 0;
uint32_t crystalLastEventAt = 0;

const float baseFrequency[NUM_LAYERS] = {110.0f, 164.81f, 220.0f, 329.63f};
const float layerPan[NUM_LAYERS] = {0.30f, 0.70f, 0.43f, 0.57f};
const float layerWeight[NUM_LAYERS] = {1.0f, 0.80f, 0.48f, 0.28f};
const char *modeNames[3] = {"CALMA", "ORBITA", "AURORA"};

enum AtmosMode : uint8_t { CALMA = 0, ORBITA = 1, AURORA = 2 };
AtmosMode currentMode = CALMA;
bool layerEnabled[NUM_LAYERS] = {true, true, false, false};
float layerLevel[NUM_LAYERS] = {};
float sparkLevel[NUM_SPARKS] = {};
// 0 = cristal original, 1 = gota resonante.
uint8_t sparkKind[NUM_SPARKS] = {};
float sparkPitch[NUM_SPARKS] = {660.0f, 880.0f};
float sparkPan[NUM_SPARKS] = {0.20f, 0.80f};

// Evolucion armonica. Grados de La menor pentatonica (+ octavas).
const int8_t scaleSemitones[] = {0, 3, 5, 7, 10, 12, 15, 17, 19, 22, 24, 27, 29};
const int8_t startingDegree[NUM_LAYERS] = {0, 3, 5, 8}; // A2 E3 A3 E4
float currentNote[NUM_LAYERS] = {0, 7, 12, 19};
float targetNote[NUM_LAYERS] = {0, 7, 12, 19};
uint32_t nextHarmonyAt = 0;

// Eventos generativos, volumenes de ataque/caida y temporizadores.
uint32_t nextCrystalAt = 0, nextGhostAt = 0, nextPulseAt = 0;
uint32_t nextMicroAt = 0;
uint32_t lastAtmosEventAt = 0;
constexpr uint32_t MIN_EVENT_GAP_MS = 5000;

enum EmotionalStage : uint8_t { REPOSO = 0, EXPANSION = 1, TENSION = 2, RESOLUCION = 3 };
EmotionalStage emotionalStage = REPOSO;
const char* stageNames[4] = {"REPOSO", "EXPANSION", "TENSION", "RESOLUCION"};
uint32_t nextStageAt = 0;
float stageEnergy = 0.0f;
float stageEnergyTarget = 0.0f;
float bellLevel = 0.0f;
float bellDecay = 0.967f; // Automaticas: cortas. Boton C en CRYSTAL: cola larga.
float ghostLevel = 0.0f, ghostTarget = 0.0f;
uint32_t ghostReleaseAt = 0;
float pulseLevel = 0.0f;
float windLevel = 0.0f;

// Ruido determinista ligero y filtro pasa-banda para "viento".
// Se calcula a tasa de audio, sin buffers adicionales ni bibliotecas.
uint32_t noiseState = 0xC6BC2795UL;
float windLowFast = 0.0f, windLowSlow = 0.0f;
uint8_t nextSpark = 0;
float masterVolume = 0.0f, movement = 0.0f, space = 0.0f;
// Edicion expresiva: A mantenido 750 ms. Los valores se conservan al soltar.
bool expressiveShift = false;
float expressivePitch = 0.0f;       // -24 .. +24 semitonos
float expressiveRing = 0.0f;       // 0 limpio, 1 ring modulation
float expressiveFx[4] = {};         // Ajuste propio de cada motor (persistente)
float pitchFactor = 1.0f;
float rawPot[3] = {};
float shiftStartPot[3] = {};
bool shiftPotArmed[3] = {};
float normalHeld[3] = {};
bool normalPickup[3] = {true, true, true};
float normalPrevious[3] = {};
// Ring modulation entera a tasa de audio (oscilador triangular bipolar).
// Evitamos sinf() y powf() por muestra.
uint32_t ringPhase = 0;
uint32_t ringPhaseStep = 0;
int32_t ringMixQ15 = 0;
int32_t feedbackQ15 = 18022;
int32_t delayWetQ15 = 0, reverbWetQ15 = 0;
// Efectos de A+P3: ATMOS=shimmer armonico, CRYSTAL=comb,
// PULSE=bitcrusher, TEXTURE=frequency shifting por SSB aproximada.
int32_t fxMixQ15 = 0;
uint8_t bitDepth = 16, bitHold = 1;
uint8_t bitHoldCounter = 0;
int16_t bitHeldL = 0;
// Comb de caracter cristalino: 2 retardos cortos, buffers separados.
int16_t fxCombL[601] = {}, fxCombR[673] = {};
uint16_t fxCombPosL = 0, fxCombPosR = 0;
// Hilbert FIR corto + tabla de fase seno/coseno. Desplazamiento espectral
// aproximado (no sustituye un frequency shifter de estudio profesional).
int16_t shiftHistoryL[16] = {}, shiftHistoryR[16] = {};
uint8_t shiftHistoryPos = 0;
int16_t sineTableQ15[256] = {};
uint32_t shiftPhase = 0, shiftPhaseStep = 0;
// Shimmer ligero: armónico octavado de la nota principal usando la voz 6.
float shimmerLevel = 0.0f;

float lfoPhase = 0.0f, breathPhase = 0.0f;
uint32_t lastControlAt = 0, lastDebugAt = 0, audioStartedAt = 0;
uint32_t maxAudioBlockUs = 0;
uint32_t ledParamUntil = 0;
float ledParamValue = 0.0f;
uint8_t ledParamIndex = 0;
bool speakerEnabled = false;

const uint8_t allButtons[7] = {
    BUTTON_PIN[0], BUTTON_PIN[1], BUTTON_PIN[2], BUTTON_PIN[3],
    BUTTON_A_PIN, BUTTON_B_PIN, BUTTON_C_PIN
};
bool buttonStable[7] = {};
bool buttonRaw[7] = {};
uint32_t buttonChangedAt[7] = {};
uint32_t buttonPressedAt[7] = {};
bool buttonHoldFired[7] = {};

// Las pulsaciones cortas ocurren al SOLTAR. Las largas no generan corto.
bool soloActive = false;
uint8_t soloLayer = 0;
bool savedLayers[NUM_LAYERS] = {true, true, false, false};
bool harmonyFrozen = false;
bool quietMode = false;
bool manualExpansion = false;

// "Director musical": controla la densidad de las capas, sin cambiar el
// estado elegido con los botones. Cada capa tiene una respiracion independiente.
uint32_t nextRestAt = 0;
uint32_t restEndsAt = 0;
bool deepRest = false;
float restBlend = 0.0f;
float layerBreathPhase[NUM_LAYERS] = {0.0f, 1.3f, 2.5f, 4.2f};
float layerBreathSpeed[NUM_LAYERS] = {0.021f, 0.017f, 0.013f, 0.026f};
float layerBreathShape[NUM_LAYERS] = {1,1,1,1};
uint32_t nextBreathChangeAt = 0;
float globalBreath = 1.0f;

// Buffers estaticos: no se reserva memoria en el bucle de audio.
// Delay: 16384 muestras por canal (aprox. 371 ms de almacenamiento).
constexpr uint16_t DELAY_SIZE = 16384;
int16_t delayL[DELAY_SIZE] = {};
int16_t delayR[DELAY_SIZE] = {};
uint16_t delayWritePos = 0;

// Cuatro lineas de reverberacion de distintas longitudes.
int16_t combL1[1423] = {}, combL2[1867] = {};
int16_t combR1[1613] = {}, combR2[1999] = {};
uint16_t combPosL1 = 0, combPosL2 = 0, combPosR1 = 0, combPosR2 = 0;

// Difusion adicional mediante dos filtros all-pass.
int16_t allpassL[367] = {}, allpassR[421] = {};
uint16_t allpassPosL = 0, allpassPosR = 0;

float delayWet = 0.0f, reverbWet = 0.0f, delayFeedback = 0.30f;
float lastAppliedPan[NUM_LAYERS] = {0.30f, 0.70f, 0.43f, 0.57f};

inline float clamp01(float x) {
    return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x);
}

inline int16_t clamp16(int32_t x) {
    if (x > 32767) return 32767;
    if (x < -32768) return -32768;
    return (int16_t)x;
}

// Compresion progresiva del pico. Evita el wrapping de 16 bits;
// no reemplaza un limitador profesional.
inline int16_t softLimit(int32_t x) {
    bool negative = x < 0;
    int32_t a = negative ? -x : x;
    if (a > 22000) a = 22000 + (a - 22000) / 5;
    if (a > 32767) a = 32767;
    return (int16_t)(negative ? -a : a);
}

// Quantizacion controlada de PULSE, con retencion de muestra opcional.
inline int16_t crushSample(int16_t input, int16_t &held, uint8_t &counter) {
    if (counter == 0) {
        const int32_t step = 1L << (16-bitDepth);
        const int32_t quantized = ((int32_t)input / step) * step;
        held = clamp16(quantized);
    }
    if (++counter >= bitHold) counter = 0;
    return held;
}

// Comb afinado para CRYSTAL, con realimentacion acotada.
inline int16_t crystalComb(int16_t x, int16_t* b, uint16_t size, uint16_t &pos) {
    const int16_t delayed = b[pos];
    const int32_t feedback = 10000 + ((int32_t)fxMixQ15 * 12000 >> 15);
    b[pos] = clamp16((int32_t)x / 2 + ((int32_t)delayed * feedback >> 15));
    if (++pos >= size) pos = 0;
    return clamp16((int32_t)x + (((int32_t)delayed * fxMixQ15) >> 15));
}

// FIR Hilbert 15 taps: desplazamiento de fase aproximado de 90 grados.
// Convierte a banda lateral unica mediante seno/coseno (sin sinf por muestra).
inline int16_t shiftSideband(int16_t sample, int16_t* history, uint8_t pos,
                              int32_t cosine, int32_t sine) {
    history[pos] = sample;
    const uint8_t c = (pos - 7U) & 15U;
    const int32_t real = history[c];
    const int32_t hilbert =
        ((int32_t)(history[(pos-6U)&15U] - history[(pos-8U)&15U]) * 19800L +
         (int32_t)(history[(pos-4U)&15U] - history[(pos-10U)&15U]) * 5200L +
         (int32_t)(history[(pos-2U)&15U] - history[(pos-12U)&15U]) * 1900L +
         (int32_t)(history[pos] - history[(pos-14U)&15U]) * 450L) >> 15;
    return clamp16((real*cosine - hilbert*sine) >> 15);
}

inline int16_t applyRing(int16_t x, int32_t modQ15) {
    if (ringMixQ15 == 0) return x;
    // Producto de la portadora por un oscilador bipolar; mezcla dry/wet.
    const int32_t wet = ((int32_t)x * modQ15) >> 15;
    return clamp16((((int32_t)x * (32768-ringMixQ15)) +
                    wet * ringMixQ15) >> 15);
}

float readPot(uint8_t pin) {
    return clamp01((float)analogRead(pin) / 4095.0f);
}

void sendOscFrequency(uint8_t osc, float frequency) {
    amy_event e = amy_default_event();
    e.osc = osc;
    float hz = frequency * pitchFactor;
    if (hz < 20.0f) hz = 20.0f;
    if (hz > 12000.0f) hz = 12000.0f;
    e.freq_coefs[COEF_CONST] = hz;
    amy_add_event(&e);
}

void sendOscPan(uint8_t osc, float pan) {
    amy_event e = amy_default_event();
    e.osc = osc;
    e.pan_coefs[COEF_CONST] = clamp01(pan);
    amy_add_event(&e);
}

void sendOscAmplitude(uint8_t osc, float amplitude) {
    amy_event e = amy_default_event();
    e.osc = osc;
    e.amp_coefs[COEF_CONST] = clamp01(amplitude);
    amy_add_event(&e);
}

void createOscillator(uint8_t osc, float frequency, float pan) {
    amy_event e = amy_default_event();
    e.osc = osc;
    e.wave = SINE;
    float hz = frequency * pitchFactor;
    if (hz < 20.0f) hz = 20.0f;
    if (hz > 12000.0f) hz = 12000.0f;
    e.freq_coefs[COEF_CONST] = hz;
    e.amp_coefs[COEF_CONST] = 0.0f;
    e.pan_coefs[COEF_CONST] = pan;
    e.velocity = 1.0f;
    amy_add_event(&e);
}

void setupControls() {
    analogReadResolution(12);
    for (uint8_t i = 0; i < 7; ++i) {
        pinMode(allButtons[i], INPUT_PULLUP);
        buttonRaw[i] = digitalRead(allButtons[i]) == LOW;
        buttonStable[i] = buttonRaw[i];
    }
    for (uint8_t i = 0; i < NUM_LAYERS; ++i) {
        pinMode(LED_BUTTON_PIN[i], OUTPUT);
        digitalWrite(LED_BUTTON_PIN[i], LOW);
    }
    for (uint8_t i = 0; i < NUM_X_LEDS; ++i) {
        pinMode(LED_X_PIN[i], OUTPUT);
        digitalWrite(LED_X_PIN[i], LOW);
    }
    pinMode(LED_ESTRELLA_PIN, OUTPUT);
    digitalWrite(LED_ESTRELLA_PIN, LOW);
}

// Debounce y eventos: toque corto al soltar; largo una sola vez por pulsacion.
// Esto evita activar una capa y luego hacer "solo" con el mismo gesto.
enum ButtonAction : uint8_t { NO_ACTION, SHORT_PRESS, LONG_PRESS };
ButtonAction readButtonAction(uint8_t index) {
    const bool raw = digitalRead(allButtons[index]) == LOW;
    const uint32_t now = millis();
    if (raw != buttonRaw[index]) {
        buttonRaw[index] = raw;
        buttonChangedAt[index] = now;
    }
    if ((uint32_t)(now - buttonChangedAt[index]) >= DEBOUNCE_MS &&
        raw != buttonStable[index]) {
        buttonStable[index] = raw;
        if (raw) {
            buttonPressedAt[index] = now;
            buttonHoldFired[index] = false;
        } else if (!buttonHoldFired[index]) {
            return SHORT_PRESS;
        }
    }
    if (buttonStable[index] && !buttonHoldFired[index] &&
        (uint32_t)(now - buttonPressedAt[index]) >= LONG_PRESS_MS) {
        buttonHoldFired[index] = true;
        return LONG_PRESS;
    }
    return NO_ACTION;
}

void enterOrLeaveSolo(uint8_t layer) {
    if (soloActive && soloLayer == layer) {
        for (uint8_t i=0; i<NUM_LAYERS; ++i) layerEnabled[i] = savedLayers[i];
        soloActive = false;
        Serial.println("SOLO desactivado: capas restauradas");
        return;
    }
    if (!soloActive) {
        for (uint8_t i=0; i<NUM_LAYERS; ++i) savedLayers[i] = layerEnabled[i];
    }
    soloActive = true;
    soloLayer = layer;
    for (uint8_t i=0; i<NUM_LAYERS; ++i) layerEnabled[i] = (i == layer);
    Serial.print("SOLO capa "); Serial.println(layer + 1);
}

void resetLayers() {
    soloActive = false;
    layerEnabled[0] = true;
    layerEnabled[1] = true;
    layerEnabled[2] = false;
    layerEnabled[3] = false;
    Serial.println("Capas restablecidas");
}

void triggerSpark() {
    static const float notes[] = {659.25f, 783.99f, 987.77f, 1174.66f, 1318.51f};
    const uint8_t spark = nextSpark;
    nextSpark = (nextSpark + 1) % NUM_SPARKS;
    sparkLevel[spark] = 1.0f;
    sparkKind[spark] = 0;
    sparkPitch[spark] = notes[random(0, 5)];
    sparkPan[spark] = random(15, 86) / 100.0f;
    sendOscFrequency(NUM_LAYERS + spark, sparkPitch[spark]);
    sendOscPan(NUM_LAYERS + spark, sparkPan[spark]);
    Serial.println("Destello espacial");
}

// En CRYSTAL, C dispara una campana doble, distinta del destello de ATMOS.
// Reutiliza los osciladores 8 y 9; no consume mas RAM ni voces AMY.
void triggerCrystalChime() {
    static const float fundamentals[] = {523.25f, 587.33f, 659.25f, 783.99f};
    const float fundamental = fundamentals[random(0, 4)];
    sendOscFrequency(BELL_A_OSC, fundamental);
    sendOscFrequency(BELL_B_OSC, fundamental * 2.71f);
    bellLevel = 1.0f;
    bellDecay = 0.986f; // Cola mas larga que la campana generativa.
    Serial.println("CRYSTAL: campana doble manual");
}

// Frecuencia musical a partir de semitonos relativos a La2 = 110 Hz.
float noteToHz(float semitones) {
    return 110.0f * powf(2.0f, semitones / 12.0f);
}

uint32_t randomInterval(uint16_t minSeconds, uint16_t maxSeconds) {
    return (uint32_t)random(minSeconds, maxSeconds + 1) * 1000UL;
}

// Nueva textura: gota afinada con descenso de tono y caida progresiva.
// Reutiliza una de las dos voces de destello; no agrega osciladores AMY.
void triggerResonantDrop() {
    const uint8_t spark = nextSpark;
    nextSpark = (nextSpark + 1) % NUM_SPARKS;
    const float notes[] = {392.0f, 440.0f, 523.25f, 587.33f, 659.25f};
    sparkKind[spark] = 1;
    sparkPitch[spark] = notes[random(0, 5)];
    sparkPan[spark] = random(15, 86) / 100.0f;
    sparkLevel[spark] = 1.0f;
    sendOscFrequency(NUM_LAYERS + spark, sparkPitch[spark] * 1.18f);
    sendOscPan(NUM_LAYERS + spark, sparkPan[spark]);
    Serial.println("Gota resonante");
}

// Cuatro estados musicales de duracion variable. No son pasos de secuenciador.
void updateEmotionalStage(uint32_t now) {
    if ((int32_t)(now - nextStageAt) < 0) return;
    emotionalStage = (EmotionalStage)(((uint8_t)emotionalStage + 1) % 4);
    if (emotionalStage == REPOSO) {
        stageEnergyTarget = 0.05f;
        nextStageAt = now + randomInterval(36, 65);
    } else if (emotionalStage == EXPANSION) {
        stageEnergyTarget = 0.45f;
        nextStageAt = now + randomInterval(34, 62);
    } else if (emotionalStage == TENSION) {
        stageEnergyTarget = 0.85f;
        nextStageAt = now + randomInterval(22, 43);
    } else {
        stageEnergyTarget = 0.18f;
        nextStageAt = now + randomInterval(30, 52);
    }
    Serial.print("Etapa musical: ");
    Serial.println(stageNames[emotionalStage]);
}

bool canTriggerAtmosEvent(uint32_t now) {
    return (uint32_t)(now - lastAtmosEventAt) >= MIN_EVENT_GAP_MS;
}

void markAtmosEvent(uint32_t now) {
    lastAtmosEventAt = now;
}

void scheduleGenerativeEvents(uint32_t now) {
    nextHarmonyAt = now + randomInterval(18, 32);
    nextCrystalAt = now + randomInterval(13, 27);
    nextGhostAt = now + randomInterval(23, 46);
    nextPulseAt = now + randomInterval(26, 48);
    nextMicroAt = now + randomInterval(14, 29);
}

void resetHarmony() {
    const float start[NUM_LAYERS] = {0.0f, 7.0f, 12.0f, 19.0f};
    for (uint8_t i = 0; i < NUM_LAYERS; ++i) {
        targetNote[i] = start[i];
        // No reiniciar currentNote: la transicion se mantiene suave.
    }
    scheduleGenerativeEvents(millis());
    emotionalStage = REPOSO;
    stageEnergy = 0.0f;
    stageEnergyTarget = 0.05f;
    nextStageAt = millis() + randomInterval(40, 65);
    Serial.println("Evolucion armonica reiniciada");
}

// La actividad generativa depende del pote 2. A cero casi no hay eventos.
void updateGenerative(uint32_t now) {
    // El motor de eventos ATMOS no genera nuevas voces en CRYSTAL.
    // Las colas ya presentes se siguen procesando.
    if (activeEngine != ENGINE_ATMOS) return;
    const float activity = movement;
    // Reposo y resolucion dejan espacio; expansion y tension permiten mas eventos.
    const float eventActivity = clamp01(activity * (0.38f + stageEnergy * 0.80f)) *
                                (deepRest ? 0.08f : 1.0f) * (quietMode ? 0.35f : 1.0f);
    if (harmonyFrozen) return;
    if ((int32_t)(now - nextHarmonyAt) >= 0) {
        nextHarmonyAt = now + randomInterval(
            currentMode == CALMA ? 27 : 17,
            currentMode == CALMA ? 48 : 35);
        if (activity > 0.12f) {
            // Solo una capa cambia de grado por vez.
            uint8_t i = (uint8_t)random(0, NUM_LAYERS);
            if (layerEnabled[i]) {
                const int8_t center = startingDegree[i];
                int8_t index = center + (int8_t)random(-2, 3);
                if (index < 0) index = 0;
                if (index >= (int8_t)(sizeof(scaleSemitones) / sizeof(scaleSemitones[0])))
                    index = (int8_t)(sizeof(scaleSemitones) / sizeof(scaleSemitones[0])) - 1;
                // Menos cambios grandes cuando el movimiento es bajo.
                if (random(0, 100) < (int)(activity * 85.0f)) {
                    targetNote[i] = (float)scaleSemitones[index];
                    Serial.print("Armonia capa "); Serial.println(i + 1);
                }
            }
        }
    }

    if ((int32_t)(now - nextCrystalAt) >= 0) {
        nextCrystalAt = now + randomInterval(
            currentMode == CALMA ? 22 : 12,
            currentMode == AURORA ? 25 : 38);
        if (eventActivity > 0.18f && canTriggerAtmosEvent(now) &&
            random(0, 100) < (int)(eventActivity * 55.0f)) {
            triggerSpark();
            markAtmosEvent(now);
        }
    }
    if ((int32_t)(now - nextGhostAt) >= 0) {
        nextGhostAt = now + randomInterval(25, 48);
        if (eventActivity > 0.15f && canTriggerAtmosEvent(now) &&
            random(0, 100) < (int)(eventActivity * 75.0f)) {
            uint8_t layer = (uint8_t)random(0, NUM_LAYERS);
            float ghostHz = noteToHz(targetNote[layer] + 12.0f);
            sendOscFrequency(GHOST_OSC, ghostHz);
            ghostTarget = 1.0f;
            ghostReleaseAt = now + randomInterval(7, 12);
            markAtmosEvent(now);
            Serial.println("Eco fantasma / swell");
        }
    }
    if ((int32_t)(now - nextPulseAt) >= 0) {
        nextPulseAt = now + randomInterval(29, 55);
        if (eventActivity > 0.25f && canTriggerAtmosEvent(now) &&
            random(0, 100) < (int)(eventActivity * 65.0f)) {
            if (currentMode == ORBITA) {
                pulseLevel = 1.0f;
                sendOscFrequency(PULSE_OSC, 55.0f);
                Serial.println("Pulso gravitacional");
            } else {
                // Dos parciales ligeramente inarmonicos: campana minimal.
                const float fundamental = noteToHz(targetNote[random(1, NUM_LAYERS)] + 12.0f);
                sendOscFrequency(BELL_A_OSC, fundamental);
                sendOscFrequency(BELL_B_OSC, fundamental * 2.37f);
                bellLevel = 1.0f;
                Serial.println("Campana lejana");
            }
            markAtmosEvent(now);
        }
    }
    if ((int32_t)(now - nextMicroAt) >= 0) {
        nextMicroAt = now + randomInterval(13, 31);
        if (eventActivity > 0.20f && canTriggerAtmosEvent(now) &&
            random(0, 100) < (int)(eventActivity * 70.0f)) {
            // Dos timbres: gota resonante o campana metalica apagada.
            if (random(0, 100) < 70) {
                triggerResonantDrop();
            } else {
                const float fundamental = noteToHz(targetNote[random(1, NUM_LAYERS)] + 12.0f);
                sendOscFrequency(BELL_A_OSC, fundamental * 0.75f);
                sendOscFrequency(BELL_B_OSC, fundamental * 1.88f);
                bellLevel = 0.68f;
                Serial.println("Metal suave");
            }
            markAtmosEvent(now);
        }
    }
}

// A+B tiene prioridad sobre los gestos individuales: evita disparar
// a la vez 'susurro' y 'congelar' al seleccionar otro motor.
void changeEngine(uint32_t now) {
    if (engineSwitching) return;
    requestedEngine = (SoundEngine)(((uint8_t)activeEngine + 1U) % 4U);
    engineSwitching = true;
    engineSwitchDown = true;
    engineSwitchAt = now;
    manualExpansion = false;
    Serial.print("CAMBIO solicitado: ");
    Serial.println(engineNames[(uint8_t)requestedEngine]);
}

void advanceEngineSwitch(uint32_t now, float dt) {
    if (!engineSwitching) return;
    if (engineSwitchDown) {
        // Descenso suave: aproximadamente 1.2 s.
        engineGate -= dt / 1.2f;
        if (engineGate <= 0.0f) {
            engineGate = 0.0f;
            activeEngine = requestedEngine;
            engineSwitchDown = false;
            engineSwitchAt = now;
            if (activeEngine == ENGINE_CRYSTAL) {
                crystalNextEventAt = now + 1700UL;
                crystalLastEventAt = now - 10000UL;
            } else if (activeEngine == ENGINE_ATMOS) {
                scheduleGenerativeEvents(now);
            } else if (activeEngine == ENGINE_PULSE) {
                nextPulseStepAt = now + 200UL;
                pulseStep = 7;
                for (uint8_t i=0; i<4; ++i) pulseEnv[i] = 0.0f;
            } else {
                nextTextureEventAt = now + 1200UL;
                nextGrainAt = now + 250UL;
                for (uint8_t i=0; i<4; ++i) textureEnv[i] = 0.0f;
                for (auto &g : grains) g.samplesLeft = 0;
            }
            Serial.print("MOTOR: ");
            Serial.println(engineNames[(uint8_t)activeEngine]);
        }
    } else {
        engineGate += dt / 2.0f;
        if (engineGate >= 1.0f) {
            engineGate = 1.0f;
            engineSwitching = false;
        }
    }
}

// En PULSE el boton C deja de repetir el bombo: golpe metalico / madera
// afinado, mas agudo y corto; long-press agrega una segunda resonancia.
void triggerPulseTimbre(bool extended) {
    const float fundamental = extended ? 783.99f : 587.33f; // Sol5 / Re5
    sendOscFrequency(BELL_A_OSC, fundamental);
    sendOscFrequency(BELL_B_OSC, fundamental * (extended ? 1.51f : 2.13f));
    bellLevel = extended ? 1.0f : 0.88f;
    bellDecay = extended ? 0.978f : 0.927f;
    pulseClickEnv = extended ? 0.48f : 0.72f;
    if (extended) {
        // Nota de respuesta con una de las voces de brillo reutilizadas.
        sendOscFrequency(NUM_LAYERS, 987.77f);
        sparkKind[0] = 0;
        sparkLevel[0] = 0.56f;
    }
    Serial.println(extended ? "PULSE C: metal doble" : "PULSE C: golpe afinado");
}

void spawnTextureGrain(float strength = 1.0f, uint8_t kind = 255) {
    TextureGrain &g = grains[nextGrainIndex];
    nextGrainIndex = (uint8_t)((nextGrainIndex + 1) % 4);
    // 25-140 ms: particulas, no notas largas.
    const uint32_t durationMs = (uint32_t)random(25, 141);
    g.samplesTotal = SAMPLE_RATE * durationMs / 1000UL;
    if (g.samplesTotal < 4) g.samplesTotal = 4;
    g.samplesLeft = g.samplesTotal;
    g.envQ15 = 0;
    g.envStep = (uint16_t)(32767UL / (g.samplesTotal / 2));
    if (g.envStep == 0) g.envStep = 1;
    g.kind = kind == 255 ? textureFlavor : (kind % 4);
    const uint32_t note = (uint32_t)random(90, 2600);
    g.phase = (uint32_t)random(0, 32767) << 16;
    g.phaseStep = (uint32_t)(((uint64_t)note << 32) / SAMPLE_RATE);
    g.filtered = 0;
    g.filterQ15 = (uint16_t)(random(12, 160) * 32768L / 1000L);
    g.panQ15 = (uint16_t)(random(8, 93) * 32768L / 100L);
    g.gainQ15 = (uint16_t)(clamp01(strength) * (float)random(55, 101) * 327.68f);
}

void triggerTextureSurge(bool extended) {
    // Ráfaga de granos de diferente longitud + resonancias sombrías.
    textureGrainGain = extended ? 1.0f : 0.78f;
    for (uint8_t i = 0; i < 4; ++i) spawnTextureGrain(textureGrainGain, (textureFlavor+i)%4);
    // Evitar resonancias sostenidas similares a flauta.
    bellLevel = 0.0f;
    nextGrainAt = millis() + 140UL;
    Serial.println(extended ? "TEXTURE: nube granular larga" : "TEXTURE: fragmentos granulares");
}

void updatePulseEvents(uint32_t now) {
    if (activeEngine != ENGINE_PULSE || engineSwitchDown) return;
    if ((int32_t)(now - nextPulseStepAt) < 0) return;
    const uint32_t stepMs = (uint32_t)(900.0f - movement * 620.0f);
    nextPulseStepAt = now + stepMs;
    pulseStep = (uint8_t)((pulseStep + 1) % 8);
    // Patron irregular pero previsible: repeticiones con variaciones discretas.
    if (pulseEnabled[0] && (pulseStep == 0 || pulseStep == 4 ||
        (pulseStep == 6 && movement > 0.70f))) {
        pulseEnv[0] = pulseStep == 0 ? 1.0f : 0.70f;
    }
    if (pulseEnabled[1] && (pulseStep == 2 || pulseStep == 6))
        pulseEnv[1] = 0.73f;
    if (pulseEnabled[2] && (pulseStep == 3 || pulseStep == 7) && movement > 0.25f)
        pulseEnv[2] = 0.64f;
    if (pulseEnabled[3] && pulseStep == 5 && movement > 0.42f)
        pulseEnv[3] = 0.52f;
    // Breves resonancias que dejan cola espacial, sin cargar voces adicionales.
    if (movement > 0.45f && pulseStep == 0 && random(100) < (int)(movement*35.0f)) {
        sendOscFrequency(GHOST_OSC, 329.63f);
        ghostTarget = 0.5f;
        ghostReleaseAt = now + 900UL;
    }
}

void updateTextureEvents(uint32_t now) {
    if (activeEngine != ENGINE_TEXTURE || engineSwitchDown) return;
    if ((int32_t)(now - nextTextureEventAt) < 0) return;
    nextTextureEventAt = now + (uint32_t)(8000.0f - movement*5200.0f) + random(2000, 6001);
    uint8_t options[4], n=0;
    for (uint8_t i=0; i<4; ++i) if(textureEnabled[i]) options[n++]=i;
    if (n==0) return;
    const uint8_t choice = options[random(0, n)];
    if (choice == 0) {
        // Particulas granuladas, no un barrido continuo de ruido.
        spawnTextureGrain(0.78f, 0);
        spawnTextureGrain(0.55f, 0);
        // Evento generativo: sin impresion USB por grano.
    } else if (choice == 1) {
        triggerTextureSurge(false);
    } else if (choice == 2) {
        spawnTextureGrain(0.86f, 2);
        // Evento generativo: sin impresion USB por grano.
    } else {
        spawnTextureGrain(0.76f, 3);
        spawnTextureGrain(0.68f, 3);
        // Evento generativo: sin impresion USB por grano.
    }
}

void updateTextureGrains(uint32_t now) {
    if (activeEngine != ENGINE_TEXTURE || engineSwitchDown) return;
    if ((int32_t)(now - nextGrainAt) < 0) return;
    const float activity = clamp01(movement);
    // Entre 150 y 1000 ms por grano: el 2 controla la densidad.
    nextGrainAt = now + (uint32_t)(520.0f - 460.0f * activity) + (uint32_t)random(0, 110);
    if (textureEnabled[0] || textureEnabled[3]) {
        const float density = textureEnabled[0] ? 0.70f : 0.43f;
        spawnTextureGrain(density * (0.65f + 0.35f * space), textureFlavor);
    }
}

void toggleCrystalSolo(uint8_t i) {
    if (crystalSoloActive && crystalSoloIndex == i) {
        for (uint8_t k=0; k<4; ++k) crystalEnabled[k] = crystalSaved[k];
        crystalSoloActive = false;
        Serial.println("CRYSTAL: SOLO cancelado");
    } else {
        if (!crystalSoloActive)
            for (uint8_t k=0; k<4; ++k) crystalSaved[k] = crystalEnabled[k];
        crystalSoloActive = true;
        crystalSoloIndex = i;
        for (uint8_t k=0; k<4; ++k) crystalEnabled[k] = (k == i);
        Serial.print("CRYSTAL: SOLO familia "); Serial.println(i+1);
    }
}

void updateButtons() {
    for (uint8_t i=0; i<NUM_LAYERS; ++i) {
        const ButtonAction action = readButtonAction(i);
        if (activeEngine == ENGINE_PULSE || activeEngine == ENGINE_TEXTURE) {
            bool* families = activeEngine == ENGINE_PULSE ? pulseEnabled : textureEnabled;
            if (action == SHORT_PRESS) {
                families[i] = !families[i];
                Serial.print(engineNames[(uint8_t)activeEngine]);
                Serial.print(" familia "); Serial.print(i+1);
                Serial.println(families[i] ? " ON" : " OFF");
            } else if (action == LONG_PRESS) {
                // SOLO de familia, como en ATMOS y CRYSTAL.
                uint8_t activeCount = 0;
                for (uint8_t k=0; k<4; ++k) if (families[k]) ++activeCount;
                if (activeCount == 1 && families[i]) {
                    for (uint8_t k=0; k<4; ++k) families[k] = true;
                } else {
                    for (uint8_t k=0; k<4; ++k) families[k] = (k==i);
                }
            }
        } else if (activeEngine == ENGINE_CRYSTAL) {
            if (action == LONG_PRESS) toggleCrystalSolo(i);
            else if (action == SHORT_PRESS) {
                if (crystalSoloActive) {
                    for (uint8_t k=0; k<4; ++k) crystalEnabled[k] = crystalSaved[k];
                    crystalSoloActive = false;
                }
                crystalEnabled[i] = !crystalEnabled[i];
                Serial.print("CRYSTAL familia "); Serial.print(i+1);
                Serial.println(crystalEnabled[i] ? " ON" : " OFF");
            }
        } else {
            if (action == LONG_PRESS) enterOrLeaveSolo(i);
            else if (action == SHORT_PRESS) {
                if (soloActive) {
                    for (uint8_t j=0; j<NUM_LAYERS; ++j) layerEnabled[j] = savedLayers[j];
                    soloActive = false;
                }
                layerEnabled[i] = !layerEnabled[i];
                Serial.print("Capa "); Serial.print(i+1);
                Serial.println(layerEnabled[i] ? " ON" : " OFF");
            }
        }
    }

    const ButtonAction a = readButtonAction(4);
    const ButtonAction b = readButtonAction(5);
    const uint32_t now = millis();

    // Solo se admite un acorde nuevo si A y B se presionan casi a la vez.
    const bool bothDown = buttonStable[4] && buttonStable[5];
    const uint32_t diff = buttonPressedAt[4] > buttonPressedAt[5]
        ? buttonPressedAt[4] - buttonPressedAt[5]
        : buttonPressedAt[5] - buttonPressedAt[4];
    if (!chordArmed && bothDown && diff <= CHORD_WINDOW_MS) {
        chordArmed = true;
        chordTriggered = false;
        chordStartedAt = now;
        buttonHoldFired[4] = true;
        buttonHoldFired[5] = true;
    }
    if (chordArmed) {
        if (bothDown && !chordTriggered &&
            (uint32_t)(now - chordStartedAt) >= ENGINE_CHORD_MS) {
            changeEngine(now);
            chordTriggered = true;
        }
        if (!bothDown) {
            // Bloquear la accion corta al soltar los botones.
            buttonHoldFired[4] = true;
            buttonHoldFired[5] = true;
        }
        if (!buttonStable[4] && !buttonStable[5]) {
            chordArmed = false;
            chordTriggered = false;
        }
    } else {
        if (a == SHORT_PRESS) {
            currentMode = (AtmosMode)(((uint8_t)currentMode + 1) % 3);
            Serial.print("Ambiente: "); Serial.println(modeNames[currentMode]);
        } else if (a == LONG_PRESS) {
            expressiveShift = true;
            for (int i=0; i<3; ++i) {
                shiftStartPot[i] = rawPot[i];
                shiftPotArmed[i] = false;
            }
            Serial.println("SHIFT ON: P1 pitch / P2 ring / P3 FX contextual");
        }
        if (b == SHORT_PRESS) {
            if (activeEngine == ENGINE_PULSE || activeEngine == ENGINE_TEXTURE) {
                bool* families = activeEngine == ENGINE_PULSE ? pulseEnabled : textureEnabled;
                for (uint8_t k=0; k<4; ++k) families[k] = true;
                if (activeEngine == ENGINE_PULSE) {
                    pulseStep = 7;
                    nextPulseStepAt = now + 120;
                } else {
                    nextTextureEventAt = now + 1800;
                }
                Serial.println("Familias reactivadas");
            } else if (activeEngine == ENGINE_CRYSTAL) {
                for (uint8_t k=0; k<4; ++k) crystalEnabled[k] = true;
                crystalSoloActive = false;
                crystalPaused = false;
                crystalNextEventAt = now + 1800;
                Serial.println("CRYSTAL: todas las familias habilitadas");
            } else {
                resetLayers(); resetHarmony(); harmonyFrozen = false;
            }
        } else if (b == LONG_PRESS) {
            if (activeEngine == ENGINE_PULSE) {
                pulsePattern = (pulsePattern + 1) % 3;
                Serial.println("PULSE: variacion de acentos");
            } else if (activeEngine == ENGINE_TEXTURE) {
                textureFlavor = (textureFlavor + 1) % 4;
                Serial.println("TEXTURE: cambiar caracter");
            } else if (activeEngine == ENGINE_CRYSTAL) {
                crystalPaused = !crystalPaused;
                Serial.println(crystalPaused ? "CRYSTAL: PAUSA" : "CRYSTAL: REANUDAR");
            } else {
                harmonyFrozen = !harmonyFrozen;
                Serial.println(harmonyFrozen ? "ARMONIA CONGELADA" : "ARMONIA LIBERADA");
            }
        }
    }
    // A+B siempre tiene prioridad. La edicion no modifica el motor.
    if (chordArmed || !buttonStable[4]) {
        if (expressiveShift) {
            expressiveShift = false;
            for (int i=0; i<3; ++i) {
                normalPickup[i] = false;
                normalPrevious[i] = rawPot[i];
            }
            Serial.println("SHIFT OFF: ajustes conservados");
        }
    }
    const ButtonAction c = readButtonAction(6);
    if (c == SHORT_PRESS) {
        if (activeEngine == ENGINE_CRYSTAL) triggerCrystalChime();
        else if (activeEngine == ENGINE_PULSE) triggerPulseTimbre(false);
        else if (activeEngine == ENGINE_TEXTURE) triggerTextureSurge(false);
        else triggerSpark();
    } else if (c == LONG_PRESS) {
        if (activeEngine == ENGINE_PULSE) {
            triggerPulseTimbre(true);
            Serial.println("PULSE: golpe metalico con respuesta cristalina");
        } else if (activeEngine == ENGINE_TEXTURE) {
            triggerTextureSurge(true);
            textureEnv[2] = 0.9f;
            Serial.println("TEXTURE: oleaje largo");
        } else if (activeEngine == ENGINE_CRYSTAL) {
            // Dos resonancias sucesivas al sostener C; sin activar la
            // expansion de ATMOS ni duplicar la pulsacion corta.
            triggerCrystalChime();
            ghostTarget = 1.0f;
            ghostReleaseAt = now + 4000UL;
            Serial.println("CRYSTAL: resonancia extendida");
        } else {
            manualExpansion = true;
            triggerSpark();
            Serial.println("EXPANSION: mantener pulsado C");
        }
    }
    if (!buttonStable[6]) manualExpansion = false;
}

// En CRYSTAL, cada evento usa voces ya existentes y respeta una separacion
// minima. El pote 2 determina la actividad y el 3 la cola espacial.
void updateCrystalEvents(uint32_t now) {
    if (activeEngine != ENGINE_CRYSTAL || crystalPaused) return;
    if ((int32_t)(now - crystalNextEventAt) < 0) return;

    // Entre 4 y 16 segundos, segun el movimiento y el ambiente.
    const uint32_t minMs = (currentMode == CALMA ? 7000UL : 4000UL);
    const uint32_t maxMs = (currentMode == CALMA ? 16000UL : 11000UL);
    const float activity = clamp01(movement);
    crystalNextEventAt = now + minMs +
        (uint32_t)((maxMs - minMs) * (1.0f - activity) *
                   (float)random(55, 101) / 100.0f);

    if (activity < 0.08f || (uint32_t)(now - crystalLastEventAt) < 3000UL)
        return;

    uint8_t enabledTypes[4];
    uint8_t count = 0;
    for (uint8_t k=0; k<4; ++k)
        if (crystalEnabled[k]) enabledTypes[count++] = k;
    if (count == 0) return;

    const uint8_t chosen = enabledTypes[random(0, count)];
    if (chosen == 0) {
        triggerSpark();
    } else if (chosen == 1) {
        triggerResonantDrop();
    } else if (chosen == 2) {
        const float hz = noteToHz(scaleSemitones[random(8, 13)]);
        sendOscFrequency(BELL_A_OSC, hz);
        sendOscFrequency(BELL_B_OSC, hz * 2.37f);
        bellLevel = 1.0f;
        bellDecay = 0.967f;
        Serial.println("CRYSTAL: campana lejana");
    } else {
        const float hz = noteToHz(scaleSemitones[random(5, 11)]);
        sendOscFrequency(GHOST_OSC, hz);
        ghostTarget = 1.0f;
        ghostReleaseAt = now + randomInterval(4, 8);
        Serial.println("CRYSTAL: nota flotante");
    }
    crystalLastEventAt = now;
}

// Respiraciones organicas no sincronizadas y descansos de 12 a 24 s.
// No se silencian las colas de delay/reverb: solo se reduce la entrada del drone.
void updateOrganicBreathing(uint32_t now, float dt) {
    if ((int32_t)(now - nextRestAt) >= 0 && !deepRest) {
        deepRest = true;
        restEndsAt = now + randomInterval(12, 24);
        Serial.println("ATMOS: descanso sonoro");
    }
    if (deepRest && (int32_t)(now - restEndsAt) >= 0) {
        deepRest = false;
        nextRestAt = now + randomInterval(65, 110);
        Serial.println("ATMOS: fin del descanso");
    }
    // Aproximadamente 3 a 6 segundos de transicion hacia el reposo.
    const float restTarget = deepRest ? 1.0f : 0.0f;
    const float restAlpha = clamp01(dt * 0.23f);
    restBlend += (restTarget - restBlend) * restAlpha;

    if ((int32_t)(now - nextBreathChangeAt) >= 0) {
        nextBreathChangeAt = now + randomInterval(35, 70);
        for (uint8_t i=0; i<NUM_LAYERS; ++i)
            layerBreathSpeed[i] = random(9, 27) / 1000.0f;
    }
    for (uint8_t i=0; i<NUM_LAYERS; ++i) {
        layerBreathPhase[i] = fmodf(layerBreathPhase[i] +
                           TWO_PI_F * layerBreathSpeed[i] * dt, TWO_PI_F);
        const float w = 0.5f + 0.5f * sinf(layerBreathPhase[i]);
        // Suelo suave: una capa nunca cae instantaneamente a cero.
        layerBreathShape[i] = 0.35f + 0.65f * w * w;
    }
    const float density = quietMode ? 0.45f : 1.0f;
    globalBreath = density * (1.0f - 0.94f * restBlend);
    if (manualExpansion) globalBreath = fmaxf(globalBreath, 0.85f);
}

void updateLeds() {
    const uint32_t now = millis();
    const bool editing = expressiveShift && (int32_t)(ledParamUntil-now) > 0;
    const float phase = (float)(now % 6000UL) / 6000.0f;
    for (uint8_t i=0; i<NUM_LAYERS; ++i) {
        const bool enabled = activeEngine == ENGINE_CRYSTAL ? crystalEnabled[i] :
            activeEngine == ENGINE_PULSE ? pulseEnabled[i] :
            activeEngine == ENGINE_TEXTURE ? textureEnabled[i] : layerEnabled[i];
        uint8_t brightness = enabled ? 190 : 0;
        if (enabled && activeEngine == ENGINE_ATMOS) {
            brightness = (uint8_t)(40.0f+175.0f*layerBreathShape[i]*globalBreath);
        } else if (enabled && activeEngine == ENGINE_CRYSTAL) {
            const bool brief = ((now/170 + i*7)%29 == 0);
            brightness = brief ? 255 : 40;
        } else if (enabled && activeEngine == ENGINE_PULSE) {
            brightness = pulseEnv[i] > 0.04f ?
                (uint8_t)(35.0f+220.0f*clamp01(pulseEnv[i])) : 12;
        } else if (enabled && activeEngine == ENGINE_TEXTURE) {
            brightness = ((now/85 + i*11)%19 == 0) ? 220 : 20;
        }
        analogWrite(LED_BUTTON_PIN[i], brightness);
    }
    for (uint8_t i=0; i<NUM_X_LEDS; ++i) {
        uint8_t brightness = 0;
        if (editing) {
            // Barra luminosa del parametro: cuatro segmentos progresivos.
            const float segment = ledParamValue * 4.0f - (float)i;
            brightness = segment <= 0 ? 0 : (segment >= 1 ? 255 : (uint8_t)(segment*255));
        } else if ((now % 8000UL) < 1000UL) {
            // Identificacion inequívoca del motor cada ocho segundos.
            brightness = i == (uint8_t)activeEngine ? 255 : 0;
        } else if (activeEngine == ENGINE_ATMOS) {
            const float wave = 0.5f + 0.5f*sinf(TWO_PI_F*phase - i*1.57f);
            brightness = (uint8_t)(25+wave*150.0f);
        } else if (activeEngine == ENGINE_CRYSTAL) {
            brightness = ((now/120 + i*13)%37 == 0) ? 255 : 6;
        } else if (activeEngine == ENGINE_PULSE) {
            brightness = pulseEnv[i] > 0.12f ? 235 : 10;
        } else {
            // Parpadeo fragmentado al ritmo de los granos, usando su estado.
            brightness = grains[i].samplesLeft > 0 ?
                         (uint8_t)(45+140*grains[i].gainQ15/32768UL) : 0;
        }
        analogWrite(LED_X_PIN[i], brightness);
    }
    // Estrella: refleja eventos, cambio de motor o respiracion.
    bool flash = sparkLevel[0] > 0.08f || sparkLevel[1] > 0.08f ||
                 (activeEngine == ENGINE_CRYSTAL && bellLevel > 0.1f) ||
                 (activeEngine == ENGINE_PULSE && pulseClickEnv > 0.08f);
    if (activeEngine == ENGINE_TEXTURE) {
        for (auto &g : grains) if (g.samplesLeft > 0 && g.envQ15 > 22000) flash = true;
    }
    analogWrite(LED_ESTRELLA_PIN, flash ? 240 :
                (uint8_t)(8+30*(0.5f+0.5f*sinf(breathPhase))));
}

void updateControls() {
    const uint32_t now = millis();
    if ((uint32_t)(now - lastControlAt) < CONTROL_MS) return;
    const float dt = (now - lastControlAt) / 1000.0f;
    lastControlAt = now;

    for (int i=0; i<3; ++i) rawPot[i] = readPot(POT_PIN[i]);
    updateButtons();
    if (expressiveShift && !chordArmed) {
        for (int i=0; i<3; ++i) {
            if (!shiftPotArmed[i] && fabsf(rawPot[i] - shiftStartPot[i]) >= 0.025f)
                shiftPotArmed[i] = true;
        }
        if (shiftPotArmed[0]) expressivePitch = (rawPot[0] - 0.5f) * 48.0f;
        if (shiftPotArmed[1]) expressiveRing = rawPot[1];
        if (shiftPotArmed[2]) expressiveFx[(uint8_t)activeEngine] = rawPot[2];
        for (uint8_t i=0; i<3; ++i) {
            if (shiftPotArmed[i] && fabsf(rawPot[i] - shiftStartPot[i]) > 0.025f) {
                ledParamUntil = now + 1300UL;
                ledParamIndex = i;
                ledParamValue = i==0 ? clamp01(expressivePitch/48.0f+0.5f) :
                                i==1 ? expressiveRing : expressiveFx[(uint8_t)activeEngine];
                shiftStartPot[i] = rawPot[i];
            }
        }
    } else {
        float* normal[3] = {&masterVolume, &movement, &space};
        for (int i=0; i<3; ++i) {
            if (!normalPickup[i]) {
                // Pick-up: solo retomar al cruzar el valor previo del control.
                const float desired = *normal[i];
                if (fabsf(rawPot[i]-desired) <= 0.035f ||
                    (normalPrevious[i]-desired) * (rawPot[i]-desired) <= 0.0f)
                    normalPickup[i] = true;
            }
            if (normalPickup[i]) *normal[i] = rawPot[i];
            normalPrevious[i] = rawPot[i];
        }
    }
    // Actualizaciones graduales: los parametros no saltan instantaneamente.
    const float goalFactor = powf(2.0f, expressivePitch / 12.0f);
    pitchFactor += (goalFactor-pitchFactor) * 0.12f;
    // Portadora de 18 a 138 Hz. A 0% conserva la señal original.
    const float ringHz = 18.0f + expressiveRing * 120.0f;
    ringPhaseStep = (uint32_t)((ringHz * 4294967296.0) / (double)SAMPLE_RATE);
    ringMixQ15 = (int32_t)(expressiveRing * 29490.0f); // max 90% wet
    const float selectedFx = clamp01(expressiveFx[(uint8_t)activeEngine]);
    fxMixQ15 = (int32_t)(selectedFx * 28000.0f);
    bitDepth = (uint8_t)(16.0f - selectedFx * 11.0f);
    if (bitDepth < 5) bitDepth = 5;
    bitHold = (uint8_t)(1.0f + selectedFx*7.0f);
    // Shift espectral: 0.8 .. 120 Hz con mezcla gradual.
    const float shiftHz = 0.8f + selectedFx * selectedFx * 119.2f;
    shiftPhaseStep = (uint32_t)((shiftHz * 4294967296.0) / (double)SAMPLE_RATE);
    advanceEngineSwitch(now, dt);
    crystalBlend = activeEngine == ENGINE_CRYSTAL ? engineGate : 0.0f;
    updateOrganicBreathing(now, dt);
    updateEmotionalStage(now);
    stageEnergy += (stageEnergyTarget - stageEnergy) * 0.012f;
    updateGenerative(now);
    updateCrystalEvents(now);
    updatePulseEvents(now);
    updateTextureEvents(now);
    updateTextureGrains(now);

    float lfoSpeed = 0.035f + movement * 0.40f;
    if (currentMode == ORBITA) lfoSpeed *= 1.7f;
    if (currentMode == AURORA) lfoSpeed *= 0.65f;

    lfoPhase = fmodf(lfoPhase + TWO_PI_F * lfoSpeed * dt, TWO_PI_F);
    breathPhase = fmodf(breathPhase + TWO_PI_F * 0.075f * dt, TWO_PI_F);

    // La ganancia digital 6x se aplica despues de AMY (en processAudio).
    const float volume = masterVolume * 0.22f;
    const float breathing = 0.80f + 0.20f * sinf(breathPhase);
    const float windTarget = (currentMode == CALMA ? 0.24f : 0.42f) * movement * space *
                             (0.70f + 0.35f * stageEnergy);
    // TEXTURE ya no tiene una cama permanente de ruido blanco.
    // La textura la crean granos de duración limitada.
    const float windGoal = activeEngine == ENGINE_ATMOS ?
        windTarget * (quietMode ? 0.5f : 1.0f) : 0.0f;
    windLevel += ((windGoal * engineGate) - windLevel) * 0.018f;
    textureNoiseLevel = 0.0f;
    pulseClickEnv *= 0.64f;
    const float drift = sinf(lfoPhase);

    for (uint8_t i = 0; i < NUM_LAYERS; ++i) {
        const float target = layerEnabled[i] ? 1.0f : 0.0f;
        layerLevel[i] += (target - layerLevel[i]) * 0.055f;

        const float offset = i * 1.37f;
        const float depthHz = (0.20f + movement * (5.0f + 2.0f * stageEnergy)) *
                              (1.0f + i * 0.25f);
        // Glissando lento hacia la proxima nota (sin saltos bruscos).
        currentNote[i] += (targetNote[i] - currentNote[i]) * 0.005f;
        float frequency = noteToHz(currentNote[i]) + sinf(lfoPhase + offset) * depthHz;
        if (currentMode == ORBITA) frequency += drift * (0.7f + i * 0.45f);
        if (currentMode == AURORA) frequency += sinf(lfoPhase * 0.5f + offset) * 1.0f;

        float colorGain = 1.0f;
        if (i == 2) colorGain = 0.40f + space * 0.60f;
        if (i == 3) colorGain = 0.30f + space * 0.70f;
        if (currentMode == AURORA && i >= 2) colorGain *= 1.15f;

        const float stageBreath = 0.94f + 0.06f * stageEnergy;
        float amplitude = 0.0f;
        if (activeEngine == ENGINE_ATMOS) {
            amplitude = volume * layerWeight[i] * layerLevel[i] *
                        colorGain * breathing * stageBreath *
                        layerBreathShape[i] * globalBreath;
        } else if (activeEngine == ENGINE_PULSE) {
            // Cuatro osciladores reciclados: graves, contrapulsos, notas y aire tonal.
            // Todos tienen decaimientos y acentos independientes.
            const float rates[4] = {0.78f, 0.83f, 0.87f, 0.91f};
            pulseEnv[i] *= rates[i];
            if (pulseEnv[i] < 0.0003f) pulseEnv[i] = 0.0f;
            const float notes[4] = {55.0f, 110.0f, 164.81f, 220.0f};
            const float alternate[4] = {65.41f, 130.81f, 196.0f, 261.63f};
            frequency = pulsePattern == 1 && (pulseStep & 4) ? alternate[i] : notes[i];
            if (pulsePattern == 2 && i == 2) frequency *= 1.5f;
            amplitude = volume * 0.75f * pulseEnv[i] * (i == 0 ? 0.90f : 0.50f);
        } else if (activeEngine == ENGINE_TEXTURE) {
            // TEXTURE V2: ni senos sostenidos ni "flauta". Solamente granos
            // breves, ataques inarmonicos y silencios entre particulas.
            textureEnv[i] *= 0.92f;
            amplitude = 0.0f;
        }
        sendOscFrequency(i, frequency);
        sendOscAmplitude(i, amplitude * engineGate);

        // Espacialidad suave: panes independientes, sin cambios bruscos.
        // AMY recibe valores 0..1; se reserva margen para evitar extremos.
        const float orbit = sinf(lfoPhase * (0.55f + 0.15f * i) + i * 1.61f);
        const float panTarget = clamp01(layerPan[i] + orbit * movement *
                                       (0.12f + 0.13f * stageEnergy));
        lastAppliedPan[i] += (panTarget - lastAppliedPan[i]) * 0.045f;
        sendOscPan(i, lastAppliedPan[i]);
    }

    for (uint8_t i = 0; i < NUM_SPARKS; ++i) {
        if (sparkKind[i] == 1) {
            // Gota: pitch cae suavemente mientras se apaga.
            sparkLevel[i] *= 0.94f;
            const float pitch = sparkPitch[i] * (1.0f + 0.18f * sparkLevel[i]);
            sendOscFrequency(NUM_LAYERS + i, pitch);
        } else {
            // Cristales originales: se preserva la velocidad de caida.
            sparkLevel[i] *= 0.90f;
        }
        if (sparkLevel[i] < 0.001f) sparkLevel[i] = 0.0f;
        const float kindGain = (sparkKind[i] == 1) ? 0.33f : 0.50f;
        // Los destellos son protagonistas en CRYSTAL, pero se mantiene margen.
        const float engineGain = 1.0f + crystalBlend * 0.35f;
        sendOscAmplitude(NUM_LAYERS + i, volume * kindGain * sparkLevel[i] *
                         engineGain * engineGate * (manualExpansion ? 1.30f : 1.0f));
    }

    // Texturas generativas (los destellos originales no se modifican).
    if ((int32_t)(now - ghostReleaseAt) >= 0) ghostTarget = 0.0f;
    // Ataque mucho mas lento: eco inverso simulado.
    const float ghostRate = ghostTarget > ghostLevel ? 0.009f : 0.035f;
    ghostLevel += (ghostTarget - ghostLevel) * ghostRate;
    pulseLevel *= 0.945f;
    bellLevel *= bellDecay;
    if (pulseLevel < 0.001f) pulseLevel = 0.0f;
    if (bellLevel < 0.001f) bellLevel = 0.0f;

    const float eventLevel = activeEngine == ENGINE_ATMOS
        ? (deepRest ? 0.40f : (quietMode ? 0.65f : 1.0f)) : 1.0f;
    if (activeEngine == ENGINE_ATMOS && expressiveFx[0] > 0.01f) {
        // Shimmer armonico ligero: añade una voz una octava mas alta
        // y la envia a la reverb. No es un pitch-shifter de cola completa.
        sendOscFrequency(GHOST_OSC, noteToHz(currentNote[0] + 12.0f));
        shimmerLevel += (expressiveFx[0]-shimmerLevel) * 0.028f;
        sendOscAmplitude(GHOST_OSC, volume * shimmerLevel * 0.22f *
                         globalBreath * engineGate);
    } else {
        shimmerLevel *= 0.96f;
        sendOscAmplitude(GHOST_OSC, volume * (0.27f + 0.25f * crystalBlend) *
                         ghostLevel * eventLevel * engineGate);
    }
    sendOscAmplitude(PULSE_OSC, volume * 0.24f * pulseLevel * eventLevel * engineGate);
    const float bellBoost = activeEngine == ENGINE_PULSE ? 1.15f : 1.0f;
    sendOscAmplitude(BELL_A_OSC, volume * (0.20f + 0.20f * crystalBlend) * bellLevel * bellBoost * eventLevel * engineGate);
    sendOscAmplitude(BELL_B_OSC, volume * (0.065f + 0.07f * crystalBlend) * bellLevel * bellBoost * eventLevel * engineGate);

    // Pot 3: espacio (mezcla de delay + reverb), sin alterar el volumen maestro.
    float delayFactor = 0.40f, reverbFactor = 0.50f;
    if (currentMode == ORBITA) { delayFactor = 0.65f; reverbFactor = 0.35f; }
    if (currentMode == AURORA) { delayFactor = 0.30f; reverbFactor = 0.75f; }
    const float pulseDelay = 0.58f, pulseReverb = 0.25f;
    const float textureDelay = 0.23f, textureReverb = 0.78f;
    const float engineDelay = activeEngine == ENGINE_CRYSTAL ? 0.34f :
         activeEngine == ENGINE_PULSE ? pulseDelay :
         activeEngine == ENGINE_TEXTURE ? textureDelay : delayFactor;
    const float engineReverb = activeEngine == ENGINE_CRYSTAL ? 0.78f :
         activeEngine == ENGINE_PULSE ? pulseReverb :
         activeEngine == ENGINE_TEXTURE ? textureReverb : reverbFactor;
    delayWet = space * engineDelay * (0.90f + stageEnergy*0.10f);
    reverbWet = space * engineReverb * (0.90f + stageEnergy*0.10f);
    delayFeedback = 0.55f + space * 0.23f; // < 1
    feedbackQ15 = (int32_t)(delayFeedback * 32768.0f);
    delayWetQ15 = (int32_t)(delayWet * 32768.0f);
    reverbWetQ15 = (int32_t)(reverbWet * 32768.0f);

    updateLeds();
    if ((uint32_t)(now - lastDebugAt) >= 2000UL) {
        lastDebugAt = now;
        Serial.print("BODOQUE V0.8.2 | ");
        Serial.print(engineNames[(uint8_t)activeEngine]);
        Serial.print(" | "); Serial.print(modeNames[currentMode]);
        Serial.print(" | "); Serial.print(stageNames[emotionalStage]);
        Serial.print(" | Vol "); Serial.print(masterVolume, 2);
        Serial.print(" | Mov "); Serial.print(movement, 2);
        Serial.print(" | Espacio "); Serial.print(space, 2);
        Serial.print(" | Pitch "); Serial.print(expressivePitch, 1);
        Serial.print(" | Ring "); Serial.print(expressiveRing, 2);
        Serial.print(" | FX "); Serial.print(expressiveFx[(uint8_t)activeEngine], 2);
        Serial.print(" | AudioBlockMax(us) "); Serial.print(maxAudioBlockUs);
        maxAudioBlockUs = 0;
        Serial.print(" | Descanso "); Serial.print(deepRest ? "SI" : "NO");
        Serial.print(" | Susurro "); Serial.print(quietMode ? "SI" : "NO");
        Serial.print(" | Congelado "); Serial.println(harmonyFrozen ? "SI" : "NO");
    }
}

// Una linea comb con amortiguacion sencilla: el parametro fb siempre < 1.
inline int16_t processComb(int16_t *buffer, uint16_t size, uint16_t &pos,
                           int16_t input, int32_t fbQ15) {
    const int16_t delayed = buffer[pos];
    buffer[pos] = clamp16((int32_t)input + (((int32_t)delayed * fbQ15) >> 15));
    if (++pos >= size) pos = 0;
    return delayed;
}

inline int16_t processAllpass(int16_t *buffer, uint16_t size,
                              uint16_t &pos, int16_t input) {
    constexpr int32_t AP_Q15 = 18022; // 0.55
    const int16_t delayed = buffer[pos];
    const int32_t output = (int32_t)delayed - (((int32_t)input * AP_Q15) >> 15);
    buffer[pos] = clamp16((int32_t)input + (((int32_t)delayed * AP_Q15) >> 15));
    if (++pos >= size) pos = 0;
    return clamp16(output);
}

void processAudio() {
    const uint32_t blockStartUs = micros();
    int16_t *samples = amy_update();
    for (int i = 0; i < AMY_BLOCK_SIZE; ++i) {
        // Conservar el aumento de salida V0.2.3, con compresion de picos.
        // Viento: ruido con filtro diferencial suave, muy por debajo del drone.
        noiseState ^= noiseState << 13;
        noiseState ^= noiseState >> 17;
        noiseState ^= noiseState << 5;
        const int32_t whiteSample = (int32_t)(noiseState & 65535U) - 32768;
        const float white = (float)whiteSample / 32768.0f;
        windLowFast += 0.075f * (white - windLowFast);
        windLowSlow += 0.004f * (white - windLowSlow);
        const int32_t wind = (int32_t)((windLowFast - windLowSlow) * windLevel * 2100.0f);
        int32_t granL = 0, granR = 0;
        if (activeEngine == ENGINE_TEXTURE && engineGate > 0.001f) {
            for (auto &g : grains) {
                if (g.samplesLeft == 0) continue;
                g.phase += g.phaseStep; // uint32_t: overflow natural
                const uint32_t trianglePhase = g.phase >> 16;
                const int32_t triangle = trianglePhase < 32768
                    ? (int32_t)trianglePhase * 2 - 32768
                    : 98304 - (int32_t)trianglePhase * 2;
                g.filtered += (((int32_t)whiteSample - g.filtered) * g.filterQ15) >> 15;
                const uint32_t elapsed = g.samplesTotal - g.samplesLeft;
                const uint32_t half = g.samplesTotal / 2;
                if (elapsed < half) {
                    const uint32_t e = (uint32_t)g.envQ15 + g.envStep;
                    g.envQ15 = e > 32767 ? 32767 : (uint16_t)e;
                } else {
                    g.envQ15 = g.envQ15 > g.envStep ? g.envQ15-g.envStep : 0;
                }
                // Cuatro familias: arena digital / vidrio roto / materia oscura / nube inversa.
                int32_t timbre = 0;
                if (g.kind==0) timbre = (triangle + g.filtered*3) / 4;
                else if (g.kind==1) timbre = (triangle*3 + (whiteSample - g.filtered)) / 4;
                else if (g.kind==2) timbre = (triangle*2 + g.filtered) / 3;
                else timbre = (triangle + (whiteSample - g.filtered)*2) / 3;
                int32_t signal = (timbre * (int32_t)g.gainQ15) >> 15;
                signal = (signal * (int32_t)g.envQ15) >> 15;
                signal = (signal * 4200) >> 15;
                granL += (signal * (32768 - g.panQ15)) >> 15;
                granR += (signal * g.panQ15) >> 15;
                --g.samplesLeft;
            }
        }
        // Los granos obedecen al volumen y a la transicion entre motores.
        granL = (int32_t)(granL * masterVolume * engineGate * 1.8f);
        granR = (int32_t)(granR * masterVolume * engineGate * 1.8f);
        // Ataque breve y percusivo de C en PULSE, distinto del bombo de 55 Hz.
        const int32_t click = activeEngine == ENGINE_PULSE
            ? (int32_t)((white - windLowFast) * pulseClickEnv * engineGate *
                        masterVolume * 700.0f) : 0;

        const int16_t rawL = softLimit((int32_t)samples[i * 2] * OUTPUT_GAIN + wind + granL + click);
        const int16_t rawR = softLimit((int32_t)samples[i * 2 + 1] * OUTPUT_GAIN + wind / 2 + granR + click);
        // Ring triangular sin sinf(): calculado una vez para ambos canales.
        ringPhase += ringPhaseStep;
        const uint32_t rp = ringPhase >> 16;
        const int32_t mod = rp < 32768
            ? (int32_t)rp * 2 - 32768
            : 98304 - (int32_t)rp * 2;
        int16_t dryL = applyRing(rawL, mod);
        int16_t dryR = applyRing(rawR, mod);
        if (fxMixQ15 > 0) {
            if (activeEngine == ENGINE_CRYSTAL) {
                dryL = crystalComb(dryL, fxCombL, 601, fxCombPosL);
                dryR = crystalComb(dryR, fxCombR, 673, fxCombPosR);
            } else if (activeEngine == ENGINE_PULSE) {
                // Downsampling y cuantizacion independientes de cada canal.
                const int16_t cL = crushSample(dryL, bitHeldL, bitHoldCounter);
                const int32_t step = 1L << (16-bitDepth);
                const int16_t cR = clamp16(((int32_t)dryR / step) * step);
                dryL = clamp16(((int32_t)dryL * (32768-fxMixQ15) +
                               (int32_t)cL * fxMixQ15) >> 15);
                dryR = clamp16(((int32_t)dryR * (32768-fxMixQ15) +
                               (int32_t)cR * fxMixQ15) >> 15);
            } else if (activeEngine == ENGINE_TEXTURE) {
                shiftPhase += shiftPhaseStep;
                const uint8_t idx = (uint8_t)(shiftPhase >> 24);
                const int32_t sn = sineTableQ15[idx];
                const int32_t cs = sineTableQ15[(uint8_t)(idx+64)];
                const int16_t shL = shiftSideband(dryL, shiftHistoryL, shiftHistoryPos, cs, sn);
                const int16_t shR = shiftSideband(dryR, shiftHistoryR, shiftHistoryPos, cs, sn);
                shiftHistoryPos = (shiftHistoryPos + 1) & 15U;
                dryL = clamp16(((int32_t)dryL*(32768-fxMixQ15) +
                               (int32_t)shL*fxMixQ15) >> 15);
                dryR = clamp16(((int32_t)dryR*(32768-fxMixQ15) +
                               (int32_t)shR*fxMixQ15) >> 15);
            }
        }

        // Delay estereo: 331 ms izquierda y 363 ms derecha.
        // DELAY_SIZE es potencia de dos: enmascarar evita divisiones por muestra.
        const uint16_t tapL = (delayWritePos + DELAY_SIZE - 14600) & (DELAY_SIZE - 1);
        const uint16_t tapR = (delayWritePos + DELAY_SIZE - 16000) & (DELAY_SIZE - 1);
        const int16_t echoL = delayL[tapL];
        const int16_t echoR = delayR[tapR];
        delayL[delayWritePos] = clamp16((int32_t)dryL + (((int32_t)echoR * feedbackQ15) >> 15));
        delayR[delayWritePos] = clamp16((int32_t)dryR + (((int32_t)echoL * feedbackQ15) >> 15));
        delayWritePos = (delayWritePos + 1) & (DELAY_SIZE - 1);

        // Reverb: cuatro combs desiguales y una etapa de difusion por canal.
        const int16_t cL1 = processComb(combL1, 1423, combPosL1, dryL / 3, 26870);
        const int16_t cL2 = processComb(combL2, 1867, combPosL2, dryL / 3, 25886);
        const int16_t cR1 = processComb(combR1, 1613, combPosR1, dryR / 3, 26542);
        const int16_t cR2 = processComb(combR2, 1999, combPosR2, dryR / 3, 25559);
        const int16_t revL = processAllpass(allpassL, 367, allpassPosL,
                                            clamp16(((int32_t)cL1 + cL2) / 2));
        const int16_t revR = processAllpass(allpassR, 421, allpassPosR,
                                            clamp16(((int32_t)cR1 + cR2) / 2));

        // Mezcla paralela: dry + ecos + reverberacion.
        const int32_t mixedL = (int32_t)dryL +
            (((int32_t)echoL * delayWetQ15) >> 15) + (((int32_t)revL * reverbWetQ15) >> 15);
        const int32_t mixedR = (int32_t)dryR +
            (((int32_t)echoR * delayWetQ15) >> 15) + (((int32_t)revR * reverbWetQ15) >> 15);
        audioOutput.write(softLimit(mixedL));
        audioOutput.write(softLimit(mixedR));
    }
    const uint32_t elapsedUs = micros() - blockStartUs;
    if (elapsedUs > maxAudioBlockUs) maxAudioBlockUs = elapsedUs;
}

void setup() {
    Serial.begin(115200);
    digitalWrite(MUTE_PIN, HIGH);
    pinMode(MUTE_PIN, OUTPUT);
    setupControls();
    for (uint16_t i=0; i<256; ++i)
        sineTableQ15[i] = (int16_t)(32767.0f*sinf(TWO_PI_F*(float)i/256.0f));
    randomSeed(analogRead(POT_PIN[1]));

    audioOutput.setBitsPerSample(16);
    audioOutput.setFrequency(SAMPLE_RATE);
    audioOutput.setLSBJFormat();
    if (!audioOutput.begin()) {
        Serial.println("ERROR: DAC no inicializado");
        while (true) delay(1000);
    }

    amy_config_t config = amy_default_config();
    config.platform.multicore = 0;
    config.audio = AMY_AUDIO_IS_NONE;
    amy_start(config);

    for (uint8_t i = 0; i < NUM_LAYERS; ++i)
        createOscillator(i, baseFrequency[i], layerPan[i]);
    createOscillator(4, 660.0f, 0.20f);
    createOscillator(5, 880.0f, 0.80f);
    createOscillator(GHOST_OSC, 440.0f, 0.27f);
    createOscillator(PULSE_OSC, 55.0f, 0.50f);
    createOscillator(BELL_A_OSC, 660.0f, 0.34f);
    createOscillator(BELL_B_OSC, 1564.0f, 0.68f);

    scheduleGenerativeEvents(millis());
    stageEnergy = 0.0f;
    stageEnergyTarget = 0.05f;
    emotionalStage = REPOSO;
    nextStageAt = millis() + randomInterval(35, 60);
    nextRestAt = millis() + randomInterval(65, 105);
    nextBreathChangeAt = millis() + randomInterval(35, 70);
    lastControlAt = lastDebugAt = audioStartedAt = millis();
    crystalNextEventAt = millis() + 2500;
    nextPulseStepAt = millis() + 500;
    nextTextureEventAt = millis() + 2500;
    nextGrainAt = millis() + 900;
    Serial.println("BODOQUE ATMOS V0.8.2 - ATMOS / CRYSTAL / PULSE / TEXTURE");
    Serial.println("SHIFT: mantener A y mover P1/P2/P3 = PITCH/RING/FX motor");
    Serial.println("Cambiar motor: mantener A+B simultaneamente 1 segundo");
}

void loop() {
    updateControls();
    processAudio();
    if (!speakerEnabled && (uint32_t)(millis() - audioStartedAt) >= 500UL) {
        digitalWrite(MUTE_PIN, LOW);
        speakerEnabled = true;
        Serial.println("Amplificador habilitado");
    }
}
