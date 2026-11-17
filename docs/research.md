# Existing-mod research

Research date: October 28, 2026 (America/New_York).

Searches covered Nexus and GitHub using vehicle ghosting, no collision, vehicle noclip and disable collision queries. No exact racing-style traffic ghosting match was verified. This is not proof none exists.

| Project | Relevant capability | Assessment |
| --- | --- | --- |
| [Straight Edged Driving](https://www.nexusmods.com/cyberpunk2077/mods/33768?tab=description) | Handling, brake and steering presets, including arcade steering | Candidate for handling; page does not establish traffic ghosting. Author requires permission for modification/asset reuse. |
| [Immersive Driving](https://github.com/ArthurHub/Cyberpunk2077-Immersive-Driving) | Keyboard input shaping, speed-sensitive steering, cruise control | Useful source reference and optional companion. Its current README targets 2.5 and directs 2.31 users to v1.4. GPL-3.0; do not copy code without satisfying its license. |
| [NCGT](https://www.nexusmods.com/cyberpunk2077/mods/31730) | Simulation-oriented handling overhaul | Different handling direction from arcade racing; no verified ghosting feature. |
| [FreeFly](https://www.nexusmods.com/cyberpunk2077/mods/780) | Noclip/flying | Its description does not establish normal driving with selective traffic collision filtering. |

Choose only one handling baseline for initial testing. Compatibility with our future collision implementation is untested.

## Technical references

- [Vehicle collider documentation](https://github.com/CDPR-Modding-Documentation/Cyberpunk-Modding-Docs/blob/main/modding-guides/vehicles/boe6s-guide-new-car-from-a-to-z/interaction-collider-.phys-files.md): vehicle chassis collisionResource and .phys assets are investigation starting points, not evidence of a runtime traffic-only toggle.
- Immersive Driving's source documents a native input hook plus redscript driving-state tracking. It explicitly warns that input offsets are patch specific. This supports investigating a native extension if scripting lacks suitable collision controls; it does not prove a collision hook is available.

No third-party mod code or assets have been copied.

## Weather and sim-racing scope extension

- [NCGT](https://www.nexusmods.com/cyberpunk2077/mods/31730) documents rain, standing water, terrain and tire behavior. Evaluate as an alternative weather/handling owner; do not stack its weather friction with ours without an explicit compatibility mode.
- [Moza/DirectInput wheel project](https://github.com/natpoh/cp2077-wheel-mod-moza) documents wheel support and force feedback for 2.31. Candidate to investigate, not verified for 2.5.
- [Logitech wheel project](https://github.com/clevergrant/cp2077-wheel-mod) and its [Nexus page](https://www.nexusmods.com/cyberpunk2077/mods/29172) provide a Logitech-specific starting point. Check current source, release support and licensing before integration.

User's wheel/base/pedal/shifter hardware and desired force-feedback scope are still pending. No existing mod has been installed, copied or declared compatible.

Scope clarification: wheel, pedals, shifters and force feedback are now confirmed. Analog throttle mapping to pedals/controller triggers is required. Exact device models remain unknown. Custom shifting behavior and sound effects are explicitly deferred to a later phase.
