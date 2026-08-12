# Aravis GigE Vision support

`AravisCameraService` uses the LGPL Aravis 0.8 C API for GigE Vision/GenICam cameras. It is configured for the photographed `IMPERX GEV-B1621C-TC000` and binds serial number `56S014` by default. No Pleora eBUS SDK is required.

The `aravis-0.8.36/` directory is an official source snapshot from the `0.8.36` tag at [AravisProject/aravis](https://github.com/AravisProject/aravis). Its license is in `COPYING`.

The local `runtime/` prefix contains the official MSYS2 MinGW64 binary package for Aravis 0.8.33 and the runtime dependencies used by this Qt MinGW build. CMake detects this prefix automatically and copies its DLLs beside the application. Package provenance is recorded in `THIRD_PARTY_NOTICES.md`.

The bundled runtime is enabled by default:

```powershell
cmake -S . -B build -G "MinGW Makefiles" -DTEA_MONITOR_REQUIRE_ARAVIS=ON
cmake --build build
```

To use another compatible build, pass a prefix containing `include/aravis-0.8`, `include/glib-2.0`, `lib/glib-2.0/include`, and matching `lib` files:

```powershell
cmake -S . -B build -G "MinGW Makefiles" `
  -DTEA_MONITOR_ARAVIS_ROOT="E:/path/to/aravis-prefix" `
  -DTEA_MONITOR_ARAVIS_RUNTIME_DIR="E:/path/to/aravis-prefix/bin"
```

Runtime DLLs must be next to `TeaMonitorFusion.exe` or on `PATH`. The application refuses to guess when multiple GigE Vision cameras are found and reports the discovered serial numbers in the connection error.
