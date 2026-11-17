#pragma once
#include <RED4ext/RED4ext.hpp>

namespace RaceTracking {
// Registers DriveNoCollision_TrackMappin(id: Uint64) -> Bool for scripts.
// Must be called while the plugin loads, before RTTI registration finishes.
void Register(RED4ext::v1::PluginHandle handle, const RED4ext::v1::Sdk* sdk);
}
