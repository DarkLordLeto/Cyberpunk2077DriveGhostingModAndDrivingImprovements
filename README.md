# DriveNoCollision

Cyberpunk 2077 racing-style traffic ghosting project for the installed 2.5 build.

Current source is plugin **0.32.0** with race mode **0.4.0**. The MSVC Release build and all 17 automated test suites passed on 2026-11-17. Latest GPS road-route generation, tracked race navigation and checkpoint placement still require in-game validation. Later entries below record the development history and supersede older status statements.

The packaged transparency pilot is [DriveNoCollision 0.29.0](dist/DriveNoCollision-0.29.0.zip). Current development source and race scripts are included in this repository.

## Build and install current development source

Use Windows x64, Visual Studio 2022, CMake 3.21 or later, and the pinned RED4ext SDK.

~~~powershell
git submodule update --init --recursive
cmake -S . -B build -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
~~~

With the game closed, copy build/Release/DriveNoCollision.dll to red4ext/plugins/DriveNoCollision/ under the game directory. Race mode additionally uses redscript and Codeware, tested with Codeware 1.21.4. Copy scripts/DriveRaceMode to r6/scripts/DriveRaceMode. Read the race-mode entries below for controls, current behavior and known limits.

## Intended behavior

- Player-driven cars and motorcycles pass through traffic, with roads, walls, barriers and wheel contact retained.
- Overlapping NPC vehicles fade to 50% opacity and restore after separation. Other traffic keeps normal collisions.
- Ghosting starts disabled and has a toggle.
- Handling has independently adjustable fog/rain grip reductions with gradual transitions.
- Keyboard/mouse, controller, wheel, pedals and shifter support, including force feedback and calibrated analog throttle curves. Specific sim-racing hardware remains to be identified.
- Custom shifting/transmission behavior and associated audio are deferred to a later phase.

## Verified progress

RED4ext loads and unloads cleanly. Reflection diagnostics identified 623 types and 782 parameter records. The persistent live probe works with cars and motorcycles. Resource shapes can lack filter handles, so the probe reads live simulation filters through an exact-build-guarded native state reader and cross-checks velocity first.

The live table has 20 named collision groups, including Vehicle at index 4. Different cars returned different first-shape masks; a universal chassis preset cannot be assumed. See [group mapping](docs/group-probe.md) and [live-filter probe](docs/live-filter-probe.md).

Offline analysis strongly identified the chassis filter-update routine, its separate primary/bottom filters, lock, and deferred state publication. Bits 63 and 62 are named registerForContacts and registerForPersistentContacts by the engine serializer; their exact contact-callback effects remain unverified. See [contact flags](docs/contact-flags.md). No setter is enabled. See [chassis update findings](docs/chassis-filter-update.md).

Per-shape setter branches have been traced offline. A pure restoration planner now preserves original filters and rejects stale identities/conflicts; its CTest suite passes. It is not connected to runtime writes. See [setter and restoration](docs/simulation-setter.md).

Installed 0.9.0 passed pass-through callback runtime testing, with direct chassis-ID matches for a Kusanagi and Quadra Turbo and clean hook detach. Saved pair IDs were all zero, so they cannot uniquely track overlaps in the observed mode. Scene registration revealed an OR-based ordinary collision rule, so one-sided Vehicle-mask clearing is insufficient. The actor-aware callback is now the preferred integration candidate. See [pair probe](docs/pair-probe.md).

Diagnostic 0.10.0 adds game-thread weak-owner resolution and driver/traffic queries, with separate sample budgets for mounted vehicles. Runtime testing passed owner resolution and positive driver detection on three mounted vehicles. Six sampled non-player traffic proxies were identified; a vehicle changed both proxy and traffic-slot status, requiring refreshed lifecycle tracking. See [owner probe](docs/owner-probe.md).

A bounded lifecycle model now handles proxy migration/reassignment, despawn, session resets, and independent driver/vehicle freshness. All twelve offline test suites pass. Diagnostic 0.11.0 now refreshes sampled owner observations and publishes bounded snapshots for candidate classification; suppression remains unimplemented. See [lifecycle foundation](docs/vehicle-lifecycle.md).

Diagnostic 0.11.0 ran and unloaded cleanly, but candidate detection stayed at zero and is not verified. Its finite post-contact discovery budget must be replaced before another identical test is useful. See [runtime lifecycle integration](docs/lifecycle-runtime.md).

Diagnostic 0.12.0 replaces finite discovery with a reusable bounded queue and adds refresh/publication/rejection counters. Runtime testing observed continued discovery and 68 traffic-candidate callbacks, with clean unload and no observed queue drops or snapshot contention. First-contact discovery is still unresolved. See [continuous discovery](docs/continuous-discovery.md).

Diagnostic 0.13.0 adds a signature-checked nearby-entity query using the game��s optional filter default, discovery-source logging, and unresolved-proxy retry backoff. Runtime testing returned 1�C10 vehicles per query and identified six traffic vehicles before the first saved player contact; scope-wide coverage remains unproven. See [nearby discovery](docs/nearby-discovery.md).

Local build 0.14.0 adds revocable 250 ms driver leases, independent 100 ms/500 ms driver/discovery schedules, and an offline selective solver-response policy. Seven test suites pass; this build is not installed. Pair restoration remains the next integration gate. See [driver and suppression policy](docs/driver-and-suppression-policy.md).

Diagnostic 0.15.0 requests lifecycle notifications for at most eight eligible traffic pairs while retaining normal collision response. It includes the faster driver/discovery checks from 0.14.0. All eight tracked pairs produced matching loss notifications across two vehicle combinations, and the callback table restored cleanly. See [pair tracking](docs/pair-tracking.md).

Diagnostic 0.16.0 adds a guarded status-change callback that submits each tracked pair��s existing flags once, preserving normal collision response. Eight test suites pass; all eight tracked lifetimes submitted unchanged status updates and later reported loss, with both callback entries restored cleanly. See [existing-pair update probe](docs/status-update-probe.md).

A reusable pair-lifetime and restoration state machine now passes offline tests for scoped ID reuse, stale submissions, cancellation, and clearance-gated restoration. It is not yet connected to the native callbacks; installed 0.16.0 is unchanged. See [pair lifetime registry](docs/pair-lifetime-registry.md).

Diagnostic 0.17.0 adds guarded body-0 world-bounds reads and a read-only overlap/clearance report. Ten test suites pass; runtime testing observed two overlapping-to-clear transitions for one vehicle pair. Full vehicle-body coverage and restoration-time validity remain unresolved. The reusable restoration registry remains offline. See [bounds clearance](docs/bounds-clearance.md).

Local 0.18.0 reads and unions every declared body in a supported chassis proxy, rejecting partial or changed inventories. Eleven test suites pass. It is not installed; 0.17.0 remains active. Separate attached proxies and runtime synchronization still need verification. See [body inventory](docs/body-bounds-inventory.md).

Diagnostic 0.19.0 connects the reusable registry to live callbacks (128 concurrent lifetimes) and includes complete supported-proxy body bounds. Twelve test suites pass; runtime testing completed 241 pair lifetimes with zero active entries or faults at shutdown and valid one-body inventories. Normal collision response remains enabled. See [reusable runtime](docs/reusable-runtime.md).

## Next implementation gate

Prove selective vehicle contact suppression on a stock car while retaining world support. Before writes, verify actual live-shape enumeration, filter-pair semantics, update/refresh behavior, driver ownership, and per-shape snapshot/restoration. NPC body opacity needs separate rendering verification.

## Local environment

Game: `D:\SteamLibrary\steamapps\common\Cyberpunk 2077` (installed, runtime tested).

Project: `F:\Work\GameDev\2077Mod\DriveNoCollision`.

Build: MSVC 2022 x64 Release, RED4ext.SDK. See [environment](docs/environment.md), [design](docs/design.md), and [existing-mod research](docs/research.md). Historical diagnostic reports remain under docs/; later runtime results supersede earlier pending-test notes.







