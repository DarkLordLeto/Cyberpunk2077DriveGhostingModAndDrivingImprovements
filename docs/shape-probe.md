# Per-shape diagnostic 0.8.0

The probe reads simulation filters for body 0, shape indices 0 through 63, stopping at the first rejected read. It logs the chassis bottom-shape bitmask and each shape's corresponding bit. No collision, query-filter, rendering, or handling writes are enabled. No raw shape/proxy pointers are retained.

## Offline evidence

The reviewed physical proxy reader at RVA 0x614568 rejects body indices at or above its live body count, then calls 0x614668. The latter's state-32 branch at 0x614992 calls getNbShapes (slot +0xc8), rejects shape >= count, retrieves one shape (slot +0xd0), and reads its simulation filter (slot +0xa8). Out-of-range shape indices return false at 0x614e00. Another proxy wrapper at 0x614618 reaches the same reader with its own body bounds check. Full bytes for these three routines are now included in runtime guards, in addition to existing guards.

The scan is bounded and diagnostic. First rejection is logged as rejection, not a guaranteed count: proxy lifetime/read failure can also stop the scan. Hitting 64 is explicitly incomplete. Body 0 is not assumed to include all vehicle bodies. The bottom-mask association comes from the chassis setter, not from a demonstrated tire-support role. Bit indices >=32 cannot be represented by this field and are never shifted into it.

## Restoration finding

The state-33 query-filter reader does NOT return raw PxFilterData: it sets the first output qword to zero and ORs the two raw qwords into the second. Therefore this read API cannot supply an exact query-filter restoration snapshot. Any future query writes need a separate verified raw snapshot path; the present probe never writes query state.

The symmetric-mask comparison at 0x2bb1008 uses state 0x21 through 0x92ed3c. It is query-related evidence, not the simulation contact-pair shader. The actual simulation-mask combination and reserved-flag semantics remain unresolved.

Evidence: artifacts/native-research/filter-reader-analysis.json, proxy-reader-wrappers.json, proxy-reader-candidates.json, cross-mask-candidates.json. tools/inspect-native-targets.py provides bounded disassembly and instruction-validated direct-call references. Candidate searches do not establish runtime binding identity by themselves.

## Validation and test

MSVC x64 Release build passed. Installed DLL hash matched the built DLL. Previous 0.7.0 DLL preserved under artifacts. Runtime test is pending.

In one game session: load a save, enter a stock car and stay in it for 10 seconds; exit, enter a motorcycle and stay for 10 seconds; exit and quit. No collisions or driving maneuvers are required. Logs should show DNC_SHAPE_SCAN_BEGIN, DNC_SHAPE_FILTER, and DNC_SHAPE_SCAN_END for each vehicle. Exact-build mismatch rejects the native probe.

## 0.8.0 runtime result — car and motorcycle passed

Source: drivenocollision-2026-10-29-17-35-03.log, archived in artifacts. Structured scan data: artifacts/shape-scan-baseline.json. The parser checks contiguous indices, bottom-mask consistency, and completion; all three scans passed. Native velocity cross-checks passed, no native rejection or warning/error lines were found in this plugin log, and DNC_UNLOADED was recorded.

| Vehicle | Accepted body-0 shapes | First rejected index | Bottom-mask shape |
| --- | --- | --- | --- |
| Semimaru (two mounts, identical results) | 7 | 7 | 0 |
| Kusanagi | 6 | 6 | 1 |

Every observed non-bottom shape had simulation mask1=0x10, mask2=0x8000000000008cf0. Each observed bottom-marked shape had mask1=0, mask2=0x58. This explains the earlier mismatch between first-shape reads: shape index 0 has different roles across these vehicles. The bottom marker still does not prove wheel-support behavior.

Live shapes also differ from resource shapes: the Semimaru logged three resource shapes but seven accepted live shapes; the Kusanagi logged four resource shapes but six accepted live shapes. Resource counts cannot drive live snapshots. No scan reached the cap. First rejection is still an observed read failure, not an independent enumeration API.

Installed 0.8.0 remains read-only. No identical repeat test is needed. Next work is offline verification of simulation pair-mask logic and the concrete simulation-filter setter/refresh path, followed by per-shape snapshot/restoration and driver ownership. Do not use normalized query-filter reads as raw snapshots. No ghosting, fading, handling, or wheel-input behavior was tested or enabled.
