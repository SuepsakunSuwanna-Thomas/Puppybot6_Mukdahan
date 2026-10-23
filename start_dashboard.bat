@echo off
title PuppyBot6 Mission Control & Sequence Studio
echo ========================================================
echo   Launching PuppyBot6 React + Tailwind + shadcn/ui Lab
echo ========================================================
echo.

:: Check if Node is available to run dev server
where npm >nul 2>&1
if %ERRORLEVEL% EQU 0 (
    echo [1] Starting Vite Hot-Reload Dev Server (Recommended for development)...
    cd "%~dp0dashboard"
    start "" http://localhost:5173
    npm run dev
    goto :end
)

:: Fallback to Python server serving dist/
where python >nul 2>&1
if %ERRORLEVEL% EQU 0 (
    echo [2] Node not found. Starting local Python server...
    python "%~dp0server.py"
    goto :end
)

echo Opening dist/index.html directly...
start "" "%~dp0dist\index.html"

:end
pause
