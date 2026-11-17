# Car and motorcycle runtime results (0.4.1)

Verified October 29, 2026 from drivenocollision-2026-10-29-12-20-14.log.

| Observation | Car | Motorcycle |
| --- | --- | --- |
| Runtime class | vehicleCarBaseObject | vehicleBikeBaseObject |
| Appearance | yaiba_semimaru_basic_urban_01 | yaiba_kusanagi_basic_player_01 |
| Entity ID in this session | 9021325 | 9021329 |
| Component count | 161 | 89 |
| Chassis component | Chassis / vehicleChassisComponent | Chassis / vehicleChassisComponent |
| Loaded chassis resource body count | 1 | 1 |
| Optional player-only resource | Not loaded | Not loaded |
| Individual asset-filter records | None emitted | None emitted |

Polling reached at least 60 polls. Player lookup, mounted-vehicle lookup, entity/component traversal, resource access, unmount detection and clean shutdown all worked. One car was captured twice after remounting.

## What this proves

The SDK-based read-only path reaches both real car and bike chassis resources in this game build. The callback lifetime fix is runtime-verified.

## What it does not prove

No DNC_ASSET_FILTER records were emitted. The 0.4.1 probe silently skips null body/shape/filter handles, empty shape lists and its size caps, so the log cannot distinguish those cases. Do not interpret this as disabled collisions, empty masks, or an absent physics body. The body-level SystemBodyParams structure inspected in the SDK contains dynamics parameters, not an exposed filter handle.

Chassis resource data is not the active physics proxy. Next investigation should trace live chassis proxy/shape access and ownership, and add explicit diagnostics for skipped resource cases if another probe is built. Do not request an identical user test without an implementation change.

No collision, material, weather, handling or input changes were enabled. Half-transparent NPC vehicles remain an unresolved rendering implementation.