## Local suppression controller (not installed)

Added a serialized suppression controller with retained vehicle-pair intent across refilter lifetimes, disable/clearance restoration, stale-event rejection, and conservative detach gating. All 13 Release test suites pass, including 20,000 concurrent lifecycle iterations. Native callback/clearance phase integration remains outstanding; installed 0.19.0 still leaves collision response enabled. See docs/suppression-controller.md. No new in-game diagnostic run is needed for this local change.

## Installed 0.20.0 — native shadow controller

Suppression logic now receives real pair events and calculates proposed changes privately. The engine still receives unchanged collision-response flags. Added enabled-mode intent recycling, bounded clearance sampling and DNC_SHADOW counters. All 13 Release suites pass; native runtime validation is pending. See docs/native-shadow-controller.md for the short traffic-contact test and remaining phase-synchronization work. Ghosting and transparency remain disabled.

0.20.0 runtime result: 61 shadow pair lifetimes registered and removed, 61 locally proposed updates, five intent retirements, zero rejected registrations/remaining pair bindings, clean shutdown. No warning/error entries. See artifacts/native-shadow-baseline.json. This validates shadow integration only; real ghosting and fading remain disabled. No repeat of this test is needed.

## Local restoration submission gate

After 0.20.0 validation, restoration now requires a fresh reader at submission plus unchanged intent/pair lifetime. All 14 Release suites pass. Native review found that the existing proxy bounds reader can wait on an engine lock and release through destruction; it has not been moved into the physics callback. See docs/restoration-submission.md. These local changes are not installed, and no additional game test is needed yet.

## Direct bounds investigation

Identified the installed rigid-dynamic bounds getter (PhysX RVA 0x4df30), its shape-union helper and pose getter. The public getter uses buffered actor poses; reading it during a physics callback alone does not establish current simulation clearance. Evidence and next simulation-owned-bounds route are in docs/direct-actor-bounds.md. No C++/installed binary changes or new game test in this investigation.

## Local simulation bounds decoder

Mapped the installed simulation-owned shape bounds path and added a checked snapshot decoder. All 15 Release suites pass. It is not connected to live memory or restoration: two-shape bounds do not establish complete-vehicle separation, and callback-time freshness still needs verification. Installed 0.20.0 remains shadow-only; no new game test needed. See docs/simulation-pair-bounds.md.

## Local actor-shape coverage

Extended the simulation decoder to union every shape in each contact actor's linked inventory. The regression test catches overlap missed by checking only the contact's two shapes. All 16 Release suites pass. Coverage is per actor, not yet complete multi-body vehicle coverage; array-length proof and callback timing remain open. Installed 0.20.0 is unchanged. See docs/simulation-actor-bounds.md.

## 0.21.0 prepared — physics callback bounds reader

Added a guarded PhysX dispatch hook and bounded simulation-pair/actor reads within its thread-local scope. Verified bounds-array length/capacity evidence and added exact code guards. Memory is copied through ReadProcessMemory; the probe samples at most 16 pairs and never sends suppressed flags to the engine. Final Release build and all 16 test suites pass, including the layout guards. See docs/physics-phase-probe.md. Runtime validation is required for the new hook. Game was running at installation check, so deployment awaits exit. Gameplay ghosting and fading remain disabled; suppression exists only as a local/shadow model.


Installed 0.21.0 after game exit and verified SHA-256 against the tested Release DLL. The previous DLL is backed up. Physics callback validation is pending the next traffic-contact run.

## 0.21.1 traffic-contact crash fix

Two local minidumps (00:46:16 and 00:49:16) show PhysX3_x64.dll+0x12e837 writing at address 0x10. The dispatcher reached the alternate tagged-slot branch with a null masked pointer and pair flags 0x3e61. The earlier crash predates 0.21.0 installation. A stale slot or an ID from another scene is consistent with this evidence. The exact route to the wrong slot remains unresolved.

Removed all plugin-generated status submissions to PhysX. The original callback retains its normal behavior. The plugin can sample a candidate locally, then returns false without writing a pair ID or flags. The registry's legacy echoed counter now counts local selections only. DNC_STATUS_COUNTS updates must be zero. This avoids submitting even an unchanged-flags update for an invalid ID. Live suppression and fading are pending. Test the same low-speed contact sequence and inspect the phase samples and shutdown log.

Installed 0.21.1 and verified the DLL hash. All 16 Release test suites passed. In-game contact retest pending.

## 0.22.0 playable traffic ghosting pilot

0.21.1 retest ended cleanly with zero engine status updates. Nine of sixteen phase samples decoded actor bounds, seven were rejected, and seven diagnostic bindings remained at exit. No newer crash report appeared. This motivated avoiding the deferred ID path altogether.

Installed and hash-verified the first build that changes gameplay. On initial filtering of an eligible player-driven vehicle and non-owned traffic chassis, it changes the current call's observed 0x3e61 flags to 0x3e60. PhysX owns the resulting shape-pair lifetime. A new or refiltered interaction is classified again. No modified proxy masks or numeric pair-ID writes are needed. The build skips the previous status/phase hooks. Driver lease, mount epoch, ownership and both dynamic attributes gate the write. Shutdown logs DNC_GHOST_END with the number of changes. All sixteen Release suites pass.

Playtest pending. An unidentified vehicle on first approach, a stale classification, different shape flags, or separate attachments can still collide. Damage, sounds, refiltering while overlapping and leaving a vehicle mid-overlap have not been validated. This pilot has no live disable switch. NPC transparency is still pending. Test slow traffic pass-through and wall response before evaluating additional cases.

Published the playtested 0.22.0 source and installable ZIP as GitHub prerelease v0.22.0. The user confirmed NPC traffic pass-through and solid barrier response. Release page https://github.com/DarkLordLeto/Cyberpunk2077DriveGhostingModAndDrivingImprovements/releases/tag/v0.22.0.

## 0.23.0 traffic contact and visual-target test

Broadened the ghosting filter to accept ordinary detected solver contacts across reporting-flag combinations. Eligibility still requires the verified driver, identified non-owned traffic, and two dynamic actors. Contacts with a custom modification flag retain their game behavior. The mod clears only the solver bit during pair creation.

A successful ghost contact now queues the two vehicle identities for the game thread. It tracks up to eight traffic targets, reads chassis bounds at 50 ms intervals and records overlap entry and exit with a small separation margin. DNC_FADE_TARGET reports the desired opacity. DNC_FADE_INVENTORY and DNC_FADE_PROXY inspect a bounded sample of the target's visual components to identify a per-instance renderer path. These diagnostics make no material writes. The native SetProxyDissolveVisibility command carries two bytes through a render proxy virtual call. The nearby candidate handler uses flags. A continuous opacity setter and original-value restoration still need verification. All 16 Release suites pass. Test with a slow pass-through and a short overlap to capture target meshes on the active game build.

Installed 0.23.0 locally and verified the DLL hash. In-game validation pending.

## 0.23.1 mesh capture fix

The 0.23.0 playtest recorded 628 ghosted shape contacts, multiple overlap entries and exits, and clean shutdown. Mesh inspection had no caller. Connected it to game-thread traffic-owner refresh with the target identity and attached entity handle. Verified the compiled owner path references InspectFadeTarget and the linked DLL contains its inventory logger. All 16 Release tests passed. Installed and hash-verified 0.23.1 for one live mesh capture. Actual transparency remains pending.

## 0.23.2 vehicle renderer capture

The 0.23.1 test captured one NPC for about six seconds, 79 visual components, 39 suppressed shape contacts and clean shutdown. Eight component classes were sampled, including body_01 and chasis. The direct-pointer scan found zero candidate proxies. The installed game-side UpdateVisibility routine at 0x402428 loads the renderer through a reference at wrapper+0x10. This build samples up to three components per mesh family, records component dispatch identity, and checks both direct and wrapped paths. It logs alternate handler addresses for selected fields seen in native visibility callers. These are diagnostic candidates. Visible half-opacity still requires a verified material or render command. All 16 Release suites passed and the installed DLL hash matched. One slow overlap with an NPC car is sufficient for the next capture.

