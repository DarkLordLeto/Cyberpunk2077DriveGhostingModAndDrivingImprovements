# Feasibility and implementation plan

## Collision experiment first

1. Identify the installed executable build and available RED4ext/redscript/CET versions.
2. Inspect matching SDK/RTTI and vehicle chassis physics data for supported collision filters. Determine whether the filter can distinguish other vehicles from static road/world surfaces, and whether it can be applied to a single live vehicle.
3. If script exposure is insufficient, investigate a RED4ext native contact/filter hook. Do not invent function names, offsets, filter bit masks or physics-engine assumptions. Record the verified symbol, signature, build and thread constraints before writing a hook.
4. Demonstrate one stock player car overlapping a stationary traffic vehicle while retaining steering, braking, suspension and road contact. Also test moving traffic: traffic actors may use different physics representations.
5. If selective filtering cannot be established, stop at a documented feasibility result; a global noclip switch is not the requested feature.

## Desired contact policy (not an engine API)

Suppress a contact only when ghosting is active, one participant is the current player-driven car or motorcycle, and the other participant is a verified vehicle target. Leave all unknown targets and world contacts unchanged. Wheel support queries must remain functional. Do not change shared global collision presets affecting every vehicle.

Player cars and motorcycles are both in scope. A stock car remains the first physics proof, followed by a stock motorcycle before calling ghosting complete. Pedestrians and roadside props remain outside the ghost-target scope.

## State and restoration

Off -> Active -> PendingDisable -> Off.

A request to disable while overlapping traffic must wait for a verified clearance check; restoring collision inside another car may cause a violent impulse. The clearance query must still detect ghosted targets. Define a recovery action if clearance cannot be reached, rather than restoring blindly.

Track vehicle identity and ownership of each changed value. Restore on vehicle exit/switch and handle unload, save load, scene transitions and shutdown using verified lifecycle events. Native hooks must not dereference stale entity pointers. Missing version support or failed symbol verification must leave ghosting unavailable. On unexpected state loss, avoid new modifications and use the verified cleanup path.

## Handling strategy

Provide independently configurable base handling and weather grip. Evaluate an existing handling mod as the baseline, but choose one owner for weather-related friction changes. Custom weather behavior is now explicitly in scope. Avoid layering multiple friction or steering writers; record compatibility before enabling companion mods.

## In-game acceptance checks

- Car against parked and moving traffic, at low and high speed.
- Ground contact on flat roads, slopes, bridges, curbs and after jumps.
- Walls/barriers remain solid; traffic-to-traffic collision remains normal.
- Ghost toggle off both clear of traffic and during overlap.
- Vehicle exit, vehicle switch, destruction, fast travel, save/reload and main menu.
- Passenger seat, scripted driving, AutoDrive and remote-control states remain excluded.
- Baseline without handling changes, then with exactly one chosen handling companion.
- Repeat across representative stock cars before adding motorcycles/custom vehicles.

A release requires an actual compiled plugin, verified game bindings, passing in-game checks and a documented compatibility matrix. This repository currently contains none of those release claims.

## Confirmed target and input requirements

User confirmed retail game version 2.5, traffic-only ghosting, and both keyboard/mouse and controller. The game is installing at D:\SteamLibrary\steamapps\common\Cyberpunk 2077. Development tools and public SDKs can support preparation; actual game data and a runnable retail installation are necessary to validate bindings and physics. No developer build is needed.

Ghosting must be independent of the input device. Provide a configurable toggle exposed through an in-game settings action, with separately bindable keyboard and controller actions after the input framework is verified. Preserve analog steering/throttle/brake values for controllers. Any future input smoothing must distinguish digital input from analog input and avoid applying keyboard-style ramps indiscriminately to a stick or trigger.


## Weather-dependent grip (confirmed requirement)

Fog and rain must reduce grip by separately configurable amounts. Initial tuning proposals, not measured physical coefficients: dry multiplier 1.00, fog 0.95, light rain 0.85, heavy rain 0.70. A fog penalty is an intentional gameplay setting; do not infer road wetness solely from visual fog density.

Represent weather state separately from the physics adapter. Verify access to current weather and transition intensity on build 2.5. If rain intensity is unavailable, expose clearly labeled discrete presets rather than fabricate an intensity signal. Unknown weather should smoothly return to baseline and log a diagnostic.

Blend toward a bounded grip target over time. Use distinct wetting and drying times, so traction changes gradually and wet-road effects can persist after rain. For overlapping fog/rain, initially use the stronger reduction, not the product; make this combination policy explicit. Do not repeatedly multiply the already-modified grip value each frame. Apply against a captured, owned baseline and restore it on disable/vehicle changes. Avoid shared vehicle records that would change NPC handling unintentionally.

Verify which native parameters control longitudinal and lateral tire friction. The intended result affects acceleration traction, cornering and braking, not merely steering sensitivity. Check existing vanilla wet-weather effects before adding a multiplier, to avoid accidental double penalties. Per-wheel surface detection and hydroplaning are later research, not promised features.

Acceptance: repeat dry/fog/light-rain/heavy-rain acceleration, braking and fixed-radius turns with identical car and inputs; test transitions, rapid toggles, save/load, unknown weather and baseline restoration. Compare multiple frame rates to ensure blending is time-based.

## Sim-racing controls (confirmed requirement)

