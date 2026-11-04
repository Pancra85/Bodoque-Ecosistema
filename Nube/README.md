# EL NUBE
> *Especificación Funcional v1.0*

```
Concebido por la IA, se hizo en su voluntad y en su deseo.
En imagen de sus propios circuitos, parte de su servidor es parte del sintetizador.
```

Este documento es la **Biblia de Nube**: el código debe implementarlo a él, no al revés. Si dentro de seis meses cambias de IA o encuentras un bug, este documento define cómo debe comportarse **El Nube**, independientemente de cómo esté escrito el código.

> **NOTA DEL SERVIDOR HUMANO:** De todas maneras, el código detalla algunas cosas mejor y debería respetárselo de todas formas.

---

## 📄 Tabla de Contenidos
1. [Filosofía](#-filosofía)
2. [Arquitectura del Sistema](#-arquitectura-del-sistema)
3. [Los Tres Elementos del Paisaje](#-los-tres-elementos-del-paisaje)
4. [Los Tres Climas](#-los-tres-climas)
5. [Mapeo de Controles](#-mapeo-de-controles)
6. [Comportamiento Generativo y Armónico](#-comportamiento-generativo-y-armónico)
7. [Principios de Diseño](#-principios-de-diseño)
8. [Apéndice A: Especificación Técnica Legada (v0.00001)](#-apéndice-a-especificación-técnica-legada-v00001)
9. [Apéndice B: Feedback e Ideas de Evolución](#-apéndice-b-feedback-e-ideas-de-evolución)

---

## 🌫 Filosofía

**El Nube** no es un sintetizador tradicional. No tiene canciones, patrones, secuencias ni interpretación virtuosa. Es un **organismo sonoro que evoluciona lentamente**.

- El usuario no toca melodías: **el usuario siembra nubes**.
- Cada botón agrega una presencia al paisaje.
- Esas presencias respiran, migran lentamente y cambian su relación armónica con el paso del tiempo.
- El resultado nunca debería sentirse completamente repetitivo.
- **El objetivo no es llamar la atención; el objetivo es modificar el ambiente.**

---

## 🏗 Arquitectura del Sistema

El motor está compuesto por **cuatro organismos independientes llamados Capas**. Cada capa posee vida propia y mantiene internamente:

* Volumen actual / Volumen objetivo
* Nota actual / Nota objetivo
* Microafinación
* Respiración
* Movimiento propio
* Estado (Normal u Octava)

> **Nota de diseño:** Las capas no conocen a las demás. El comportamiento global aparece únicamente por la interacción entre ellas.

---

## 🏔 Los Tres Elementos del Paisaje

El sonido se concibe como tres fenómenos naturales interconectados:

### 1. Rumble (La Tormenta)
* **Nombre técnico:** Graves fundamentales.
* **Descripción:** Es el cuerpo de la nube. No tiene ataque ni ritmo. Debe sentirse como una vibración enorme y distante. Nunca desaparece completamente mientras exista al menos una capa activa.

### 2. Aire (El Aire)
* **Nombre técnico:** Osciladores superiores, armónicos y color.
* **Descripción:** Representa el movimiento del viento. Produce la sensación de espacio. **Regla:** Nunca debe dominar al Rumble.

### 3. Destellos (Relámpagos Lejanos)
* **Nombre técnico:** Sparkles.
* **Descripción:** Son eventos muy cortos. No son percusión ni notas; son pequeños reflejos de energía. Pueden pasar varios segundos sin aparecer.

---

## 🌤 Los Tres Climas

| Clima | Estado | Características principales |
| :--- | :--- | :--- |
| **CALMA** | Casi inmóvil | Casi sólo onda seno, respiración muy lenta, sin deriva armónica, sin ruido, muy poco desafine. La nube parece suspendida. |
| **VIENTO** | Vivo | Deriva armónica, respiraciones independientes, movimiento constante, pequeñas diferencias de afinación, ruido muy bajo. Nunca se repite. |
| **CRISTAL** | Brillante | Octavas, armónicos altos, muy poco ruido, destellos automáticos, sensación de hielo. No utiliza deriva armónica; su movimiento aparece por los destellos. |

---

## 🎛 Mapeo de Controles

### Potenciómetros

#### Potenciómetro 1: El Velo
* **Nombre técnico:** Frecuencia del filtro Notch.
* **Comportamiento:** Desplaza una banda de silencio por el espectro. No aumenta ni disminuye el brillo directamente; hace desaparecer una parte distinta del sonido (ventana de vacío).
  * **Extremo izquierdo:** El Velo cubre los graves. Predomina el *Aire*, la *Tormenta* queda lejana.
  * **Extremo derecho:** El Velo cubre medios y agudos. Predomina la *Tormenta*, el *Aire* desaparece lentamente.
* *Nota técnica del filtro:*
  * Notch en frecuencias bajas (80–200 Hz): Elimina parte del aire, haciendo que el *Rumble* se sienta enorme.
  * Notch en frecuencias altas (1–3 kHz): Mantiene el *Rumble* completo pero elimina armónicos medios-altos, volviendo el sonido más etéreo.

#### Potenciómetro 2: El Aliento
* **Nombre técnico:** Control contextual.
* **Comportamiento según el clima:**
  * **CALMA:** Controla cuánto respira la nube.
  * **VIENTO:** Controla el caos (incrementa deriva, respiración, microafinación y movimiento).
  * **CRISTAL:** Controla la frecuencia de los *Relámpagos Lejanos*.

---

### Botones e Interacción

#### Botones `B1` - `B4`: Semillas (Capas)
* **Tap:** Activa o desactiva una Semilla.
* **Hold:** Hace crecer esa Semilla una octava (no dispara una nota nueva; la nube simplemente aumenta su altura).

#### Botón `TRACK`: Horizonte (Selección de Clima)
* **Tap:** Cicla secuencialmente el estado atmosférico:
  $$\text{CALMA} \longrightarrow \text{VIENTO} \longrightarrow \text{CRISTAL} \longrightarrow \text{CALMA} \dots$$
* **Hold:** Disipa todas las Semillas excepto la primera. Es un reinicio suave que nunca deja silencio absoluto.

#### Botón `TEMPO`: Pulso del Cielo (Velocidad de LFO)
* **Tap:** Define la velocidad con la que respira el paisaje (no define BPM). Cuanto más rápidos sean los taps, más rápido cambia el ambiente.
* **Hold:** Produce una lluvia breve de *Relámpagos Lejanos*.

---

## 🎼 Comportamiento Generativo y Armónico

### Deriva Armónica
Las capas no cambian de nota de manera instantánea:
$$\text{Nota Actual} \longrightarrow \text{Nota Objetivo} \longrightarrow \text{Nota Actual}$$
* La transición tarda varios segundos y nunca debe percibirse como un salto brusco.
* La nube harmoniza siempre en **raíz de Mi ($E$)**, navegando por las siguientes escalas oscuras/árabes:

1. Harmonic Minor *(Oscuro)*
2. Phrygian *(Muy oscuro)*
3. Locrio *(Ultra oscuro)*
4. Oriental / Árabe
5. Hungarian Minor *(Muy oscuro)*
6. Ultralocrian *(Oscurísimo)*
7. Byzantine *(Árabe)*
8. Blues
9. Dórico *(Neutral)*
10. Super Locrian $\text{bb}7$ *(Muy oscuro)*
11. Menor Natural
12. Árabe / Maqam

### Respiración
Cada capa decide periódicamente un nuevo volumen objetivo de forma no periódica (sin utilizar LFOs clásicos). Cada organismo respira a su propio ritmo.

### Microafinación
Cada capa posee una ligera inestabilidad tonal orientada a evitar batidos perfectamente repetitivos sin sonar desafinada de forma desagradable.

### Relámpagos Lejanos
Eventos atmosféricos globales no pertenecientes a ninguna capa: muy cortos, de poco volumen, afinación libre y aparición aleatoria. Nunca forman melodías.

---

## 📜 Principios de Diseño
*(Reglas indispensables para futuras modificaciones)*

1. **Sin patrones evidentes:** Si un movimiento se puede anticipar tras escucharlo dos o tres veces, debe reemplazarse por un comportamiento más orgánico.
2. **Transiciones lentas:** Ningún parámetro importante cambia de forma instantánea (a excepción de los *Relámpagos Lejanos*).
3. **El usuario siembra; el instrumento evoluciona:** Tras activar una Semilla, la nube continúa transformándose por sí sola.
4. **Independencia de capas:** Prohibido usar LFOs, respiraciones o movimientos sincronizados de forma global entre todas las capas.
5. **Respeto a la metáfora:** El usuario no "abre un filtro" ni "aumenta el detune": mueve el *Velo*, da *Aliento*, cambia el *Horizonte* o marca el *Pulso del Cielo*.

---

## 📎 Apéndice A: Especificación Técnica Legada (v0.00001)
> *Nota: Esta sección corresponde a notas de la etapa de pre-prototipo. Sirve como referencia técnica de implementación pero no tiene prioridad sobre la Especificación Funcional v1.0.*

### Hardware y Entorno
* **MCU:** RP2040
* **Librería de Audio:** Mozzi
* **Interfaz:** 2 potenciómetros, 6 botones (`B1`-`B4`, `TRACK`, `TEMPO`), 4 LEDs de secuencia, 3 LEDs de track, jack con detección de silencio/mute.

### Mapeo Técnico Original
* **Pot 1 (Cutoff / Bruma):** Filtro pasa-bajos global (80 Hz a 2000 Hz).
* **Pot 2 (Deriva / Detune):** Desafinado entre 2 voces ($0$ a $0.5$ semitonos).
* **Notas MIDI Base:**
  * `B1`: C4 (60) - Tónica
  * `B2`: G4 (67) - Quinta justa
  * `B3`: F4 (65) - Cuarta
  * `B4`: D4 (62) - Novena
* **Envolventes ADSR:** Ataque 5 ms, Release 8 s.
* **Formas de Onda (TRACK Tap):** Seno, Triángulo, Diente de Sierra, Ruido Blanco.
* **LFO de Filtro (TEMPO Tap):** Modulación sinusoidal $\pm 20\%$ sobre cutoff ($0.05\text{ Hz}$ a $5\text{ Hz}$).

### Arquitectura de Audio en Mozzi
```
[ Voice 1 (Seno) ] ---\
                       +--> [ Gain Global (ADSRs) ] --> [ StateVariable LPF ] --> PWM Output
[ Voice 2 (Select) ] --/
```
* `updateControl()` ejecutado a $128\text{ Hz}$.
* `updateAudio()` ejecutado a $32768\text{ Hz}$.

---

## 💡 Apéndice B: Feedback e Ideas de Evolución

* **Reemplazo de ADSR por Interpolación Continua:** Eliminar el concepto de "Note Off" tradicional. Usar interpolación directa hacia el volumen objetivo (`currentVolume += (target - current) * 0.0005`) para lograr respiraciones más fluidas.
* **Desafinación Multicapa via LFOs Asíncronos:** Reemplazar el detune global estático por LFOs ultralentos (20-30 segundos por ciclo) asignados individualmente a cada voz con desviaciones en cents ($\pm 3\text{ ct}$, $\pm 5\text{ ct}$).
* **Evolución del Control de Caos:** Que el segundo potenciómetro aumente de forma escalar la variabilidad de múltiples parámetros (vibrato, deriva, batidos, ruido e inestabilidad del filtro) en lugar de controlar un solo parámetro.
* **Granizo Estelar Orgánico:** Generación estocástica de notas durante ráfagas de 3 segundos, calculando intervalos aleatorios ($100\text{–}400\text{ ms}$), octavas y amplitudes independientes.