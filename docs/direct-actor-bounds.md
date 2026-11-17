# Direct actor bounds investigation — 2026-10-30

Outcome: identified the installed rigid-dynamic bounds getter, but did not approve it as a restoration reader. No runtime code or installed DLL changed in this investigation. Installed 0.20.0 remains shadow-only. Previous 14 CTest suites apply to the unchanged C++ source; they were not rerun for these analysis scripts/docs.

## Installed binary evidence

PhysX3_x64.dll timestamp 1690925236, image size 2625536. Named exports and expected MSVC RTTI type descriptors were absent for the requested actor functions/types. A bounded code/vtable search found candidates by the hidden struct return buffer and XMM2 inflation argument, then identified actor types through their concrete-type-name virtual functions.

- Rigid dynamic vtables at RVAs 0x20ada8, 0x20b048 and 0x20b2e8 reference the type-name getter 0x4d8c0, whose string is PxRigidDynamic. These are multiple related tables; not every table is assumed to be an actual constructed runtime object's final table.
- Their +0x48 slot points to 0x4df30. It writes a 24-byte min/max XYZ box, applies inflation, and calls 0x68710 with actor+0x28 as shape-manager argument and the actor as a separate argument.
- Bounds helper 0x68710 crosses an unwind-entry boundary. Its first unwind entry stops before most of the body. The reviewed linear range through return is 0x68710–0x68a72 inclusive. It loads the shape count at manager+8, calls actor vslot +0xa0, iterates shapes, and computes their union. An indirect geometry-related call remains; no claim of an entirely audited lock-free call graph is made.
- The rigid-dynamic +0xa0 slot resolves to 0x4d990. It reads the pose at actor+0x140 and combines it with a body-to-actor transform selected by actor+0x17c bit 0x200 (actor+0xb0 or a buffer through actor+0x70). The assembly alone does not establish update timing.
- Rigid static and articulation candidates were identified separately; they are not admitted by the current dynamic-traffic suppression policy.

Artifacts: actor-bounds-candidates.json, direct-bounds-path.json, direct-bounds-full.json, direct-actor-pose.json under artifacts/native-research. The direct-bounds-path helper listing is deliberately only the initial unwind entry; use direct-bounds-full for its body. The exploratory 0x4b650 entry in that latter artifact is an unclassified bounded slice and is not part of the verified getter chain.

## Source cross-check and implication

NVIDIA's NpRigidActorTemplate::getWorldBounds calls its shape manager, which obtains actor.getGlobalPose and unions transformed shapes. NpRigidDynamic::getGlobalPoseFast uses Scb::Body::getBody2World. Scb::Body returns mBufferedBody2World, and syncState copies from the simulation body core when the applicable buffered-write flag is absent.

Sources:
- https://raw.githubusercontent.com/NVIDIAGameWorks/PhysX-3.4/master/PhysX_3.4/Source/PhysX/src/NpRigidActorTemplate.h
- https://raw.githubusercontent.com/NVIDIAGameWorks/PhysX-3.4/master/PhysX_3.4/Source/PhysX/src/NpShapeManager.cpp
- https://raw.githubusercontent.com/NVIDIAGameWorks/PhysX-3.4/master/PhysX_3.4/Source/PhysX/src/NpRigidDynamic.h
- https://raw.githubusercontent.com/NVIDIAGameWorks/PhysX-3.4/master/PhysX_3.4/Source/PhysX/src/buffering/ScbBody.h

Inference: moving the public getter into statusChange is insufficient to establish simulation-current clearance; the installed callback/synchronization ordering must be proven. The getter avoids the game proxy lookup but still requires a valid borrowed actor and stable shape inventory. Saved raw actor pointers are not authorized for later dereference merely because pairFound once supplied them. PxActor is not an independently retainable reference-counted actor through the public API used here.

## Next concrete route

Trace simulation-owned interaction/shape bounds from the already-reviewed status dispatch's pair manager entry. Determine current-transform/bounds timing and lifetime within that dispatch, rather than carrying raw actor pointers from pairFound or using buffered public poses. Establish the exact supported dynamic-pair layout and full body/attachment coverage before replacing the restoration reader. If using per-shape clearance instead, explicitly redesign the controller's vehicle-wide invariant rather than silently weakening it.

No new game launch/test is needed for this investigation. This does not change the outstanding requirements for enable/disable/seat transitions, restoring before hook removal, fading, or later handling/weather/input work.