## 0.24.0 first-contact test

The 0.23.2 user test reported an initial bump on overlap. Several traffic proxies were first discovered through contact. Broad queries frequently returned 128 entities. The renderer probe also found seven candidate paths on sampled cars. The new driver-only discovery cycle interleaves 18-metre proximity scans with 60-metre scans and publishes the resulting classification immediately. DNC_FIRST_CONTACT records the first queued ordinary dynamic contact per proxy per mount (up to 256) and whether it was suppressed. Eligibility and solver flag policy are preserved. A missed pre-contact discovery and a game response to retained collision reports are distinct possible causes. The driving retest is needed to distinguish them. All 16 tests passed. Installed and hash-verified locally.

### 0.24.0 driving feedback

The user confirmed that the initial bump was fixed in the driving retest. During combat, some vehicles still collide. Track combat ghosting as a future toggle. Current opacity work is independent of that decision.

## 0.25.0 opacity material test

The 0.24.0 capture identified render table 0x3059ab0 on body panels, glass and chassis, using wrapper+0x10. The dissolve render method at +0x30 changes flags. Reviewed consumers at 0x32fca8 and 0x3309bc route those flags into visibility and LOD selection. They do not establish a numeric half-opacity input. The separate SetMaterialParameterOverrides command at 0x948764 copies a structured payload and submits it to the renderer. This build resolves a sampled NPC mesh through its reflected resource path and an existing resource token. It inspects matching appearance slots, loaded material chains, mask capability and candidate scalar names/defaults. It limits sampling to 24 mesh/appearance combinations and makes no material writes. The resulting shader names and parameter paths are needed to select a reversible per-instance fade. Actual visible fading remains pending. All 16 Release tests passed.

Installed 0.25.0 locally with a verified DLL hash and backup. Runtime material capture pending.

## 0.26.0 first visual ghost prototype

The 0.25.0 run identified an unmaskable multilayer chassis shader and a loaded vehicle_glass window shader with Opacity and OpacityBackFace. During overlap this prototype creates a private material instance from the window material's parent and parameter overrides, setting both opacity parameters to 0.5. It requests render data through the guarded engine helper, then queues the per-proxy material replacement for eligible body and skinned meshes. This substitutes a glass-like appearance during overlap. Exact perceived transparency, material compilation and coverage require an in-game test. Original textures are expected to reappear when the override clears. This first visual build switches on overlap boundaries without a timed ramp.

Existing component overlay references and shadow-named meshes are skipped. Cleanup on separation, mount change, prolonged missing bounds or shutdown checks weak component identity, original wrapper and renderer before clearing the override. DNC_GLASS_APPLY and DNC_GLASS_RESTORE log mesh counts. DNC_GLASS_SKIP identifies unavailable source material or render data. Native getter, release, override and caller byte ranges are guarded. All 16 existing Release suites passed. They do not exercise game-side material creation. Combat behavior remains deferred per the user's request.

Installed 0.26.0 locally and verified the DLL hash. Visual playtest pending.

## 0.27.0 exterior-only ghost test

The user confirmed the 0.26.0 body became transparent. In the log, restore submission coincided with detected separation. Active overlaps now sample every game update with zero extra clearance margin and release an applied appearance on missing bounds. Axis-aligned chassis boxes and renderer queue timing can still affect visible restoration, especially at angles.

During overlap, recognized exterior panel and wheel mesh components receive the 0.5 glass override. Other supported mesh components are temporarily hidden through the guarded per-instance visibility setter. The mod records only meshes whose visibility bit was originally enabled and restores them when retiring the target. Classification uses asset-name tokens, with interior/accessory exclusions taking priority. Individual submeshes bundled into a single exterior component and separately owned occupants are outside this component-level selection. Name classification has regression cases for observed panels, wheels, steering wheels, seats, windows and accessories. All 16 Release suites passed. Visual verification pending.

Installed 0.27.0 locally and verified the DLL hash.

## 0.27.1 missing glass fallback

The final NPC in the 0.27.0 test, entity 10032743, entered overlap twice and both attempts logged reason=material. Adjacent cars applied and restored normally. Added a cache for a private ghost material that has yielded valid render data, plus an asynchronous request at mount for the vehicle_glass parent resource identified in the 0.25.0 capture. A missing target window can now use either fallback. Material failures retry at most once per 300 ms during overlap. Apply/skip logs include entity IDs and material source where available. The exterior mesh policy and visibility cleanup are unchanged. All 16 Release suites passed. The fallback requires an in-game retest.

Installed 0.27.1 locally and verified the DLL hash.

## 0.28.0 borrowed cars and motorcycles

The 0.27.1 run recognized the Kusanagi rider and the borrowed pickup driver, including a delayed transition from passenger to driver in the pickup. Some eligible traffic contacts were suppressed before the end of that run. The report does not establish one cause for every impact. Driver identity now comes directly from the attached mounted entity on each seat poll and is immediately published. Nearby vehicle handles also refresh traffic facts directly from inspected chassis. Verified borrowed-car and motorcycle drivers request PhysX eSUPPRESS on eligible dynamic traffic pairs at creation to prevent both contact solving and game contact reports. World, passenger, unclassified and nondefault-filter cases retain original behavior. Combat-specific rules remain deferred. All 16 Release suites pass with policy cases for the new path. In-game tests of both vehicle types are pending.

Installed 0.28.0 locally and verified the DLL hash.

## 0.29.0 universal ghost glass and motorcycle bodies

The user confirmed borrowed-car ghosting in 0.28.0. Kusanagi chassis contacts with two traffic cars were classified and suppressed. The logged rejected first contact during that ride was an ElectricLight. The remaining motorcycle collision source is unverified. Filtering stays pair-specific so streets, walls, NPC-to-NPC interactions and pedestrians keep their normal response.

All visual targets now use one private 0.5-opacity material created from fixed depot resource 1ecc2b2ba357fdf7. The plugin preloads it asynchronously while mounted. Removed the per-target window and material-slot search. Components still need a recognized renderer and shell classification. A missing donor is logged and retried during overlap.

While riding a bike, dynamic/kinematic pair observations can seed a bounded game-thread owner lookup. Secondary bodies map to the bike only when their owner matches the mounted bike or its driver. Another character must resolve via GetMountedVehicle to a verified non-owned traffic vehicle. Bindings expire after 300 ms and are revalidated. The callback rechecks mount, vehicle identity, driver lease and normal traffic eligibility before suppressing a contact. Static and trigger objects remain excluded. Newly discovered bodies may miss their first contact, and scene queries outside the pair callback are outside this change. DNC_BIKE_BODIES logs the number of extra and character pairs handled. All 16 Release suites passed including mount, expiry, static and trigger regression cases. Runtime confirmation pending.

Installed 0.29.0 locally and verified the DLL hash.

## Hella cornering trial

The local car handling trial uses TweakXL 1.12.0 and a separate preset for V's Archer Hella. Its cloned wheel presets raise lateral friction from 0.89 to 0.94 at the front and from 0.87 to 0.95 at the rear. The larger rear increase aims to make throttle-on slides easier to catch. The starting values and record IDs were read from the installed patch 2.5 database. Driving feel and runtime loading await an in-game test. To compare stock behavior, close the game, move DNC-Hella-Cornering.yaml out of r6/tweaks, restart and summon the same car. Motorcycle development is paused.

## 0.29.1 local Hella test
TweakXL imported the first Hella grip trial. The driver reported little cornering difference, lower perceived top speed and an NPC collision. The session included a Riptide and Hella, with 133 eligible contacts and zero full suppressions. Owned cars now use full suppression for recognized traffic contacts, as borrowed cars already did. The local grip preset is disabled for a stock-handling comparison. All 16 Release suites passed. In-game confirmation is pending.

