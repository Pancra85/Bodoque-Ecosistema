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

