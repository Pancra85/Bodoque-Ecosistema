#include <Arduino.h>
#include <Control_Surface.h>
#include "oledInfo.h"

// ==========================================
// DEFINICIÓN DE PINES
// ==========================================

const pin_t PIN_BOTON_1 = 2;   // Nota 40
const pin_t PIN_BOTON_2 = 4;   // Nota 41
const pin_t PIN_BOTON_3 = 5;   // Nota 42
const pin_t PIN_BOTON_4 = 7;   // Nota 43

const pin_t PIN_LED_1 = 1;
const pin_t PIN_LED_2 = 3;
const pin_t PIN_LED_3 = 6;
const pin_t PIN_LED_4 = 8;

const pin_t PIN_BOTON_A = 0;
const pin_t PIN_BOTON_B = 24;
const pin_t PIN_BOTON_C = 25;

const pin_t PIN_POT_ALFA  = 29; 
const pin_t PIN_POT_OMEGA = 27; 
const pin_t PIN_POT_PI    = 26; 

const pin_t PIN_LED_X1       = 11;
const pin_t PIN_LED_X2       = 10;
const pin_t PIN_LED_X3       = 9;
const pin_t PIN_LED_ESTRELLA = 19;

// ==========================================
// ESTRUCTURAS Y OBJETOS MIDI
// ==========================================
USBMIDI_Interface midiusb;

// NoteButton manda el mensaje de nota al DAW automáticamente
NoteButton noteButtons[] = {
    { PIN_BOTON_1, { 40, CHANNEL_1 } },
    { PIN_BOTON_2, { 41, CHANNEL_1 } },
    { PIN_BOTON_3, { 42, CHANNEL_1 } },
    { PIN_BOTON_4, { 43, CHANNEL_1 } }
};

const pin_t noteLedPins[4]   = { PIN_LED_1, PIN_LED_2, PIN_LED_3, PIN_LED_4 };
const uint8_t noteNumbers[4] = { 40, 41, 42, 43 };

AH::Button btnPageA { PIN_BOTON_A };
AH::Button btnPageB { PIN_BOTON_B };
AH::Button btnPageC { PIN_BOTON_C };

FilteredAnalog<7> pots[] = {
    { PIN_POT_ALFA },  
    { PIN_POT_OMEGA }, 
    { PIN_POT_PI }     
};

// ==========================================
// ESTADO Y MATRICES
// ==========================================
uint8_t currentLayerPage = 0;

const uint8_t potCC[3][3] = {
    { 0, 3, 6 }, 
    { 1, 4, 7 }, 
    { 2, 5, 8 }  
};

uint8_t potValues[3][3] = {0};

bool oledPluggedIn = false;
unsigned long oledLastRefresh = 0;
const unsigned long oledRefreshTime = 10;

// Variables para el Clock
uint8_t bpmClockCount = 0;
unsigned long bpmMeasureStart = 0;     
unsigned long bpmLastClockMillis = 0;  
uint16_t currentBPM = 0;
bool bpmReceiving = false;
const unsigned long BPM_TIMEOUT = 1500;

unsigned long estrellaTimer = 0;
const unsigned long ESTRELLA_FLASH_MS = 60;

// ==========================================
// CALLBACKS MIDI
// ==========================================

bool channelMessageCallback(ChannelMessage cm) {
    uint8_t type = cm.header & 0xF0;
    uint8_t note = cm.data1;
    uint8_t velocity = cm.data2;

    // Escuchamos a Ableton (ignorando el canal exacto) y prendemos los LEDs
    if (type == 0x90 && velocity > 0) { // Note On
        for (int i = 0; i < 4; i++) {
            if (note == noteNumbers[i]) {
                digitalWrite(noteLedPins[i], HIGH);
            }
        }
    } 
    else if (type == 0x80 || (type == 0x90 && velocity == 0)) { // Note Off
        for (int i = 0; i < 4; i++) {
            if (note == noteNumbers[i]) {
                digitalWrite(noteLedPins[i], LOW);
            }
        }
    }
    return false; // Permite que Control_Surface siga operando internamente
}

bool sysExMessageCallback(SysExMessage se) { return false; }
bool sysCommonMessageCallback(SysCommonMessage sc) { return false; }

