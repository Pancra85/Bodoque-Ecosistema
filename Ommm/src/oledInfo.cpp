#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <AMY-Arduino.h>

#include "synth.h"
#include "controls.h"

// ==========================================
// CONFIGURACIÓN OLED
// ==========================================
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

bool introActive;

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ==========================================
// FUNCIONES DEL OLED
// ==========================================

bool initOLED()
{
    Wire.setSDA(12);
    Wire.setSCL(13);
    Wire.begin();
    // Wire.setClock(1000000U);

    if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS))
    {
        // Si falla, el programa sigue pero sin pantalla
        return false;
    }

    display.setRotation(2); // Rotación
    display.clearDisplay();
    display.setTextSize(1);
    display.display();
    return true;
}

struct ParamLabel
{
    const char *name;
    float value;
    int precision; // 0 = entero, >0 = decimales
};

void getPageLabels(Layer &l, LayerMenuPage page, ParamLabel labels[3])
{
    switch (page)
    {
    case PAGE_PITCH_FILTER_RES:

        labels[0] = {"Res", l.resonance, 1};
        labels[1] = {"Nota", l.baseNote, 1};
        labels[2] = {"C.Off", l.filterFreq, 0};
        break;
    case PAGE_TYPE_WAVE_COMPLEX:
        labels[0] = {"Filt", (float)l.filterType, 0};
        labels[1] = {"Wave", (float)l.waveform, 0};
        labels[2] = {"Grosr", (float)l.complexity, 0};
        break;
    case PAGE_VOL_HARMONY_SCALE:
        labels[0] = {"Vol", (float)l.volume, 1};
        labels[1] = {"Harm", (float)l.harmonyMode, 0};
        if (l.harmonyMode == HARMONY_INTERVAL)
            labels[2] = {"2Osc", (float)l.harmonyIntervalSemitones, 1};
        else if (l.harmonyMode == HARMONY_SEQUENCE)
            labels[2] = {"Escal", (float)l.harmonyParam, 0};
        else if (l.harmonyMode == HARMONY_CHORD)
            labels[2] = {"Acord", (float)l.harmonyParam, 0};
        else
            labels[2] = {"", 0.0f, 0};
        break;
    case PAGE_LENGTH_MELODY_TIEMPO:
        labels[0] = {"Largo", (float)l.sequenceLength, 0};

        labels[1] = {"Melod", (float)l.density, 0};
        labels[2] = {"Tiemp", (float)l.millisPerStep, 0};
        break;
    case PAGE_DISTORTION:
        labels[0] = {"Tipo", (float)audioEffects.distType, 0};
        labels[1] = {"Drive", audioEffects.distDrive, 1};
        labels[2] = {"Mix", audioEffects.distMix, 1};
        break;
    case PAGE_CHORUS:
        labels[0] = {"Level", audioEffects.chorusLevel, 1};
        labels[1] = {"Depth", audioEffects.chorusDepth, 1};
        labels[2] = {"Freq", audioEffects.chorusLfoFreq, 2};
        break;
    case PAGE_REVERB:
        labels[0] = {"Reverb", audioEffects.reverbLevel, 1};
        labels[1] = {"Time", audioEffects.reverbLiveness, 1};
        labels[2] = {"Damp", audioEffects.reverbDamping, 1};
        break;
    case PAGE_MEMORY:
        labels[0] = {"FXs", effectsEnabledSelection ? 1.0f : 0.0f, 0};
        labels[1] = {"Load", (float)loadMemorySlot, 0};
        labels[2] = {"Save", (float)currentMemorySlot, 0};
        break;
    }
}

void printValue(float value, int precision)
{
    if (precision <= 0)
        display.print((int)value);
    else
        display.print(value, precision);
}

