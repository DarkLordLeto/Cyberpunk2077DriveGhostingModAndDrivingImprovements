# Suppression controller — local implementation, not installed

Implemented 2026-10-30 in src/SuppressionController.hpp. The installed DLL remains 0.19.0 with collision response unchanged. This controller is compiled by its tests, not wired to native callbacks.

The controller serializes its fixed-capacity registry with a mutex. Loss waits for the short internal critical section instead of dropping the event on contention. There are no engine calls, logging, arbitrary callbacks or allocations inside that section. This establishes internal state consistency; it does not establish engine callback ordering, real-time latency or safe native lock ordering.

Intent is keyed by session, both entity IDs and both generation-bearing proxy IDs. It survives pairLost/refilter pair recreation, including after StopAll. New identities require all eligibility/readiness gates. Pair tokens reject delayed loss from an earlier lifetime. The native loss callback only supplies a numeric pair ID, so a phase-ordered mapping to lifetime tokens is still required by the adapter.

StopAll prevents new intents but retains existing overlap intent. Clearance tickets carry intent generation and revision. Begin, loss and stop invalidate evidence, including already queued restoration. ConfirmClear queues exact original pair flags; eNOTIFY remains enabled for lifetime tracking. Detach is refused until all live pairs are gone and all retained intents have subsequently been cleared. There is deliberately no force-reset operation that could discard altered pairs.

Validation: Release build and all 13 CTest suites pass. New cases exercise disable while overlapping, same-ID refilter continuity, stale loss, evidence replay, original flag restoration, invalidation of queued restoration by new contact, and 20,000 pair lifetimes across four concurrent workers with no lost bookkeeping events.

Remaining native integration requirements:
- Engine-phase ownership must extend through callback output consumption. The controller mutex only protects its own data; returning an update does not prove the engine has applied it.
- Clearance must cover the complete vehicle at the restoration phase. Current sequential game-thread proxy bounds are diagnostic evidence only.
- Connect eligible identities and seat/session changes; retire separated intents during enabled operation to avoid fixed-capacity exhaustion. Current StopAll/clear path provides shutdown cleanup only.
- Preserve intent across engine refilter ordering, handle unsupported attributes and capacity without restoring an existing overlap, and establish a drain-before-unhook shutdown path.
- Transparency remains separate and unimplemented.

Do not enable suppression merely because these unit tests pass. No additional unchanged-response user test is requested for this change.

## Follow-up: 0.20.0

The controller is now wired into native callbacks in shadow mode only. LostCurrent resolves native IDs under the controller lock, and clear unbound intents can be reclaimed while accepting new vehicles. This supersedes the local-only and shutdown-only cleanup statements above. See native-shadow-controller.md; real engine suppression remains disabled.

## Follow-up: restoration freshness

Queued clearance is now insufficient to emit restoration: SubmitNext refuses it, while SubmitNextChecked requires a fresh noexcept reader and revalidates lifetime/revision afterward. The existing native shadow path uses SubmitNext, so it cannot emit restoration proposals through an unchecked path. Native phase and reader safety remain unresolved; see restoration-submission.md. All 14 Release suites pass locally; the installed binary remains the previously tested 0.20.0.
