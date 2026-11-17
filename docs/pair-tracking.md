# Scoped pair tracking diagnostic: 0.15.0

Build and all eight Release CTest suites passed. Runtime notification tracking passed on 2026-10-30. Includes the previously uninstalled 0.14.0 driver lease and independent polling changes.

Installed PhysX3_x64.dll SHA256: 4d19dd03dc56b92ac902ffe0c7f3ed2297a31e0b24663d3d674dc94ba8374f41; PE timestamp 1690925236, image size 2625536. Function RVA 0x138430 (910 bytes) matches the upstream runFilter allocation/callback/release flow. The call at 0x138693 uses the verified hidden return buffer ABI; 0x138710 checks notification mask 0x0c; 0x138784 returns untracked indices to the free list. This supports recycled pair ID zero in earlier logs. Full disassembly is in artifacts/native-research/physx-lifecycle-candidates.json.

The game's pairLost table entry points to a three-byte shared ret-0 stub at 0x1ff2b0; statusChange points to xor-al-al/ret at 0x664f00. The disassembly helper reads beyond leaf stubs when no unwind entry exists: only these initial instructions belong to the callbacks. Do not treat the following instructions as callback code. Do not globally detour these shared stubs.

The probe atomically replaces only the pairLost slot at the verified game callback table plus 8, checking its original pointer/stub and the exact reviewed PhysX function bytes. It restores page protection after pointer exchange and restores the pointer on shutdown. Installation rollback and restoration failures are reported. No statusChange replacement is installed.

For at most eight eligible, driver-authorized dynamic traffic pairs, pairFound changes original default filter flags from 0 to eNOTIFY (0x0c). It leaves every pair-response flag unchanged, so normal collision solver response stays enabled. This is the first diagnostic that changes filter outputs; it requests lifecycle notifications only. Immutable fixed records retain callback scope, pair ID, proxy IDs and original pair flags. Logging occurs on the game thread. Lost events mark records atomically, with no script calls or allocations. Recycled IDs across separate callback objects and successive lifetimes are covered by offline tests. Records are never reused in this probe.

DNC_TRACK_READY confirms installation; DNC_TRACK_FOUND and DNC_TRACK_LOST report observed lifetimes; DNC_TRACK_RESTORED confirms the callback-table slot was restored. Eligibility uses exact non-kinematic dynamic attributes (1/1), unchanged default original filter output and current driver lease. Discovery and snapshot diagnostics remain active.

Test: approach a traffic car slowly, gently touch it, then separate by several car lengths. Repeat with another traffic car, then exit. The goal is matched found/lost records, not passing through cars. Collision suppression and fading remain disabled. An eNOTIFY decision can outlive the diagnostic but still uses normal solver response; after slot restoration its loss callback is the original empty stub.

Next gate: verify live pair IDs/loss ordering, then implement statusChange/restoration with original per-pair settings and overlap-safe disabling. Returning original response on driver expiry alone does not modify an already-created pair.

Reference: https://raw.githubusercontent.com/NVIDIAGameWorks/PhysX-3.4/master/PhysX_3.4/Source/SimulationController/src/ScNPhaseCore.cpp . Installed-machine code was checked independently; no upstream source code is copied into the plugin.

## Runtime result: 2026-10-30

Archived drivenocollision-2026-10-30-12-09-24.log; structured evidence in artifacts/pair-tracking-baseline.json. All eight found records received matching lost records, across two vehicle combinations. IDs 0¨C2 were reused after the first combination lost its records; the second combination used IDs 0¨C4. Original pair flags remained 0x3e61 in all audit records. Callback-table restoration, hook detach and plugin unload succeeded with no warning/error entries.

This verifies scoped tracking and the pairLost calling convention in this session. Lost notifications appeared within tens of milliseconds of several found notifications; the test cannot distinguish refiltering from actual geometric separation. Do not use a pairLost event alone to decide whether vehicles have stopped overlapping visually. A future reusable registry must remove exact callback-scope/pair-ID lifetimes, handle refilter lost/found transitions, and aggregate multiple shape pairs per vehicle combination.

No repeat of this tracking test is needed. Next: inspect/verify statusChange dispatch, implement reusable lifetime state with original pair flags, and validate a no-response-change restoration round trip before enabling solver suppression.
