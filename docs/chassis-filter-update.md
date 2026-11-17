# Chassis filter update investigation (2026-10-29)

Offline analysis of the installed 2.5 executable. These addresses are research evidence, not enabled runtime bindings. Installed diagnostic remains 0.7.0; no collision or visual mutations were added.

## Strongly identified chassis setter

RVA 0x81afa8 matches the older reference SetFilterData(chassis, primaryFilterData, bottomFilterData) structurally, despite its old byte pattern no longer matching. Evidence:

- Reads chassis proxy ID at +0x160; creates the known proxy helper at 0x612ed4.
- Acquires the byte lock at chassis +0x16c through 0x22ca300 (atomic compare/exchange with wait loop).
- Sends 16 bytes from primaryFilterData +0x30 as state 0x21 (query filter), and +0x40 as state 0x20 (simulation filter).
- These first writes use body index 0 and shape index 0xffffffff. This is consistent with an all-shapes sentinel, but the concrete proxy implementation must confirm its meaning before invocation.
- Iterates set bits of the 32-bit field at chassis +0x174 and issues corresponding shape-index writes from bottomFilterData +0x30/+0x40. This agrees with the reference bottom-shape mask. It establishes distinct filters for selected shapes; it does not prove which shapes carry tire support or identify the Semimaru's shape 0.
- Calls 0x612eac to publish an existing cached entry, then the known unlock helper 0x2e3200.

The chassis-wide setter is unsuitable for preserving arbitrary original per-shape filters without further work: it intentionally applies broad primary and bottom presets.

## State update and synchronization

RVA 0x613258 routes updates through two paths. Its observed arguments are proxy ID, cache, cache entry, body index, shape index, state ID, input bytes, byte count, and a final byte flag (meaning unverified).

With a cache, it calls 0x613320, which copies state data into cache records. Without a cache, it resolves a shared proxy, calls virtual slot +0x58, then releases the shared reference. This is the write counterpart to the direct-state reader's slot +0x50. Neither slot has been invoked for writes by the mod.

RVA 0x612eac passes the cached entry to 0x612f34 and clears the helper's entry pointer. The latter validates the proxy, marks an existing entry, and exchanges a pointer into the manager table. This is evidence of publishing deferred updates, not proof of immediate contact-pair refresh. The consumer and concrete virtual setter still require inspection. A chassis lock alone must not be assumed sufficient for direct PhysX mutation from the Running callback.

## Preserved flag masks

The two constants used by mask2 helper 0x3209a0 at table RVA 0x36f5038 are:

- 0x8000000000000000 (bit 63)
- 0x4000000000000000 (bit 62)

Their meanings and interaction with pair filtering remain unverified. Preserve both; never treat them as named collision groups.

## Reproduction and boundaries

Run tools/refine-filter-update.py to find functions accessing +0x160 and +0x174, then narrow by calls to known proxy helpers. It yielded 108 candidates; 0x81afa8 also calls both known create/unlock routines. Offset matches alone produce many unrelated functions and are not binding verification.

Run tools/inspect-chassis-update.py for bounded function disassembly saved in artifacts/native-research/chassis-filter-update.json. Raw candidate disassembly is in filter-update-refined.json. All addresses apply only to the inspected executable (PE timestamp 1792368047, SizeOfImage 92385280).

Remaining before gameplay writes: establish live body/shape enumeration and roles, confirm pair-mask semantics including flags, verify concrete setter/contact refresh and safe callback context, implement driver-only ownership and per-shape snapshot/restoration. NPC opacity is a separate unimplemented rendering requirement. No additional game launch is needed for this offline milestone.

Follow-up: the concrete actor-state setter uses 0xff as the all-shapes sentinel, whereas this chassis wrapper supplies 0xffffffff before dispatch/cache packing. Do not invoke the direct path with the wrapper's sentinel assuming equivalence. See simulation-setter.md for per-shape setter evidence and the tested restoration planner.
