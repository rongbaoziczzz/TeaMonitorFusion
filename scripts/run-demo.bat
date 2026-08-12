@echo off
setlocal
cd /d "%~dp0.."
if not exist "build\TeaMonitorFusion.exe" (
  echo Build the project first with build_and_run.bat.
  pause
  exit /b 1
)
start "TeaMonitorFusion demo" "build\TeaMonitorFusion.exe" --demo
exit /b 0
