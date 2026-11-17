# Existing-pair update probe: 0.16.0

All eight Release CTest suites pass. Runtime status submission and subsequent loss notifications passed on 2026-10-30. Normal collision response remains enabled; this is not ghosting.

The installed PhysX function at RVA 0x12e700 dispatches statusChange through callback slot +0x10. RCX is callback instance, RDX points to uint32 pair ID, R8 to uint16 pair flags, R9 to uint16 filter flags. Boolean AL controls a loop: true selects the retained pair index, routes element pairs through refilterInteraction at 0x130940, updates interaction state, then asks again. This function spans split unwind entries; the complete reviewed range is 0x12e700¨C0x12e8f9. Its 506 bytes are guarded in VerifiedPhysX.hpp. Evidence: artifacts/native-research/physx-status-dispatch.json.

The game callback table's statusChange entry points to the shared 32 c0 c3 (xor al,al; ret) stub at 0x664f00. Only the scoped table slot is exchanged; the shared stub is untouched. Both pairLost and statusChange entries are restored at unload. Failed installation rolls back installed slots, and failures are logged.

For each of the eight tracked audit records, the status callback calls the original function first. If it returns false, the probe can emit the active pair ID with its original pair flags and tracking filter flags 0x0c once. No solver/detection/reporting pair bits are changed. Records already marked lost and records in another callback scope are skipped. One-shot claiming prevents an infinite statusChange loop. Claimed and emitted updates have separate markers so a lost-before-emission record is not logged as an echo.

The fixed records are still an audit probe, not a reusable suppression registry. IDs are selected within the physics status callback rather than queued on the game thread. Correctness relies on the engine's end-of-simulation status dispatch relative to pair removal, consistent with reviewed installed/upstream flow; this is not a general lock-free solution to arbitrary concurrent engine pair deletion. Future reversible suppression requires explicit lifecycle synchronization and original-setting restoration.

DNC_STATUS_ECHO means the callback submitted a same-flags update; it is not an independent engine readback. DNC_STATUS_COUNTS reports callback invocations and updates at unload. DNC_STATUS_RESTORED and DNC_TRACK_RESTORED confirm both table slots were restored.

Test: gently contact a traffic car at low speed, stay near it for a second, then separate. Repeat with another car and exit. Check status echoes, subsequent losses, and clean restoration. Collision suppression, driver-change restoration of suppressed pairs, and NPC fading remain unimplemented.

## Runtime result: 2026-10-30

Archived log: drivenocollision-2026-10-30-16-27-13.log. Report: artifacts/status-update-baseline.json. All eight found lifetimes submitted status echoes with pairFlags=0x3e61 and filterFlags=0x000c, then received matching lost notifications. The capture covers one vehicle combination with reused pair IDs 3 and 5, not eight distinct cars. No unmatched or remaining captured lifetimes were found.

Shutdown reported 12,161 status callback invocations and eight updates. Both callback-table entries restored, the pairFound hook detached, and the plugin unloaded without warnings/errors. This verifies the scoped status callback ABI and one-shot submission path in this session, not an independent readback of changed engine settings. Every pair retained normal collision response.

No repeat status-echo test is needed. Next: reusable pair lifetime registry and controlled solver-response suppression with explicit restoration state. Preserve scene/callback scope and generation on ID reuse, and handle loss/refilter races. Restoring solver response while vehicles geometrically overlap remains a separate behavior to design and validate.
