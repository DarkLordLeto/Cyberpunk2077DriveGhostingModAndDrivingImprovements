# Reusable pair lifetime and restoration model

Implemented in src/PairLifetimeRegistry.hpp. All nine Release CTest suites pass. This is an offline serial state machine; the runtime still uses the eight-entry 0.16.0 audit probe. Installed DLL unchanged.

Each active pair is keyed by callback scope and PhysX pair ID, plus a monotonically increasing lifetime token. Removed entries are reusable; tokens never rewind on reset. Duplicate live keys and capacity overflow are rejected. Original filter/pair flags are retained once per lifetime. Non-default original filters are rejected. Suppression requests use the existing gated selective-contact policy.

Prepare returns a unique submission ticket and the target flags. Submitted records submission, not engine readback. Lost/reset invalidates queued and in-flight work. Every retry receives a new ticket, preventing a canceled ticket from acknowledging or canceling its replacement. Payload checks reject altered receipts.

A stop request cancels an unsent suppression. If suppression may already have been submitted, restoration waits for explicit ConfirmClear. Stopping during an in-flight submission preserves a corrective restoration request after clearance. Restore returns original pair flags while retaining eNOTIFY tracking until pair loss; it does not release tracking by returning the original filter mask.

Tests cover scope collisions, duplicate IDs, exhaustion, solver-bit preservation, clear-before-restore, loss/reset, ID reuse, stale receipts, stop during submission, cancellation/retry and changed payloads.

Runtime integration requirements remain: serialize engine pair lifetime events/status updates; map current engine IDs to current lifetime tokens inside callbacks; establish clearance from trustworthy overlap/bounds information, not pairLost alone; handle refilter lost/found transitions for the same vehicle overlap; feed current driver/session/enable state; preserve restoration during disable/unload. ConfirmClear must not be called merely because time elapsed or a pairLost event arrived. This model does not supply clearance detection or refilter continuity.

No game test is required for this offline change. Collision suppression and fading remain disabled.
