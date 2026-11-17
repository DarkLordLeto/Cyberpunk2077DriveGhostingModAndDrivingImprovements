# Restoration submission gate — local, not installed

Implemented after the successful 0.20.0 shadow run. The installed DLL remains the tested 0.20.0 artifact; the newly built local DLL has not been deployed. Collision suppression and fading remain disabled.

A queued clearance ticket no longer authorizes restoration by itself. SubmitNext refuses restoration. SubmitNextChecked prepares a lifetime-specific update, releases the controller mutex, invokes a noexcept clearance reader for the exact vehicle identity, then reacquires the mutex and checks intent generation/revision and pair lifetime before recording submission. Unknown/overlapping reads or intervening changes cancel the update and revoke clearance. Suppression requires no restoration read. Native functions are never called under the controller mutex.

This closes the model bug where vehicles could re-overlap without a pair-lifetime event after ConfirmClear but before submission. It also rejects numeric pair-ID reuse and controller changes during the read. It does NOT establish physical stability after the read or engine consumption of the returned output; native phase ownership remains required.

All 14 Release CTest suites pass. RestorationSubmissionTests verifies rejection by the unchecked path, re-overlap with no lifecycle event, loss and same-ID replacement during a reader callback, state mutation during reading, exact original-flag restoration, and cleanup. Its reentrant callbacks also exercise the absence of a held controller mutex. Existing concurrent tests remain passing.

## Native reader review

The existing ReadVerifiedProxyBounds path cannot simply be moved into OnStatus without establishing engine lock ordering:
- Resolver 0x27d430 calls 0x1ffada0 at 0x27d47b, passing manager+0x2e2054.
- 0x1ffada0 calls 0x22ca420, which performs byte cmpxchg and loops through pause/umonitor/umwait paths while acquisition is unavailable.
- ReleaseSharedProxy at 0x27d374 decrements the reference; on the last reference it calls 0x2750b0 and a virtual destruction path before further cleanup.

Evidence is saved in artifacts/native-research/restoration-reader-locks.json and restoration-reader-lock-acquire.json. This establishes a potential wait and destruction path, not that deadlock actually occurs. We have not established what locks the engine holds during our callback or whether this reference could be the last one there. Therefore no native reader was added to the status callback and no new runtime experiment is requested.

Next integration work: establish a callback-safe bounds acquisition/lifetime strategy (including proxy/body lifetime, scene phase, and lock ordering), then connect it to SubmitNextChecked. Separate attached proxies, disable/seat changes, capacity behavior for retained overlaps and drain-before-unhook still require native handling. Do not treat the new controller gate as engine synchronization proof.
