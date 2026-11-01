# Cyberpunk 2077 Drive Ghosting

DriveNoCollision 0.29.0 is an experimental RED4ext plugin for Cyberpunk 2077 patch 2.5 on Windows. While you are driving, recognized NPC traffic vehicles can pass through your vehicle. Roads, walls and barriers retain their normal collisions. A live driving test confirmed traffic pass-through and solid wall contact. The Release build passed all 16 automated test suites.

## Install

1. Install [RED4ext](https://github.com/WopsS/RED4ext/releases). The tested version was 1.31.0.
2. Close the game. Download [DriveNoCollision-0.29.0.zip](dist/DriveNoCollision-0.29.0.zip) and extract it into your Cyberpunk 2077 game directory.
3. Check that `red4ext/plugins/DriveNoCollision/DriveNoCollision.dll` exists under the game directory. Launch the game and drive toward NPC traffic. Ghosting activates automatically for eligible contacts.

To uninstall, close the game and remove `red4ext/plugins/DriveNoCollision/DriveNoCollision.dll`.

## Scope of this release

The plugin changes contact response when the engine creates an eligible player-driver and traffic collision pair. It targets the verified game and PhysX builds. Traffic that has not yet been identified, separate attachments and unsupported contact types may still collide. Damage and sound effects have limited testing. Diagnostic logging is enabled in this pilot.

During overlap, the plugin applies a shared glass material at 50% opacity to supported exterior panels and wheels. It hides supported interior meshes and restores the original appearance after separation. Vehicle models can vary in coverage. Borrowed cars passed the driving test. Motorcycle collisions remain unresolved and some cars can still collide during combat. Car handling, weather grip, wheel controls and shifting are planned separately.

## Build from source

Use a Windows x64 Visual Studio 2022 build environment, CMake 3.21 or later and the pinned RED4ext SDK submodule.

```powershell
git submodule update --init --recursive
cmake -S . -B build -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

The plugin is built at `build/Release/DriveNoCollision.dll`.

