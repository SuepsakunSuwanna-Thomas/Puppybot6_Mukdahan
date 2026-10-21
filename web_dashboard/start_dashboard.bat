@echo off
title PuppyBot6 Web Serial Dashboard
echo ========================================================
echo   Launching PuppyBot6 Web Serial Mission Control Lab...
echo ========================================================
echo.

:: Check if Python is installed
python --version >nul 2>&1
if %ERRORLEVEL% EQU 0 (
    echo Starting local server with Python...
    python "%~dp0server.py"
) else (
    echo Python not detected. Opening index.html directly in browser...
    start "" "%~dp0index.html"
)
pause