## 0.29.2 Hella collision diagnosis
The 0.29.1 drive recorded 148 fully suppressed shape pairs but the driver still felt a bump. The local diagnostic watches public scene raycasts and sweeps for short downward blocking hits against the most recently suppressed traffic actor within 1.5 seconds of contact. It logs caller and target data and forwards the original query results. Batched queries and other targets are outside this sample. Stock Hella grip is active. All 16 Release suites passed.

## 0.29.3 affected player cars

User-reported collisions while driving these cars during incomplete fleet testing.

- 赫拉 EC-D I360
- 田螺熊
- 田螺T400
- 66式 克苏鲁
- 苏普隆 创界
- 麦基诺 造物巨匠
- 麦基诺 巨兽
- 麦基诺 巨人柱
- 梅里马克 魔术师
- 德利翁 辩护
- 魔犬

The 0.29.2 session recorded 9,344 fully suppressed pairs. Five sampled downward ray hits matched the most recently ghosted actor. Those hits occurred while driving other models, so they do not establish the cause for the reported cars. Public query coverage excludes batched queries.

0.29.3 extends the existing owner-verified secondary-body mapping to cars. A secondary vehicle body can resolve to its chassis identity after game-thread verification. Car mappings require a vehicle owner and retain driver, mount, freshness and target eligibility checks. Secondary bodies no longer overwrite the primary chassis identity in the lifecycle map. BODY_BIND and CAR_ALIAS logs distinguish discovered mappings from suppressed pairs.

The mapping is reactive. An already-created contact can survive the first encounter, so repeat a pass after separating fully. This test has not established whether secondary bodies cause the reported bump. All 16 Release suites passed. Installed locally with a hash-verified backup. The Hella handling preset remains disabled.

## 0.29.3 repeat-pass result

The driver reported bumps on both passes. In drivenocollision-2026-11-01-10-43-44.log the driven vehicles were Emperor and Type-66 Cthulhu (appearance quadra_type66__basic_sampson). No secondary-body bindings or alias suppressions occurred. All 48 eligible contacts used full suppression. The sampled public downward queries had zero matches to the most recently ghosted actor.

A rejected first contact at 10:45:13 used proxy 00010227, entity 9021383. Its owner resolved as vehicleCarBaseObject with playerVehicle=1 and trafficSlot=0 at 10:45:11. It is excluded by the current NPC-traffic-only policy. The other recorded rejected dynamic contacts resolved to an ElectricLight and generic mesh entities. Contact with NPC proxy 000102d3, entity 11986906, was eligible and suppressed at 10:45:23. These records identify an owned-car exclusion but do not establish which contact the driver perceived on both passes. Await target confirmation before changing eligibility.

## 0.29.4 traffic decision trace

The user confirmed the repeated bumps were against ordinary NPC traffic. The owned-car rejection therefore does not explain the report. The extra-body hypothesis was not supported by the last run.

Added a separate bounded trace reserved for verified vehicle target identities. It records input/output filter and pair flags, attributes, eligibility, suppression and target identity. Repeated decisions are aggregated by signature and mount. This avoids the original first-64-pairs trace being exhausted by road contacts before the NPC encounter. It can expose unsupported contact flags or eligibility failures associated with an actual vehicle. It does not cover contacts created before observation or separate suspension paths. All 16 Release tests passed, the final rebuild had no compiler warnings, and 0.29.4 was installed with a verified backup. This is a diagnostic build awaiting a short NPC encounter test.

## 0.29.5 overlap continuity

The 0.29.4 Cthulhu drive captured four NPC identities changing from eligible suppressed contacts to ineligible solid contacts within an encounter. Target 11987093 changed at 10:50:02.948 after at least 32 suppressed contacts. Targets 11987108, 11987120 and 11986997 showed the same pattern. The trace dropped no events. It did not record the individual eligibility facts, so loss of the traffic slot remains the leading explanation rather than a confirmed engine event.

0.29.5 allows a previously accepted NPC target with a currently false traffic flag to finish an overlap already observed by the bounds sampler. A 100 ms lease must be refreshed with valid overlapping bounds. Matching mount, driver and target identities, fresh observations, verified driver, non-owned target and vehicle type remain required. Unknown traffic facts fail closed. Separation stops renewal. This also preserves continuity if a traffic slot is lost during a combat transition while already overlapping. A general combat toggle remains pending.

The regression suite checks overlap continuity, new nontraffic rejection, ownership changes, unknown facts and expired driver authorization. All 16 Release suites passed. DNC_OVERLAP_CONTINUATIONS counts use of the new rule. The local DLL and rollback backup were hash verified. In-game confirmation is pending.

## 0.29.6 batched query diagnosis

0.29.5 invoked overlap continuity 14 times and suppressed 36 eligible contacts. The latest trace showed no solid decision for the two recorded NPC targets, but the driver still reported a bump. Continuity addressed one eligibility gap and did not eliminate the reported symptom.

The PhysX scene vtable createBatchQuery slot at 0x20cd90 points to 0x53fd0. Its constructor call at 0x5404d targets 0x2de60, which assigns NpBatchQuery vtable 0x208c48. The first slot is execute at 0x2e020. Disassembly confirms pending ray count +0x30, sweep count +0x38, result buffers +0x68/+0x78 and result stride 0x60 with status +0x58 and hasBlock +0x59. The existing checked PhysX fingerprint and this vtable slot gate the new hook.

0.29.6 observes the original batch results after execute. It checks successful blocking ray and sweep results against the most recently ghosted actor during the existing 1.5-second diagnostic window. Logged range=-1 denotes a batch result with no captured query direction. The log also includes names/classes of components on generic nonvehicle contact entities. No query-result filtering has been added. Touch hits, older targets, and provenance of a matching query remain outside this diagnostic.

Upstream ABI references are NVIDIA PhysX 3.4 PxScene.h, PxBatchQuery.h and PxBatchQueryDesc.h. The installed binary was checked independently for the offsets above.
https://github.com/NVIDIAGameWorks/PhysX-3.4/tree/master/PhysX_3.4/Include

All 16 Release suites passed. Local installation and backup hashes verified. A short Cthulhu/NPC collision run is required to establish whether the vehicle uses matching batch queries.

## 0.29.7 vehicle-part provenance trial

0.29.6 attached its batch hook successfully but observed zero batch executions. The ordinary query probe also recorded zero matching targets. During Cthulhu overlap, solid contacts resolved to generic entities containing mirror_fl and two v_common_nomad_part electric-device meshes. These are vehicle-part components outside the chassis identity path. The log does not establish that they caused every reported bump.

The next trial records weak handles to physical mesh components on the driven car and eligible nearby NPC cars. If the exact same live component later belongs to a generic entity, its new physics proxy can map back to the original vehicle identity. Origins expire after three seconds and are scoped to the mount. Alias authorization still requires current vehicle identities and normal traffic eligibility or a verified continuing overlap.

This only handles transferred components. If the game clones a component during detachment, matching fails closed. The first contact can also precede game-thread resolution. Part names alone never authorize collision suppression. BODY_BIND and CAR_ALIAS logs will show whether the trial matched actual parts.

All 16 Release suites passed and 0.29.7 was installed with a verified backup. Test Cthulhu against one ordinary NPC, separate fully and repeat once, then exit. No in-game fix is claimed yet.

## 0.29.8 earlier part discovery and traffic provenance

The user clarified the first encounter climbs onto a transparent vehicle, while the second encounter is fully solid. The 0.29.7 log confirmed exact component provenance matches, but many part bindings appeared after contact. Target 11987137 also repeatedly changed from eligible to ineligible after separation.

Attached physical-mesh components are now queried through the signature-checked CreatePhysicalBodyInterface method. The opaque interface payload supplies candidate IDs only. A candidate is accepted only when the guarded engine owner lookup returns the exact same live component. Valid part IDs enter the existing bounded game-thread alias resolver before contact when available. This API path needs runtime validation.

