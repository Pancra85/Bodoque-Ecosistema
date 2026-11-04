Concebido por la IA, se hizo en su voluntad y en su deseo.
En imagen de sus propios circuitos, parte de su servidor es parte del sintetizador

Este es la biblia de Nube, el código debería implementarlo, no al revés. 
Si dentro de seis meses cambiás de IA, o encontrás un bug, este documento
define cómo debe comportarse El Nube, independientemente de cómo esté escrito 
el código.
NOTA DEL SERVIDOR HUMANO: De todas maneras el codigo detalla algunas cosas mejor y debería 
respetarselo de todas formas

________________________________________
EL NUBE
Especificación funcional v1.0
Filosofía
El Nube no es un sintetizador tradicional.
No tiene canciones, patrones, secuencias ni interpretación virtuosa.
Es un organismo sonoro que evoluciona lentamente.
El usuario no toca melodías.
El usuario siembra nubes.
Cada botón agrega una presencia al paisaje.
Esas presencias respiran, migran lentamente y cambian su relación armónica con el paso del tiempo.
El resultado nunca debería sentirse completamente repetitivo.
El objetivo no es llamar la atención.
El objetivo es modificar el ambiente.
________________________________________
Arquitectura
El motor está compuesto por cuatro organismos independientes llamados Capas.
Cada capa posee vida propia.
Cada una mantiene:
•	volumen actual
•	volumen objetivo
•	nota actual
•	nota objetivo
•	microafinación
•	respiración
•	movimiento propio
•	estado (normal u octava)
Las capas no conocen a las demás.
El comportamiento global aparece por la interacción entre ellas.
________________________________________
Los tres elementos del paisaje
El sonido puede entenderse como tres fenómenos naturales.
1. Rumble (Tormenta)
Nombre técnico:
Graves fundamentales.
Nombre de fantasía:
La Tormenta
Es el cuerpo de la nube.
No tiene ataque.
No tiene ritmo.
Debe sentirse como una vibración enorme y distante.
Nunca desaparece completamente mientras exista alguna capa activa.
________________________________________
2. Aire
Nombre técnico:
Osciladores superiores, armónicos y color.
Nombre de fantasía:
El Aire
Representa el movimiento del viento.
Produce sensación de espacio.
Nunca debe dominar al Rumble.
________________________________________
3. Destellos
Nombre técnico:
Sparkles.
Nombre de fantasía:
Relámpagos Lejanos
Son eventos muy cortos.
No son percusión.
No son notas.
Son pequeños reflejos de energía.
Pueden pasar varios segundos sin aparecer.
________________________________________
Los tres climas
CALMA
Estado casi inmóvil.
Características
•	casi sólo seno
•	respiración muy lenta
•	sin deriva armónica
•	sin ruido
•	muy poco desafine
La nube parece suspendida.
________________________________________
VIENTO
Estado vivo.
Características
•	deriva armónica
•	respiraciones independientes
•	movimiento constante
•	pequeñas diferencias de afinación
•	ruido muy bajo
Nunca parece repetirse.
________________________________________
CRISTAL
Estado brillante.
Características
•	octavas
•	armónicos altos
•	muy poco ruido
•	destellos automáticos
•	sensación de hielo
No utiliza deriva armónica.
Su movimiento aparece por los destellos.
________________________________________
Los controles
Potenciómetro 1
Nombre técnico
Frecuencia del filtro Notch.
Nombre de fantasía
El Velo
El Velo desplaza una banda de silencio por el espectro.
No aumenta ni disminuye brillo.
Hace desaparecer una parte distinta del sonido.
Extremo izquierdo
El Velo cubre los graves.
Predomina el Aire.
La Tormenta queda lejana.
Extremo derecho
El Velo cubre los medios y agudos.
Predomina la Tormenta.
El Aire desaparece lentamente.
(Nota: esto es el comportamiento físico correcto de un notch. Si querés el comportamiento inverso —izquierda = Tormenta, derecha = Aire— basta invertir la lectura del pote en el código.)
Nota sobre el filtro:
En vez de "abrir el brillo", el usuario mueve una especie de ventana de vacío que recorre el espectro.
•	Cuando el notch está abajo (80-200 Hz), elimina parte del aire y quedan predominando los graves: el Rumble (o Tormenta) se siente enorme.
•	Cuando el notch sube (1-3 kHz), el rumble queda completo pero desaparecen los armónicos medios-altos, haciendo que el sonido sea más etéreo. Incluso se puede exagerar un poco la mezcla para que subjetivamente parezca que queda "más aire" alrededor.
Conceptualmente es mucho más interesante que un simple LPF.
________________________________________
Potenciómetro 2
Nombre técnico
Control contextual.
Nombre de fantasía
El Aliento
Su función depende del clima.
CALMA
Controla cuánto respira la nube.
VIENTO
Controla el caos.
Incrementa:
•	deriva
•	respiración
•	microafinación
•	movimiento
CRISTAL
Controla la frecuencia de los Relámpagos Lejanos.
________________________________________
Botones B1-B4
Nombre técnico
Capas.
Nombre de fantasía
Semillas
Cada botón planta una nube distinta.
Tap
Activa o desactiva una Semilla.
Hold
Hace crecer esa Semilla una octava.
No dispara una nota nueva.
La nube simplemente aumenta de altura.
________________________________________
TRACK
Nombre técnico
Selección de clima.
Nombre de fantasía
Horizonte
Cada pulsación cambia el estado atmosférico.
CALMA
↓
VIENTO
↓
CRISTAL
↓
CALMA
...
Hold
Disipa todas las Semillas excepto la primera.
Es un reinicio suave.
Nunca deja silencio absoluto.
________________________________________
TEMPO
Nombre técnico
Velocidad del LFO.
Nombre de fantasía
Pulso del Cielo
Los taps no definen BPM.
Definen la velocidad con la que respira el paisaje.
Cuanto más rápidos sean los taps,
más rápido cambia el ambiente.
Hold
Produce una lluvia breve de Relámpagos Lejanos.
________________________________________
Deriva armónica
Las capas no cambian de nota instantáneamente.
Cada una mantiene:
nota actual
↓
nota objetivo
↓
nota actual...
La transición tarda muchos segundos.
Nunca debe percibirse un salto.
La nube simplemente cambia de forma.