void printOptionValue(Layer &layer, LayerMenuPage page, int valueIndex, ParamLabel &label)
{
    if (page == PAGE_TYPE_WAVE_COMPLEX && valueIndex == 0)
    {
        const int filterTypes[] = {FILTER_NONE, FILTER_LPF, FILTER_BPF, FILTER_HPF, FILTER_LPF24};
        const char *filterNames[] = {"NO", "LPF", "BPF", "HPF", "L24"};
        for (int i = 0; i < 5; i++)
        {
            if (layer.filterType == filterTypes[i])
            {
                display.print(i);
                display.print(filterNames[i]);
                return;
            }
        }
    }
    else if (page == PAGE_TYPE_WAVE_COMPLEX && valueIndex == 1)
    {
        const int waveforms[] = {SAW_DOWN, TRIANGLE, PULSE, NOISE};
        const char *waveNames[] = {"SAW", "TRI", "PUL", "NOI"};
        for (int i = 0; i < 4; i++)
        {
            if (layer.waveform == waveforms[i])
            {
                display.print(i);
                display.print(waveNames[i]);
                return;
            }
        }
    }
    else if (page == PAGE_VOL_HARMONY_SCALE && valueIndex == 1)
    {
        const char *harmonyNames[] = {"NO", "INT", "CHO", "SEQ"};
        const int harmonyIndex = (int)layer.harmonyMode;
        if (harmonyIndex >= 0 && harmonyIndex < 4)
        {
            display.print(harmonyIndex);
            display.print(harmonyNames[harmonyIndex]);
            return;
        }
    }
    else if (page == PAGE_VOL_HARMONY_SCALE && valueIndex == 2 && layer.harmonyMode == HARMONY_SEQUENCE)
    {
        const int scaleIndex = (int)layer.harmonyParam;
        if (scaleIndex >= 0 && scaleIndex < NUM_SCALE_TYPES)
        {
            display.print(scaleIndex);
            const char *scaleName = SCALES[scaleIndex].name;
            for (int i = 0; i < 3 && scaleName[i] != '\0'; i++)
                display.print((char)(scaleName[i] >= 'a' && scaleName[i] <= 'z' ? scaleName[i] - ('a' - 'A') : scaleName[i]));
            return;
        }
    }
    else if (page == PAGE_VOL_HARMONY_SCALE && valueIndex == 2 && layer.harmonyMode == HARMONY_CHORD)
    {
        const int chordIndex = (int)layer.harmonyParam;
        if (chordIndex >= 0 && chordIndex < NUM_CHORD_TYPES)
        {
            display.print(chordIndex);
            const char *chordName = CHORDS[chordIndex].name;
            for (int i = 0; i < 3 && chordName[i] != '\0'; i++)
                display.print((char)(chordName[i] >= 'a' && chordName[i] <= 'z' ? chordName[i] - ('a' - 'A') : chordName[i]));
            return;
        }
    }
    else if (page == PAGE_DISTORTION && valueIndex == 0)
    {
        display.print(audioEffects.distType ? "1FLD" : "0DRV");
        return;
    }

    if (label.name[0] == '\0')
        return;

    printValue(label.value, label.precision);
}

struct EyeState
{
    int pupilX, pupilY;             // posición actual de la pupila (suavizada)
    int targetPupilX, targetPupilY; // hacia dónde se está moviendo
    unsigned long nextLookTime;

    enum BlinkPhase
    {
        EYE_OPEN,
        EYE_CLOSING,
        EYE_CLOSED,
        EYE_OPENING
    };
    BlinkPhase blinkPhase;
    unsigned long blinkPhaseStart;
    unsigned long nextBlinkTime;
    bool pendingDoubleBlink;
};

EyeState eye;

const int EYE_CENTER_X = 64;
const int EYE_CENTER_Y = 32;
const int EYE_WIDTH = 118;
const int EYE_HEIGHT = 56;
const int PUPIL_RADIUS = 13;
const int PUPIL_RANGE_X = (EYE_WIDTH / 2) - PUPIL_RADIUS - 6; // margen para no tocar el borde
const int PUPIL_RANGE_Y = (EYE_HEIGHT / 2) - PUPIL_RADIUS - 6;

