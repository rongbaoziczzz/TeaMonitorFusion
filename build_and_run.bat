@echo off
setlocal
set "CMAKE_EXTRA_ARGS="
if defined QT_ROOT set "CMAKE_EXTRA_ARGS=-DCMAKE_PREFIX_PATH=%QT_ROOT%"
cmake -S . -B build -G "MinGW Makefiles" %CMAKE_EXTRA_ARGS%
if errorlevel 1 exit /b 1
cmake --build build
if errorlevel 1 exit /b 1
echo 构建完成。请直接双击 build\TeaMonitorFusion.exe 运行正式界面。
