@echo off
setlocal EnableExtensions EnableDelayedExpansion
title Bodoque - Cargador RP2040

rem Trabajar siempre desde la carpeta donde esta este BAT.
cd /d "%~dp0"
if errorlevel 1 goto :errorFolder

cls
echo.
echo ==============================================
echo          BODOQUE - CARGADOR RP2040
echo ==============================================
echo.

rem Listar los archivos UF2 locales, ordenados alfabeticamente.
set "COUNT=0"
for /f "delims=" %%F in ('dir /b /a-d /o:n "*.uf2" 2^>nul') do (
    set /a COUNT+=1
    set "FILE!COUNT!=%%~fF"
    echo   !COUNT!. %%F
)

if !COUNT! EQU 0 goto :noUf2
echo.

:chooseFirmware
set "CHOICE="
set "SELECTED="
set /p "CHOICE=Elegi el numero del programa que queres cargar: "
for /l %%N in (1,1,!COUNT!) do if "!CHOICE!"=="%%N" set "SELECTED=!FILE%%N!"

if not defined SELECTED (
    echo.
    echo Opcion invalida. Elegi un numero de la lista.
    echo.
    goto :chooseFirmware
)

for %%F in ("!SELECTED!") do set "SELECTED_NAME=%%~nxF"
echo.
echo Seleccionado: !SELECTED_NAME!
echo.

rem Si la placa ya esta en BOOTSEL, no hace falta tocar el puerto serie.
call :detectBootDrive
if defined BOOTDRIVE goto :copyFirmware

rem Detectar puertos COM y obtener el nombre descriptivo del dispositivo.
rem COM1 se excluye siempre: normalmente es un puerto interno/no util para esta placa.
set "PORTCOUNT=0"
set "PORTTEMP=%TEMP%\bodoque_ports_%RANDOM%.tmp"

powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "[Console]::OutputEncoding = [System.Text.Encoding]::ASCII; Get-CimInstance -ClassName Win32_PnPEntity | Where-Object { $_.Name -match '\(COM[0-9]+\)' } | ForEach-Object { if ($_.Name -match '\((COM[0-9]+)\)') { $port = $matches[1]; if ([int]$port.Substring(3) -gt 1) { $name = $_.Name; if ($_.DeviceID -match 'VID_2E8A') { $name = '[Raspberry Pi USB] ' + $name }; $name = $name -replace '[^\x20-\x7E]', '?'; Write-Output ($port + '|' + $name) } } }" > "!PORTTEMP!" 2>nul

if exist "!PORTTEMP!" (
    for /f "usebackq tokens=1,* delims=|" %%A in ("!PORTTEMP!") do (
        set /a PORTCOUNT+=1
        set "PORT!PORTCOUNT!=%%A"
        set "PORTNAME!PORTCOUNT!=%%B"
    )
    del "!PORTTEMP!" >nul 2>&1
)

if !PORTCOUNT! EQU 0 goto :noComPorts

rem Con un solo puerto disponible, seleccionarlo directamente sin preguntar.
if !PORTCOUNT! EQU 1 (
    set "SELECTED_COM=!PORT1!"
    set "SELECTED_DEVICE=!PORTNAME1!"
    echo Puerto serie detectado: !SELECTED_COM! - !SELECTED_DEVICE!
    echo Se utilizara automaticamente porque es el unico puerto COM disponible.
    goto :portSelected
)

rem Si hay varios puertos, intentar reconocer automaticamente una Pico/RP2040
rem por el nombre descriptivo que informa Windows.
set "MATCHCOUNT=0"
for /l %%N in (1,1,!PORTCOUNT!) do call :checkKnownPort %%N

if !MATCHCOUNT! EQU 1 (
    set "SELECTED_COM=!MATCHED_COM!"
    set "SELECTED_DEVICE=!MATCHED_DEVICE!"
    echo Dispositivo RP2040 identificado automaticamente:
    echo   !SELECTED_COM! - !SELECTED_DEVICE!
    goto :portSelected
)

:showPortMenu
echo Puertos serie detectados (COM1 omitido):
echo.
for /l %%N in (1,1,!PORTCOUNT!) do echo   %%N. !PORT%%N! - !PORTNAME%%N!
echo.
echo Selecciona el puerto COM que corresponde a la RP2040.
echo Si Windows no identifica el modelo, comproba el dispositivo en Administrador de dispositivos.
echo.

:choosePort
set "PORTCHOICE="
set "SELECTED_COM="
set "SELECTED_DEVICE="
set /p "PORTCHOICE=Numero del puerto (o Q para salir): "
if /i "!PORTCHOICE!"=="Q" goto :cancelled

for /l %%N in (1,1,!PORTCOUNT!) do if "!PORTCHOICE!"=="%%N" (
    set "SELECTED_COM=!PORT%%N!"
    set "SELECTED_DEVICE=!PORTNAME%%N!"
)

