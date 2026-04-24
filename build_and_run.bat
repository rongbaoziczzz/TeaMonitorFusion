@echo off
setlocal
cmake -S . -B build -G "MinGW Makefiles"
if errorlevel 1 exit /b 1
cmake --build build
if errorlevel 1 exit /b 1
echo 构建完成。请直接双击 build\TeaMonitorFusion.exe 运行正式界面。
