cmake_minimum_required(VERSION 3.16)

project(TeaMonitorFusion VERSION 1.1 LANGUAGES CXX)

include(CTest)
include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

if(MSVC)
    add_compile_options(/utf-8)
endif()

find_package(Qt6 REQUIRED COMPONENTS Widgets Gui Sql)

set(PROJECT_SOURCES
    src/main.cpp
    src/launcherwindow.h
    src/launcherwindow.cpp
    src/mainwindow.h
    src/mainwindow.cpp
    src/resources.qrc
    src/models/devicedefinitions.h
    src/models/spectrumcalibration.h
    src/models/spectrumcalibration.cpp
    src/services/abstractcameraservice.h
    src/services/abstractspectrometerservice.h
    src/services/mockcameraservice.h
    src/services/mockcameraservice.cpp
    src/services/mockspectrometerservice.h
    src/services/mockspectrometerservice.cpp
    src/services/cameraframeconverter.h
    src/services/cameraframeconverter.cpp
    src/services/araviscameraservice.h
    src/services/araviscameraservice.cpp
    src/services/oceandirectspectrometerservice.h
    src/services/oceandirectspectrometerservice.cpp
    src/services/seaspectrometerservice.h
    src/services/seaspectrometerservice.cpp
    src/services/deviceworker.h
    src/services/deviceworker.cpp
    src/services/filelogger.h
    src/services/filelogger.cpp
    src/services/auditstore.h
    src/services/auditstore.cpp
    src/widgets/spectrumchartwidget.h
    src/widgets/spectrumchartwidget.cpp
)

add_executable(TeaMonitorFusion ${PROJECT_SOURCES})

target_link_libraries(TeaMonitorFusion PRIVATE Qt6::Widgets Qt6::Gui Qt6::Sql)
target_compile_definitions(TeaMonitorFusion PRIVATE TEA_MONITOR_VERSION="${PROJECT_VERSION}")

if(MSVC)
    target_compile_options(TeaMonitorFusion PRIVATE /W4 /permissive-)
