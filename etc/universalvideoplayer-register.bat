@echo off
setlocal

"%~dp0/universalvideoplayer" --register
if %errorlevel% neq 0 (
    echo Registration failed. Make sure universalvideoplayer is in the same folder as this script.
    pause
    exit /b %errorlevel%
)

pause
endlocal
