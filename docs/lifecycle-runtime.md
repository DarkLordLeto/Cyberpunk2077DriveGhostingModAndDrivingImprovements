# Diagnostic lifecycle integration: 0.11.0

Build and all four Release CTest suites passed. Runtime execution completed cleanly on 2026-10-30, but positive candidate classification was not verified. The pair callback still calls the original function once and returns its exact return pointer without changing any arguments or outputs.

The game thread refreshes at most 16 known proxy owners per 50 ms opportunity. Owner resolution, attachment checks, and Bool script calls remain on that thread. The bounded discovery source is still the existing sampled callback stream (maximum 256 known proxies); this is not pre-contact discovery and is unsuitable for first-contact ghosting. Per-mount sample budgets remain unchanged.

Vehicle observations and independent driver authorization expire after 1,500 ms. The current full proxy ID must match the mounted chassis observation before granting diagnostic driver authorization. Unresolved, detached or non-entity owners retire matching records. A changed mounted proxy resets the model epoch. Driver detection is refreshed with owner observations, while mount polling still runs every two seconds; neither is an immediate exit/seat-change hook.

TrySnapshot stores one complete model behind an atomic flag. Readers and writer attempt once and skip on contention. The reader sees a const model only while holding the flag; no references escape. No allocation, spin loop or blocking wait is introduced in the callback. This serializes simultaneous readers and favors preserving normal behavior over guaranteeing every sample. Exception-safe release is covered by a test, along with four concurrent readers checking coherent multiword snapshots.

DNC_LIFECYCLE logs cumulative candidate and contention counts plus known proxy count. A candidate means a fresh verified player driver paired with a fresh non-player-owned traffic vehicle. It does not establish NPC occupancy or authorize collision suppression. Counters represent callbacks, not unique cars or overlaps. A failed reset publication may leave an old snapshot readable until expiry; acceptable only because this remains diagnostic. Runtime enabling requires a separate immediately visible session/driver authorization gate.

Next validation: one car near moving traffic for about 30 seconds, then leave the car for several seconds and exit. Check increasing candidate counts, clean refresh and shutdown, and absence of warning/errors. Zero candidates may reflect bounded post-contact discovery and must not be interpreted as proof of a failed vehicle query.

Remaining before suppression: nearby/pre-contact discovery, robust eligibility/mission rules, prompt lifecycle invalidation, existing-pair refilter/toggle behavior, overlap tracking and NPC fade restoration.

## Runtime result: 2026-10-30

See artifacts/lifecycle-probe-baseline.json and archived log drivenocollision-2026-10-30-11-12-23.log. Last periodic counters: 13,977 total callbacks, 970 player matches, 17 known proxies, zero candidates and zero observed snapshot contention. Driver detection succeeded; unmount, hook detach and unload were logged without warnings/errors.

All 64 saved player samples were consumed. The sole identified traffic proxy (000113bd) appeared in two saved contacts drained together at 11:15:04.378, when its owner was first resolved. These contacts had already run before classification. Later callbacks are counted but not individually recorded, so their identities are unknown. Refresh outcomes/rejection reasons were not logged; discovery timing is an established limitation, not a proven sole explanation of zero candidates.

Do not mark runtime classification as passed. Do not request an identical test. Replace finite sample-based discovery with continuous bounded observation, and find a pre-contact nearby-vehicle source. Add refresh/publication and rejection diagnostics so the next test can distinguish missing ownership from stale driver/traffic state. Continuous contact observation alone still cannot ghost the first contact.
