# Native chassis investigation and probe 0.5.0

## Offline verification

Inspected the installed 2.5 executable read-only with pefile/capstone isolated in dependencies/analysis-python. The shipped address database contains many unnamed entries: symbol-text search produced no matching chassis names, while exact reference hashes resolved all seven selected functions. Address section numbers were mapped through the executable PE section table, not treated as direct RVAs.

Disassembly is saved in artifacts/native-research/disassembly.json. Three independent chassis getters read the 32-bit physics proxy identifier at component offset 0x160. This corroborates the reference layout for this executable, not arbitrary future builds.

| Reference hash | Function interpretation | Installed RVA |
| --- | --- | --- |
| 856298520 | Chassis is kinematic | 0x2606014 |
| 1665341632 | Chassis linear velocity | 0x53bd00 |
| 2145786159 | Chassis angular velocity | 0x53bc80 |
| 411244519 | Chassis transform | 0x2e28d4 |
| 1570510966 | Resolve physics proxy | 0x6174f0 |
| 1210388152 | Create proxy helper | 0x612ed4 |
| 1189351231 | Unlock proxy helper | 0x2e3200 |

Reference signatures derive from Let There Be Flight and its pinned SDK, then were checked against local disassembly. No reference physics implementation was linked into our DLL.

## Probe behavior

0.5.0 calls only the first three read-only getters when an attached mounted vehicle is first inspected. Native calls require matching executable timestamp, image size, and exact 24-byte entry signatures for all three functions. Failed guards log DNC_NATIVE_SKIPPED. Native output logs the proxy ID, kinematic state, and linear/angular velocity. Non-finite values are rejected. No active proxy pointers are retained or traversed, and no physics locks or filter writes are added.

Resource diagnostics now explicitly log shape counts, missing filter handles, null bodies/shapes and count-limit skips. The previous absence of filter records was ambiguous; this version disambiguates it.

The native getters report cached physics values; their success alone will NOT establish mutable live-shape access, filtering semantics, threading or restoration. Those still require investigation. No ghosting or fading is active.

## Validation and test

Release x64 build passed; installed DLL hash matched. Runtime validation is pending. Previous 0.4.1 DLL preserved.

Load a save, sit in one stock car for 10 seconds, then exit and quit. A motorcycle can be included in the same run if convenient. Expected new markers: DNC_NATIVE_CHASSIS (or a precise rejection), DNC_BODY and DNC_SHAPE_FILTER_MISSING / DNC_ASSET_FILTER. No collision or driving maneuver is necessary.

## Reproduction tools

- tools/inspect-symbols.py: extracts targeted named symbols without printing the large database.
- tools/inspect-native.py: resolves selected hashes and records offline disassembly.
- tools/generate-native-guards.py: generates src/VerifiedBuild.hpp from the exact executable. Do not run it on a new build and assume compatibility: signatures/ABI must be re-reviewed first.

Disassembly windows may include adjacent functions after a return. Only reviewed entry paths support the conclusions above. Analysis artifacts are local research, not intended for distribution with the mod.

## 0.5.0 runtime result — October 29, 2026

Source log: drivenocollision-2026-10-29-12-30-11.log, preserved under artifacts/logs.

- Observed vehicleCarBaseObject, appearance thorton_mackinaw__basic_suburban_1, 147 components.
- Exact-build guards passed and native getter calls completed.
- Chassis proxy ID: 0x000107f1 (session-specific, not a reusable identifier).
- Kinematic flag: false. Linear velocity approximately (-0.0005, 0.0023, -0.0009), angular velocity approximately (-0.0026, 0.0003, 0.0000). Values are finite and consistent with a nearly stationary vehicle; units are not independently calibrated by this test.
- Loaded chassis resource contains one body and seven non-null shapes.
- All seven shapes have null per-shape filterData handles in the resource representation.
- Optional player-only resource not loaded.
- Probe completed and plugin unloaded cleanly.

This resolves the earlier ambiguity: the resource shapes exist; their individual filter handles are absent. It does NOT mean physical collisions are absent or that live masks are zero. Filters may be supplied elsewhere during actor creation; that remains a hypothesis to trace.

Next: trace the verified chassis proxy to its active shape/filter state and identify filter assignment/ownership before designing a reversible mutation. Do not patch resource shape pointers or assume an absent resource filter implies an empty live filter. No repeat of this same test is needed. Vehicle-body transparency remains unimplemented.