if not defined SELECTED_COM (
    echo Puerto invalido. Proba nuevamente.
    echo.
    goto :choosePort
)

:portSelected
echo.
echo Puerto seleccionado: !SELECTED_COM! - !SELECTED_DEVICE!
echo.
echo Si tenes abierto el programa Arduino o VSCode, cerra el Monitor Serie de Arduino/VSCode antes de continuar.
echo.
pause

echo.
echo Enviando el pulso USB CDC a 1200 baud...
echo Comando: mode !SELECTED_COM!: BAUD=1200 PARITY=N DATA=8 STOP=1
echo.
mode !SELECTED_COM!: BAUD=1200 PARITY=N DATA=8 STOP=1
if errorlevel 1 goto :modeFailed

echo.
echo Esperando que la RP2040 entre en modo BOOTSEL...
call :waitBootDrive
if defined BOOTDRIVE goto :copyFirmware

echo.
echo No aparecio la unidad BOOTSEL.
echo Esto puede indicar que el firmware no implementa el reinicio CDC a 1200 baud,
echo o que se selecciono otro puerto COM.
echo.
choice /c RMN /n /m "R = elegir otro COM, M = entrar manualmente en BOOTSEL, N = salir: "
if errorlevel 3 goto :finishedWithError
if errorlevel 2 goto :manualBoot
if errorlevel 1 goto :showPortMenu

:manualBoot
echo.
echo 1. Desconecta la placa del USB.
echo 2. Manten apretado BOOT o BOOTSEL.
echo 3. Conecta el USB sin soltar el boton.
echo 4. Solta BOOT cuando aparezca la unidad RPI-RP2.
echo.
pause
call :waitBootDrive
if not defined BOOTDRIVE goto :driveNotFound
goto :copyFirmware

:copyFirmware
echo.
echo Unidad BOOTSEL detectada: %BOOTDRIVE%\
echo Copiando el firmware...
echo.
copy /B /Y "!SELECTED!" "%BOOTDRIVE%\"
if errorlevel 1 goto :copyFailed

echo.
echo Firmware enviado correctamente.
echo La RP2040 deberia reiniciarse automaticamente y ejecutar el nuevo programa.
echo.
pause
exit /b 0

:detectBootDrive
set "BOOTDRIVE="
for %%D in (D E F G H I J K L M N O P Q R S T U V W X Y Z) do (
    if exist "%%D:\INFO_UF2.TXT" if not defined BOOTDRIVE set "BOOTDRIVE=%%D:"
)
exit /b 0

:waitBootDrive
set "BOOTDRIVE="
for /l %%T in (1,1,30) do (
    if not defined BOOTDRIVE (
        call :detectBootDrive
        if not defined BOOTDRIVE timeout /t 1 /nobreak >nul
    )
)
exit /b 0

:checkKnownPort
set "CHECK_DEVICE=!PORTNAME%1!"
echo(!CHECK_DEVICE!| findstr /i /c:"Raspberry Pi Pico" /c:"Raspberry Pi USB" /c:"RP2040" /c:"Bodoque" /c:"Pico" >nul
if not errorlevel 1 (
    set /a MATCHCOUNT+=1
    set "MATCHED_COM=!PORT%1!"
    set "MATCHED_DEVICE=!PORTNAME%1!"
)
exit /b 0

:modeFailed
echo.
echo ERROR: Windows no pudo configurar !SELECTED_COM! a 1200 baud.
echo Verifica que el puerto sea el de la RP2040 y que no este abierto por otro programa.
goto :finishedWithError

:noComPorts
echo No se detectaron puertos COM.
echo.
echo El reinicio a 1200 baud requiere que el firmware exponga un puerto USB CDC.
echo Si la placa esta en BOOTSEL, reconectala y ejecuta el cargador nuevamente.
echo Tambien podes entrar manualmente con el boton BOOTSEL.
echo.
choice /c SM /n /m "S = entrar manualmente en BOOTSEL, M = salir: "
if errorlevel 2 goto :finishedWithError
goto :manualBoot

:missingPicotool
rem Etiqueta conservada por compatibilidad con versiones anteriores.
goto :finishedWithError

:noUf2
echo No se encontraron archivos .uf2 en esta carpeta:
echo %~dp0
goto :finishedWithError

:errorFolder
echo ERROR: No se pudo acceder a la carpeta del cargador.
goto :finishedWithError

:driveNotFound
echo.
echo ERROR: No se detecto la unidad BOOTSEL (RPI-RP2).
echo No se copio ningun archivo.
goto :finishedWithError

:copyFailed
echo.
echo ERROR: Windows no pudo copiar el UF2 a %BOOTDRIVE%\
echo Comprueba que la unidad BOOTSEL siga conectada.
goto :finishedWithError

:cancelled
echo.
echo Operacion cancelada.
pause
exit /b 0

:finishedWithError
echo.
pause
exit /b 1
