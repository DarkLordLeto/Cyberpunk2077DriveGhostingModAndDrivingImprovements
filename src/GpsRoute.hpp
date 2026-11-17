#pragma once
#include <RED4ext/RED4ext.hpp>

namespace GpsRoute {
// Registers the road route natives used by race mode to place checkpoints:
//   DriveNoCollision_GpsRouteStart(from: Vector4, to: Vector4) -> Int32
//     Starts a GPS road route query. Returns the query id, or -1.
//   DriveNoCollision_GpsRoutePoll(id: Int32) -> Int32
//     0 while the route is being computed, the number of route points once it
//     is ready (the points are then cached), -1 if it failed, -2 if unavailable.
//   DriveNoCollision_GpsRoutePoint(index: Int32) -> Vector4
//     A point of the last route returned by Poll, ordered from `from` to `to`.
// Must be called while the plugin loads, before RTTI registration finishes.
void Register(RED4ext::v1::PluginHandle handle, const RED4ext::v1::Sdk* sdk);
}
