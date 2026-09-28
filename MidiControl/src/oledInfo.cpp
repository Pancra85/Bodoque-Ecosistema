#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ==========================================
// CONFIGURACIÓN OLED
// ==========================================
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ==========================================
// FUNCIONES DEL OLED
// ==========================================

bool initOLED() {
    Wire.setSDA(12);
    Wire.setSCL(13);
    Wire.begin();
    
    if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
        // Si falla, el programa sigue pero sin pantalla
        return false; 
    }

    display.setRotation(2); // Rotación que pediste
    display.clearDisplay();
    display.setTextSize(1);
    display.display();
    return true;
}

void updateOLED(uint8_t currentLayerPage, const uint8_t potValues[3][3],
                bool bpmReceiving, uint16_t currentBPM) {
    display.clearDisplay();

    if (bpmReceiving) {
        display.drawRect(76, 0, 52, 14, SSD1306_WHITE);
        display.setCursor(80, 3);
        display.setTextColor(SSD1306_WHITE);
        display.print("BPM: ");
        display.print(currentBPM);

        display.drawLine(42, 14, 42, 64, SSD1306_WHITE);
        display.drawLine(85, 14, 85, 64, SSD1306_WHITE);
        display.drawLine(0, 28, 128, 28, SSD1306_WHITE);

        for (int col = 0; col < 3; col++) {
            int startX = col * 43;

            if (col == currentLayerPage) {
                display.fillRect(startX, 14, 42, 14, SSD1306_WHITE);
                display.setTextColor(SSD1306_BLACK);
            } else {
                display.setTextColor(SSD1306_WHITE);
            }

            display.setCursor(startX + 18, 17);
            display.print(col == 0 ? "A" : col == 1 ? "B" : "C");
            display.setTextColor(SSD1306_WHITE);

            display.setCursor(startX + 14, 30);
            display.print(potValues[2][col]);
            display.setCursor(startX + 14, 42);
            display.print(potValues[0][col]);
            display.setCursor(startX + 14, 54);
            display.print(potValues[1][col]);
        }

        display.display();
        return;
    }

    // Dibujar líneas verticales para separar las 3 cajas (Columnas A, B, C)
    // Pantalla de 128px de ancho: 128 / 3 = ~42px por columna
    display.drawLine(42, 0, 42, 64, SSD1306_WHITE);
    display.drawLine(85, 0, 85, 64, SSD1306_WHITE);
    
    // Línea horizontal para separar los títulos de los valores
    display.drawLine(0, 15, 128, 15, SSD1306_WHITE);

    // Bucle para dibujar cada una de las 3 columnas
    for(int col = 0; col < 3; col++) {
        int startX = col * 43; // Coordenada X inicial de cada columna (0, 43, 86)
        
        // --- FILA 1: Títulos A, B, C ---
        if (col == currentLayerPage) {
            // Resaltar la caja seleccionada (Fondo blanco, texto negro)
            display.fillRect(startX, 0, 42, 15, SSD1306_WHITE);
            display.setTextColor(SSD1306_BLACK);
        } else {
            // Cajas inactivas (Fondo negro, texto blanco)
            display.setTextColor(SSD1306_WHITE);
        }
        
        display.setCursor(startX + 18, 4); // Centrar título aprox.
        if(col == 0) display.print("A");
        else if(col == 1) display.print("B");
        else display.print("C");
        
        // Volver a color blanco para los valores numéricos
        display.setTextColor(SSD1306_WHITE);

        // --- FILA 2: Valor de Pi --- (Índice 2 en nuestra matriz)
        display.setCursor(startX + 14, 20);
        display.print(potValues[2][col]);

        // --- FILA 3: Valor de Alfa --- (Índice 0 en nuestra matriz)
        display.setCursor(startX + 14, 36);
        display.print(potValues[0][col]);

        // --- FILA 4: Valor de Omega --- (Índice 1 en nuestra matriz)
        display.setCursor(startX + 14, 52);
        display.print(potValues[1][col]);
    }

    // Refrescar pantalla
    display.display();
}
