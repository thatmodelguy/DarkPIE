# DarkPIE

A dark-themed [GlovePIE](https://glovepie.org/) compatible script host for Windows. Load original `.pie` scripts and map speech, keyboard, mouse, controllers, MIDI, and OSC.

## Standalone build

A portable Windows build is `DarkPIE.exe` plus the Qt files next to it. Run **as administrator** so games can see injected keys (same as GlovePIE).

## Build from source

Qt 6.8 (MSVC 2022, 64-bit) and CMake 3.21+ are required.

```bat
cmake -S . -B build
cmake --build build --config Release --target DarkPIE
```

The executable is `build\Release\DarkPIE.exe`.