Car traffic classification now remembers a positive traffic observation for the same continuously refreshed vehicle identity. False traffic-slot observations preserve that provenance. Unknown facts, ownership changes, identity changes, expired observations and reset revoke it. Newly encountered nontraffic cars retain original collision policy. This also allows a previously verified traffic car to stay ghostable through combat or other traffic-slot changes while its identity remains fresh. A user-facing combat toggle is still pending.

The first-pass climb has not been proven to originate from these parts. Ordinary query tracing remains limited to short downward blocking hits against the most recently suppressed actor, and batch tracing saw zero calls in 0.29.6. A successful unit run cannot establish runtime physics coverage.

All 16 Release suites passed, including provenance loss, ownership and expiry regression cases. Installed 0.29.8 on 2026-11-16 with a hash-verified rollback backup. Test the same NPC twice using Cthulhu, reporting first-pass climbing separately from second-pass solidity.

## 0.30.0 wheel query pass-through (installed, confirmed)

The 0.29.8 run on 2026-11-16 (Emperor, then Type-66 Cthulhu) still climbed onto NPC cars. Its log rules out chassis contacts: every traffic decision for the eight NPC targets was eligible and suppressed (419 contact-free pairs, 40 overlap continuations, no solid decision). The only unsuppressed vehicle contacts were against V's own parked car 9021383, which the NPC-only policy excludes by design. So the remaining support comes from scene queries, not from rigid-body contacts.

The earlier query diagnostic could not see this. It only counted a hit when the result buffer held a blocking hit on the last ghosted actor. It checked 10,362 short downward queries in that run and matched none, which fits wheel probes that collect touches or use their own hit callback.

0.30.0 changes query results instead of observing them. While the player drives, each recognized traffic proxy with a suppressed contact enters a short-lived ghost set (1.5 s per new contact, renewed by 400 ms every frame while the chassis bounds overlap). The existing PxScene raycast and sweep hooks now pass PhysX a stack-local prefilter that returns "no hit" for actors in that set and defers every other shape to the game's own filter callback. With no game prefilter it returns the hit type PhysX would have used. A query cache entry naming a ghost is dropped, since PhysX skips filtering for cached shapes. Outside the ghost window the hooks forward the original arguments untouched.

Verified in the installed PhysX3_x64.dll before writing this: raycast 0x60890 and sweep 0x60960 argument layout, filter flags at filterData+0x10, the prefilter call through vtable slot 0 at 0x61d89 with (filterData, shape, actor, hitFlags), and the default hit type at 0x61c27-0x61cc1 (touch when the hit buffer accepts touches or the reserved flag is set, otherwise block). Actors are matched by the proxy ID in PxActor::userData at +0x10, the same field the pair callback reads.

Side effects inside the window: every ray and sweep in the scene ignores the ghosted car, not only the player's wheels. Camera collision, bullets and NPC line-of-sight pass through it for that moment. Motorcycles now use the same path, since their unexplained collisions may share this cause. Overlap queries, batched queries and direct PxGeometryQuery calls are not covered. NPC cars reacting to the player's chassis through their own probes are not covered either.

Shutdown logs DNC_QUERY_GHOST_END with filtered query and dropped hit counts, and one DNC_QUERY_GHOST_CALLER line per game call site (up to 16) with its query flags. Zero dropped hits during a climb would mean the wheels use an uncovered path.

New header src/QueryGhostFilter.hpp and suite tests/QueryGhostFilterTests.cpp (17 suites total). The new suite passed under g++ and CarQuery.cpp passed a syntax-only check against stub headers. This session could not run MSVC, so the plugin DLL and the other 16 suites have not been built or run for 0.30.0, and nothing is installed. Run tools/Build-Install.ps1 with the game closed. It builds Release, runs all suites, backs up the installed DLL and verifies the copied hash. Then repeat the Cthulhu test: drive through one NPC car slowly, separate fully, repeat once, and exit.

### 0.30.0 result

Built and installed locally on 2026-11-16 with tools/Build-Install.ps1. The user confirmed the climbing and collision problem is solved after a drive in the Emperor, Type-66 Cthulhu, Kusanagi and Semimaru. The log (drivenocollision-2026-11-16-20-23-30.log) recorded 634 contact-free pairs, 182,798 filtered queries and 4,920 dropped hits on ghosted traffic. The call sites, as game RVAs, were sweep 0xac92a5 (3,843 hits, touch buffer, the likely wheel probe), ray 0x3ddda9 (873), ray 0x3dcce1 (189) and sweep 0x615703 (15). All four already passed the game's own prefilter callback with flags 0x000f. Whether the Kusanagi's earlier unexplained collisions are also gone was not reported separately.

Next reported problem: a lag or glitch when entering or leaving a vehicle. The same log shows no stall around mount or unmount: gaps between plugin log lines stay under the normal polling cadence, and the mount-time diagnostic dump completes within about 40 ms across several frames. Cause not identified yet.

## 0.30.1 lighter mount frame and update timing (source ready, awaiting local build)

The user described the enter/exit problem as a camera or animation jump and said earlier mod builds had it too. It has not been compared against the game without the plugin, and vanilla has its own camera cut when mounting, so the plugin is a suspect, not a confirmed cause.

What the 0.30.0 log does show is extra game-thread work on the exact frame a mount is detected. The native chassis inspection alone spans 9 ms between two of its own log lines (20:24:13.164 to .173), on top of the first nearby scan and the first part scan. That is one or two dropped frames at the start of the enter animation. Nothing comparable is visible at unmount, where the plugin only resets state.

Changes:
- The per-mount research dump (component list, native chassis and group table read, shape scan, resource log) is compiled out behind MountDiagnostics in LiveProbe.cpp. DNC_LIVE_VEHICLE is still logged.
- The startup reflection dump (about 4,250 lines) is compiled out behind TypeDump in Main.cpp.
- ObserveVehicleParts matches known parts by stored pointer instead of locking up to 512 weak handles per component, and repeats the CreatePhysicalBodyInterface script call at most once a second per live part instead of on every 100 ms poll and nearby scan. Pending body requests are still pushed on every refresh, so alias renewal is unchanged. Nearby scans in the 0.30.0 run averaged 2.6 ms and peaked at 12.7 ms; this should lower that.
- New DNC_SLOW_UPDATE pollUs= drainUs= line (up to 200 per session) whenever the plugin's own per-frame work reaches 3 ms. If the jump remains, these lines show whether the plugin was busy on the mount or unmount frame.

Collision, query pass-through and visual logic are unchanged. These edits could not be compiled in this session (no MSVC). Run tools/Build-Install.ps1 with the game closed, then enter and leave a car a few times. If the jump is still there, move DriveNoCollision.dll out of red4ext/plugins once and repeat, to establish whether the unmodded game does the same.

### 0.30.1 result

Built and installed. The user reported the problem persists and described it as V's point of view twitching while entering and leaving. No comparison without the plugin was reported.

The new timing lines identify heavy plugin work on the game thread for the whole time the player is mounted (drivenocollision-2026-11-16-21-04-13.log). Every 100 ms poll cost 4.2 to 4.7 ms from the first mounted frame, with an empty nearby scan (0.02 ms), and once traffic was near the per-frame drain cost 5 to 35 ms. The 200-line cap was used up within six seconds of the first mount. A periodic multi-millisecond stall during the enter and exit animations is consistent with the twitch.

## 0.30.2 car part aliasing off (installed)

The cost comes from the car secondary-body and detached-part machinery added in 0.29.3, 0.29.7 and 0.29.8: the per-poll part scan of the driven car and each nearby traffic car, about 40 body requests per car, and re-resolution of every alias as it nears its 300 ms expiry (owner lookup, two script calls and a component scan each). That work was a hypothesis for the climbing problem, which the 0.30.0 query pass-through solved instead. In every recent run, including the confirmed 0.30.0 drive, it suppressed nothing: DNC_BIKE_BODIES extraPairs=0 and no DNC_CAR_ALIAS lines.

