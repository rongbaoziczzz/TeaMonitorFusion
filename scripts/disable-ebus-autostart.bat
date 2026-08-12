@echo off
setlocal
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0disable-ebus-autostart.ps1"
if errorlevel 1 (
    echo.
    echo 修复失败。请确认此批处理文件是以管理员身份运行的。
    pause
    exit /b 1
)
echo.
pause