else()
    target_compile_options(TeaMonitorFusion PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(TARGET Qt6::QSQLiteDriverPlugin)
    add_custom_command(TARGET TeaMonitorFusion POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:TeaMonitorFusion>/sqldrivers"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:Qt6::QSQLiteDriverPlugin>"
            "$<TARGET_FILE_DIR:TeaMonitorFusion>/sqldrivers/"
    )
endif()

target_include_directories(TeaMonitorFusion PRIVATE
    src
    src/models
    src/services
    src/widgets
)

set(_tea_monitor_device_support_dir "${CMAKE_CURRENT_SOURCE_DIR}/third_party/device_support")
set(_tea_monitor_bundled_seasdk_root "${_tea_monitor_device_support_dir}/SeaSDK")
set(_tea_monitor_bundled_oceandirect_root "${_tea_monitor_device_support_dir}/OceanDirect/sdk")
if(EXISTS "${_tea_monitor_bundled_oceandirect_root}/OceanDirect SDK/include/api/OceanDirectAPI.h")
    set(_tea_monitor_bundled_oceandirect_root "${_tea_monitor_bundled_oceandirect_root}/OceanDirect SDK")
endif()
set(_tea_monitor_bundled_aravis_root "${_tea_monitor_device_support_dir}/Aravis/runtime")

set(TEA_MONITOR_SEASDK_ROOT "" CACHE PATH "SeaSDK 根目录")
set(TEA_MONITOR_OCEANDIRECT_ROOT "" CACHE PATH "OceanDirect SDK 根目录")
set(TEA_MONITOR_ARAVIS_ROOT "" CACHE PATH "Aravis/GLib 开发包根目录")
set(TEA_MONITOR_ARAVIS_RUNTIME_DIR "" CACHE PATH "Aravis/GLib 运行 DLL 目录（可选）")
option(TEA_MONITOR_REQUIRE_SEASDK "SeaSDK 缺失时终止配置" OFF)
option(TEA_MONITOR_REQUIRE_OCEANDIRECT "OceanDirect SDK 缺失时终止配置" OFF)
option(TEA_MONITOR_REQUIRE_ARAVIS "Aravis 缺失时终止配置" OFF)

if(NOT TEA_MONITOR_SEASDK_ROOT)
    if(EXISTS "${_tea_monitor_bundled_seasdk_root}/include/SeaSDKWrapper.h")
        set(TEA_MONITOR_SEASDK_ROOT "${_tea_monitor_bundled_seasdk_root}" CACHE PATH "SeaSDK 根目录" FORCE)
    elseif(DEFINED ENV{SEASDK_ROOT} AND NOT "$ENV{SEASDK_ROOT}" STREQUAL "")
        set(TEA_MONITOR_SEASDK_ROOT "$ENV{SEASDK_ROOT}" CACHE PATH "SeaSDK 根目录" FORCE)
    endif()
endif()

if(NOT TEA_MONITOR_OCEANDIRECT_ROOT AND
   EXISTS "${_tea_monitor_bundled_oceandirect_root}/include/api/OceanDirectAPI.h")
    set(TEA_MONITOR_OCEANDIRECT_ROOT "${_tea_monitor_bundled_oceandirect_root}" CACHE PATH "OceanDirect SDK 根目录" FORCE)
elseif(NOT TEA_MONITOR_OCEANDIRECT_ROOT AND DEFINED ENV{OCEANDIRECT_ROOT} AND
       NOT "$ENV{OCEANDIRECT_ROOT}" STREQUAL "")
    set(TEA_MONITOR_OCEANDIRECT_ROOT "$ENV{OCEANDIRECT_ROOT}" CACHE PATH "OceanDirect SDK 根目录" FORCE)
endif()

if(NOT TEA_MONITOR_ARAVIS_ROOT AND
   EXISTS "${_tea_monitor_bundled_aravis_root}/include/aravis-0.8/arv.h")
    set(TEA_MONITOR_ARAVIS_ROOT "${_tea_monitor_bundled_aravis_root}" CACHE PATH
        "Aravis/GLib 开发包根目录" FORCE)
elseif(NOT TEA_MONITOR_ARAVIS_ROOT AND DEFINED ENV{ARAVIS_ROOT} AND
       NOT "$ENV{ARAVIS_ROOT}" STREQUAL "")
    set(TEA_MONITOR_ARAVIS_ROOT "$ENV{ARAVIS_ROOT}" CACHE PATH
        "Aravis/GLib 开发包根目录" FORCE)
endif()

set(SEASDK_INCLUDE_DIR "${TEA_MONITOR_SEASDK_ROOT}/include")
set(OCEANDIRECT_ROOT "${TEA_MONITOR_OCEANDIRECT_ROOT}")
set(TEA_MONITOR_SEASDK_ENABLED OFF)
set(TEA_MONITOR_OCEANDIRECT_ENABLED OFF)
set(TEA_MONITOR_ARAVIS_ENABLED OFF)

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(SEASDK_LIB_DIR "${TEA_MONITOR_SEASDK_ROOT}/bin/windows 64bit")
else()
    set(SEASDK_LIB_DIR "${TEA_MONITOR_SEASDK_ROOT}/bin/windows 32bit")
endif()

if(TEA_MONITOR_SEASDK_ROOT AND EXISTS "${SEASDK_INCLUDE_DIR}/SeaSDKWrapper.h" AND EXISTS "${SEASDK_LIB_DIR}/SeaSDK.lib")
    set(TEA_MONITOR_SEASDK_ENABLED ON)
    target_compile_definitions(TeaMonitorFusion PRIVATE TEA_MONITOR_HAS_SEASDK=1)
    target_include_directories(TeaMonitorFusion PRIVATE "${SEASDK_INCLUDE_DIR}")
    target_link_directories(TeaMonitorFusion PRIVATE "${SEASDK_LIB_DIR}")
    target_link_libraries(TeaMonitorFusion PRIVATE SeaSDK)

    add_custom_command(TARGET TeaMonitorFusion POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${SEASDK_LIB_DIR}/SeaSDK.dll"
        "$<TARGET_FILE_DIR:TeaMonitorFusion>/SeaSDK.dll"
    )
endif()

if(TEA_MONITOR_REQUIRE_SEASDK AND NOT TEA_MONITOR_SEASDK_ENABLED)
    message(FATAL_ERROR "要求启用 SeaSDK，但未找到有效头文件和库")
endif()

if(WIN32 AND EXISTS "${OCEANDIRECT_ROOT}/include/api/OceanDirectAPI.h" AND
   EXISTS "${OCEANDIRECT_ROOT}/lib/OceanDirect.dll")
    set(TEA_MONITOR_OCEANDIRECT_ENABLED ON)
    target_compile_definitions(TeaMonitorFusion PRIVATE TEA_MONITOR_HAS_OCEANDIRECT=1)

    foreach(_oceandirect_dll OceanDirect.dll NetOceanDirect.dll)
        if(EXISTS "${OCEANDIRECT_ROOT}/lib/${_oceandirect_dll}")
            add_custom_command(TARGET TeaMonitorFusion POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${OCEANDIRECT_ROOT}/lib/${_oceandirect_dll}"
                "$<TARGET_FILE_DIR:TeaMonitorFusion>/${_oceandirect_dll}"
            )
        endif()
    endforeach()
endif()

if(TEA_MONITOR_REQUIRE_OCEANDIRECT AND NOT TEA_MONITOR_OCEANDIRECT_ENABLED)
    message(FATAL_ERROR "要求启用 OceanDirect SDK，但未找到有效头文件和 OceanDirect.dll")
endif()

find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(ARAVIS QUIET IMPORTED_TARGET aravis-0.8)
endif()

if(TARGET PkgConfig::ARAVIS)
    add_library(TeaMonitorAravis INTERFACE)
    target_link_libraries(TeaMonitorAravis INTERFACE PkgConfig::ARAVIS)
    set(TEA_MONITOR_ARAVIS_ENABLED ON)
elseif(TEA_MONITOR_ARAVIS_ROOT)
    find_path(ARAVIS_INCLUDE_DIR arv.h
        PATHS "${TEA_MONITOR_ARAVIS_ROOT}/include/aravis-0.8" NO_DEFAULT_PATH)
    find_path(ARAVIS_GLIB_INCLUDE_DIR glib.h
        PATHS "${TEA_MONITOR_ARAVIS_ROOT}/include/glib-2.0" NO_DEFAULT_PATH)
    find_path(ARAVIS_GLIB_CONFIG_INCLUDE_DIR glibconfig.h
        PATHS "${TEA_MONITOR_ARAVIS_ROOT}/lib/glib-2.0/include" NO_DEFAULT_PATH)
    find_library(ARAVIS_LIBRARY NAMES aravis-0.8 libaravis-0.8
        PATHS "${TEA_MONITOR_ARAVIS_ROOT}/lib" NO_DEFAULT_PATH)
    find_library(ARAVIS_GOBJECT_LIBRARY NAMES gobject-2.0 libgobject-2.0
        PATHS "${TEA_MONITOR_ARAVIS_ROOT}/lib" NO_DEFAULT_PATH)
    find_library(ARAVIS_GIO_LIBRARY NAMES gio-2.0 libgio-2.0
        PATHS "${TEA_MONITOR_ARAVIS_ROOT}/lib" NO_DEFAULT_PATH)
    find_library(ARAVIS_GLIB_LIBRARY NAMES glib-2.0 libglib-2.0
        PATHS "${TEA_MONITOR_ARAVIS_ROOT}/lib" NO_DEFAULT_PATH)

    if(ARAVIS_INCLUDE_DIR AND ARAVIS_GLIB_INCLUDE_DIR AND ARAVIS_GLIB_CONFIG_INCLUDE_DIR AND
       ARAVIS_LIBRARY AND ARAVIS_GOBJECT_LIBRARY AND ARAVIS_GIO_LIBRARY AND ARAVIS_GLIB_LIBRARY)
        add_library(TeaMonitorAravis INTERFACE)
        target_include_directories(TeaMonitorAravis INTERFACE
            "${ARAVIS_INCLUDE_DIR}"
            "${ARAVIS_GLIB_INCLUDE_DIR}"
            "${ARAVIS_GLIB_CONFIG_INCLUDE_DIR}")
        target_link_libraries(TeaMonitorAravis INTERFACE
            "${ARAVIS_LIBRARY}"
            "${ARAVIS_GOBJECT_LIBRARY}"
            "${ARAVIS_GIO_LIBRARY}"
            "${ARAVIS_GLIB_LIBRARY}")
        set(TEA_MONITOR_ARAVIS_ENABLED ON)
    endif()
endif()

if(TEA_MONITOR_ARAVIS_ENABLED)
    target_compile_definitions(TeaMonitorFusion PRIVATE TEA_MONITOR_HAS_ARAVIS=1)
    target_link_libraries(TeaMonitorFusion PRIVATE TeaMonitorAravis)

    if(NOT TEA_MONITOR_ARAVIS_RUNTIME_DIR AND EXISTS "${TEA_MONITOR_ARAVIS_ROOT}/bin")
        set(TEA_MONITOR_ARAVIS_RUNTIME_DIR "${TEA_MONITOR_ARAVIS_ROOT}/bin" CACHE PATH
            "Aravis/GLib 运行 DLL 目录（可选）" FORCE)
    endif()
    if(WIN32 AND TEA_MONITOR_ARAVIS_RUNTIME_DIR)
        file(GLOB _aravis_runtime_dlls CONFIGURE_DEPENDS
            "${TEA_MONITOR_ARAVIS_RUNTIME_DIR}/*.dll")
        foreach(_aravis_runtime_dll IN LISTS _aravis_runtime_dlls)
            add_custom_command(TARGET TeaMonitorFusion POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${_aravis_runtime_dll}"
                "$<TARGET_FILE_DIR:TeaMonitorFusion>/")
            install(FILES "${_aravis_runtime_dll}" DESTINATION ${CMAKE_INSTALL_BINDIR})
        endforeach()
    elseif(WIN32)
        message(WARNING "已找到 Aravis 开发库，但未配置运行 DLL；请设置 TEA_MONITOR_ARAVIS_RUNTIME_DIR 或 PATH")
    endif()
endif()

if(TEA_MONITOR_REQUIRE_ARAVIS AND NOT TEA_MONITOR_ARAVIS_ENABLED)
    message(FATAL_ERROR "要求启用 Aravis，但未找到 aravis-0.8、GLib 头文件和库")
endif()

message(STATUS "TeaMonitorFusion 功能清单:")
message(STATUS "  SeaSDK:       ${TEA_MONITOR_SEASDK_ENABLED}")
message(STATUS "  OceanDirect:  ${TEA_MONITOR_OCEANDIRECT_ENABLED}")
message(STATUS "  Aravis:       ${TEA_MONITOR_ARAVIS_ENABLED}")
message(STATUS "  Simulators:   ON")

install(TARGETS TeaMonitorFusion RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
if(TEA_MONITOR_SEASDK_ENABLED)
    install(FILES "${SEASDK_LIB_DIR}/SeaSDK.dll" DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()
if(TEA_MONITOR_OCEANDIRECT_ENABLED)
    install(FILES "${OCEANDIRECT_ROOT}/lib/OceanDirect.dll" DESTINATION ${CMAKE_INSTALL_BINDIR})
    if(EXISTS "${OCEANDIRECT_ROOT}/lib/NetOceanDirect.dll")
        install(FILES "${OCEANDIRECT_ROOT}/lib/NetOceanDirect.dll" DESTINATION ${CMAKE_INSTALL_BINDIR})
    endif()
endif()

if(WIN32 AND COMMAND qt_generate_deploy_app_script)
    qt_generate_deploy_app_script(
        TARGET TeaMonitorFusion
        OUTPUT_SCRIPT tea_monitor_deploy_script
        NO_UNSUPPORTED_PLATFORM_ERROR
    )
    install(SCRIPT ${tea_monitor_deploy_script})
endif()

if(BUILD_TESTING)
    add_executable(TeaMonitorFusionCoreTests
        tests/core_tests.cpp
        src/models/spectrumcalibration.h
        src/models/spectrumcalibration.cpp
        src/services/mockcameraservice.h
        src/services/mockcameraservice.cpp
        src/services/mockspectrometerservice.h
        src/services/mockspectrometerservice.cpp
        src/services/cameraframeconverter.h
        src/services/cameraframeconverter.cpp
        src/services/araviscameraservice.h
        src/services/araviscameraservice.cpp
        src/services/oceandirectspectrometerservice.h
        src/services/oceandirectspectrometerservice.cpp
        src/services/seaspectrometerservice.h
        src/services/seaspectrometerservice.cpp
        src/services/deviceworker.h
        src/services/deviceworker.cpp
        src/services/auditstore.h
        src/services/auditstore.cpp
    )
    target_include_directories(TeaMonitorFusionCoreTests PRIVATE src src/models src/services)
    target_link_libraries(TeaMonitorFusionCoreTests PRIVATE Qt6::Core Qt6::Gui Qt6::Sql)
    if(TEA_MONITOR_ARAVIS_ENABLED)
        target_compile_definitions(TeaMonitorFusionCoreTests PRIVATE TEA_MONITOR_HAS_ARAVIS=1)
        target_link_libraries(TeaMonitorFusionCoreTests PRIVATE TeaMonitorAravis)
    endif()
    if(MSVC)
        target_compile_options(TeaMonitorFusionCoreTests PRIVATE /W4 /permissive-)
    else()
        target_compile_options(TeaMonitorFusionCoreTests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    if(TARGET Qt6::QSQLiteDriverPlugin)
        add_custom_command(TARGET TeaMonitorFusionCoreTests POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_FILE_DIR:TeaMonitorFusionCoreTests>/sqldrivers"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE:Qt6::QSQLiteDriverPlugin>"
                "$<TARGET_FILE_DIR:TeaMonitorFusionCoreTests>/sqldrivers/"
        )
    endif()
    add_test(NAME core_calibration_and_simulators COMMAND TeaMonitorFusionCoreTests)
endif()
