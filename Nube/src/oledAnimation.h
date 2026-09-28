#ifndef OLED_ANIMATION_H
#define OLED_ANIMATION_H

#include <Arduino.h>
// #include "main.cpp"

enum TextureMode : uint8_t
{
    TEXTURE_CALM = 0,
    TEXTURE_WIND = 1,
    TEXTURE_CRYSTAL = 2
};

bool initOLEDAnimation();
void renderSynthAnimation(uint16_t pot1, uint16_t pot2, uint16_t pot3,const bool layerEnabled[4],                          TextureMode textureMode);

#endif