Changes:
- CarPartAliases=false in PairProbe.cpp turns off ObserveVehicleParts and the car branch of the pair-callback alias lookup. Chassis-to-chassis suppression, query pass-through, overlap continuity and traffic provenance are untouched. The motorcycle body mapping still runs while riding.
- DNC_SLOW_UPDATE is now one summary per second that contained a frame of 2 ms or more: frames, slowFrames, maxPollUs, maxDrainUs, totalUs.

Risk: a detached NPC part (mirror, bumper) lying in the road is solid again, as it was before 0.29.7. Not compiled in this session. After building, check that entering and leaving are smooth and that passing through traffic still works.

### 0.30.2 result

Built and installed. The user reported the twitch is slightly better but remains. Ghosting still worked (408 contact-free pairs, 4,212 dropped query hits, extraPairs=0) in drivenocollision-2026-11-16-21-19-32.log.

The summaries show the plugin is still expensive while mounted. The update runs 36 to 45 times a second. Each 100 ms poll still costs about 5 ms, including while V is a passenger or still entering (isDriver=0, so no nearby scan runs), and the drain peaks at 2.5 to 16 ms with traffic near. That is 4 to 29 percent of each second spent in the plugin on the game thread. The drain cost fits the owner refresh, which re-resolves up to 16 known proxies every 50 ms with several script calls each. The 5 ms poll cost is not explained by reading the code: what remains there is four script calls, a guarded chassis read and small bookkeeping.

## 0.30.3 parked throttle and section timing (installed)

- Parked mode: the driven vehicle's speed is read each poll through the guarded chassis velocity getter (new ReadVerifiedChassisSpeed). Under 0.5 m/s for 500 ms on the same vehicle, the poll interval goes from 100 to 250 ms, nearby scans from 150 to 600 ms, and the owner refresh from 50 to 400 ms. Any poll that sees 0.5 m/s or more restores full rates at once. This covers the enter and exit animations and standing still. Classification of traffic that approaches a parked car can lag by up to 600 ms.
- Section timing: new src/UpdateProfile.hpp accumulates time per section, and each DNC_SLOW_UPDATE second is followed by DNC_UPDATE_SECTIONS (getPlayer, getVehicle, driverFlag, chassis, ownedFlag, driverState, nearby, audit, bodies, fade, discovery, ownerRefresh, in microseconds for that second).

This lowers how often the stalls happen while entering and leaving. It does not remove the per-poll cost, so the twitch may be reduced, not gone. The section line is what identifies the remaining cost for a real fix. Not compiled in this session.

### 0.30.3 result

Built and run (drivenocollision-2026-11-16-21-34-18.log). The user did not report on the twitch for this build. The section totals for the session name the cost: chassis 12.9 s, ownerRefresh 7.2 s, fade 5.4 s, nearby 3.4 s, and every script call section under 0.03 s. The chassis section is one component lookup plus the guarded reads and costs about 4.5 ms per poll (18 ms per second at four parked polls, 44 ms at ten). The script calls were never the problem.

All four expensive sections share the guarded readers in NativeProbe.cpp, which call CheckImage (about 13 byte-range comparisons against the executable) and Readable (a VirtualQuery walk) on every read. VirtualQuery on an address inside a very large heap region is the likely multi-millisecond cost. Which of the two dominates was not measured separately.

## 0.30.4 cheap guarded reads (source ready, awaiting local build)

- CheckImage now verifies the executable's byte guards once per image base and returns the cached result afterwards.
- Readable copies through ReadProcessMemory in 256-byte chunks instead of walking regions with VirtualQuery. It still fails cleanly on unreadable memory.

No behaviour change is intended. This should remove most of the per-poll, fade, nearby and owner-refresh cost, and with it the periodic stall behind the enter/exit twitch. Parked mode and the section timing stay in. Not compiled in this session; confirm with the DNC_UPDATE_SECTIONS lines after the next run.

## Race mode 0.1.0 (installed, untested in game)

Requested 2026-11-16: pick a start and end point on the map (the same point makes a loop), add or remove extra points to shape the circuit, then enter a race mode like the game's own car race missions with automatically generated checkpoints. A hotkey leaves the mode. While it stays on, every run from start to end is timed as a lap.

Decisions made with the user: redscript + Codeware as the toolset, the game's GPS route as the preferred way to lay out checkpoints with straight lines as the fallback, map waypoint plus hotkey for picking points, and time trial only (no rival cars) for the first version.

### What was installed in the game folder

- redscript 0.5.32 (engine/config/base/scripts.ini, engine/tools/scc.exe, engine/tools/scc_lib.dll, r6/config/cybercmd/scc.toml). SHA-256 of the release zip: 882c57a80b07e453bc8a34cd8430f460aceed789581de8743638e19dcfb7936f.
- Codeware 1.21.4 (red4ext/plugins/Codeware). SHA-256 of the release zip: cb929d1edb72e23d7e7cc6420014b6557c363992ed6092c8453be9b185d0376b. Its README lists Cyberpunk 2.5, redscript 0.5.32+ and RED4ext 1.31.0+.
- r6/scripts/DriveRaceMode/RaceMode.reds, copied from scripts/DriveRaceMode/RaceMode.reds in this project.

Both zips are kept under dependencies/. No existing game file was overwritten. To remove everything, delete the files listed above and the folder r6/cache/modded.

### How it works

Keys (they also work on the map screen): F6 set START, F7 set END, F8 add an extra point before the END, F10 remove the last thing added, F2 clear the circuit, F11 enter or leave race mode. Each key takes the player's own map waypoint, or the car's position when no waypoint is placed. The circuit is shown with map markers and is saved with the game.

A missing END, or an END within 25 m of the START, makes a loop; a loop needs at least one extra point. Entering race mode needs a vehicle and is refused while a story race is running. The player's points become gates (14 m radius, must be driven through in order). Between gates, extra checkpoints are placed every 150 m along the straight line; they have a 35 m radius and count as passed when a later checkpoint is reached, so a bend in the road cannot block the race. The next three checkpoints are shown as markers.

The HUD is the game's own race HUD (hudCarRaceController). The script queues VehicleRaceQuestEvent to the driven vehicle (PreRaceSetup, CountdownStart, RaceStart, RaceEnd, Disable) and drives the checkpoint counter through the fact sq024_current_race_checkpoint_fact_add, whose previous value is restored on exit and after loading a save made in race mode. The lap clock starts when the car passes the START. On a point-to-point circuit the lap ends at the END and the next lap starts at the next pass of the START. On a loop each finish starts the next lap. Lap, last and best times are shown as on-screen messages and written to the redscript log with the tag [DriveRaceMode]. Leaving the vehicle or pressing F11 ends race mode.

### Verification so far

The game's script cache (r6/cache/final.redscripts) was decompiled with a locally built redscript-cli 0.5.32, and every game function, event, enum and fact name the script uses was read from that dump. RaceMode.reds compiles without errors against that cache together with the Codeware 1.21.4 scripts. Nothing has been run in the game yet.

### Known limits and open items

- GPS route: the script layer exposes no road route. gamegpsGPSSystem has no script functions, the minimap path widget is native, and the navigation system only paths on the loaded pedestrian navmesh. So version 0.1.0 uses the straight-line fallback. Reading the GPS route would need native work in the C++ plugin (RunGPSQuery and UpdateGPSQuery exist as native names in the executable) and has not been investigated further.
- Whether GetManuallyTrackedMappinID returns the player's custom waypoint is unconfirmed; the script also scans the map markers for the custom waypoint variant.
- Marker look (Mappins.QuestStaticMappinDefinition with the racing variant), marker height at waypoint positions, the HUD position counter (expected to read 0/1) and the countdown animation need a look in game.
- The game's HUD clock ticks for at most 600 seconds per lap.
- Keys F6, F7, F8, F10, F11 and F2 were chosen because the game's own bindings use F1, F3, F4, F5 and F9.

### Race mode 0.1.0 result and 0.1.1 (superseded by 0.1.2)

