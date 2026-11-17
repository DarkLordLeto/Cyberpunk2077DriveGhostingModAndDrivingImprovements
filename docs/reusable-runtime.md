# Reusable runtime tracking diagnostic: 0.19.0

Build and all twelve Release CTest suites pass. Reusable unchanged-response tracking and complete single-body inventories passed runtime testing on 2026-10-30. Includes the previously local-only 0.18.0 complete chassis-proxy body inventory.

DiagnosticPairRuntime integrates PairLifetimeRegistry with the actual pairFound/pairLost/statusChange path. It holds up to 128 concurrent lifetimes and reclaims entries on pair loss. The eight immutable audit entries remain only for detailed logs/bounds sampling; they no longer limit tracking. Each accepted lifetime submits one unchanged-flags status echo. No suppression API is exposed by this adapter, and normal collision response remains enabled.

Registry access uses a single try-only atomic flag. A busy begin is declined, leaving the original pair decision unchanged; a busy status call yields no update. A busy loss event latches a diagnostic fault and stops all later registry updates/registrations. This deliberately exposes missing lifecycle synchronization without attempting to use potentially stale IDs. It is acceptable only for unchanged-response diagnostics; it cannot safely manage already-suppressed pairs and must be replaced or proven unnecessary before enabling suppression. Existing tracked pairs retain normal collision response and can age out through the engine after callback restoration.

DNC_REGISTRY reports begun, lost, echoed, rejected, contention, active and fault counters every five seconds and at shutdown. They are independently loaded cumulative counters rather than one atomic snapshot. DNC_REGISTRY_FAULT is an error requiring investigation; do not mark the test passed if it occurs. Current callback IDs are mapped to lifetime tokens while holding the registry guard.

Tests cover 20,000 reuse cycles, scoped pair IDs, unchanged flags, one-shot updates, capacity/reclamation, and concurrent producers with fault-closed diagnostic behavior. Previous suppression state-machine and bounds tests remain enabled.

Full body inventory logs bodies=A:B and inventory=proxy-all. It requires a supported reader layout and every declared body to succeed. This does not prove coverage of separately owned attachment/trailer proxies or freshness at a later restoration instant.

Validation: gently contact two traffic cars in sequence, keeping close for about two seconds then separating several car lengths from each. Drive near traffic for roughly one minute and exit. Check total lifetimes exceed eight, active pairs drain, no registry fault, valid complete body inventories, and clean callback restoration. No motorcycle repeat requested for this combined test.

Remaining work: concurrency-safe suppression lifetime management, overlap continuity across refilter events, restoration-time clearance, prompt disable handling, and NPC fading. This build does not enable ghosting.

## Runtime result: 2026-10-30

Archived drivenocollision-2026-10-30-20-49-47.log; report artifacts/reusable-runtime-baseline.json. Final counters: begun=241, lost=241, echoed=227, rejected=0, contention=0, active=0, fault=0. Reuse continued beyond both the old eight-record audit limit and the 128-entry simultaneous capacity. No more than 128 simultaneous entries is claimed. Fourteen lifetimes did not submit echoes, consistent with loss before status dispatch; their individual histories are not all logged.

The complete-inventory bounds sample reported bodies=1:1 and transitioned overlapping to clear, later unknown. This validates supported proxy count extraction, shared-reference handling and union reads for these one-body proxies. Multi-body and separate attached-proxy coverage remain untested.

Both callback table entries restored, pairFound detached, and plugin unload completed with no warnings/errors. No registry fault or contention was observed in this session, which does not establish correctness of a suppression controller under arbitrary concurrent losses. The current adapter's loss-conflict fault fallback is diagnostic-only.

No repeated unchanged-response test is needed. Next functional step is a controlled suppression mode with a loss path that cannot silently abandon altered pairs, preservation across refilter lifetimes, and a defined disable/restore policy.
