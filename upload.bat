@echo off
title PuppyBot6 - Build and Upload
echo ========================================================
echo   PuppyBot6 - Compiling and Uploading via PlatformIO...
echo ========================================================
pio run --target upload
if %ERRORLEVEL% NEQ 0 (
    echo.
    echo [ERROR] Upload failed! 
    echo - Make sure board is connected via USB.
    echo - If necessary, hold BOOTSEL button while plugging in USB.
    echo.
) else (
    echo.
    echo [SUCCESS] Upload completed successfully!
    echo.
)
pause
