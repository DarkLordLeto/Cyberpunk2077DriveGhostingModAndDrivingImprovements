# Live-vehicle probe 0.4.0

## What is implemented

Read-only discovery of GetPlayer/GetMountedVehicle global functions, with unique-name, parameter-count, parameter-type and handle-return checks. SDK sizes are checked against live RTTI before inspecting entity/chassis/resource layouts. A mismatch disables the probe and logs DNC_LIVE_UNAVAILABLE.

Every two seconds during Running, obtain temporary handles to the player and mounted vehicle. Inspect attached vehicles only. Log entity identity, class, appearance and component names/types once per mount/vehicle change. For chassis components, read only already-loaded collision resources and log shape filter presets, simulation masks and query masks. No resource loads, physics writes, material overrides, input hooks or saved-game changes are requested.

This probe observes mounted vehicles, including passengers; it does not yet establish driver-only authorization. The eventual mutating feature must verify the driver seat. Resource filter values are asset data and are not proof of the live physics actor's current filters. No NPC-nearby scan is implemented yet.

## Test steps

1. Launch and load a normal save with access to vehicles.
2. Enter a stock car, remain seated for at least 10 seconds, then exit it.
3. If a motorcycle is readily available, enter it for at least 10 seconds too, then dismount.
4. Quit normally and let the developer inspect the log. No collision test or save overwrite is needed.

Expected markers: DNC_LIVE_READY, DNC_LIVE_VEHICLE, DNC_COMPONENT, DNC_RESOURCE / DNC_ASSET_FILTER, DNC_LIVE_COMPLETE. DNC_LIVE_UNAVAILABLE indicates a rejected signature/layout; do not bypass that check. An unloaded resource is reported rather than forcibly loaded.

## Validation

MSVC x64 Release compilation passed. Main/Query/Supports exports verified. Installed DLL matched the build hash. In-world runtime validation is pending. Previous 0.3.0 DLL is preserved under artifacts/0.3.0.

## Source investigation

- Let There Be Flight: https://github.com/jackhumbert/let_there_be_flight, commit 513ea3a0974f8f8399231a890c9f9b0ba196e171. FlightConfiguration.cpp::AddColliders accesses chassis physics proxies and applies simulation/query filters to newly created shapes. Its source-specific proxy layouts, synchronization and masks are not verified for this project and have not been copied into the plugin. FlightController.reds uses GetMountedVehicle(player), establishing the lookup as a relevant investigation target.
- Codeware: https://github.com/psiberx/cp2077-codeware, commit decce71323d6f6183cf78143882032a3c70aac9e. src/Red/Rendering.hpp exposes render-proxy visibility and material overrides; its opacity field belongs to HighlightParams. Neither is an established half-transparent vehicle-body implementation.
- RED4ext SDK remains pinned to 5246e659d783e5834cc08a90e56c8932c9ed3449. The probe uses SDK-owned entity, resource and handle interfaces. Reference repositories are research-only dependencies, not installed or linked into the plugin.

## Decision after the test

Use the observed car/bike chassis and filter presets to determine the next live physics inspection. Verify a per-instance shape/filter path and restoration mechanism before mutating anything. Vehicle fading still requires a supported per-instance material/fade mechanism; do not substitute highlights, fully hiding vehicles, or changing shared material opacity.

## 0.4.1 polling fix

The 0.4.0 in-world test reached DNC_LIVE_READY but captured no vehicles. Root cause: the Running callback returned true. RED4ext 1.31.0 StateSystem::Run removes callbacks returning true, contrary to the SDK Running-state comment. Verified against the official v1.31.0 implementation at https://github.com/WopsS/RED4ext/blob/v1.31.0/src/dll/StateSystem.cpp (locate StateSystem.cpp in the release tree if its directory changes).

0.4.1 returns false to retain the callback. It logs DNC_LIVE_POLL on the first poll and every 15 polls (approximately 30 seconds), and logs state changes such as waiting-for-player, player-not-mounted and vehicle-attached. This separates continued polling from lookup failures. The existing two-second throttle remains in place.

Release build passed. Installed hash and Main/Query/Supports exports verified on October 29, 2026. In-world validation is pending. Repeat the car/motorcycle test above; the previous run did not capture chassis data. No gameplay modifications are active.

## 0.4.1 runtime result
Both a vehicleCarBaseObject (161 components) and vehicleBikeBaseObject (89 components) were captured successfully. Each exposed one chassis resource body. No individual shape-filter records were emitted; see vehicle-results.md for limitations. Polling and clean shutdown verified. No new binary installed after this run.