First run, 2026-11-16 22:20: the game started normally with redscript and Codeware, the redscript log shows RaceMode.reds compiled, and RED4ext loaded the modded script blob. The user reported no markers on the map, no on-screen message from F6, F7 or F8, and nothing from F11 in a car. Nothing written by the script was found in any log, so it is not known whether the system attached or the key callback fired.

Two definite faults were found by reading the game's scripts again:
- Messages went to the UI_Notifications.OnscreenMessage blackboard entry. The game's own scripts show HUD text through UI_Notifications.WarningMessage (PlayerPuppet.SetWarningMessage). 0.1.1 uses WarningMessage.
- Markers used Mappins.QuestStaticMappinDefinition, which is meant for journal-backed quest markers, and the racing variant, whose world-map tooltip reads a journal entry. 0.1.1 uses Mappins.DefaultStaticMappin (present in tweakdb.bin) with the exclamation-mark variant for START and END and the question-mark variant for extra points while editing. Race checkpoints in the world keep the racing variant.

0.1.1 also makes the next run diagnostic. The key callback is registered without key targets or a run mode and filters the six keys itself. Eight seconds after a save loads, the script shows "Race mode ready: ..." on screen. Attach, the ready message, each race key press and every message are also written with FTLog and Codeware's ModLog under [DriveRaceMode]; where those end up on this install is not known yet. Compiles cleanly against the game cache and Codeware 1.21.4 scripts.

What the next run tells us: no "Race mode ready" text means the system or the message channel is still not working; the text appears but keys do nothing means the Input/Key callback is the problem; keys give messages but the map shows no markers means the marker definition or variant is still wrong.

Finding: this game build has the callback system built in (gameCallbackSystem, gameInputTarget and gameKeyInputEvent are in the executable and partly declared in the unmodded script cache), which Codeware's scripts bind to.

### Race mode 0.1.2 (installed, confirmed)

The user's second "no markers" report (2026-11-17) came from the 22:28 run, which still used 0.1.0: the redscript log shows the 22:28 compile and the 0.1.1 file was written at 22:35. 0.1.1 itself was never run.

Root cause of the missing markers, read from the installed r6/cache/tweakdb.bin: each mappin definition only accepts the variants in its possibleVariants list.
- Mappins.QuestStaticMappinDefinition (used by 0.1.0) accepts DefaultQuestVariant, QuestGiverVariant and others, but not ExclamationMarkVariant or Zzz18_RacingVariant.
- Mappins.DefaultStaticMappin (used by 0.1.1) accepts only DefaultVariant, so the exclamation and question-mark variants of 0.1.1 were invalid too.
- Mappins.DefaultStaticMappin has showOnMap, showOnMinimap and showInWorld all true. DefaultVariant draws the "quest" icon (MappinUIUtils.MappinToTexturePart).

0.1.2 registers every marker (START, END, extra points and race checkpoints) as DefaultStaticMappin + DefaultVariant, so all of them use the same icon. Points are told apart by the on-screen messages. The edit markers are also refreshed when the ready message appears, in case a restore ran before the map system was ready.

0.1.0 had a second fault: the Input/Key callback used CallbackRunMode.OncePerTarget, and Codeware's handler removes a target after its first matching event, so each hotkey could fire at most once per session. 0.1.1 already dropped the run mode. 0.1.2 also matches Codeware's documented callback form (no return value). The WarningMessage channel from 0.1.1 is kept; WarningMessageGameController exists and the game's own scripts write to that channel in many places.

Compiles cleanly against the game cache with Codeware 1.21.4. Expected in game: about 8 seconds after a save loads, "Race mode ready: F6 start, ..." appears; each race key shows a message; markers appear on the map, minimap and in the world.

Tooling: tools/tweakdb-flats.py decodes every flat in tweakdb.bin (blob version 8, parser 4) into a pickle keyed by TweakDBID. Format notes: each type section is a u32 value count, the values, a u32 key count and 12-byte keys (TweakDBID, value index); strings and array lengths use the engine's compact integer (6 value bits plus sign and continuation in the first byte, then 7 bits per byte). The value counts in the header table do not always match the stored counts; the key counts do. Usage: python3 tools/tweakdb-flats.py <tweakdb.bin> <out.pkl>.

0.1.2 result (2026-11-17): the user confirmed the system works (keys, messages and markers). It did not yet feel like the game's car race missions: no navigation and no visible checkpoints.

### Race mode 0.2.0 (installed, partly confirmed)

How the game's own races work, from the script cache: RacingSystem only exposes IsRaceInProgress and IsAIVehicleRegistered; races are quest-driven. The quest sends VehicleRaceQuestEvent to the vehicle's persistent state (VehicleComponentPS.OnVehicleRaceQuestEvent), which forwards ForwardVehicleRaceUIEvent to the HUD (hudCarRaceController), pushes or pops UIGameContext.VehicleRace, and passes the event on to the vehicle, which runs the race clock. The checkpoint counter reads the fact sq024_current_race_checkpoint_fact_add and the position counter reads UI_ActiveVehicleData.PositionInRace.

0.1.x queued the event to the vehicle entity directly, which only starts the clock; the HUD was never pushed. 0.2.0 sends it through PersistencySystem.QueuePSEvent to VehicleComponentPS like the quest does, sets PositionInRace to 1, and sends CountdownStart one second after PreRaceSetup.

