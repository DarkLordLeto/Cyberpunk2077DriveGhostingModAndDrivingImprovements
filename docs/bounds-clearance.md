# Body bounds and clearance diagnostic: 0.17.0

Build and all ten Release CTest suites pass. Runtime body-0 bounds reads and overlap/clear transitions were observed on 2026-10-30. Collision response remains enabled. PairLifetimeRegistry remains offline; this diagnostic does not call ConfirmClear or issue restoration updates.

The already guarded state reader at game RVA 0x614668 routes states 35 and 39 to 0x614fbb. That branch calls the actor virtual slot +0x48 with float inflation 1.01, adds the physics world-origin offset, and writes two float4 values (minimum/maximum) with W=1. The complete branch lies within the existing exact-build ShapeStateReader guard. Evidence: artifacts/native-research/shape-state-full.json. The guard includes the native wrapper; current PhysX provides actor bounds as described in the upstream PxActor interface.

ReadVerifiedWorldBounds uses the generation-checked direct state accessor for body 0, state 35, and a 32-byte output. It does not retain proxy/actor pointers. It rejects invalid IDs, read failures, non-finite/inverted/degenerate bounds and unexpected W fields. All calls run on the game thread, never in physics callbacks.

For up to eight audited pair lifetimes, unique actor combinations are sampled every 200 ms even after pairLost. DNC_BOUNDS_RELATION records changes among unknown, overlapping and clear. A clear result requires a strict axis gap greater than 0.5 world units between the returned inflated boxes. Unknown is never clear. DNC_BOUNDS records coordinates when valid. No distance-only assumption or pairLost-based restoration is used.

Important scope: these are body-0 actor bounds, not a proven union of every physics body belonging to a vehicle. Additional bodies/trailers/attachments may require enumeration and union. Game-thread reads are sequential and asynchronously sampled; they do not authorize restoration at a later simulation instant. Overlapping AABBs are conservative and do not establish exact mesh penetration. The geometry classifier is intentionally disconnected from restoration until coverage, freshness/motion and synchronization requirements are handled.

Test: gently touch a traffic car and remain close for roughly two seconds, then back away several car lengths and wait two seconds. Exit. Look for valid bounds and overlapping-to-clear transitions, including relation to pair-loss events. No motorcycle repeat is needed for this binding check.

## Runtime result: 2026-10-30

Archived log drivenocollision-2026-10-30-18-15-24.log; structured report artifacts/bounds-clearance-baseline.json. For proxies 000108d3 and 000107f4, the relation sequence was overlapping, clear, overlapping, clear, unknown. Two overlapping-to-clear transitions were recorded. At the first clear sample the Y-axis gap was about 2.326 world units; at the second clear sample the X-axis gap was about 2.201 units, both exceeding the configured 0.5 margin.

The second overlapping sample had pairLost=1 for the historical audited pair. It demonstrates that a previous pair-loss notification is not continuing clearance evidence; it does not prove that this particular loss was caused by refiltering. The final unknown sample is not interpreted as clear or assigned a cause without further evidence.

All eight tracked lifetimes had loss records and unchanged-flag status echoes. Both callback slots restored and unload completed without warning/error records. No repeat body-0 binding test is needed.

Remaining before restoration authorization: cover all relevant vehicle bodies; synchronize or conservatively account for observation age/motion; preserve overlap intent across refiltered pair lifetimes; connect the serial reusable registry without dropping loss events. The current sampling path still does not authorize collision writes.
