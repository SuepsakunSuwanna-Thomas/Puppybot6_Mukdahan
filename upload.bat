@echo off
title PuppyBot6 - Build and Upload
cd /d "%~dp0"

set "PIO_EXE=%USERPROFILE%\.platformio\penv\Scripts\platformio.exe"
if exist "%PIO_EXE%" (
    "%PIO_EXE%" run --target upload
) else (
    python "%~dp0upload.py"
)
pause