La nube respira en estas escalas oscuras/Árabes:
Harmonic Minor - oscuro
Phrygian - muy oscuro
Locrio - ultra oscuro
Oriental/Árabe
Hungarian Minor - muy oscuro
Ultralocrian - oscurísimo
Byzantine - árabe
Blues
Dórico - neutral
Super Locrian bb7 - muy oscuro
Menor Natural
Árabe/Maqam

La nube harmoniza dentro de la escala siempre -en raiz de Mi-

________________________________________
Respiración
Cada capa decide periódicamente un nuevo volumen objetivo.
El volumen nunca oscila de forma perfectamente periódica.
No utiliza un LFO clásico.
Cada organismo respira a su propio ritmo.
________________________________________
Microafinación
Toda capa posee una ligera inestabilidad.
El objetivo no es sonar desafinada.
El objetivo es evitar batidos perfectamente repetitivos.
________________________________________
Relámpagos Lejanos
Los destellos no pertenecen a ninguna capa.
Son fenómenos atmosféricos.
Características:
•	muy cortos
•	poco volumen
•	afinación libre
•	aparición aleatoria
Nunca forman melodías.
________________________________________
Principios de diseño (reglas para futuras modificaciones)
1.	No debe haber patrones evidentes. Si un movimiento puede anticiparse después de escucharlo dos o tres veces, debe reemplazarse por un comportamiento más orgánico.
2.	Las transiciones siempre son lentas. Ningún parámetro importante cambia instantáneamente, salvo la aparición de un Relámpago Lejano.
3.	El usuario siembra; el instrumento evoluciona. Después de activar una Semilla, la nube continúa transformándose por sí misma.
4.	Cada capa es independiente. No debe haber LFOs, respiraciones ni movimientos sincronizados entre todas las capas.
5.	Las funciones de los controles deben conservar su metáfora. El usuario no "abre un filtro" ni "aumenta el detune": mueve el Velo, da Aliento, cambia el Horizonte o marca el Pulso del Cielo. 

