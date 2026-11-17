# Continuous contact discovery: 0.12.0

Build and all five Release test suites pass. Continuous discovery and positive candidate classification were observed in-game on 2026-10-30. Collision outputs remain unchanged.

Every player-matching callback now attempts to enqueue both proxy IDs plus the complete mounted observation token. The fixed 512-entry FIFO is reusable, supports concurrent producers through a single try-only atomic flag, and skips on contention/full capacity without spinning or allocating. The Running callback consumes up to 128 records per frame. Records from a previous mounted interval are discarded. At most 256 known proxies are retained, with cyclic replacement and retirement of the replaced identity. This replaces the previous permanent discovery cutoff; the first 64 per-mount detailed logs remain only an audit sample.

DNC_DISCOVERY separates queued/dropped observations, missing-or-expired owners, rule-rejected complete pairs, owner-refresh outcomes, and snapshot publication successes/skips. These counts are cumulative. Rule rejection still groups driver/ownership/traffic predicates; it does not identify each predicate separately. World objects without entity identities contribute to missingOwners; that counter is not exclusively an error count.

Automated coverage includes 10,000 FIFO reuse cycles, full/empty behavior, four concurrent producers with accepted-item accounting, snapshot contention/coherence, lifecycle invalidation, and existing filter/restoration tests.

This remains post-contact discovery and cannot prevent a first collision. Reflection provides a possible next route: gameObject.GetEntitiesAroundObject;FloatTargetSearchFilter, returning array:handle:entEntity. Both parameters are optional; the filter is gameTargetSearchFilter. The pinned SDK exposes only an opaque 24-byte filter, so construction/default/filter semantics and query coverage need verification before calling it. No nearby query is enabled.

Validation for this version: a single car driven through traffic for roughly one minute, then exit. Confirm discovery continues after 64 player callbacks, queue/publication counters progress, and whether any fresh repeated traffic pairs become candidates. Positive classification is not guaranteed for any one traffic encounter because the first callbacks occur before owner resolution. Do not interpret zero candidates as failure without examining the new counters.

## Runtime result: 2026-10-30

Archived log: drivenocollision-2026-10-30-11-23-10.log. Structured report: artifacts/continuous-discovery-baseline.json.

Latest periodic counters show 757 queued player-matching callbacks, 109 known proxies and 68 classified traffic candidates. Discovery continued well past the 64-record detailed log budget. Queue drops, snapshot contention and publication skips were all zero in the recorded counters. Hook detach and plugin unload completed without warning/error records. These are periodic counts and candidate callbacks, not final totals or distinct vehicles.

There were 655 missing-owner assessments and 34 rule rejections. Owner refresh ran 26,526 times, resolving 7,270 and failing to resolve 19,256 times. This reveals unnecessary repeated refresh of stale proxies; no frame-time performance conclusion can be drawn from these counts. Future discovery should remove/back off unresolved proxies and prioritize fresh nearby vehicles.

This verifies the diagnostic discovery-to-snapshot-to-classification path on this session. It does not establish NPC occupancy, prompt seat-change invalidation, first-contact coverage, suppression lifecycle, or fading. No repeat of this test is required. The next functional gate is pre-contact nearby-vehicle discovery.
