# Complete simulation actor-shape unions — local implementation

Added SimulationActorBounds.hpp and SimulationActorBoundsTests after the pair-only decoder. Release build and all 16 test suites pass. These decoders remain snapshot-only and are not included in the live plugin. No installed DLL change or game run is needed.

Installed PhysX function 0x12b2b0 reads ActorSim+0x38 as the element-list head, follows ElementSim+8, filters element type bits at +0x18 and updates each shape with 0x1290a0. Its callers include an actor-update task at 0x111e70 and a split scene-update function entry at 0x112240. A separate task at 0x111e20 updates an explicit array of shapes. Evidence: bounds-update-phase-callers.json and bounds-update-scene.json in artifacts/native-research. A preliminary inspection of 0x148c80/0x148cd0 found element release and broadphase insertion paths, not the bounds-array allocation proof; do not use that artifact as length evidence.

The new decoder walks all elements belonging to each actor in a decoded contact. It verifies ownership and scene, rejects cycles, duplicate element indices, non-shape elements, invalid bounds, missing contact shapes, and inventories exceeding its fixed limit. It unions every admitted shape and rechecks links/IDs before publishing output. It does not allocate or retain engine pointers. Coherent snapshot ownership remains the caller's responsibility; rechecks do not prove live-memory synchronization or detect every ABA mutation.

The regression fixture demonstrates why this matters: the two contact shapes are clear, while an additional shape on the first actor overlaps the second actor. The actor union correctly reports overlap. Tests also exercise cyclic/foreign lists, duplicate bounds indices, truncated inventories, missing contact shape and mutation during traversal with unchanged output on failure.

Coverage: complete shapes for TWO ACTORS only. A chassis proxy can contain multiple actors, and a vehicle can have separate attached proxies. This result must not be treated as complete-vehicle separation without an independent coverage check.

Array size and timing status: upstream BoundsArray uses an allocator-backed array and can force its size to expanded capacity, including spare space for SIMD. Therefore size is an addressability limit, not a count of live shapes. We have NOT yet validated the installed BoundsArray size/capacity offsets or complete simulation/CCD ordering relative to statusChange. The decoder continues to require an independently supplied bounded span. No phase-scoped live reader was added.

Sources supporting array semantics and the next scheduling trace:
- https://github.com/NVIDIAGameWorks/PhysX-3.4/blob/master/PhysX_3.4/Source/LowLevelAABB/include/BpSimpleAABBManager.h
- https://github.com/NVIDIAGameWorks/PhysX-3.4/blob/master/PhysX_3.4/Source/SimulationController/src/ScScene.cpp

Next: prove allocation bounds from the installed allocator/init path, capture a phase-scoped simulation view, and validate multi-body coverage. Native disable/drain and rendering fade work remain outstanding before a functional ghosting release.
