# Simulation-owned pair bounds — local decoder

Implemented src/SimulationPairBounds.hpp and SimulationPairBoundsTests. Build and all 15 Release CTest suites pass. This is snapshot-only code, not included by the live plugin. Installed 0.20.0 remains unchanged and no game test is requested.

## Installed layout evidence

- PhysX status dispatch 0x12e700 has a verified direct caller at 0xf5610 in function 0xf55d0. That function obtains contact-manager outputs, calls filtering, then advances scene timestamp fields. This matches upstream Scene::endSimulation. It establishes a phase boundary, not the freshness of every bound.
- NPhaseCore+0 is the owner scene. NPhaseCore+0x98 points to the filter-pair manager; manager+0 points to tagged entries and manager+8 is the entry count, consistent with the earlier reviewed filter allocation code. Low tag bits zero indicate element-element interaction. The decoder also requires overlap interaction type zero at +0x24, filter-pair flag 0x20 at +0x25 and matching retained ID at +0x38.
- RefilterInteraction 0x130940 reads shape elements at interaction+0x28/+0x30. Shape element+0x10 points to its ActorSim, whose +0x48 is scene. Element+0x18 packs the 29-bit element ID and two type bits.
- Shape update 0x128f60 computes a simulation shape pose via 0x128a10 and writes scene+0x818's BoundsArray, whose +8 is the data pointer. Bounds use index*24 and min/max XYZ floats. The same indexing is independently used by updateContactDistance at 0x129140 and the explicit-array update at 0x1290a0.
- The decoder DOES NOT guess a bounds-array size field. Its caller supplies a separately verified span length and a coherent snapshot. Header pointers are cross-checked against that supplied view.

Evidence: simulation-phase-entry.json, simulation-end-phase.json, shape-simulation-bounds-candidates.json and simulation-bounds-update-callers.json under artifacts/native-research. Tools/inspect-physx-targets.py maps direct callers with instruction-boundary verification and labels unwind ranges as initial entries; split functions must be followed separately.

## Decoder contract

The decoder copies no persistent engine pointers into its output. It accepts an injected memory-copy reader and an owned or correctly borrowed bounds span, validates scene/callback identity, pair tag/ID/type, shape type, bounds range and finite nondegenerate boxes. Re-reading the structural fields rejects observable changes. This is not an ABA-proof concurrency mechanism and cannot manufacture a safe borrow from arbitrary live memory.

The output contains TWO SHAPE boxes, not complete actor/vehicle boxes. It must not be wired directly to the vehicle-wide ConfirmClear/SubmitNextChecked path. Existing complete-proxy coverage checks cannot be replaced by a single interaction's boxes.

Regression tests cover valid decoding, bad tags, pair-array range, cross-scene shape, bounds-index range, stale numeric ID, non-shape type, NaN boxes, observable replacement during reading, unchanged output on failure and address overflow.

## Timing and remaining integration

Upstream ShapeSim::updateCached reads simulation core transforms and updates bounds by element ID; updateSweptBounds may expand bounds across start/end poses for CCD. We have located installed update callers but have NOT established that every bound is current/conservative at the status callback across all supported paths. Public buffered actor poses are avoided by this route, but that alone is not a freshness proof.

Next: verify update/CCD ordering and bounded span length from installed code, then provide a phase-scoped borrowed view from the status-dispatch owner. Aggregate complete vehicle shape coverage or explicitly redesign/test a per-shape restoration invariant. No raw pointer retained from pairFound is considered safe by default. Native drain/disable handling remains required before suppression is enabled.

Primary source references:
- https://raw.githubusercontent.com/NVIDIAGameWorks/PhysX-3.4/master/PhysX_3.4/Source/SimulationController/src/ScScene.cpp
- https://raw.githubusercontent.com/NVIDIAGameWorks/PhysX-3.4/master/PhysX_3.4/Source/SimulationController/src/ScShapeSim.cpp
- https://raw.githubusercontent.com/NVIDIAGameWorks/PhysX-3.4/master/PhysX_3.4/Source/SimulationController/src/ScElementSim.h
