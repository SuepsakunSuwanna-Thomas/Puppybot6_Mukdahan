@echo off
title PuppyBot6 - Build and Upload
cd /d "%~dp0"
if exist "%USERPROFILE%\.platformio\penv\Scripts\python.exe" (
    "%USERPROFILE%\.platformio\penv\Scripts\python.exe" "%~dp0upload.py"
) else (
    python "%~dp0upload.py"
)