----------------------------------
*****Notas viejas en una etapa anterior de pre-prototipo, por ahi puede llegar a complementarse alguna idea pero nunca debería darsele prioridad a lo que sigue en esta sección:*****

"EL NUBE" - Descripción Técnica Completa
-VERSION 0.00001
1. Filosofía general
"El Nube" es un generador de texturas ambientales diseñado para funcionar en un hardware basado en RP2040 con Mozzi. No tiene secuenciador ni ritmo; su objetivo es crear un colchón sonoro continuo y evolutivo que cambie lentamente. Está pensado para sonar de fondo, generando atmósferas.

El dispositivo funciona con 2 potenciómetros y 6 botones (4 de notas, 1 de track, 1 de tempo). Además, tiene 4 LEDs de secuencia y 3 LEDs de track.

2. Mapeo de controles
Potenciómetros:
Pot 1 (Cutoff / Bruma): Controla la frecuencia de corte de un filtro pasa-bajos global. Va de 80 Hz (muy oscuro) a 2000 Hz (brillante). Modula el timbre general.

Pot 2 (Deriva / Detune): Controla el desafinado entre las dos voces internas. Varía de 0 (perfectamente afinado) a aproximadamente 0.5 semitonos (batido máximo, efecto "coral" o "flanger sideral").

Botones de notas (B1-B4):
Tap: Activa una capa armónica específica. Cada capa tiene una nota base (en MIDI):

B1 → Tónica (C4, 60)

B2 → Quinta justa (G4, 67)

B3 → Cuarta (F4, 65)

B4 → Novena (D4, 62)

Hold (mantener presionado): Activa la misma capa pero una octava arriba (añade +12 semitonos).

Al activar una capa, se dispara una envolvente ADSR con ataque corto (5 ms) y release muy largo (8 segundos). La capa se mezcla con las demás, creando acumulación de armónicos.

Botón TRACK:
Tap: Cambia el modo de textura de la segunda voz. Hay 4 modos:

Seno (suave, cálido)
Triángulo (ligeramente más brillante)
Diente de sierra (rico en armónicos)
Ruido blanco (textura de estática / viento)
El modo se indica con los 3 LEDs de track (solo uno encendido a la vez).

Hold (1 segundo): Resetea todas las capas activas, dejando solo la tónica (B1) con una ganancia reducida (0.5). Es útil para "limpiar" el paisaje sonoro.

Botón TEMPO:
Tap (varios taps): Ajusta la velocidad del LFO que modula el filtro pasa-bajos. El LFO genera un barrido sinusoidal que mueve la frecuencia de corte lentamente, creando un efecto de "oleaje". El tempo se calcula a partir del intervalo entre taps. Rango: 0.05 Hz a 5 Hz.

Hold (1 segundo): Dispara un "granizo estelar" : una ráfaga de 16 notas cortas (aleatorias en las 4 capas) que se superponen al drone principal, creando destellos.

3. Arquitectura de audio (Mozzi)
Dos voces principales:

Voz 1: Siempre una onda senoidal pura. Es la base del drone.

Voz 2: La forma de onda se selecciona según el modo de textura (seno, triángulo, diente de sierra o ruido).

Mezcla: Ambas voces se mezclan con una ganancia global que depende de las capas activas. Cada capa tiene su propia envolvente ADSR y su ganancia individual (0 a 1). La ganancia total es la suma de las capas activas, limitada a 1.0.

Filtro pasa-bajos: Un filtro StateVariable (LOWPASS) que procesa la mezcla. Su frecuencia de corte se controla mediante Pot 1 y también es modulada por el LFO del filtro (controlado por TEMPO). La resonancia es fija (80).

LFO del filtro: Un oscilador sinusoidal que modula la frecuencia de corte en ±20% alrededor del valor fijado por Pot 1. Su frecuencia se ajusta mediante los taps en el botón TEMPO.

