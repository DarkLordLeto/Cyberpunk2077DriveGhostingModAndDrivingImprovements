# Live simulation-filter read probe 0.6.0

## Verified offline path

Tracing installed executable callers establishes:
- Chassis kinematic and velocity getters route through the physics-state reader at RVA 0x27d19c.
- Reader arguments: cache pointer, proxy ID, body index, shape index, state ID, output pointer, output byte size. It returns a boolean status.
- The reader resolves a reference-counted proxy internally and releases the reference before returning.
- Null cache takes the direct proxy virtual read path; a populated cache can take a cached-value path.
- SDK physics::StateValue identifies LinearVelocity as 4 and SimulationFilter as 32.

Evidence: artifacts/native-research/proxy-state-disassembly.json and proxy-cache-disassembly.json. No raw PhysX object pointer is retained or manually traversed by our plugin.

## Runtime experiment

All four native entry points are guarded by exact executable timestamp/image size and 24-byte entry signatures. On vehicle inspection:
1. Read the existing chassis velocity using the already-tested getter.
2. Ask the direct state reader for 12-byte LinearVelocity of body 0. Require success, finite coordinates and agreement within 0.1 per axis with the getter. This tolerance is a diagnostic consistency bound, not a calibrated physical unit.
3. Only after agreement, request a 16-byte SimulationFilter for body 0 / shape 0.
4. Log the masks only if the engine reports success. Otherwise ignore the output and report unsupported. Never interpret a failure as zero filters.

The ABI and state ID have source/disassembly evidence. Runtime support for SimulationFilter is still unverified; velocity agreement does not prove that state 32 will be accepted. No filter writes are present. No assumption is made that resource shape count equals the live shape count.

## Test

Load a save, enter one stationary stock car for 10 seconds, exit it and quit. No collision test is needed. Expected markers: DNC_PROXY_READ_CHECK and either DNC_LIVE_SIM_FILTER, DNC_LIVE_FILTER_UNSUPPORTED or DNC_LIVE_FILTER_SKIPPED.

## Validation

MSVC x64 Release build passed and installed DLL hash matched. Previous 0.5.0 DLL preserved. In-game validation pending. Ghosting, fading and handling changes remain disabled/unimplemented.

## Next decision

A successful filter read provides a baseline snapshot; it does not identify vehicle/road group bits or provide a restoration-safe setter. Investigate group mapping and mutation/refresh semantics separately before any pass-through experiment. A rejected read means the proxy's read implementation must be traced rather than guessing masks or using global collision disabling.

## 0.6.0 runtime result — successful live filter read

Verified October 29, 2026, source drivenocollision-2026-10-29-12-36-42.log.

- Vehicle: vehicleCarBaseObject, appearance v_standard3_thorton_mackinaw_larimore_poor_03.
- Native chassis getters succeeded; kinematic=false.
- Direct physics-state velocity read: success=1, matches=1.
- Direct SimulationFilter read for body 0 / shape 0 succeeded:
  - mask1: 0x0000000000000010
  - mask2: 0x8000000000008cf0
- Plugin unloaded cleanly; no writes were performed.

This establishes a working read path for this vehicle's active simulation filter on this executable. These masks are an observed baseline, NOT a ghosting preset. Bit meanings, filtering rules, other shapes/vehicle types, synchronization and reversible setter semantics are not yet established. Do not clear a guessed bit or copy these masks to other vehicles.

Next: obtain named collision-group/preset mapping, verify the filter update path, and design snapshot/restore for the player vehicle's affected shapes while preserving wheel/road contacts. NPC fading remains a separate unresolved rendering task. No identical repeat test is needed.
