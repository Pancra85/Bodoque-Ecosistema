// #define MOZZI_AUDIO_PIN_1 14 //salida audio mozzi mono
// #define MOZZI_AUDIO_PIN_2 15  //salida audio mozzi stereo derecho (no se usa)
// #include <Mozzi.h>
// #include <Oscil.h>
// #include <tables/sin2048_int8.h> // Tabla de onda senoidal
// #include <AudioDelayFeedback.h>  // Librería para delays con eco
// #include <EventDelay.h>          // Para disparar el sonido sin bloquear el audio

// // Tamaño del búfer de Delay. 8192 celdas equivalen a 0.5 segundos (a 16384Hz).
// #define DELAY_CELLS 16384 

// // Usamos el delay sin parámetros extra para máxima compatibilidad
// AudioDelayFeedback <DELAY_CELLS> aDelay;

// // Oscilador para generar el sonido (Frecuencia, Tabla)
// Oscil<SIN2048_NUM_CELLS, AUDIO_RATE> aSin(SIN2048_DATA);

// // Temporizador para disparar una nota cada 1500 milisegundos
// EventDelay pulseTimer;

// // Variables de control
// int gain = 0; 

// void setup() {
//   startMozzi(); 
  
//   // Configurar el tiempo del delay al máximo de su capacidad (500ms)
// aDelay.setDelayTimeCells((uint16_t)DELAY_CELLS);
  
//   // Nivel de retroalimentación (feedback) del eco (rango de -128 a 127).
//   aDelay.setFeedbackLevel(100); 
  
//   // Configurar el temporizador de eventos
//   pulseTimer.set(1500); 
// }

// void updateControl() {
//   // Si ya pasaron los 1500ms, disparamos un pulso de sonido corto
//   if (pulseTimer.ready()) {
//     aSin.setFreq(440); // Nota LA (440Hz)
//     gain = 255;        // Encender volumen del pulso
//     pulseTimer.start(); // Reiniciar el temporizador
//   }

//   // Atenuar el volumen rápidamente para que sea un pulso corto
//   if (gain > 0) {
//     gain -= 5; 
//   }
// }

// AudioOutput updateAudio(){
//   // 1. Generar la señal limpia
//   int32_t cleanSignal = (aSin.next() * gain) >> 8; 

//   // 2. Procesar la señal a través del módulo de delay
//   int32_t delayedSignal = aDelay.next(cleanSignal);

//   // 3. Mezclar el sonido limpio con el sonido del eco
//   int32_t mixedSignal = cleanSignal + delayedSignal;

//   // Retornar la señal de audio adaptada para Mozzi
//   return MonoOutput::fromAlmostNBit(9, mixedSignal);
// }

// void loop() {
//   audioHook(); // Procesa el audio en tiempo real
// }