long randomMsBetween(float minSec, float maxSec)
{
    return random((long)(minSec * 1000), (long)(maxSec * 1000));
}

void initEyeAnimation()
{
    eye.pupilX = eye.targetPupilX = 0;
    eye.pupilY = eye.targetPupilY = 0;
    eye.nextLookTime = millis() + randomMsBetween(0.3f, 1.0f);
    eye.nextBlinkTime = millis() + randomMsBetween(5.0f, 7.0f);
    eye.blinkPhase = EyeState::EYE_OPEN;
    eye.pendingDoubleBlink = false;
}

void updateEyeState()
{
    unsigned long now = millis();

    // --- Movimiento de la pupila ---
    if (now >= eye.nextLookTime)
    {
        eye.targetPupilX = random(-PUPIL_RANGE_X, PUPIL_RANGE_X + 1);
        eye.targetPupilY = random(-PUPIL_RANGE_Y, PUPIL_RANGE_Y + 1);
        eye.nextLookTime = now + randomMsBetween(0.3f, 1.0f);
    }
    eye.pupilX += (eye.targetPupilX - eye.pupilX) / 3; // suavizado simple, sin saltos bruscos
    eye.pupilY += (eye.targetPupilY - eye.pupilY) / 3;

    // --- Máquina de estados del parpadeo ---
    const unsigned long CLOSING_MS = 60;
    const unsigned long CLOSED_MS = 60;
    const unsigned long OPENING_MS = 80;

    switch (eye.blinkPhase)
    {
    case EyeState::EYE_OPEN:
        if (now >= eye.nextBlinkTime)
        {
            eye.blinkPhase = EyeState::EYE_CLOSING;
            eye.blinkPhaseStart = now;
        }
        break;

    case EyeState::EYE_CLOSING:
        if (now - eye.blinkPhaseStart >= CLOSING_MS)
        {
            eye.blinkPhase = EyeState::EYE_CLOSED;
            eye.blinkPhaseStart = now;
        }
        break;

    case EyeState::EYE_CLOSED:
        if (now - eye.blinkPhaseStart >= CLOSED_MS)
        {
            eye.blinkPhase = EyeState::EYE_OPENING;
            eye.blinkPhaseStart = now;
        }
        break;

    case EyeState::EYE_OPENING:
        if (now - eye.blinkPhaseStart >= OPENING_MS)
        {
            eye.blinkPhase = EyeState::EYE_OPEN;

            if (eye.pendingDoubleBlink)
            {
                eye.pendingDoubleBlink = false;
                eye.nextBlinkTime = now + randomMsBetween(0.2f, 1.0f); // segundo pestañeo rápido
            }
            else
            {
                eye.nextBlinkTime = now + randomMsBetween(5.0f, 7.0f);
                eye.pendingDoubleBlink = (random(0, 100) < 20); // 20% de chance de doble pestañeo
            }
        }
        break;
    }
}

int computeBlinkClosure()
{
    const unsigned long CLOSING_MS = 60;
    const unsigned long OPENING_MS = 80;
    unsigned long elapsed = millis() - eye.blinkPhaseStart;
    int maxClosure = EYE_HEIGHT / 2 + 2; // un poco de margen para tapar del todo

    switch (eye.blinkPhase)
    {
    case EyeState::EYE_OPEN:
        return 0;
    case EyeState::EYE_CLOSING:
        return (int)((float)elapsed / CLOSING_MS * maxClosure);
    case EyeState::EYE_CLOSED:
        return maxClosure;
    case EyeState::EYE_OPENING:
        return maxClosure - (int)((float)elapsed / OPENING_MS * maxClosure);
    }
    return 0;
}

