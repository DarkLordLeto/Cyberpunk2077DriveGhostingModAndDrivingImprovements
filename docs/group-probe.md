# Named collision groups: probe 0.7.0

## Offline evidence

The preset loader (reference hash 3094614336, installed RVA 0x5bf910) reaches the physics singleton at RVA 0x3d32a80 and its collision-filter resource at offset 0x50. A 12-byte getter at RVA 0x5bfa1c confirms that pointer chain. Filter-building code at RVA 0x32053c accesses collisionGroups at resource offset 0xa8, matching SDK and RTTI layout.

The named-group bit builder at RVA 0x3c8764 iterates the group table and uses BTS with the table index to set output bits. Filter construction sources simulation mask1 and mask2 from separate named collections. The mask2 update helper at RVA 0x3209a0 preserves two flag masks; their precise semantics remain unverified. Do not blanket-clear high bits or assume every set bit is a collision group.

Evidence saved in preset-disassembly.json, filter-build-disassembly.json and group-bit-disassembly.json under artifacts/native-research. Disassembly windows can extend into adjacent functions; only reviewed entry paths underpin these findings.

## Probe behavior

After a successful live filter read, read the engine-owned group table. Require exact executable/function guards, readable memory pages, expected RTTI resource size/property offset, and group counts no greater than 64. Log each collision-group name/index with the corresponding mask1/mask2 bit. The resource and singleton are not retained or modified. Runtime verification of this table access is pending.

The probe does not interpret a group name as permission to mutate it. No setter or restoration path is implemented yet. Group mapping is necessary but insufficient for selective traffic ghosting.

## Test

Load a save, enter a stock car for 10 seconds, then exit and quit. Expected: DNC_GROUP_TABLE, DNC_COLLISION_GROUP, DNC_GROUP_TABLE_COMPLETE. Rejections log DNC_GROUPS_UNAVAILABLE. No impact test is needed.

## Validation

MSVC x64 Release build passed; installed hash matched. Previous 0.6.0 DLL preserved. Native physics writes and NPC visual fades remain inactive.

## Required mutation design

Before enabling a prototype: verify the engine's filter-update function and physics refresh/synchronization; enumerate actual affected live shapes rather than assuming asset counts; capture original per-shape simulation and query state; alter only confirmed vehicle-contact groups; preserve wheel support/world groups and flag bits; read back changes; restore only mod-owned changes after clear separation. Handle exit/destruction/streaming without stale proxy IDs. NPC-to-NPC collisions must remain unchanged.

## 0.7.0 runtime result — group mapping succeeded

Source log: drivenocollision-2026-10-29-12-44-46.log. Full log and structured mapping preserved under artifacts/.

The table contains 20 groups. Verified indices include Static=2, Dynamic=3, Vehicle=4, Terrain=7, VehicleBlocker=15. Vehicle's group bit is therefore 0x10 in this runtime table. The mapping alone does not establish how the engine combines both actors' masks or its high-bit flags.

This test used yaiba_semimaru_basic_urban_01 (161 components, three resource shapes), not the prior Mackinaw. Body 0 / shape 0 reported mask1=0x0 and mask2=0x58: Dynamic, Vehicle and Destructible bits set in mask2. The prior Mackinaw reported 0x10 / 0x8000000000008cf0. Do not treat one vehicle's first shape as a universal chassis preset. A zero mask1 and absent Static/Terrain bits need explanation through the filter shader/shape role before mutation.

Velocity cross-check passed; group table and filter reads completed; plugin unloaded cleanly. No physics changes were performed. No new binary was installed after this test.

Next investigation: actual live-shape enumeration and filter-shader semantics, followed by verified setter/refresh and per-shape snapshot/restoration. Preserve all unrelated bits and flags. No identical repeat test is required.

Later 0.8.0 multi-shape testing resolved the first-shape discrepancy: Semimaru shape 0 is bottom-marked, whereas Kusanagi shape 0 is not. Both vehicles share the same two observed filter patterns across their scanned live shapes. See shape-probe.md for the validated results; simulation pair semantics remain unresolved.
