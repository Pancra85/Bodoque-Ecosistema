<img width="625" height="315" alt="image" src="https://github.com/user-attachments/assets/bf2d1487-cae4-407d-8577-b76c27af9f7a" />#Como cargar un programa del catálogo ecosistema existente de forma simple al Bodoque:
1- Bajar la carpeta de los programas del Bodoque: https://github.com/Pancra85/Bodoque-Ecosistema
 <img width="625" height="315" alt="image" src="https://github.com/user-attachments/assets/1386783f-ffef-4b23-9cbd-f9b637d71583" />
Guardarlos en /Documentos/Platformio/ o en cualquier lugar que gusten.
2- Dentro de lo que bajamos, vamos a la carpeta …/_drag&drop/. Ahí están todos los programas listos para subir en formato .uf2. 
Cada archivo uf2 es un programa diferente.
3- Enchufar por USB el Bodoque.
4- Correr el archivo cargar.bat dentro de esa carpeta.
5- Elegir el programa deseado de la lista. El programa se sube automáticamente. LISTO!


#Guía completa para desarrollar tu propio programa para el Bodoque:
1- Bajar e instalar VSCODE: https://code.visualstudio.com/download al instalarlo pongan todo que SI así como está
2- Bajar e instalar el GIT: https://github.com/git-for-windows/git/releases/download/v2.56.1.windows.1/Git-2.56.1-64-bit.exe
3- Dentro de VSCODE, ir a extensiones (en la barra de la izquierda) y buscar "Platformio IDE"
4- Loguearse desde el VSCODE (abajo a la izquierda), crear una cuenta de Github para poder acceder a la inteligencia artificial que viene incluida dentro del VSCODE.
5- Bajar la carpeta de los programas del Bodoque: https://github.com/Pancra85/Bodoque-Ecosistema
<img width="625" height="315" alt="image" src="https://github.com/user-attachments/assets/52a5d7c4-27a5-470a-bff9-18a550a66b55" />
Guardarlos en /Documentos/Platformio/ o en cualquier lugar que gusten.
Esos son todos los programas existentes del Bodoque, más la carpeta _etc/esqueleto vacio que nos va a servir luego como base para nuestra creación.
6- Abrimos el VSCODE.
En el Explorador de Windows, buscamos dentro de las carpetas del Ecosistema que acabamos de guardar \PlatformIO\Bodoque-Ecosistema\_etc\esqueleto vacio, hacemos una copia de esa carpeta dentro de Bodoque-Ecosistema. Le cambiamos el nombre a gusto, o sea que la carpeta va a quedar ahora en \PlatformIO\Bodoque-Ecosistema\Mi Programa\ junto con todos los demás programas.
Ahora arrastrar esa carpeta "Mi Programa” a la barra izquierda del VSCODE (se llama workspace la barra).
Con eso logramos cargar el programa a desarrollar en VSCODE.
<img width="625" height="335" alt="image" src="https://github.com/user-attachments/assets/90291916-90d1-4700-92e4-10aec367f5be" />
7-Una vez cargado, en el workspace vamos a la carpeta \src y ahí adentro, al archivo main.cpp. Ese es el archivo principal de cualquier programa. Ahí están las funciones principales, luego los demás archivos .cpp contienen otras funciones más específicas.
En el programa de esqueleto, main.cpp no usa ninguna función específica, salvo updateAudioOutput() que está definida en synth.cpp.
O sea, que nos importa de todo esto? Que para un programa básico hay que darle de contexto a la IA los archivos:
•	main.cpp: Básicamente no tiene nada importante todavía pero ahí debe construirse el programa.
•	synth.cpp: Contiene info de cómo debe manejarse la salida de audio según el hardware.
•	y hardwarePins.h: Este módulo contiene los lugares físicos donde se enchufan botones, leds y perillas, así como también los nombres de los controles y como debo referirme a ellos con la IA.
Nombres de los controles, definidos en hardwarePins.h:
const short POT_PIN[TOTAL_POTS] = {A0,A1,A3}; Pot son los potenciometros (perillas), puede ser POT_PIN[0], POT_PIN[1] o POT_PIN[2]

// Botones de Notas MIDI
const short BUTTON_PIN[NUM_NOTE_BUTTONS] = {2, 4, 5, 7}; Botones I,II,III y IV.
const short BUTTON_A_PIN = 0; Boton A.
const short BUTTON_B_PIN = 24; Boton B.
const short BUTTON_C_PIN = 25; Boton C.

// LEDs
const short LED_BUTTON_PIN[NUM_NOTE_BUTTONS] = {11,10,9,15};
const short LED_X_PIN[NUM_X_LEDS] = {1,3,6,8};
const short LED_ESTRELLA_PIN = 19;
Todos esos son los leds. 

Hay algunos controles que están sueltos, por ejemplo: BUTTON_A o LED_ESTRELLA_PIN y algunos están agrupados, por ejemplo: BUTTON_PIN[NUM_NOTE_BUTTONS]. En ese caso, se pone el número específico entre los corchetes, por ejemplo el Botón I sería: BUTTON_PIN[0].

8- Ejemplo de cómo le pediría algo a la IA, Claude o ChatGPT:
“Quiero hacer un sintetizador, estoy usando Platformio en VSCODE, el microcontrolador es un RP2040 y está conectado a un DAC PT8211. Te voy a pasar para contexto los módulos .cpp con los que cuento ya y las conexiones de pins.
Mi sintetizador quiero que funciones de la siguiente manera:
Al apretar botón BUTTON_A_PIN quiero que suene un oscilador con forma de diente de sierra, al girar la perilla POT_PIN[0] va a cambiar el cutoff de un filtro LPF y al girar POT_PIN[1] cambia la resonancia del filtro. El POT_PIN[2] cambiará la frecuencia de la nota  del oscilador.
……bla bla…”

A- Lo más simple: Si vamos a usar la IA incorporada de GitHub (nos va a pedir loguearnos / crear nueva cuenta, bueno lo hacemos), arrastramos los archivos que necesita conocer la IA y en el Chat ponemos el prompt de arriba reescrito para nuestros requerimientos
<img width="625" height="336" alt="image" src="https://github.com/user-attachments/assets/663f13fb-d26f-4a7e-87a3-66cb4d48f485" />

B- Si se nos acabaron los tokens o lo que sea, podemos usar otra IA externa copiando y pegando el contenido de los archivos a mano, así:
Main.cpp:
[COPIAMOS Y PEGAMOS CONTENIDO DE MAIN.CPP]
Synth.cpp:
[COPIAMOS Y PEGAMOS CONTENIDO DE SYNTH.CPP]
hardwarePins.h:
[COPIAMOS Y PEGAMOS CONTENIDO DE HARDWAREPINS.CPP]

Una vez que nos responde, copiamos todo y lo pegamos en el VSCODE en el modulo donde corresponde. 

C- Si sabes lo que estás haciendo con la IA, hacelo como vos soles hacerlo. Todo oki, adelante!.
8- Enchufar el Bodoque, prenderlo y apretar UPLOAD en la barra de abajo de todo, tiene un ícono de una flechita hacia la derecha. La primera vez tarda 5 minutos en bajar todo.
9- En caso de que al tratar de subir el programa falle por no encontrar el Bodoque:
<img width="625" height="240" alt="image" src="https://github.com/user-attachments/assets/ee1a3e92-dfbf-42e3-ac25-8a7d5970ec25" />
Hay que instalar los drivers USB del RP2040, bajando Zadig y con el Bodoque enchufado por USB, apretar INSTALL DRIVER: https://zadig.akeo.ie/


