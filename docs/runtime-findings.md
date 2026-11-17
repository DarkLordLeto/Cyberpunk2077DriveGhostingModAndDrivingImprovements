# Runtime diagnostic result: 0.3.0

Source: drivenocollision-2026-10-28-22-27-18.log, preserved under artifacts/logs.

Successful load, Running-state scan, completion and unload. Scan count: 623 types. Parameter records: 782. Moving inspection later exposed substantially more function metadata. Native metadata still contains incomplete entries; do not assume every listed function is callable from script.

## Useful observed signatures

- worldWeatherScriptInterface.GetRainIntensity -> Float
- worldWeatherScriptInterface.GetRainMoisture -> Float
- worldWeatherScriptInterface.GetRainPuddles -> Float
- worldWeatherScriptInterface.GetRainIntensityType -> worldRainIntensity
- entPhysicalMeshComponent.CreatePhysicalBodyInterface(optional Int32 bodyIndex) -> handle:entPhysicalBodyInterface
- physicsSimulationFilter.SimulationFilter_BuildFromPreset(out physicsSimulationFilter f, CName preset)
- entEntity.FindComponentByName -> handle:entIComponent (full parameters in JSON index)

These are observed reflection signatures, not evidence that the functions have been successfully invoked by our mod.

## Remaining gaps

- PhysicalBodyInterface.SetSimulationFilter and SetQueryFilter are present by name, but no parameters are populated. Do not invoke either with a guessed signature.
- Vehicle chassis is not proven to expose the same body interface as a PhysicalMeshComponent.
- No verified whole-vehicle opacity setter was found. Highlight opacity, clear-coat opacity and UI animation fades are not substitutes for making an NPC vehicle half transparent.
- No live actor, collision mask or material has been modified.

## Next engineering step

Investigate SDK/source/native implementations of the body-filter functions and render-component controls, with per-instance ownership and restoration requirements. Use the captured metadata to choose a concrete world-level experiment before asking for another test. Repeating the same main-menu scan is unnecessary.

The weather getters offer a separate feasible lead for the later weather module. Obtaining the weather-system instance and validating values during actual rain remain required.
