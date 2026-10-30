@echo off
title PuppyBot6 - Build and Upload
cd /d "%~dp0"

echo ============================================================
echo    PuppyBot6 - Automated Build and Upload
echo ============================================================
echo.

set PIO_EXE=%USERPROFILE%\.platformio\penv\Scripts\platformio.exe
if exist "%PIO_EXE%" goto :FOUND_PIO

set PIO_EXE=%USERPROFILE%\.platformio\penv\Scripts\pio.exe
if exist "%PIO_EXE%" goto :FOUND_PIO

where platformio >nul 2>&1
if %errorlevel% equ 0 (
    set PIO_EXE=platformio
    goto :FOUND_PIO
)

where pio >nul 2>&1
if %errorlevel% equ 0 (
    set PIO_EXE=pio
    goto :FOUND_PIO
)

echo [ERROR] PlatformIO is not found in the system!
echo Please make sure PlatformIO is installed.
echo.
pause
exit /b 1

:FOUND_PIO
echo [INFO] Found PlatformIO: "%PIO_EXE%"
echo [INFO] Compiling and Uploading to PuppyBot6 (Metro RP2350)...
echo.

"%PIO_EXE%" run -e adafruit_metro_rp2350 -j 4 --target upload

if %errorlevel% equ 0 (
    echo.
    echo ============================================================
    echo  [SUCCESS] Code uploaded to PuppyBot6 successfully!
    echo ============================================================
) else (
    echo.
    echo ============================================================
    echo  [ERROR] Upload failed! (Exit code: %errorlevel%)
    echo  Tip: Hold BOOTSEL button and re-plug USB cable if stuck.
    echo ============================================================
)

echo.
pause