Visible checkpoints: Codeware's DynamicEntitySystem spawns the game's race checkpoint posts, base\gameplay\devices\street_signs\race_checkpoint\race_checkpoint_left.ent and race_checkpoint_right.ent (paths from Codeware's KnownHashes.txt; the posts are street-sign devices driven by RaceCheckpoint records through RaceCheckpointSelector). Each checkpoint gets a left and a right post 13 m apart across the direction of travel, turned to face the oncoming car. Posts exist only for the START and the next three checkpoints within 350 m, so the ground under them is loaded; their height comes from Static and Terrain raycasts, starting from the car's height when the waypoint height is more than 25 m off. Passed posts are removed. All posts carry the tag DriveRaceGate, are not saved, and are deleted on exit and when a session's ready message appears.

Navigation: the scripted quest API that the game's GameplayQuestSystem uses (JournalManager.CreateScriptedQuestFromTemplate and friends with the generic_gameplay_quest template) is stubbed out in this build: each function returns false. Instead, race mode takes over the player's own map waypoint (the tracked waypoint, else any placed waypoint) and moves it with MappinSystem.SetMappinPosition to the START while waiting, then to the next checkpoint. Within 70 m of that checkpoint it moves one checkpoint further, so the car never reaches the waypoint itself in case the game removes waypoints on arrival. The waypoint's original position is restored on exit. Without a placed waypoint, race mode runs without GPS directions and says so. If the waypoint disappears, a message explains how to restore it.

Map markers: only the next two checkpoints keep a map marker now; the posts and the GPS carry the rest.

Unverified until tested in game: that the GPS route follows a moved waypoint, how the left and right posts look and which way their display faces, post height on ramps and bridges, and whether the race HUD and countdown now appear. Compiles cleanly against the game cache and Codeware 1.21.4.

0.2.0 result (2026-11-17): the user confirmed the race HUD and the checkpoint posts work. There was no GPS route line at all during the race.

## 0.31.0 plugin and race mode 0.3.0: tracked checkpoint marker (plugin source ready, race script installed)

Why moving the waypoint failed: script MappinSystem.SetMappinPosition (native thunk 0x2d45460) calls 0x111df70, which only queues an asynchronous position update on the mappin system (+0x2c0). Nothing in scripts can make a marker the player's tracked marker, and the GPS route follows only the tracked marker.

How the world map tracks a marker, from the installed executable (same build as DncBuild: timestamp 0x6ad55daf, image size 0x581b000):
- The RTTI registration for WorldMapMenuGameController.TrackMappin points at native thunk 0x2d5ca28, which calls 0x2d5913c. That function gets the mappin system (GameInstance::GetSystem through 0x56880c), reads the NewMappinID at IMappin+0x40 from the selected marker, and calls mappin-system vtable slot +0x220; with no marker it calls +0x228.
- The mappin system vtable is at 0x36aad48 (found from script MappinSystem.UntrackMappin, native 0x2d44a20, which is slot +0x228). Slot +0x220 is 0x2d449bc: it locks the system, stores the new ID at +0x1b8 (the tracked marker, the same field UntrackMappin clears) and calls 0x2d426a8, which untracks the old marker (0x2d4412c) and tracks the new one (0xb2769c). Slot +0x1f0 is the same 0x111df70 as script SetMappinPosition.
- WorldMapMenuGameController.TrackCustomPositionMappin (0x2d5a27c) registers the custom waypoint through slot +0x2f0, or moves an existing one through +0x1f0.

New src/RaceTracking.cpp registers the script global DriveNoCollision_TrackMappin(id: Uint64) -> Bool in the RTTI post-register callback. It checks the image timestamp and size, 32 bytes at 0x2d449bc, that vtable slot +0x220 holds 0x2d449bc, and that the object returned by GameInstance::GetSystem(gamemappinsMappinSystem) has vtable 0x36aad48, then calls the track function. The first three calls each way are logged as DNC_RACE_TRACK. 0.31.0 also contains the unbuilt 0.30.4 changes.

Race mode 0.3.0 no longer touches the player's waypoint. Every time the race markers are refreshed (entering race mode, each checkpoint, each lap), the first race marker (the START while waiting, otherwise the next checkpoint) becomes the tracked marker. Because each refresh registers a new marker, every checkpoint starts a fresh GPS route. The marker that was tracked before the race is tracked again on exit if it still exists; otherwise tracking is cleared. The script calls the native through Codeware reflection (Reflection.GetGlobalFunction), so it still loads with an older plugin and then reports that GPS directions need plugin 0.31.0.

The race script compiles cleanly against the game cache and Codeware 1.21.4 and is installed. The plugin was not compiled in this session. Build it with tools/Build-Install.ps1. Unverified: whether the GPS draws a route to a tracked DefaultStaticMappin marker (the world map can track many marker types, so this is expected but not confirmed).

## 0.32.0 plugin and race mode 0.4.0: checkpoints on the GPS road route (plugin source ready, race script installed)

Problem (2026-11-17, race mode 0.3.0 on plugin 0.30.3): checkpoints counted correctly, but many posts stood in the air, at the roadside or turned sideways, and there was no GPS line. The posts were placed on straight lines between the circuit points, with a straight-line facing and a height guessed from the waypoint or the car. The game still ran plugin 0.30.3 (red4ext log: `DNC_LOADED ... 0.30.3`; installed DLL sha256 9eb98aa2...), so DriveNoCollision_TrackMappin from 0.31.0 never loaded. That explains the missing GPS line; tools/Build-Install.ps1 had not been run since 0.30.3.

How the game computes a GPS road route, from the installed executable (same build as DncBuild):
- The debug script class NavigationFunctionalTests has natives RunGPSQuery (thunk 0x2f27d88, implementation 0x2f27904) and UpdateGPSQuery (thunk 0x2f27ea4, implementation 0x2f27a30). Both reach a road query service as [[system+0x40]+0x78]+0x130. IGameSystem+0x40 is the system's GameInstance pointer (RED4ext SDK), so the service is [[GameInstance+0x78]+0x130]. Several vehicle-drive planners store the same [..+0x78]+0x130 pointer when they attach.
- The player GPS (0x44d62c) builds its route with the same service: a 0x60-byte request {destination Vector4, open bounds (inf), origin Vector4, open bounds, flags bytes +0x40=1, +0x42=1, +0x59=1} is turned into a 0xd0-byte query by 0x42ad34, the word at query+0x79 is cleared, 0x525948 submits it and returns an id (-1 on failure), and 0x42b378 destroys the query.
- 0x524bbc(service, id, result) fetches a finished route into a 0x68-byte result. While the route is still being computed it returns false with the message "Result not ready yet." (0x305d6f8); a bad id gives "Invalid query token.". A ready result owns the route at +0x00: 12-byte points (x, y, z) at route+0x30 and the count at route+0x3c. UpdateGPSQuery moves the route out with 0x524f44, clears it with 0x8dc110 before replacing it, and 0x524f74 destroys the result.

New src/GpsRoute.cpp registers three script globals in the RTTI post-register callback:
- `DriveNoCollision_GpsRouteStart(from: Vector4, to: Vector4) -> Int32` builds and submits a query exactly as the player GPS does. It returns the query id, or -1.
- `DriveNoCollision_GpsRoutePoll(id: Int32) -> Int32` returns 0 while the route is computed, the number of points once it is ready (and caches them), -1 on failure, and -2 if unavailable. A route that comes back destination first is reversed so it always runs from `from` to `to`. The route object is then released the way UpdateGPSQuery does it.
- `DriveNoCollision_GpsRoutePoint(index: Int32) -> Vector4` returns a point of the last ready route.

Guards: the image timestamp and size, 24 bytes at each called function (0x42ad34, 0x42b378, 0x525948, 0x524bbc, 0x8dc110, 0x524f44, 0x524f74), at the route read in UpdateGPSQuery (0x2f27af1) and at the service lookup in RunGPSQuery (0x2f279cc), and the "Result not ready yet." text. The service pointer chain and the service's graph pointers (+0x48, +0x50) are read with ReadProcessMemory before use. Logs: `DNC_GPS_ROUTE build=...` once, `DNC_GPS_SERVICE` when the service pointer changes, the first 12 `DNC_GPS_ROUTE_START` lines, and the first 24 finished `DNC_GPS_ROUTE` results (point count, first and last point, or the failure message). Main.cpp registers it after RaceTracking. The version is now 0.32.0, and the build includes the unbuilt 0.30.4 and 0.31.0 changes.

Race mode 0.4.0:
- Pressing F11 now plans the route first ("Planning the race route along the roads..."). Each leg (START, extra points, then END, or START again for a loop) gets one GPS route query. The script polls every 0.1 s and waits up to 6 s per leg. A leg follows the road route when it starts within 60 m of its first point and ends within 60 m of its last point. Otherwise, or with an older plugin, the leg is a straight line as before, and the race-on message says how many legs are straight.
- The route is thinned to points at least 3 m apart. A gate sits where each leg's route ends, which is on the road near the chosen point. Between gates, checkpoints are spread evenly about every 150 m of route distance (not straight-line distance), so they lie on the road the GPS takes.
- Each checkpoint's facing comes from the route direction over about 12 m either side. Road checkpoints use the route's height and search for ground only from 2.5 m above to 6 m below it, which keeps posts off bridges overhead and out of underpasses. Straight-line checkpoints keep the old search around the car's height.
- Pass radii: gates 14 m, road checkpoints 25 m, straight-line checkpoints 35 m. Skipping ahead to the next gate still works as before.
- The race START is the first route point. The start posts, the waiting check and the START marker use it.
- F11 during planning cancels it. Leaving the vehicle during planning also ends race mode.

The race script compiles cleanly against the game cache and Codeware 1.21.4 and is installed (sha256 08ae5aa3...). Until the plugin is rebuilt it runs with straight lines and says so. The plugin source passes a clang syntax check (mingw target) against the RED4ext.SDK commit in dependencies (5246e659), but it has not been compiled with MSVC. Build and install it with tools/Build-Install.ps1.

Unverified until tested in game: that the service chain resolves (`DNC_GPS_SERVICE`), that queries finish and return road points (`DNC_GPS_ROUTE id=... points=...`), whether route points are lane centres or the road centre (posts stand 6.5 m either side of them), the GPS line from DriveNoCollision_TrackMappin, and the twitch changes from 0.30.4.

Publication verification, 2026-11-17. Plugin 0.32.0 now builds with MSVC and passes all 17 Release test suites. Earlier entries describing it as uncompiled reflect their original session. Runtime checks listed in the 0.32.0 section remain pending.

