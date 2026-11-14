@echo off
setlocal

"%~dp0/universalvideoplayer" --unregister
if %errorlevel% neq 0 (
    echo Deregistration failed. Make sure universalvideoplayer is in the same folder as this script.
    pause
    exit /b %errorlevel%
)

pause
endlocal
