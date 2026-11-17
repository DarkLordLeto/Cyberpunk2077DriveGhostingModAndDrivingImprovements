# Driver authorization and prospective suppression: 0.14.0

Built locally; not installed. Game still has diagnostic 0.13.0. All seven Release CTest suites pass. No native collision output is changed by this build.

DriverGate holds a 250 ms lease for the exact entity, generation-bearing chassis proxy and mounted-interval token. Refresh and revocation increment a separately published revision before updating the snapshot, so failed publication cannot leave an older revision authorized. Reads try once and compare revisions before/after the lease check. Unknown/passenger states, expiry, backward timestamps and identity/mount changes reject authorization. This provides a bounded decision-time check, not an atomic guarantee spanning a subsequent engine mutation.

Live driver checks now run at a 100 ms cadence, read the verified chassis proxy on each pass, and revoke on detected loss of the mounted vehicle. Nearby discovery runs independently at 500 ms and logs query-plus-processing elapsedUs. These timings are unmeasured in-game. A stall expires the lease; a seat change can still precede detection by a polling interval. Immediate seat/session/pause event integration remains outstanding. Existing driver facts in the larger lifecycle snapshot remain an additional conservative gate.

The pure PlanTrafficContact policy is tested offline and not invoked by the hook. It requires enablement, driver authorization, traffic eligibility and verified restoration readiness; accepts only exact observed non-kinematic rigid-dynamic attributes (1/1) and a default original filter result; clears only bit 0 of pair flags. Exhaustive 16-bit tests preserve every other pair flag, every non-default engine filter decision and all missing-gate cases.

The upstream PhysX reference identifies pair bit 0 as eSOLVE_CONTACT. Keeping reporting/detection bits can allow continued overlap observation but may retain game-side damage/audio responses. This must be tested separately; do not equate clearing solver response with a completed no-collision feature.

Source: https://raw.githubusercontent.com/NVIDIAGameWorks/PhysX-3.4/master/PhysX_3.4/Include/PxFiltering.h (reviewed 2026-10-30). The reference also documents that pair decisions persist until overlap/filter changes or explicit resetFiltering, and eNOTIFY enables pairLost/statusChange tracking. These upstream semantics require confirmation for the installed customized engine. Captured pair IDs are all zero, so existing callback arguments cannot currently serve as unique overlap keys.

Next implementation gate: establish tracked pair identity and pairLost/statusChange or a supported refilter path, preserve original per-pair flags, and define safe restoration on separation/toggle/driver change. Do not set lifecycleReady merely because driver/traffic observations are fresh. No new user driving test is needed for this offline step.
