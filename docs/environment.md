# Local environment

Verified October 28, 2026 (America/New_York).

- Game: D:\SteamLibrary\steamapps\common\Cyberpunk 2077
- Product version: 2.5
- Executable file version: 3.0.5813403
- Visual Studio 2022 C++ Build Tools detected.
- RED4ext 1.31.0 installed from official release; SHA256 verified against https://github.com/wopss/RED4ext/releases/tag/v1.31.0.
- No pre-existing files were overwritten.
- Installed files: bin/x64/winmm.dll, red4ext/RED4ext.dll, red4ext/LICENSE.txt, red4ext/THIRD_PARTY_LICENSES.txt.
- CET, redscript compiler, Codeware, TweakXL and ArchiveXL were not found at the checked standard paths.

Installation is verified on disk only. Launch the game and inspect RED4ext logs before claiming successful runtime initialization. No custom mod plugin or collision change is installed yet.

Next development step: select a patch-compatible RED4ext SDK, build a minimal diagnostics plugin, then investigate per-vehicle collision filtering without modifying global world collision.

RED4ext runtime initialization and clean shutdown verified in red4ext-2026-10-28-22-10-45.log. Diagnostics 0.1.0 built and installed; its runtime check is pending. DLL SHA256: 56CFCC28594CA3F139B375D880F0DB2E2AE651BE6859DF34B64931095F9212F5

Diagnostics 0.1.0 runtime success verified from the 22:16 log. Expanded diagnostics 0.2.0 built and installed; runtime pending.

Diagnostics 0.3.0 runtime success verified from the 22:27:18 log. Reflection index saved under artifacts/reflection-functions.json.

Diagnostics 0.4.0 installed, awaiting in-world validation. SHA256: 13BF53621759FE3D8AFD0568513B41BFBFDB1D6A7B865CB6627253AEC7879BD4

October 29, 2026: diagnostics 0.4.1 installed with persistent polling fix. SHA256: 5CD9819F9DF10719252B2406E6F316E3362CFCAC3FCA6DEB4B7D0BF46F35C95C. Runtime test pending.

October 29: 0.4.1 continuous polling, car and motorcycle discovery, component/resource inspection and clean shutdown verified. See vehicle-results.md.

October 29: 0.5.0 compiled and installed. SHA256: B50F3CE30D3601973F2FAC55B930ED3224DED55D88864E6EC271EEBAE3111CEE. Native getter runtime validation pending.

0.5.0 native getter runtime validation passed on the Mackinaw car. Seven non-null resource shapes all lacked individual filterData. Successful log preserved; no new DLL installed.

0.6.0 installed. SHA256: DF026B8246C3293390C398982B67C1F5231AA09CF61BEA2B11F54C0050083326. Read-only direct state/filter probe pending runtime validation.

0.6.0 runtime verified: direct velocity cross-check and body-0/shape-0 simulation-filter read succeeded. No physics writes performed. Baseline saved under artifacts/live-filter-baseline.json.

0.7.0 installed. SHA256: EDB9E8E983A1F7AAB4874438A22101CAD8CDDFAD8A97B20966569C8FC566AF48. Named-group mapping runtime pending.

0.7.0 runtime mapping passed: 20 collision groups; Vehicle index 4, Static index 2, Terrain index 7. Semimaru first-shape mask differs from prior Mackinaw. See group-probe.md.