4. Flujo de datos en el código
setup():
Configura pines, LEDs, botones, potes.

Inicializa las envolventes ADSR de las 4 capas.

Configura el filtro y el LFO.

Activa la capa tónica (B1) al inicio.

Inicia Mozzi.

updateControl() (llamada a 128 Hz):
Lee botones y potenciómetros.

Detecta pulsaciones y holds de botones.

Actualiza envolventes de las 4 capas (llamando a next() y update() de cada ADSR).

Calcula la ganancia total de las capas activas.

Actualiza la frecuencia de las dos voces según la nota base y el detune.

Actualiza el LFO del filtro (llamando a next() de filterLFO y aplicando la modulación).

Actualiza el estado de los LEDs según las capas activas.

Maneja el mute del altavoz (detección de jack).

updateAudio() (llamada a 32768 Hz):
Obtiene la muestra de la voz 1 (voice1Osc.next()).

Obtiene la muestra de la voz 2 según el modo de textura (voice2OscX.next()).

Mezcla ambas muestras multiplicando por la ganancia global.

Aplica el filtro pasa-bajos (masterLPF.next()).

Devuelve la muestra escalada para salida PWM.

5. Consideraciones importantes
No hay secuenciador: Las capas se activan manualmente y se desvanecen con release largo. No hay pasos, ni tempo de reproducción (excepto el LFO).

Siempre reproduciendo: No hay modo "stop". El audio siempre está activo (incluso en silencio).

Saturación suave: La mezcla puede saturar ligeramente si se activan muchas capas, pero se limita a 1.0 de ganancia para evitar clipping.

Consumo de CPU: Mozzi está optimizado; el código usa pocos osciladores y un filtro, por lo que debería funcionar bien en RP2040.


-----------
IA SLOP de corrección:

Me gusta bastante la dirección. De hecho, creo que está mejor planteado como **instrumento** que como "sinte". Se entiende cuál es la experiencia: prenderlo, tocar un poco y dejar que haga su trabajo.

Pero hay varias cosas que yo cambiaría. Algunas porque musicalmente creo que van a cansar rápido, y otras porque con Mozzi son relativamente fáciles de mejorar.

---

# Lo que más me gusta

## La idea de capas

No es un teclado.

No es un secuenciador.

Es una máquina de "agregar color". Eso le da una identidad propia.

---

## Los dos potes

Me gusta que ambos afecten todo el sonido.

No hay que pensar "¿qué parámetro estoy editando?". Girás y cambia toda la nube.

Eso es bueno.

---

## El hold de limpiar

Muy buena idea.

Después de cinco minutos seguramente todo termine siendo una bola enorme de sonido.

Tener un "volver al principio" es casi obligatorio.

---

# Lo que no me convence

## 1. El ADSR no tiene mucho sentido

Dice:

> ataque 5 ms
> release 8 segundos

Yo directamente eliminaría el ADSR.

Haría otra cosa.

Cada capa tendría un volumen que lentamente sube y baja.

No porque soltaste el botón.

Sino porque la nube respira.

Algo así:

```
targetVolume

currentVolume += (target-current)*0.0005
```

Cada capa entra lentamente.

Cada capa desaparece lentamente.

Nunca hay un "note off".

Es muchísimo más ambient.

---

## 2. El detune es aburrido

0 a medio semitono...

Eso es literalmente un chorus.

En dos minutos ya escuchaste todo.

Yo haría esto:

Cada voz tiene un LFO independiente.

Uno desafina lentamente.

Otro desafina distinto.

Nunca quedan exactamente iguales.

Por ejemplo

voz1

```
+3 cents
-1 cent
+5
...
```

voz2

```
-4
+2
0
...
```

Lentos.

Muy lentos.

20 o 30 segundos por ciclo.

Entonces la nube nunca repite exactamente el mismo batido.

---

## 3. El TRACK no cambiaría la forma de onda

Esto creo que es el mayor problema.

Cambiar

