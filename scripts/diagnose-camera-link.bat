@echo off
setlocal
net session >nul 2>&1
if not "%errorlevel%"=="0" (
  powershell.exe -NoProfile -Command "Start-Process -FilePath '%~f0' -Verb RunAs"
  exit /b
)
cd /d "%~dp0.."
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0recover-camera-link.ps1" -ProbeAravis -ForceKnownIp
set "rc=%ERRORLEVEL%"
echo.
if not "%rc%"=="0" echo Camera diagnostic failed with code %rc%.
pause
exit /b %rc%
