@echo off
setlocal enabledelayedexpansion

echo ===================================================
echo  IKEA Bekant Firmware - Lokaler CLI-Build (ohne MPLAB X)
echo ===================================================

where xc8-cc >nul 2>nul
if %errorlevel% neq 0 (
    echo [!] 'xc8-cc' wurde nicht im PATH gefunden.
    echo [*] Bitte installiere den XC8 Compiler oder nutze den automatischen
    echo     GitHub Actions Build auf GitHub (unter dem Reiter 'Actions').
    pause
    exit /b 1
)

echo [*] Starte Kompilierung mit xc8-cc...
cd "%~dp0\..\firmware"
make
if %errorlevel% equ 0 (
    echo.
    echo [+] Erfolgreich gebaut! HEX-Datei:
    echo     firmware\dist\bekantfirmware.hex
) else (
    echo.
    echo [!] Build fehlgeschlagen.
)
pause
