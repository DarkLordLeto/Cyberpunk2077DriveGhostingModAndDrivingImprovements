# Native shadow controller: 0.20.0

The controller now consumes actual native pair lifetimes with player/traffic entity identities and mounted-interval scope. The status hook drains proposed suppression into a private local PairUpdate and discards it. Only the pre-existing diagnostic unchanged-flags echo is written to engine output. No suppressed pair flags reach the engine; fading is not implemented.

Added LostCurrent for native numeric IDs. It resolves and removes the current lifetime under one lock. This relies on engine ordering of numeric IDs; a late native callback after ID reuse is not distinguishable. Token-based Lost remains available for events captured with a token.

Enabled-mode retirement now reclaims an intent when it has no live bindings and a matching clearance ticket is confirmed. New contacts invalidate that ticket. The game-thread shadow integration samples at most four candidate vehicle pairs per 200 ms, with a rotating cursor. Unknown bounds retain intent. This does not solve retention when a proxy disappears permanently and remains a potential shadow-capacity limit.

DNC_SHADOW logs begun, lost, proposed, cleared, rejected, active and collisionWrites=0. Proposed means local controller output only, not a collision change. There is no runtime switch that enables the shadow output. All thirteen Release suites pass; added one-slot reuse through 1,000 distinct vehicle identities without disabling the controller. Installed DLL hash and backup recorded in artifacts/install-0.20.0-*.json.

Runtime validation pending: gently contact two traffic cars, separate several car lengths, remain nearby about five seconds after each, and exit. Either a car or motorcycle is sufficient for this integration check. Inspect shadow registration/loss parity, proposed output, clearance retirement and rejection counters, plus clean shutdown. No visual ghosting is expected.

Evidence revisited: NVIDIA PhysX 3.4 ScNPhaseCore::fireCustomFilteringCallbacks consumes statusChange output and calls refilterInteraction in its end-of-simulation flow. This matches the previously guarded installed status dispatch at RVA 0x12e700. It does not establish synchronization for our game-thread clearance reads or all scene mutations. Source: https://raw.githubusercontent.com/NVIDIAGameWorks/PhysX-3.4/master/PhysX_3.4/Source/SimulationController/src/ScNPhaseCore.cpp

Next: use observed native controller outcomes to address integration faults; establish phase-local clearance and drain-before-unhook handling before enabling solver suppression. Do not infer actual ghosting from shadow counters.

## Runtime result: 2026-10-30

Archived drivenocollision-2026-10-30-21-13-53.log; report artifacts/native-shadow-baseline.json. Final shadow counters: begun=61, lost=61, proposed=61, cleared=5, rejected=0, active=0, collisionWrites=0. The diagnostic registry likewise reported 61 begun/lost/echoed, no rejection, contention or fault. Both callback slots restored and unload completed without warning/error log entries.

Native shadow registration, proposed-update generation, pair removal and enabled-mode clearance retirement worked in this run. Five retired intents do not establish five distinct vehicles. Zero active bindings does not prove zero retained intents because the latter count was not logged. Proposed changes remained local; no actual suppression/restoration or fading was validated. No repeat shadow contact test is needed based on this result. Next work remains phase-local clearance and safe native drain/restoration before enabling suppression.
