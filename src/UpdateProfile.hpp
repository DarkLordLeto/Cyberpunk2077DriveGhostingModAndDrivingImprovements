#pragma once
#include <array>
#include <chrono>
// Game-thread-only accumulators for the per-second DNC_UPDATE_SECTIONS line.
namespace DncProfile {
enum Section { GetPlayer, GetVehicle, DriverFlag, ChassisRead, OwnedFlag, DriverState, Nearby,
    Audit, Bodies, Fade, Discovery, OwnerRefresh, SectionCount };
inline std::array<long long, SectionCount> microseconds{};
inline std::chrono::steady_clock::time_point Now() { return std::chrono::steady_clock::now(); }
inline void Add(Section section, std::chrono::steady_clock::time_point since) {
    microseconds[section] += std::chrono::duration_cast<std::chrono::microseconds>(Now() - since).count();
}
}
