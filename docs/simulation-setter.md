# Simulation-filter setter and restoration foundation

## Installed-binary evidence (2026-10-29)

Read-only analysis, no runtime setter invocation. Reader addresses in non-executable PE sections identify candidate adjacent virtual setter slots:

| Candidate vtable RVA | Reader (+0x50) | Setter (+0x58) |
| --- | --- | --- |
| 0x3060380 | 0x614568 | 0x6135c0 |
| 0x3066cb8 | 0x614618 | 0x26d6130 |
| 0x3060d38 | 0x612dcc | 0x612aa4 |

All three candidate setters perform body-index checks and call actor-state setter 0x613a80. Its state-32 branch begins at 0x613cf8: gets live shape count (+0xc8), validates a concrete index, gets one shape (+0xd0), splits the two input qwords into four words without discarding bits, and calls the shape simulation-filter setter (+0xa0). State 32 therefore supports explicit per-shape 128-bit writes. The branch returns success after the void virtual setter; success is not an independent readback guarantee.

Out-of-range indices enter a comparison against 0xff. Only that value selects all shapes in this concrete routine. This differs from the 0xffffffff value observed at the chassis wrapper before cache packing. Cache entries store a one-byte shape index, which explains a possible truncation route, but direct-versus-cached dispatch must not be assumed equivalent. Our restoration planner rejects both sentinel encodings and only accepts explicit body/shape indices below 255.

State 33 writes only the second qword into the upper half of the raw query filter and zeros the lower half. Combined with the already verified lossy reader, it is unsuitable for raw query-filter round trips. No query-filter writes are planned in the current restoration core.

No explicit scene reset-filtering call occurs in the reviewed state-32 branch. This does not establish that refiltering is absent: it may occur inside the shape virtual setter. Bundled PhysX exports did not expose a named shape setter/resetFiltering entry. Diagnostic-string search only found a cloth setter, not the rigid-shape setter. These searches are negative evidence, not verified contact-refresh behavior.

The upstream [PhysX 3.4 NpShape source](https://github.com/NVIDIAGameWorks/PhysX-3.4/blob/master/PhysX_3.4/Source/PhysX/src/NpShape.cpp) forwards simulation filter changes through its shape layer and checks writability in checked builds. It is reference material only; it does not establish the exact behavior of the game's bundled DLL. Direct invocation remains disabled pending safe synchronization and concrete refresh verification.

Artifacts: proxy-vtable-candidates.json, proxy-setter-analysis.json, actor-state-setter.json under artifacts/native-research. Tools: find-proxy-vtables.py, inspect-native-targets.py, inspect-physx-exports.py, find-physx-setter.py. All game RVAs are specific to PE timestamp 1792368047 / SizeOfImage 92385280.

## Implemented pure restoration planner

src/FilterSnapshot.hpp is independent of RED4ext and does not call game APIs. It captures explicitly declared complete inventories and retains each shape's original 128 bits across repeated confirmed edits. Confirmation requires expected-before bookkeeping and a matching readback. Restore planning returns an original value only for a matching entity/proxy/epoch and a current value matching the last confirmed edit. Missing/duplicate shapes, external changes, and lifetime mismatches produce no write. Already-original values require no write.

The caller must establish a non-reused epoch and invalidate it on shape/body rebuild or reorder. Entity/proxy IDs alone are insufficient. Queued write tracking, pending-operation draining, synchronization, ownership/driver validation, overlap clearance, immediate pre-write revalidation, and execution acknowledgments belong to the future engine adapter. This planner is not a transaction executor or a solution to those runtime requirements. In particular, the diagnostic's first rejected read does not by itself prove a complete stable inventory suitable for Capture.

## Validation

MSVC x64 Release build passed. CTest passed FilterSnapshotTests, including exact original/high-bit restoration, repeated edits, failed readback, stale preimages, external conflicts, stale lifetimes, missing/duplicate shapes, incomplete capture, wildcard rejection, and restoration confirmation. Test masks are synthetic edit values, not an approved collision policy. Tests use explicit checks that remain active in Release builds.

The installed 0.8.0 diagnostic remains unchanged. No gameplay behavior is enabled, and no new game launch is requested for this milestone. Next: identify the simulation pair-filter callback and the concrete shape setter/refresh path, then implement a synchronized engine adapter with lifecycle and overlap safeguards.
