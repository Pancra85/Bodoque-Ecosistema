# 🌊 Ommm -> Para el sintetizador Bodoque

Un sintetizador de dos capas (A y B) pensado para paisajes sonoros evolutivos: texturas que se mueven solas, melodías generativas y una capa de efectos opcional para darle cuerpo.

---

## 🎛️ Controles generales

| Control | Función |
|---|---|
| **Botón A** | Selecciona la Capa A |
| **Botón B** | Selecciona la Capa B |
| **Botón C** | Entra al menú de **Efectos** |
| **Botones 1 / 2 / 3 / 4** | Eligen la página de parámetros dentro del contexto actual (capa o efectos) |
| **3 potenciómetros** | Controlan los 3 parámetros de la página activa (ver más abajo qué hace cada uno) |

Cada vez que cambiás de página o de capa, los potenciómetros quedan "trabados" hasta que los movés — así evitás que un parámetro salte de golpe solo por haber cambiado de menú. Movelos un poco para "destrabarlos" y que empiecen a responder.

Al encender, el instrumento arranca con una pequeña animación de un ojo en la pantalla mientras calienta los efectos — apenas tocás cualquier control, pasa al menú normal.

---

## 🎚️ Las capas (A y B)

Cada capa es una voz independiente, con su propio grupo de osciladores, filtro, LFOs y generador de melodía. Se navega por 4 páginas:

### Página 1 — Tono y Filtro
- **Pote 1 — Resonancia**: qué tan marcado suena el filtro cerca del corte.
- **Pote 2 — Corte del filtro**: de oscuro/apagado a brillante. Es más sensible en la parte alta del recorrido, para dar más control fino en las frecuencias donde más se nota.
- **Pote 3 — Nota / Tono**: la altura general de la capa (la "raíz" sobre la que se arma todo lo demás).

### Página 2 — Forma de onda y Grosor
- **Pote 1 — Tipo de filtro**: apagado, pasa-bajos, pasa-banda, pasa-altos, u otro más pronunciado.
- **Pote 2 — Grosor**: cuánta "masa" tiene la capa. A más grosor, se suman más osciladores y LFOs, se desafinan un poco entre sí, y empiezan a moverse solos — la capa pasa de un tono simple a una textura densa y viva.
- **Pote 3 — Forma de onda**: el timbre base de los osciladores (más suave, más áspero, más hueco, más ruidoso, según la opción).

> 💡 Con el filtro apagado, los LFOs que normalmente moverían el filtro pasan a mover el tono en su lugar — la capa se vuelve más inquieta y caótica en vez de quedarse quieta.

### Página 3 — Volumen y Armonía
- **Pote 1 — Volumen**: qué tan presente está la capa en la mezcla.
- **Pote 2 — Detalle de armonía**: cambia de función según el modo elegido en el Pote 3 (ver tabla abajo).
- **Pote 3 — Modo de armonía**: cómo se comporta la capa musicalmente.

| Modo de armonía | Qué hace el Pote 2 en ese modo |
|---|---|
| **Ninguno** | La capa suena fija en su nota, sin movimiento armónico. |
| **Intervalo** | Ajusta libremente un sobretono por encima de la raíz (como afinar un armónico a mano). |
| **Acorde** | Elige qué tipo de acorde arma la capa. |
| **Secuencia** | Elige la escala sobre la que se genera la melodía. |

### Página 4 — Secuencia
Solo tiene efecto cuando la capa está en modo **Secuencia**:
- **Pote 1 — Largo**: cuántos pasos tiene la melodía antes de repetirse.
- **Pote 2 — Velocidad**: qué tan rápido avanza la secuencia.
- **Pote 3 — Densidad**: qué tan seguido hay una nota nueva. Al mínimo, silencios largos; al máximo, una nota en cada paso. Cambiar la densidad genera una melodía nueva en el momento.

> 💡 Cada combinación de semilla + densidad da siempre la misma melodía — así que podés "volver" a una idea que te gustó ajustando los mismos valores.

---

## 🌫️ Menú de Efectos (Botón C)

Una capa extra, opcional, que se aplica a la mezcla final de todo el instrumento.

### Página 1 — Distorsión
- **Pote 1**: tipo de distorsión (recorte duro o saturación más redondeada).
- **Pote 2**: mezcla (limpio ↔ totalmente distorsionado).
- **Pote 3**: intensidad del drive.

### Página 2 — Memoria
- **Pote 1**: activa o desactiva el motor de efectos para todo el instrumento (chorus/reverb). **Requiere reiniciar** para aplicarse — al mover este pote, el equipo se reinicia solo.
- **Pote 2**: elige en qué de las 20 posiciones vas a **guardar** el sonido actual.
- **Pote 3**: elige qué posición **cargar**. La primera posición del recorrido restaura los valores de fábrica.

### Página 3 — Chorus
- **Pote 1**: cantidad de chorus en la mezcla.
- **Pote 2**: velocidad del movimiento.
- **Pote 3**: profundidad del efecto.

### Página 4 — Reverb
- **Pote 1**: cantidad de reverb en la mezcla.
- **Pote 2**: absorción de agudos (cuanto más alto, más opaca la cola del reverb).
- **Pote 3**: tamaño/duración de la cola.

> ⚠️ Activar el motor de efectos le resta capacidad al instrumento para sostener capas muy densas (menos osciladores simultáneos por capa). Si preferís capas más grandes y no usás mucho chorus/reverb, podés dejarlo desactivado.

---

## 🔁 Guardar y recuperar sonidos

El instrumento recuerda hasta **20 presets** completos: ambas capas (tono, filtro, armonía, secuencia) y el estado de los efectos. Se guardan y cargan desde la **Página 2 del menú de Efectos**. Al encender, siempre arranca cargando la posición 1.