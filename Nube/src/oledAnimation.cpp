#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "oledAnimation.h"

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

bool initOLEDAnimation()
{
    Wire.setSDA(12);
    Wire.setSCL(13);
    Wire.begin();
    if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS))
    {
        return false;
    }

    display.setRotation(2);
    display.clearDisplay();
    display.display();
    return true;
}

// Estructuras necesarias para la animación
struct AnimCloud
{
    float x;
    float y;
    uint8_t scale;
};

struct AnimParticle
{
    float x;
    float y;
    float speed;
};

// Función principal de renderizado
void renderSynthAnimation(uint16_t pot1, uint16_t pot2, uint16_t pot3,
                        const bool layerEnabled[4],
                        TextureMode textureMode)
{
  const int16_t OLED_W = 128;
  const int16_t OLED_H = 64;

  // Formas de nube: círculos con la base alineada. Antes el centro quedaba
  // parejo respecto a los costados -> arco de medialuna. Ahora todos los
  // puffs apoyan en la misma línea de base y el bulto grande queda arriba.
  struct Puff { int8_t dx, dy; uint8_t r; };
  struct PuffSet { const Puff* puffs; uint8_t count; };

  static const Puff puffsFar[] = {
    { -6, 0, 3 }, { -2, -1, 4 }, { 2, -1, 4 }, { 6, 0, 3 }
  };
  static const Puff puffsMid[] = {
    { -10, 0, 4 }, { -5, -1, 5 }, { 0, -3, 7 }, { 5, -1, 5 }, { 10, 0, 4 }
  };
  static const Puff puffsNear[] = {
    { -15, 0, 5 }, { -8, -1, 6 }, { 0, -3, 8 }, { 8, -1, 6 }, { 15, 0, 5 }
  };
  static const PuffSet puffSets[] = {
    { puffsFar,  (uint8_t)(sizeof(puffsFar)  / sizeof(Puff)) },
    { puffsMid,  (uint8_t)(sizeof(puffsMid)  / sizeof(Puff)) },
    { puffsNear, (uint8_t)(sizeof(puffsNear) / sizeof(Puff)) },
  };
  const uint8_t CLOUD_DEPTHS = (uint8_t)(sizeof(puffSets) / sizeof(PuffSet));

  struct Cloud { float x, y, speed; uint8_t depth; };
  const uint8_t MAX_CLOUDS = 6;
  static Cloud clouds[MAX_CLOUDS];

  struct RainDrop { float x, y, speed; };
  const uint8_t MAX_RAIN = 20;
  static RainDrop rain[MAX_RAIN];

  static bool     initialized = false;
  static uint32_t lastFrame   = 0;
  static int8_t   flashFrames = 0;
  static int16_t  boltX       = 0;

  if (!initialized) {
    randomSeed(micros());
    for (uint8_t i = 0; i < MAX_CLOUDS; i++) {
      clouds[i].depth = random(0, CLOUD_DEPTHS);
      clouds[i].y     = 6 + clouds[i].depth * 10 + random(0, 10);
      clouds[i].speed = 0.2f + clouds[i].depth * 0.25f + random(0, 10) / 100.0f;
      clouds[i].x     = random(0, OLED_W);
    }
    for (uint8_t i = 0; i < MAX_RAIN; i++) {
      rain[i].x     = random(0, OLED_W);
      rain[i].y     = random(0, OLED_H);
      rain[i].speed = 1.4f + random(0, 10) / 10.0f;
    }
    initialized = true;
  }

  // como máximo ~30 fps, sin importar cada cuánto te llamen esta función
  const uint32_t now = millis();
  if (now - lastFrame < 33) {
    return;
  }
  lastFrame = now;

  // -------- parámetros del sinte -> parámetros de la escena --------
  uint8_t activeLayers = 0;
  for (uint8_t i = 0; i < 4; i++) {
    if (layerEnabled[i]) activeLayers++;
  }

  const float instability = pot2 / 16384.0f;               // pot2: inestabilidad
  const float energyBoost = 1.0f + activeLayers * 0.06f;   // más capas activas = más energía
  const float windFactor  = (1.0f + instability * 1.3f) * energyBoost *
                             (textureMode == TEXTURE_WIND ? 1.3f : 1.0f);

  const uint8_t visibleClouds =
      2 + (uint8_t)((pot1 / 16384.0f) * (MAX_CLOUDS - 2));  // pot1: bruma/cobertura

  display.clearDisplay();

  // -------- nubes: mover --------
  for (uint8_t i = 0; i < visibleClouds; i++) {
    clouds[i].x -= clouds[i].speed * windFactor;
    if (clouds[i].x < -40) {
      clouds[i].depth = random(0, CLOUD_DEPTHS);
      clouds[i].y     = 6 + clouds[i].depth * 10 + random(0, 10);
      clouds[i].speed = 0.2f + clouds[i].depth * 0.25f + random(0, 10) / 100.0f;
      clouds[i].x     = OLED_W + random(5, 40);
    }
  }

  // -------- nubes: dibujar de atrás hacia adelante --------
  // así una nube de adelante tapa el cuerpo y la carita de las que quedan atrás
  for (uint8_t depth = 0; depth < CLOUD_DEPTHS; depth++) {
    for (uint8_t i = 0; i < visibleClouds; i++) {
      if (clouds[i].depth != depth) continue;

      const PuffSet &set = puffSets[depth];
      const int16_t cx = (int16_t)roundf(clouds[i].x);
      const int16_t cy = (int16_t)roundf(clouds[i].y);

      for (uint8_t p = 0; p < set.count; p++) {
        const float phase = (now / 500.0f) + p * 0.8f + i * 1.7f;
        const int8_t bob  = (int8_t)roundf(sinf(phase) * (0.5f + instability * 1.2f));
        display.fillCircle(cx + set.puffs[p].dx, cy + set.puffs[p].dy + bob,
                            set.puffs[p].r, SSD1306_WHITE);
      }

      // carita triste en el medio
      display.drawPixel(cx - 2, cy - 1, SSD1306_BLACK);
      display.drawPixel(cx + 2, cy - 1, SSD1306_BLACK);
      display.drawLine(cx - 3, cy + 2, cx,     cy + 1, SSD1306_BLACK);
      display.drawLine(cx,     cy + 1, cx + 3, cy + 2, SSD1306_BLACK);
    }
  }

  // -------- lluvia: sólo en modo WIND, intensidad = pot3 --------
  if (textureMode == TEXTURE_WIND) {
    const uint8_t activeDrops = (uint8_t)((pot3 / 16384.0f) * MAX_RAIN);
    for (uint8_t i = 0; i < activeDrops; i++) {
      rain[i].y += rain[i].speed * energyBoost;
      if (rain[i].y > OLED_H) {
        rain[i].y = -4;
        rain[i].x = random(0, OLED_W);
      }
      display.drawFastVLine((int16_t)rain[i].x, (int16_t)rain[i].y, 3, SSD1306_WHITE);
    }
  }

  // -------- rayos: sólo en modo CRYSTAL, frecuencia = pot2 --------
  if (textureMode == TEXTURE_CRYSTAL && flashFrames == 0) {
    const float chance = instability * 0.03f;
    if ((float)random(0, 10000) / 10000.0f < chance) {
      flashFrames = 2;
      boltX = (int16_t)random(10, OLED_W - 10);
    }
  }

  const bool flashing = (flashFrames > 0);
  if (flashing) {
    static const int8_t boltPts[][2] = { {0,0}, {-3,7}, {2,14}, {-2,21}, {3,28} };
    for (uint8_t i = 0; i + 1 < 5; i++) {
      display.drawLine(boltX + boltPts[i][0],     2 + boltPts[i][1],
                        boltX + boltPts[i + 1][0], 2 + boltPts[i + 1][1],
                        SSD1306_WHITE);
    }
    flashFrames--;
  }

  display.invertDisplay(flashing);
  display.display();
}