Support calibrated analog steering, throttle and brake, alongside existing keyboard/mouse and gamepad paths. Hardware models are pending. Wheel, pedals, shifters and force feedback are confirmed requirements. Clutch and handbrake scope remain unspecified. Capture and calibrate shifter inputs when the device backend is available, but defer their custom transmission behavior to the later shifting phase; do not claim transmission support from button remapping alone.

Device profiles should include steering range, inversion, dead zones, saturation and pedal calibration; support independent USB pedal devices where the selected input backend permits it. Avoid keyboard input smoothing on wheel axes. Define one active driving input owner so wheel and gamepad values cannot overwrite each other. Keep mouse/on-foot controls usable, and support disconnect/reconnect with neutral input reset.

Evaluate existing wheel plugins before writing a new device backend. Their documented 2.31 support does not establish compatibility with our 2.5 target. If using a companion plugin, it owns wheel input and force output; our mod must not duplicate those hooks. Weather grip and ghosting should work independently of input hardware.

Force feedback is requested. Verify available telemetry and device support first. Distinguish telemetry-derived forces from synthetic centering. Provide adjustable gain and bounded output; stop forces on pause, focus loss, device disconnect and shutdown. Suppressed traffic contacts must not generate artificial crash kicks. Validate initially at low gain on the actual device.

Acceptance: full-range calibration, analog braking, simultaneous axes, multi-device inputs, input switching, disconnect/reconnect, menus/on-foot transitions, and weather/ghosting behavior across all supported control paths. Device and game-version support must be explicitly listed after testing.


## Analog throttle mapping (confirmed requirement)

Provide independent bindings for wheel/pedal devices and controller triggers. Normalize each calibrated throttle axis to [0, 1], with configurable inversion, minimum/maximum endpoints, dead zone and response curve. Preserve partial throttle instead of converting an axis into a digital accelerate action. Brake mapping follows the same calibration model. Keep keyboard throttle available through its own digital-input path.

Support separate USB pedals where the backend permits. Account for combined versus separate pedal axes explicitly; combined axes may prevent simultaneous throttle and braking and must be identified in setup. Device identity and calibration should persist without relying solely on enumeration order.

Input arbitration must select a deliberate active driving device rather than add competing throttle values together. Reset output on disconnect, focus loss and leaving player-driven mode. When switching devices, require the newly selected throttle control to return to neutral before accepting input, avoiding unexpected acceleration from a held axis.

Validate 0%, 25%, 50%, 75% and 100% throttle, inverted axes, dead zones, simultaneous throttle/brake, controller triggers, reconnect and switching between wheel, controller and keyboard. Check whether the base game already applies analog scaling before adding our response curve.

## Later phase: custom shifting and transmission audio

User explicitly defers custom shifting behavior because it needs additional engineering and sound effects. Keep the initial driving/ghosting/weather release compatible with existing game transmission behavior. The user's desired custom shifting model is a new feature request, not a verified claim that the base game has no automatic gear simulation.

Reserve normalized shift-up/shift-down and gear-selection inputs in the device abstraction. Do not simulate manual gears solely by playing sounds or mapping shifter buttons to unrelated game actions.

Later feasibility work must inspect existing gear, RPM, ratio and torque behavior before selecting a transmission implementation. Design engagement/disengagement, shift timing, torque interruption, gear-selection rules and optional clutch behavior together. Synchronize engine RPM sound and shift effects to the actual transmission state using verified audio interfaces and appropriately licensed assets. H-pattern versus sequential mode, reverse selection, clutch model and any stall/over-rev behavior require later design decisions.

Milestone order: (1) traffic ghosting proof, (2) baseline handling and configurable weather grip, (3) calibrated wheel/pedal/controller input and force feedback with shifter detection/bindings, (4) custom transmission behavior and audio. Research may overlap, but shifting must not block the initial milestones.


## NPC-vehicle ghost visuals (confirmed requirement)

During ghosting, an NPC vehicle intersecting the player-driven car or motorcycle must fade toward 50% opacity and return to its prior appearance after separation. Only overlapping targets fade; the player vehicle stays fully visible. Use short time-based fade-in/out transitions, with exact durations tuned in-game. Consider a small approach margin and swept overlap checks to avoid missing high-speed pass-throughs. Track each NPC vehicle independently when multiple overlaps occur.

Trigger detection must remain operational when contact response is disabled. Maintain the same target identity and overlap state for physics and visuals; do not depend on collision-hit callbacks that ghosting suppresses. Use hysteresis to avoid visual flicker at overlap boundaries. Do not restore collision while still overlapping when toggling ghosting off.

Investigate a per-instance mesh fade/render-proxy or material override before implementation. Highlight opacity is not mesh opacity. Opaque vehicle materials may require supported dithering or a different rendering technique; 50% numeric alpha alone is not proof of half transparency. Validate body, wheels, glass, lights, decals and damage meshes, plus first/third-person views. NPC occupants may remain visible inside a faded body; assess the appearance before deciding whether occupant meshes also need fading.

Never modify shared materials globally. Preserve original appearance and restore only overrides owned by this mod. Handle LOD/component changes, streaming/despawn, destruction, save load and shutdown without stale pointers or persistent translucent vehicles. Verify no ghost impact effects or crash force-feedback impulses occur for suppressed traffic contacts.

Acceptance: car and motorcycle pass-throughs; stationary/moving traffic; multiple targets; high speed; fading back after separation; no effect on unrelated NPC cars; no residual fade after disable/reload. Ghosting is not feature-complete until both physical pass-through and these visuals work.
