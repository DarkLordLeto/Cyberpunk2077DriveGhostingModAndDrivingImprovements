# Vehicle lifecycle foundation

Implemented in src/VehicleLifecycle.hpp; validated by VehicleLifecycleTests in Release on 2026-10-30. All three CTest suites pass. Not connected to the installed diagnostic or collision writes.

The bounded game-thread model tracks complete observations using entity ID plus the full generation-bearing proxy ID. A new proxy for an entity retires its old mapping; a proxy assigned to another entity retires the previous owner. Explicit retirement requires the exact identity, so a delayed removal for an old proxy cannot delete a migrated owner. Session reset invalidates previous epoch tokens and disables classification.

Unknown and negative facts replace previous positives. Freshness is exclusive at the configured deadline and rejects backward time. Driver authorization has an independent deadline, and refreshing vehicle observations cannot extend it. Capacity exhaustion rejects new identities instead of evicting fresh records. Expired slots can be reclaimed.

Pair assessment is symmetric. Only a currently verified player driver paired with a fresh non-player-owned vehicle reporting traffic membership can produce TrafficCandidate. World objects, unrelated pairs, self pairs and missing/expired observations keep the original decision. The candidate result does not prove NPC occupancy, does not handle mission exceptions, and must not directly suppress contacts.

Integration contract: all mutations occur on the game thread, with a monotonic clock and freshness in matching units. Future physics readers require a separately implemented immutable snapshot publisher with safe reclamation. Do not share this mutable container concurrently. Resolve/discover traffic before first contact; the existing bounded post-callback sample drain is insufficient. Driver changes need prompt invalidation, beyond the existing two-second diagnostic poll.

Remaining runtime work includes nearby vehicle discovery, driver/occupant eligibility, snapshot publication, scene reset events, pair suppression/refilter lifecycle, and visual fade restoration. The current code deliberately supplies classification only; expiry cannot restore an already suppressed PhysX pair.
