# Diagnostics prototype 0.1.0

This is a read-only RED4ext plugin targeting patch 2.5 (raw executable version 3.0.88.46235, equivalent to the displayed 3.0.5813403). It logs load/unload and probes seven candidate vehicle/collision RTTI classes after type registration. Class presence does not prove a writable collision-filter API exists.

No physics hooks, save changes, handling adjustments, weather effects or device input changes are implemented.

## Build

Use Visual Studio 2022 C++ Build Tools and CMake:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

Official SDK: https://github.com/WopsS/RED4ext.SDK
Pinned source commit: 5246e659d783e5834cc08a90e56c8932c9ed3449
Local dependency directory: dependencies/RED4ext.SDK
Retain upstream licensing when distributing SDK-derived code/binaries.

## Runtime check

Launch the game to its main menu, then exit. Inspect red4ext/logs and the DriveNoCollision plugin directory for logs. Expected markers: DNC_LOADED, DNC_TYPE, DNC_DIAGNOSTICS_COMPLETE, DNC_UNLOADED. Missing types are diagnostic findings, not necessarily failures. Absence of the completion marker requires investigation before any physics prototype.

Installed DLL: D:/SteamLibrary/steamapps/common/Cyberpunk 2077/red4ext/plugins/DriveNoCollision/DriveNoCollision.dll
To disable this prototype, close the game and move the DLL out of the plugins directory.

## Validation

Release x64 build passed. Export table contains Main, Query and Supports. Installed DLL hash matched the build artifact. Runtime execution is pending the next game launch.

## Update: 0.2.0
0.1.0 loaded, scanned and unloaded successfully. Expanded 0.2.0 is installed and awaits a main-menu launch. It enumerates relevant native metadata and function signatures; see collision-findings.md. Previous DLL and successful log are preserved in artifacts/.

## Update: 0.3.0
0.2.0 scanned 288 types and unloaded cleanly but function signature metadata was incomplete. 0.3.0 is built and installed, scans once in Running, and includes entity/render types. Runtime validation is pending. Previous DLL/log preserved.

## 0.3.0 runtime verified
The 22:27:18 run loaded, scanned 623 types with 782 parameter records, and unloaded cleanly. See runtime-findings.md. No new DLL installed after this run; no repeat main-menu test needed.

## 0.4.0 installed
Read-only live-vehicle probe added. See live-probe.md for the in-world test. Compilation, exports and installed hash verified; runtime pending.

## 0.4.1 installed
Corrected Running callback lifetime (return false). Added periodic polling and state-transition logs. 0.4.0 captured no vehicles. Repeat in-world test; no repeat menu-only test is needed.

## 0.5.0 installed
Added exact-build-guarded native chassis getters and explicit missing resource-filter logging. Offline verification and test instructions: native-probe.md. Runtime pending.

## 0.6.0 installed
Guarded native state reader added, with velocity cross-check before body-0/shape-0 SimulationFilter read. See live-filter-probe.md. Compiled and installed; runtime pending.

## 0.7.0 installed
Added engine collision-group table mapping after successful filter read. See group-probe.md. Build and installed hash passed; runtime pending.
