# Grabador y reproductor I2S

Al encender, espera a que se presione cualquiera de los cuatro botones de nota
para empezar a grabar el INMP441 en SRAM. Al soltar ese mismo boton, reproduce
la toma en bucle por el DAC PT8211. La grabacion se pierde al apagar o reiniciar.

- Microfono: BCLK GP12, WS GP13 y DATA GP16.
- PT8211: BCLK GP20, WS GP21 y DATA GP22.
- Botones de nota: GP2, GP4, GP5 y GP7 (con pull-up interno).
- `LED_X_PIN`: muestra el nivel de entrada durante la grabacion (un LED por
  cada 25% de amplitud, apagados en silencio y llenandose desde el ultimo LED
  del arreglo hacia el primero).
- `LED_BUTTON_PIN`: muestra la barra de progreso durante la reproduccion; en reversa
  avanza en orden inverso.
- Audio guardado: mono, 32 kHz y 8 bits; el buffer de 160 KiB permite hasta
  5,12 s. Al llenarse, detiene la grabacion y espera que sueltes el boton
  antes de reproducir.
- Salida al PT8211: 16 bits, LSBJ, con frecuencia variable y la señal mono en
  ambos canales.
- Se descartan los primeros 200 ms despues de iniciar el microfono para evitar
  el transitorio inicial.
- `POT_PIN[1]` controla la velocidad y el sentido: a la izquierda reproduce
  hacia atras y a la derecha hacia adelante. La zona central configurable
  (`playbackSpeedDeadbandMin/Max`) usa la velocidad minima hacia adelante; al
  alejarse del centro, la velocidad sube hasta el maximo en cada sentido. Los
  LEDs de progreso tambien avanzan en orden inverso durante la reproduccion
  hacia atras.
- `POT_PIN[0]` controla el volumen linealmente. El maximo es x16 y satura la
  salida de 16 bits. Limites, correccion inicial x0,5 y tiempos se pueden
  cambiar en `src/synth.cpp`.
- `POT_PIN[2]` controla la frecuencia I2S solicitada entre 1 y 32000 Hz con
  una curva convexa hacia valores altos (`outputSampleRateCurve`). El avance
  de las muestras se compensa para mantener la velocidad de reproduccion.
  Debajo de 120 Hz, el I2S queda en su minimo fisico de 120 Hz.

Si el nivel grabado queda muy bajo o satura, ajustar `PCM_SHIFT` en
`src/synth.cpp`.
