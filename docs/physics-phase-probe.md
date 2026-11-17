# Physics phase bounds probe: 0.21.0

Prepared for the first runtime test of simulation-owned bounds. This build still preserves collision response. No fading or actual solver suppression has been enabled. It includes the locally tested restoration freshness gate and simulation decoders.

PhysicsPhaseProbe attaches to the already guarded PhysX status-dispatch function at RVA 0x12e700. Its observed ABI is void(owner, contactManagerOutputs, bool adaptiveForce). A thread-local owner is valid only during the original dispatcher call and is restored on exit. Native pair IDs supplied to the unchanged-flags status echo are decoded in that scope. This is a new hook/ABI integration and requires runtime validation before being treated as working.

Bounds length is now independently supported by installed code: origin-shift function 0xf8610 iterates BoundsArray+0x10 entries and indexes BoundsArray+8 with stride 24. Element initialization at 0x148b70 reads capacity at +0x14 (high allocator bit masked), reserves when needed and updates size. The probe accepts a nonempty size no greater than masked capacity and caps it at 1,048,576 addressable entries. This size is not a live-shape count. Relevant code regions are guarded by VerifiedPhaseLayout.hpp, generated from the installed DLL.

Memory fields and individual boxes are copied through ReadProcessMemory, not unchecked dereferences or a game proxy-manager call. Failed copies produce invalid/rejected samples. No heap allocation or logging occurs inside the physics callback. Work is limited to 16 total samples and at most 128 linked elements per actor. Kernel copies add overhead; this is a validation path, not a final per-frame implementation. Fields are rechecked after decoding. This still does not prove every simulation bound's update freshness or complete multi-body vehicle coverage.

DNC_PHASE_SAMPLE records phase, header, pairRead and actorRead success; array length; shape counts; pair/actor bounds relation (0 unknown, 1 overlapping, 2 clear); microseconds; collisionWrites=0. DNC_PHASE_RESTORED confirms dispatch hook detachment. Existing pair/status hooks retain their cleanup behavior.

Validation request once installed: load a save, gently contact two NPC traffic cars using either your car or motorcycle, back away several car lengths, then exit. Logs must show DNC_PHASE_READY, valid pair/actor decodes, sensible shape counts, bounded sample times and clean hook restoration. A successful run verifies layout/ABI observations only, not actual ghosting or restoration. No repeat of earlier shadow-only behavior is being requested; this exercises the newly added dispatch hook and simulation-bound reader.

Rendering investigation remains unresolved: SDK vehicle clear-coat opacity and Codeware highlight opacity are not full vehicle transparency. Do not use them as substitutes for the requested fade.
