@echo off
setlocal EnableExtensions EnableDelayedExpansion

cd /d "%~dp0.."
set "marker=%~dp0..\diagnostics\camera-recovery-admin.exit"
del /q "%marker%" >nul 2>&1

net session >nul 2>&1
if "%errorlevel%"=="0" goto elevated

echo Starting administrator camera link recovery...
start "Camera link recovery" "%~dp0recover-camera-link.bat"
for /l %%i in (1,1,120) do (
  if exist "%marker%" goto recovery_done
  timeout /t 1 /nobreak >nul
)
echo Camera link recovery timed out. Check the administrator prompt and log:
echo %~dp0..\diagnostics\camera-recovery-admin.log
pause
exit /b 1

:recovery_done
set /p rc=<"%marker%"
if not "!rc!"=="0" (
  echo Camera link recovery failed with code !rc!.
  if exist "%~dp0..\diagnostics\camera-recovery-admin.log" type "%~dp0..\diagnostics\camera-recovery-admin.log"
  pause
  exit /b !rc!
)
echo Camera link is ready. Starting TeaMonitorFusion live camera mode...
start "TeaMonitorFusion" "%~dp0..\build\TeaMonitorFusion.exe" --camera-live
exit /b 0

:elevated
echo This script is running elevated. For a normal user-level application, run it by double-clicking instead.
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0recover-camera-link-elevated.ps1"
set "rc=%ERRORLEVEL%"
if not "%rc%"=="0" (
  echo Camera link recovery failed with code %rc%.
  if exist "%~dp0..\diagnostics\camera-recovery-admin.log" type "%~dp0..\diagnostics\camera-recovery-admin.log"
  pause
  exit /b %rc%
)
echo Camera link is ready. Starting TeaMonitorFusion live camera mode...
start "TeaMonitorFusion" "%~dp0..\build\TeaMonitorFusion.exe" --camera-live
exit /b 0
