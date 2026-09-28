Un ejemplo de controlador para la plataforma Bodoque con posibilidad de pantalla OLED

botones 1,2,3 y 4 mandan notas midi: 40,41,42 y 43.
Los potenciometros alfa,omega y pi mandan CC, de la siguiente manera. Si apretas boton A (y así comienza el programa por default) mandan CC 0,1,2.
Si apretas boton B va a otra página y entonces los mismos potenciometros mandan CC 3,4 y 5. 
Si apretas boton C, esos potenciometros mandan CC 6,7 y 8 respectivamente.
El led x1 se prende cuando esta en la pagina A, led x2 se prende cuando está en pagina B y led x3 se prende cuando está en pagina C.
Cuando hay un cambio de CC se prende por un instante el led Estrella.