bool realTimeMessageCallback(RealTimeMessage rt) {
    if (rt.message == 0xF8) { 
        unsigned long nowMicros = micros();
        unsigned long nowMillis = millis();

        if (bpmLastClockMillis != 0 && (nowMillis - bpmLastClockMillis > BPM_TIMEOUT)) {
            bpmClockCount = 0;
        }

        if (bpmClockCount == 0) {
            bpmMeasureStart = nowMicros;
        }

        bpmClockCount++;
        bpmLastClockMillis = nowMillis;

        if (bpmClockCount > 24) {
            unsigned long elapsedMicros = nowMicros - bpmMeasureStart;
            if (elapsedMicros > 0) {
                currentBPM = 60000000UL / elapsedMicros; 
                bpmReceiving = currentBPM > 0;
            }
            bpmClockCount = 0; 
        }
    }
    return true; 
}

// ==========================================
// FUNCIONES AUXILIARES
// ==========================================

void setPage(uint8_t newPage) {
    if (currentLayerPage != newPage) {
        currentLayerPage = newPage;
        
        digitalWrite(PIN_LED_X1, currentLayerPage == 0 ? HIGH : LOW);
        digitalWrite(PIN_LED_X2, currentLayerPage == 1 ? HIGH : LOW);
        digitalWrite(PIN_LED_X3, currentLayerPage == 2 ? HIGH : LOW);
    }
}

void triggerEstrellaLED() {
    digitalWrite(PIN_LED_ESTRELLA, HIGH);
    estrellaTimer = millis();
}

void updateEstrellaLED() {
    if (digitalRead(PIN_LED_ESTRELLA) == HIGH) {
        if (millis() - estrellaTimer >= ESTRELLA_FLASH_MS) {
            digitalWrite(PIN_LED_ESTRELLA, LOW);
        }
    }
}

// ==========================================
// SETUP & LOOP
// ==========================================

void setup() {
    pinMode(PIN_LED_X1, OUTPUT);
    pinMode(PIN_LED_X2, OUTPUT);
    pinMode(PIN_LED_X3, OUTPUT);
    pinMode(PIN_LED_ESTRELLA, OUTPUT);

    // Inicializamos manualmente los pines de los LEDs de notas
    for (int i = 0; i < 4; i++) {
        pinMode(noteLedPins[i], OUTPUT);
    }

    btnPageA.begin();
    btnPageB.begin();
    btnPageC.begin();

    FilteredAnalog<>::setupADC();
    Control_Surface.begin();

    Control_Surface.setMIDIInputCallbacks(
        channelMessageCallback,
        sysExMessageCallback,
        sysCommonMessageCallback,
        realTimeMessageCallback
    );

    oledPluggedIn = initOLED();
    setPage(0);
}

void loop() {
    // Procesa entrada/salida MIDI de forma automática (botones de nota)
    Control_Surface.loop();

    // NOTA: Eliminamos la lógica de leer los botones físicos para apagar los LEDs acá.
    // Ahora los LEDs son dependientes 100% de la retroalimentación que mande Ableton.

    // 2. Navegación de páginas
    if (btnPageA.update() == AH::Button::Pressed) setPage(0);
    if (btnPageB.update() == AH::Button::Pressed) setPage(1);
    if (btnPageC.update() == AH::Button::Pressed) setPage(2);

    // 3. Lectura de potenciómetros
    bool ccChanged = false;
    for (uint8_t i = 0; i < 3; i++) {
        if (pots[i].update()) {
            uint8_t midiValue = pots[i].getValue();

            if (midiValue != potValues[i][currentLayerPage]) {
                uint8_t ccNumber = potCC[i][currentLayerPage];

                midiusb.sendControlChange({ ccNumber, CHANNEL_1 }, midiValue);
                potValues[i][currentLayerPage] = midiValue;

                ccChanged = true;
            }
        }
    }

    if (ccChanged) {
        triggerEstrellaLED();
    }

    updateEstrellaLED();

    // Timeout del BPM
    if (bpmReceiving && (millis() - bpmLastClockMillis >= BPM_TIMEOUT)) {
        bpmReceiving = false;
        currentBPM = 0;
    }

    // 4. Actualización de OLED
    if (oledPluggedIn && (millis() - oledLastRefresh >= oledRefreshTime)) {
        updateOLED(currentLayerPage, potValues, bpmReceiving, currentBPM);
        oledLastRefresh = millis();
    }
}