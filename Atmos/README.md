# Bodoque Atmos

Sintetizador ambiental generativo desarrollado para el hardware Bodoque, utilizando RP2040, AMY Synthesizer y PlatformIO.

## Descripcion

Bodoque Atmos permite explorar paisajes sonoros generativos mediante cuatro motores independientes, controles fisicos, modulacion y efectos digitales.

## Motores sonoros

- **ATMOS:** drones armonicos, respiracion organica y evolucion lenta.
- **CRYSTAL:** campanas, cristales y eventos minimalistas.
- **PULSE:** pulsos graves, acentos y patrones ritmicos.
- **TEXTURE:** particulas granulares sintetizadas y texturas experimentales.

## Controles principales

- Potenciometro 1: volumen.
- Potenciometro 2: movimiento y actividad.
- Potenciometro 3: profundidad espacial.
- Botones 1-4: activar o desactivar capas o familias sonoras.
- Boton A: cambiar ambiente.
- Boton B: restablecer capas o parametros musicales.
- Boton C: disparar un sonido caracteristico del motor.
- Mantener A+B: cambiar de motor sonoro.

## Controles expresivos

Mantener pulsado A permite utilizar los potenciometros como controles secundarios:

- A + Pot 1: pitch global (+/-24 semitonos).
- A + Pot 2: ring modulation.
- A + Pot 3: efecto caracteristico de cada motor.

Los efectos contextuales son:

- ATMOS: shimmer armonico.
- CRYSTAL: comb resonante.
- PULSE: bitcrusher.
- TEXTURE: frequency shifter aproximado.

## Hardware

- Microcontrolador RP2040.
- DAC PT8211.
- Motor de sintesis AMY.
- Botones, potenciometros y LEDs integrados en Bodoque.

Las conexiones se definen en `src/hardwarePins.h`.

## Compilacion

1. Instalar Visual Studio Code y PlatformIO.
2. Abrir la carpeta `Atmos`.
3. Ejecutar PlatformIO Build.
4. Cargar el firmware mediante PlatformIO Upload.

La configuracion y dependencias estan declaradas en `platformio.ini`.

## Estado

Version experimental 0.8.2.

Compilacion verificada con PlatformIO. El proyecto continua en desarrollo y requiere pruebas de estabilidad y rendimiento para uso prolongado.

## Creditos

Desarrollado para el ecosistema Bodoque.

Basado en la estructura de hardware y las herramientas del proyecto Bodoque-Ecosistema.

Repositorio original:
https://github.com/Pancra85/Bodoque-Ecosistema
