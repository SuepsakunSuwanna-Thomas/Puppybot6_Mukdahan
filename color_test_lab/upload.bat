@echo off
chcp 65001 >nul
cd /d "%~dp0"

where python >nul 2>nul
if %ERRORLEVEL% EQU 0 (
    python upload.py
) else (
    title PuppyBot6 - Color Lab Uploader
    echo ========================================================
    echo   🚀 PUPPYBOT6 - STANDALONE COLOR LAB UPLOADER
    echo ========================================================
    echo กำลังคอมไพล์และอัปโหลด...
    echo.
    pio run -t upload
    pause
)
