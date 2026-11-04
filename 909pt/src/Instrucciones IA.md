Bodoque 909pt:

vamos a hacer una maquina de ritmo, es una plataforma de sintesis hecho con una RP2040, un DAC PT8211 y la libreria AMY, no puede manejar muy rapido los floats ni divisiones, asi que evitemos eso donde se pueda. 
Apenas prende empieza a un bpm=130.
Todos los golpes que se aprietan suenan y se graban en el secuenciador de 16 pasos.
Los botones BUTTON_PIN[] son 4 golpes diferentes en este orden: kick, snare, hihat closed, hihat open.
Vamos a empezar con eso y despues vemos. 
A medida que el secuenciador va avanzando de step, se muestra en los leds LED_BUTTON_PIN[], que son 4, o sea que indican en que posicion de la negra esta.
Fijate que la funcion updateAudioOutput() está hecha para tener una salida especial para el PT8211, no es como generalmente se usa la libreria AMY.
Te paso los archivos que tengo hecho hasta ahora, es un esqueleto, vos escribi en el archivo correspondiente así queda prolijo, cada funcion dentro de su archivo correspondiente, y no hagas mas archivos.


Te paso un ejemplo de AMY que genera sonido de batería para que veas como es, pero yo quiero usar los sonidos de la 909 de AMY y tampoco quiero usar el sistema de ticks por que es muy pesado, quiero usar un sistema simple, hecho desde cero hecho con una variable currentStep, y una variable array bool que indique donde están los golpes. Ojo que el ejemplo genera los sonidos para otro DAC, es para que te des una idea:

// BillieJeanDrums - Making a simple drum pattern with AMY.
// see https://github.com/shorepine/amy/blob/main/docs/billie_jean.md

#include <AMY-Arduino.h>

void setup() {
  amy_config_t amy_config = amy_default_config();
  amy_config.features.startup_bleep = 1;
  // Install the default_synths on synths (MIDI chans) 1, 2, and 10.
  amy_config.features.default_synths = 1;

  // If running an AMYboard, we set these parameters for you automatically. 
  // For your own boards, set the pins and features you want.
  #ifndef AMYBOARD_ARDUINO
  amy_config.audio = AMY_AUDIO_IS_I2S;
  // Pins for i2s board
  amy_config.i2s_bclk = 8;
  amy_config.i2s_lrc = 9;
  amy_config.i2s_dout = 10;
  #endif
  
  amy_start(amy_config);
}

struct timed_note {
  float start_time;  // In ticks
  int note;
  float velocity;
};

// 35 is kick, 37 is snare, 42 is closed hat, 46 is open hat
// Notes must be sorted in start_time order.
timed_note notes[] = {
  { 0.0, 42, 1.0},  //  0 HH + BD
  { 0.0, 35, 1.0},
  { 1.0, 42, 1.0},  //  1 HH
  { 2.0, 42, 1.0},  //  2 HH + SN
  { 2.0, 37, 1.0},
  { 3.0, 42, 1.0},  //  3 HH
  { 4.0, 42, 1.0},  //  4 HH + BD
  { 4.0, 35, 1.0},
  { 5.0, 42, 1.0},  //  5 HH
  { 6.0, 42, 1.0},  //  6 HH + SN
  { 6.0, 37, 1.0},
  { 7.0, 46, 1.0},  //  7 OH
};
// Time (in ticks) at which we reset to the start of the table.
float cycle_len = 8.0;

float millis_per_tick = 250;
float base_tick = 0;  // Time of beginning of current cycle.

int note_tab_index = 0;
int note_tab_len = sizeof(notes) / sizeof(timed_note);

void loop() {
  // Your loop() must contain this call to amy:
  amy_update();

  // Calculate "tick time" and choose note.
  float tick_in_cycle = millis() / millis_per_tick - base_tick;
  if (tick_in_cycle >= cycle_len) {
    // Start the next cycle - reset the cycle base_tick, reset the note_tab index.
    tick_in_cycle -= cycle_len;
    base_tick += cycle_len;
    note_tab_index = 0;
  }

  // Play any notes for this moment from the note table.
  while(note_tab_index < note_tab_len 
        && tick_in_cycle >= notes[note_tab_index].start_time) {
    // Grab the note parameters
    int midi_note = notes[note_tab_index].note;
    float velocity = notes[note_tab_index].velocity;
    // Time to play the note.
    amy_event e = amy_default_event();
    e.synth = 10;  // drums channel
    e.midi_note = midi_note;
    e.velocity = velocity;
    amy_add_event(&e);
    note_tab_index++;
  }
}