# Pair-callback probe 0.9.0

## Scene registration establishes the shader identity

RVA 0x72e084 gets PxPhysics through the named PxGetPhysics import, initializes a scene descriptor at rbp-0x60 through 0x72e568, and stores function 0x320794 at descriptor +0x38. It calls PxPhysics slot +0x90 (createScene). The independent scene setup at 0x72f500 also stores 0x320794 at descriptor +0x38 before calling the same slot. The descriptor layout matches upstream [PxSceneDesc](https://github.com/NVIDIAGameWorks/PhysX-3.4/blob/master/PhysX_3.4/Include/PxSceneDesc.h): +0x38 is filterShader, +0x40 is filterCallback on x64. Actual active gameplay scene association still needs runtime verification.

## Ordinary simulation-pair rule

The shader at 0x320794 has a hidden return-buffer argument on Windows x64. That shifts the visible arguments and explains why earlier searches based on ordinary callback register positions missed it. Function 0x320964 converts raw filter words to two 64-bit masks without losing incoming bits (the freshly initialized destination flags are zero).

For the non-trigger branch, the assembly at 0x320801..0x320814 computes:

    (a.mask1 & b.mask2) | (b.mask1 & a.mask2)

If zero, it returns filter flags 2 (SUPPRESS) without changing pair flags. Otherwise it ORs pair flags with 0xc01, and returns 4 (CALLBACK) if either shape has either contact-registration flag; otherwise it returns 0 (DEFAULT). These flag names are consistent with upstream [PxFiltering.h](https://github.com/NVIDIAGameWorks/PhysX-3.4/blob/master/PhysX_3.4/Include/PxFiltering.h). Triggers use a separate, directional branch and are outside the ordinary-pair model.

This rules out the naive one-sided mask edit: clearing Vehicle from the player's mask2 leaves the other vehicle's mask2 matching the player's Vehicle category. Clearing that category too is not generally world-safe: a world shape may rely on its own response mask to match Vehicle. CollisionFilterModelTests includes both counterexamples, a small exhaustive truth table, captured car/bike mask combinations, symmetry and registration flag tests. These are offline model checks, not in-game collision tests.

## Actor-aware callback registration

Scene setup 0x72f500 constructs a filter callback via 0x72f890, stores it at descriptor +0x40, and gives it vtable 0x35a1ba8. Entries are pairFound 0xc0edc0, pairLost 0x1ff2b0, statusChange 0x664f00, and destructor 0xa0dc58. pairFound reads a 32-bit value at actor0+0x10 and actor1+0x10, and uses the original engine's type decoding and contact-notification rules. Both regular captured chassis masks request this callback for accepted ordinary pairs; bottom-to-bottom pairs are already suppressed by the shader.

A pair-aware hook is the preferred candidate for traffic-only suppression because it can inspect actor identity while retaining existing world/query masks. It is not yet implemented as a suppression hook. Driver validation, identifying actual NPC vehicles (rather than only category bits), handling existing pairs on toggle, lifecycle/streaming, thread-safe state publication, and separation before restoration remain required. Pair callbacks may run concurrently; game object lookups and rendering changes must not be performed there.

## Observer implementation

PairProbe.cpp attaches through RED4ext's supported hooking API only if the executable header, reviewed function bytes and registered vtable entry match. It uses an explicit return-buffer ABI, calls the original function once with every argument unchanged, and returns its exact returned pointer. It does not change pair/filter outputs.

It captures up to 16 general samples and 64 matching-player samples for the session in fixed arrays. Writers reserve distinct indices with lock-free atomics and publish records with release/acquire; the Running callback logs them. No heap allocations, game object resolution, pointers retained in records, or logging occur in the physics callback. Counters continue after sample capacity is reached. The observed player proxy is cleared when a subsequent mounted-state poll detects an unavailable/unmounted vehicle; the poll can lag by two seconds. A matching integer is a correlation probe, not sufficient identity authorization for gameplay writes. Player-proxy tracking currently follows the once-per-mount chassis inspection and is not a rebuild-safe owner registry.

Expected logs: DNC_PAIR_ATTACHED, DNC_PAIR_SAMPLE, DNC_PAIR_PLAYER_MATCH, DNC_PAIR_COUNTS, DNC_PAIR_DETACHED. Rejected installation logs DNC_PAIR_SKIPPED. The hook is removed on plugin unload. A detach failure is logged and not reported as clean unload.

## Validation and user test

MSVC x64 Release build and both CTest suites passed. Runtime validation of the callback ABI and actor-ID mapping is pending. Installed 0.9.0 retains 0.8.0 diagnostic features; no ghosting, fading, handling, or input changes are active.

Load a save, drive a car normally near traffic for approximately 20 seconds, then do the same on a motorcycle and quit. Deliberate crashes are unnecessary. This checks callback execution and correlation with the mounted chassis; it does not prove traffic-specific classification or ghosting. Sample capacity may be exhausted before the second vehicle; counts remain useful but the log must be checked before claiming both correlations.

Evidence: physics-creation-callers.json, scene-filter-callback.json, scene-main-registration.json, scene-callback-constructors.json, pair-callback-vtable.json, pair-found-analysis.json under artifacts/native-research.

## 0.9.0 runtime result — callback and car/bike correlation succeeded

Source: drivenocollision-2026-10-29-22-39-33.log, archived in artifacts. Parsed report: pair-probe-baseline.json. The callback attached and detached, the plugin unloaded cleanly, and its log contains no warning/error lines. The latest periodic counters were 38,988 total calls and 600 player matches; these are lower bounds on the session totals because shutdown did not emit a final counter snapshot.

- Kusanagi player bike, entity 9021330 / chassis proxy 00010490: five saved matching callback records.
- Quadra Turbo car, entity 9021331 / chassis proxy 00010b24: 59 saved matching records, including both actor orderings across the observed samples. Three records pair it with the earlier bike's proxy 00010490, giving a known vehicle-to-vehicle correlation. That other vehicle is the previously mounted bike; do not label it NPC traffic.
- A later Kusanagi variant, entity 9021332 / proxy 000141bd: mounted and scanned after the 64-record matching sample cap. Aggregate player-match counters increased during its interval, but there are no saved per-pair records for it. Do not claim equivalent direct sample verification for this variant.

Every saved player record has observedPlayer equal to actor0 or actor1. Recorded outputs remained original filterFlags=0, pairFlags=0x3e61. This is runtime evidence that the explicit hidden-return-buffer ABI and +0x10 actor ID read work on this build; it is not an exhaustive ABI or concurrency proof.

All saved records report pair ID 0, including distinct actor combinations. The argument cannot serve as a unique lifetime/overlap key in the observed mode. This might reflect the engine's untracked-pair mode, but that explanation remains an inference until the PhysX call site is checked. A future overlap registry needs verified shape/actor identity and generations or a validated tracked-pair mechanism, plus unload/rebuild invalidation. Do not key fading/restoration solely by this field.

No gameplay collision values were changed. No identical repeat test is needed. The next implementation step is identifying eligible NPC-vehicle counterparts and driver ownership, then designing toggle/overlap lifecycle and callback-safe publication before enabling suppression. Static/dynamic attributes alone cannot distinguish vehicles from props or road/world objects. Confirm current callback coverage for relevant vehicle shapes as part of that work.
