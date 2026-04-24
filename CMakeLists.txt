cmake_minimum_required(VERSION 3.16)

project(TeaMonitorFusion VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

if(MSVC)
    add_compile_options(/utf-8)
endif()

set(CMAKE_PREFIX_PATH "E:/Qt/6.10.1/mingw_64")

find_package(Qt6 REQUIRED COMPONENTS Widgets Gui)

set(PROJECT_SOURCES
    src/main.cpp
    src/launcherwindow.h
    src/launcherwindow.cpp
    src/mainwindow.h
    src/mainwindow.cpp
    src/resources.qrc
    src/models/devicedefinitions.h
    src/services/abstractcameraservice.h
    src/services/abstractspectrometerservice.h
    src/services/mockcameraservice.h
    src/services/mockcameraservice.cpp
    src/services/mockspectrometerservice.h
    src/services/mockspectrometerservice.cpp
    src/services/ebuscameraservice.h
    src/services/ebuscameraservice.cpp
    src/services/oceandirectspectrometerservice.h
    src/services/oceandirectspectrometerservice.cpp
    src/services/seaspectrometerservice.h
    src/services/seaspectrometerservice.cpp
    src/widgets/spectrumchartwidget.h
    src/widgets/spectrumchartwidget.cpp
)

add_executable(TeaMonitorFusion ${PROJECT_SOURCES})

target_link_libraries(TeaMonitorFusion PRIVATE Qt6::Widgets Qt6::Gui)

target_include_directories(TeaMonitorFusion PRIVATE
    src
    src/models
    src/services
    src/widgets
)

set(SEASDK_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../Qt_test001/SeaSDK 1.2.0/[6] DLL and SO")
set(SEASDK_INCLUDE_DIR "${SEASDK_ROOT}/include")
set(OCEANDIRECT_ROOT "E:/Program Files/Ocean Optics/OceanDirect SDK")
set(EBUS_ROOT "C:/Program Files/Pleora Technologies Inc/eBUS SDK")

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(SEASDK_LIB_DIR "${SEASDK_ROOT}/bin/windows 64bit")
else()
    set(SEASDK_LIB_DIR "${SEASDK_ROOT}/bin/windows 32bit")
endif()

if(EXISTS "${SEASDK_INCLUDE_DIR}/SeaSDKWrapper.h" AND EXISTS "${SEASDK_LIB_DIR}/SeaSDK.lib")
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

if(NOT MINGW AND EXISTS "${OCEANDIRECT_ROOT}/include/api/OceanDirectAPI.h" AND EXISTS "${OCEANDIRECT_ROOT}/lib/OceanDirect.lib")
    target_compile_definitions(TeaMonitorFusion PRIVATE TEA_MONITOR_HAS_OCEANDIRECT=1)
    target_include_directories(TeaMonitorFusion PRIVATE "${OCEANDIRECT_ROOT}/include")
    target_link_directories(TeaMonitorFusion PRIVATE "${OCEANDIRECT_ROOT}/lib")
    target_link_libraries(TeaMonitorFusion PRIVATE OceanDirect)

    add_custom_command(TARGET TeaMonitorFusion POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${OCEANDIRECT_ROOT}/lib/OceanDirect.dll"
        "$<TARGET_FILE_DIR:TeaMonitorFusion>/OceanDirect.dll"
    )
endif()

if(EXISTS "${EBUS_ROOT}/Includes/PvSystem.h" AND EXISTS "${EBUS_ROOT}/Libraries")
    target_compile_definitions(TeaMonitorFusion PRIVATE TEA_MONITOR_HAS_EBUS_SDK=1)
    target_include_directories(TeaMonitorFusion PRIVATE "${EBUS_ROOT}/Includes")
    target_link_directories(TeaMonitorFusion PRIVATE "${EBUS_ROOT}/Libraries")
    target_link_libraries(TeaMonitorFusion PRIVATE PvBase PvDevice)
endif()
