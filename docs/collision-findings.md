# Collision feasibility findings

## Runtime evidence: diagnostics 0.1.0

Verified against the October 28, 2026 22:16 startup log:
- Plugin loaded and unloaded successfully.
- Post-registration callback completed.
- vehicleBaseObject: size 3008, 2 declared properties, 105 declared functions.
- vehicleCarBaseObject: size 3168.
- vehicleChassisComponent: size 400, 2 declared properties.
- physicsFilterData: size 104, 4 declared properties.
- physicsCustomFilterData: size 96, 3 declared properties.
- physicsCollisionFilterResource: size 1848, 5 declared properties.
- physicsFilterDataSource was not a class. SDK source confirms it is an enum, so this is expected.

## SDK evidence (not yet a live mutation API)

The pinned SDK exposes this resource structure:

vehicleChassisComponent.collisionResource / optionalPlayerOnlyCollisionResource
-> physicsSystemResource.bodies
-> physicsSystemBody.collisionShapes
-> physicsICollider.filterData
-> physicsFilterData.queryFilter / simulationFilter / preset / customFilterData
-> physicsCustomFilterData.collisionType / collideWith / queryDetect

SimulationFilter contains two 64-bit masks. Their bit meanings are NOT verified. Separate simulation/query data is promising, but does not establish how changes propagate to live physics actors or preserve wheel support.

Do not edit shared resource data: it may affect other vehicles using the same asset. Before a ghosting prototype, determine the supported per-instance runtime interface, its threading requirements, filter ownership, and any required physics refresh. The optional player-only resource is a research lead, not a proven toggle.

## Next probe: diagnostics 0.2.0

Expanded one-time native reflection scan covers vehicle*, physics*, weather classes, and core entity/component ancestors. It records parent classes, properties, instance/static function signatures and parameter types without invoking those functions. This should identify relevant exposed operations and reduce repeated narrow probes.

0.2.0 Release build passed and installed hash matched. Runtime scan is pending. Source and SDK metadata alone cannot prove selective traffic ghosting works.

## 0.2.0 result and 0.3.0 correction

The 22:20:24 log reports 288 types, completion and clean unload. However, functions including value-returning getters were logged as Void and parameter records were absent. Their signatures are NOT verified: null return metadata was ambiguously labeled Void. Early callback timing is a working hypothesis, not a proven cause.

0.3.0 scans once during the Running lifecycle state, includes all ent-prefixed native metadata for render components, and labels absent return metadata unavailable-or-void. Build and installed-file hash checks passed. Runtime verification remains pending; if signatures are still incomplete, investigate SDK layout/script initialization instead of invoking those functions.

NPC body opacity has not been established. entRenderHighlightOpacityEvent controls highlight opacity and is not evidence of a vehicle-body fade API. No collision/visual mutations have been enabled.
