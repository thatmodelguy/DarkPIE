cmake_minimum_required(VERSION 3.21)
project(DarkPIE VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

if(NOT DEFINED CMAKE_PREFIX_PATH AND EXISTS "${CMAKE_SOURCE_DIR}/qt/6.8.3/msvc2022_64")
    set(CMAKE_PREFIX_PATH "${CMAKE_SOURCE_DIR}/qt/6.8.3/msvc2022_64")
endif()

find_package(Qt6 REQUIRED COMPONENTS Widgets)
if(WIN32)
    enable_language(RC)
endif()

add_library(darkpie_core STATIC
    src/lang/Lexer.cpp
    src/lang/Parser.cpp
    src/lang/ScriptCheck.cpp
    src/lang/Vm.cpp
    src/devices/DeviceBus.cpp
    src/devices/KeyboardDevice.cpp
    src/devices/SpeechDevice.cpp
    src/devices/MouseDevice.cpp
    src/devices/CursorDevice.cpp
    src/devices/GameControllers.cpp
    src/devices/MidiDevice.cpp
    src/devices/OscDevice.cpp
)
target_include_directories(darkpie_core PUBLIC src)
target_compile_definitions(darkpie_core PUBLIC UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
target_link_libraries(darkpie_core PUBLIC ole32 uuid sapi user32 gdi32 shell32 dinput8 dxguid xinput hid setupapi winmm ws2_32)

add_executable(DarkPIE WIN32
    src/main.cpp
    src/ui/MainWindow.cpp
    src/ui/ScriptEditor.cpp
    src/runtime/Engine.cpp
    res/darkpie.qrc
    res/app.rc
)
target_link_libraries(DarkPIE PRIVATE darkpie_core Qt6::Widgets)
if(MSVC)
    target_link_options(DarkPIE PRIVATE "/MANIFESTUAC:level='requireAdministrator' uiAccess='false'")
endif()

add_executable(darkpie_tests
    tests/test_lang.cpp
)
target_link_libraries(darkpie_tests PRIVATE darkpie_core)
target_include_directories(darkpie_tests PRIVATE src)

enable_testing()
add_test(NAME lang COMMAND darkpie_tests)