void drawEyeAnimation()
{
    display.clearDisplay();
    updateEyeState();

    // Forma del ojo: un "estadio" (rectángulo con puntas redondeadas), casi toda la pantalla
    int ex = EYE_CENTER_X - EYE_WIDTH / 2;
    int ey = EYE_CENTER_Y - EYE_HEIGHT / 2;
    display.fillRoundRect(ex, ey, EYE_WIDTH, EYE_HEIGHT, EYE_HEIGHT / 2, SSD1306_WHITE);

    // Pupila, siguiendo el movimiento suavizado
    display.fillCircle(EYE_CENTER_X + eye.pupilX, EYE_CENTER_Y + eye.pupilY, PUPIL_RADIUS, SSD1306_BLACK);

    // Párpados: tapan desde arriba y desde abajo según el estado del parpadeo
    int closure = computeBlinkClosure();
    if (closure > 0)
    {
        display.fillRect(0, 0, SCREEN_WIDTH, ey + closure, SSD1306_BLACK);
        display.fillRect(0, ey + EYE_HEIGHT - closure, SCREEN_WIDTH, SCREEN_HEIGHT - (ey + EYE_HEIGHT - closure), SSD1306_BLACK);
    }
    display.display();
}

void updateOLED()
{
    display.clearDisplay();

    // variable para saber si debe mostrar los separadores y el nombre del menu
    bool displayMenuInfo = selectedLayer != MENU_EFFECTS ||
                           (currentLayerPage != PAGE_REVERB && currentLayerPage != PAGE_CHORUS) || effectsEnabledConfig == 1;

    const int displayedLayerIndex = selectedLayer == MENU_LAYER_1 ? 1 : 0;
    Layer &l = layers[displayedLayerIndex];
    ParamLabel labels[3];
    getPageLabels(l, currentLayerPage, labels);

    display.setTextColor(SSD1306_WHITE);

    // --- Separadores de las zonas superiores ---
    if (currentLayerPage == PAGE_MEMORY)
    {
        display.drawLine(0, 42, 128, 42, SSD1306_WHITE);
    }
    else
    {
        if (displayMenuInfo)
        {
            display.drawLine(64, 0, 64, 42, SSD1306_WHITE);
            display.drawLine(0, 21, 128, 21, SSD1306_WHITE);
            display.drawLine(0, 42, 128, 42, SSD1306_WHITE);
        }
    }

    // --- Zona 1: Capa actual (arriba izq.) ---
    display.setCursor(2, 5);

    if (displayMenuInfo && currentLayerPage != PAGE_MEMORY)
    {
        display.print("Capa:");
        const int boxX = 42, boxY = 0, boxW = 18, boxH = 16;
        int colorLayerBox;
        int colorLayerText;
        if (l.active)
        {
            colorLayerBox = SSD1306_WHITE;
            colorLayerText = SSD1306_BLACK;
        }
        else
        {
            colorLayerBox = SSD1306_BLACK;
            colorLayerText = SSD1306_WHITE;
        }
        display.fillRect(boxX, boxY, boxW, boxH, colorLayerBox); // recuadro invertido: capa activa
        display.setTextColor(colorLayerText);

        display.setCursor(boxX + 6, boxY + 5);
        switch (selectedLayer)
        {
        case MENU_LAYER_0:
            display.print("A");
            break;
        case MENU_LAYER_1:
            display.print("B");
            break;
        case MENU_EFFECTS:
            if (displayMenuInfo && currentLayerPage != PAGE_MEMORY)
                display.print("FX");
            break;
        }
    }
    display.setTextColor(SSD1306_WHITE);

    if (currentLayerPage == PAGE_MEMORY)
    {
        const int optionX[3] = {2, 44, 86};
        const int optionWidth = 40;

        for (int i = 0; i < 3; i++)
        {
            const bool selected = (int)memoryMenuAction == i;
            if (selected)
                display.fillRect(optionX[i], 3, optionWidth, 36, SSD1306_WHITE);
            else
                display.drawRect(optionX[i], 3, optionWidth, 36, SSD1306_WHITE);

            display.setTextColor(selected ? SSD1306_BLACK : SSD1306_WHITE);
            display.setCursor(optionX[i] + 4, 9);
            display.print(labels[i].name);
            display.setCursor(optionX[i] + 4, 25);
            if (i == 1)
            {
                if (loadMemorySlot < 0)
                    display.print("INIT");
                else
                    display.print(loadMemorySlot);
            }
            else if (i == 2)
                display.print(currentMemorySlot);
            else
                display.print(effectsEnabledSelection ? "SI" : "NO");
        }
        display.setTextColor(SSD1306_WHITE);
    }
    else if ((currentLayerPage == PAGE_CHORUS || currentLayerPage == PAGE_REVERB) && !effectsEnabledConfig)
    {
        display.setCursor(19, 12);
        display.print("FX DESABILITADO");
        display.setCursor(10, 27);
        display.print("MAS OSCILADORES (");
        display.print(MAX_OSCS_PER_LAYER_NOFX);
        display.print(")");
    }
    else
    {
        // --- Zonas de parámetros ---
        display.setCursor(68, 7);
        if (labels[0].name[0] != '\0')
        {
            display.print(labels[0].name);
            display.print(":");
            printOptionValue(l, currentLayerPage, 0, labels[0]);
        }

        display.setCursor(2, 28);
        if (labels[1].name[0] != '\0')
        {
            display.print(labels[1].name);
            display.print(":");
            printOptionValue(l, currentLayerPage, 1, labels[1]);
        }

        display.setCursor(68, 28);
        if (labels[2].name[0] != '\0')
        {
            display.print(labels[2].name);
            display.print(":");
            printOptionValue(l, currentLayerPage, 2, labels[2]);
        }

        if (currentLayerPage == PAGE_PITCH_FILTER_RES && l.filterType == FILTER_NONE)
        {
            display.drawLine(68, 11, 126, 11, SSD1306_WHITE);
            display.drawLine(68, 32, 126, 32, SSD1306_WHITE);
        }

        if (currentLayerPage == PAGE_LENGTH_MELODY_TIEMPO && l.harmonyMode != HARMONY_SEQUENCE)
        {
            display.drawLine(68, 11, 126, 11, SSD1306_WHITE);
            display.drawLine(2, 32, 62, 32, SSD1306_WHITE);
            display.drawLine(68, 32, 126, 32, SSD1306_WHITE);
        }

        int activeOscillators = 0;
        for (int i = 0; i < l.numOscOnLayer; i++)
        {
            if (l.oscs[i].active)
                activeOscillators++;
        }
        if (currentLayerPage == PAGE_VOL_HARMONY_SCALE &&
            (l.harmonyMode == HARMONY_INTERVAL || l.harmonyMode == HARMONY_CHORD) && activeOscillators <= 1)
            display.drawLine(68, 32, 126, 32, SSD1306_WHITE);
    }

    // --- Zona 5: barra de menú [1][2][3][4] ---
    const int numPages = 4;
    const int menuY = 46, menuH = 16;
    const int boxWidth = 128 / numPages;
    int selectedPageIndex;
    if (selectedLayer == MENU_EFFECTS)
    { // se fija que pagina esta elegida segun este en efetos o no
        selectedPageIndex = (int)currentLayerPage - numPages;
    }
    else
    {
        selectedPageIndex = (int)currentLayerPage;
    }
    for (int i = 0; i < numPages; i++)
    {
        int x = i * boxWidth;
        display.drawRect(x, menuY, boxWidth, menuH, SSD1306_WHITE);
        if (i == selectedPageIndex)
        {
            display.fillRect(x + 1, menuY + 1, boxWidth - 2, menuH - 2, SSD1306_WHITE);
            display.setTextColor(SSD1306_BLACK);
        }
        else
        {
            display.setTextColor(SSD1306_WHITE);
        }
        display.setCursor(x + boxWidth / 2 - 3, menuY + 4);
        display.print(i + 1);
        display.setTextColor(SSD1306_WHITE);
    }

    display.display();
}