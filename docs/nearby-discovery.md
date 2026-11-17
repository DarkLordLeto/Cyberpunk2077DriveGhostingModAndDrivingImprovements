# Nearby-query diagnostic: 0.13.0

Build and all five Release CTest suites passed; the nearby query returned vehicles successfully in the 2026-10-30 runtime test.

The installed RTTI identifies gameObject.GetEntitiesAroundObject with optional Float range and optional gameTargetSearchFilter, returning array:handle:entEntity. The pinned SDK's ExecuteFunction passes the explicit argument count through CStack to the engine script dispatcher. The diagnostic supplies a 60-unit Float radius and omits the optional filter, leaving defaults to the game. It does not construct or zero the opaque 24-byte filter. Metadata verifies instance method, parameter names/types, optional filter, return type and array layout before invocation. Default filter semantics and vehicle coverage are hypotheses to test, not verified facts.

The query runs on the game thread through the existing two-second mounted-vehicle poll, using the player gameObject as context. Returned strong handles exist only during the call. Processing is capped at 256 entities and 512 components per entity. Only attached vehicle objects with the verified chassis layout are considered. Chassis proxy reads reuse exact-build byte checks and are subsequently resolved through the guarded weak-owner helper. The query failure disables further calls for the session.

DNC_NEARBY_READY reports signature acceptance. DNC_NEARBY_RESULT reports returned/inspected/vehicle/proxy counts and truncation. New proxy discoveries log DNC_DISCOVERED source=nearby-query or source=contact. A nearby-source record alone does not prove absence of an earlier queued contact; use the no-contact test below to establish practical pre-contact coverage.

Unresolved known proxy owners now retry with bounded exponential delays from 200 ms to 6,400 ms. Successful resolution clears the failure count; replacement clears the old slot's backoff. This reduces repeated reads of vanished proxies but does not guarantee that a transiently unresolved proxy will be reacquired immediately.

Test: sit in a car beside moving traffic for 20¨C30 seconds without touching it, then drive alongside traffic briefly and exit. Verify vehicles/proxies from the nearby query and clean shutdown. No repeat collision test or motorcycle pass is required at this stage.

This remains diagnostic. Two-second discovery is not sufficient coverage for high-speed first contact; query completeness, scheduling, driver invalidation, suppression/refilter lifecycle and opacity must still be addressed.

## Runtime result: 2026-10-30

See artifacts/nearby-discovery-baseline.json and archived drivenocollision-2026-10-30-11-31-11.log. The signature guard passed. All 34 logged query results returned 1¨C10 attached vehicles with chassis proxies, with no processing truncation. Fifteen unique proxy discoveries were attributed to the nearby query and eighteen to contacts.

Six non-player vehicles reporting trafficSlot=1 were resolved before the first saved player-contact record. The initial query resolved four such vehicles, and the 11:33:34 periodic counter still reported zero player matches after nearby discovery began at 11:33:20. This establishes practical pre-contact discovery in the tested scene. The query also returns player-owned vehicles and non-traffic vehicles; retain eligibility filtering.

Last periodic counters: 53 traffic-candidate callbacks, 163 queued observations, no queue drops, no snapshot contention or publication skips. Owner refresh counted 10,897 resolutions and 39 failures. This is consistent with reduced repeated failed lookups but is not a controlled performance comparison with 0.12.0. Hook detach and unload were clean; no warning/error records.

No repeat of this query test is needed. Coverage is not proven for every vehicle or scene, and two-second polling is insufficient for high-speed guarantees. Next gates: prompt driver/session invalidation, a measured discovery schedule, and selective contact suppression with existing-pair/refilter and overlap lifecycle handling. Fading remains a separate rendering integration.