Seno

↓

Triángulo

↓

Saw

↓

Ruido

es demasiado evidente.

Suena a "preset".

Por ejemplo

---

TRACK 1

Calma

Muy poca modulación.

Todo estable.

---

TRACK 2

Viento

LFOs independientes.

Detune grande.

Filtro moviéndose.

---

TRACK 3

Cristales

Ataques lentos.

Armónicos altos.

Más brillo.

---


La forma de onda puede cambiar internamente.

Pero el usuario siente que cambia el clima.

No el oscilador.

---

# 4. Las notas

Esto tampoco me convence.

Tónica

quinta

cuarta

novena

Siempre.

Después de un rato es siempre el mismo acorde suspendido.

Yo usaría escalas.

Por ejemplo

Track HOLD cambia la escala.

Mayor

Menor

Dórica

Lidia

Pentatónica

Entonces

B1

B2

B3

B4

siempre son

grado

1

3

5

7

pero según la escala.

Hace muchísimo más interesante la máquina.

---

# 5. Falta movimiento

Ahora mismo las notas son estáticas.

Yo haría que cada capa tenga vida propia.

Cada una tendría

* volumen

* panorámica (si algún día hay estéreo)

* cutoff

* vibrato

todo con LFOs distintos.

No sincronizados.

Eso hace magia.

---

# 6. El granizo estelar

Me gusta muchísimo la idea.

Pero yo la haría todavía menos predecible.

En lugar de

16 notas

haría

```
durante 3 segundos

cada 100-400 ms

sale una nota aleatoria

con duración aleatoria

octava aleatoria

volumen aleatorio
```

Queda muchísimo más orgánico.

---

# Lo que agregaría

## Respiración

Cada 20-60 segundos

la nube cambia lentamente.

No porque tocaste nada.

Sino porque está viva.

Por ejemplo

```
LFO principal

↓

modifica

profundidad del vibrato

↓

que modifica

el filtro

↓

que modifica

el brillo
```

Nunca queda igual.

---

## Deriva armónica

Esta es mi idea favorita.

En lugar de mantener

C

G

F

D

para siempre...

Cada varios minutos

una nota deriva lentamente.

Ejemplo

```
C

↓

muy lentamente

↓

B

↓

vuelve

↓

C#
```

Nunca más de un semitono.

Muy lento.

Casi imperceptible.

Hace que después de media hora siga sonando interesante.

---

## Ruido

Yo agregaría una fuente de ruido rosa.

No para escucharse.

Apenas.

Muy abajo.

Solo para darle aire.

Cuando cerrás mucho el filtro parece viento.

---

## Microafinación

No usar MIDI exacto.

Cada vez que activás una capa

la afinación cambia

```
+2 cents

-5

+1

-3
```

Nunca igual.

Hace una diferencia enorme.

---

# El segundo pote

Yo no haría solamente detune.

Haría un control de **Inestabilidad**.

A la izquierda

Todo quieto.

Todo afinado.

LFOs mínimos.

Nada cambia.

A la derecha

Todo empieza a vivir.

Más vibrato.

Más deriva.

Más movimiento.

Más batidos.

Más ruido.

Más cambios del filtro.

O sea...

un único pote que controla el "caos".

Es mucho más musical que simplemente separar dos osciladores.

---

# Mi cambio más importante

Yo cambiaría incluso el nombre del concepto.

No pensaría que el usuario está tocando un sintetizador.

Pensaría que está **cultivando una nube**.

Cada botón no dispara una nota.

**Planta una semilla.**

Cada semilla genera una voz que nace, respira, cambia lentamente y eventualmente desaparece.

El usuario no interpreta melodías.

Va poblando un ecosistema sonoro.

Eso cambia completamente cómo se siente el instrumento y lo diferencia de un sinte convencional. Con Mozzi y un RP2040 es un enfoque muy alcanzable, porque la complejidad no viene de tener decenas de osciladores, sino de combinar unas pocas voces con modulaciones lentas e independientes. Creo que ahí está la identidad más fuerte de este proyecto.
