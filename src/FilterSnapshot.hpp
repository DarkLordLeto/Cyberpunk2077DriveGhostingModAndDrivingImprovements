#pragma once
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace Dnc {
struct SimulationMask {
    uint64_t category{}, contacts{};
    bool operator==(const SimulationMask&) const = default;
};
struct VehicleLifetime {
    uint64_t entity{}, epoch{};
    uint32_t proxy{};
    bool operator==(const VehicleLifetime&) const = default;
};
struct ShapeMask {
    uint32_t body{}, shape{};
    SimulationMask value{};
};
struct ShapeEdit {
    ShapeMask original;
    SimulationMask applied;
};
enum class RestoreAction { WrongLifetime, MissingShape, AlreadyOriginal, RestoreOriginal, Conflict };
struct RestoreStep {
    uint32_t body{}, shape{};
    RestoreAction action{};
    std::optional<SimulationMask> write;
};

// Pure planning only. The future engine adapter must establish a non-reused
// lifetime epoch, execute under valid physics synchronization, drain queued
// writes, and re-read/revalidate immediately before each write. These checks
// alone cannot establish shape identity after a body is rebuilt or reordered.
class FilterSnapshot {
public:
    static std::optional<FilterSnapshot> Capture(VehicleLifetime owner,
                                                 std::span<const ShapeMask> shapes,
                                                 bool inventoryComplete) {
        if (!owner.entity || !owner.epoch || !inventoryComplete || shapes.empty()) return std::nullopt;
        for (size_t i=0; i<shapes.size(); ++i) {
            // Reserve both native/deferred all-shape sentinel encodings.
            if (shapes[i].body >= 255 || shapes[i].shape >= 255) return std::nullopt;
            for (size_t j=0; j<i; ++j)
                if (shapes[i].body==shapes[j].body && shapes[i].shape==shapes[j].shape) return std::nullopt;
        }
        return FilterSnapshot(owner,shapes);
    }

    // Record only after the engine adapter has observed an applied write.
    // Repeated edits retain the first original value for eventual restoration.
    bool ConfirmApplied(VehicleLifetime owner, uint32_t body, uint32_t shape,
                        SimulationMask expectedBefore, SimulationMask intended,
                        SimulationMask readback) {
        if (owner!=owner_ || readback!=intended) return false;
        for (auto& edit: edits_) {
            if (edit.original.body!=body || edit.original.shape!=shape) continue;
            if (edit.applied!=expectedBefore) return false;
            edit.applied=intended;
            return true;
        }
        return false;
    }

    std::vector<RestoreStep> PlanRestore(VehicleLifetime owner, std::span<const ShapeMask> current) const {
        std::vector<RestoreStep> result;
        for (const auto& edit: edits_) {
            if (edit.applied==edit.original.value) continue; // No outstanding owned change.
            RestoreStep step{edit.original.body,edit.original.shape,RestoreAction::WrongLifetime,{}};
            if (owner==owner_) {
                const ShapeMask* match=nullptr;
                bool duplicate=false;
                for (const auto& value: current) {
                    if (value.body!=step.body || value.shape!=step.shape) continue;
                    if (match) duplicate=true;
                    match=&value;
                }
                if (duplicate) step.action=RestoreAction::Conflict;
                else if (!match) step.action=RestoreAction::MissingShape;
                else if (match->value==edit.original.value) step.action=RestoreAction::AlreadyOriginal;
                else if (match->value==edit.applied) {
                    step.action=RestoreAction::RestoreOriginal;
                    step.write=edit.original.value;
                } else step.action=RestoreAction::Conflict;
            }
            result.push_back(step);
        }
        return result;
    }
private:
    FilterSnapshot(VehicleLifetime owner, std::span<const ShapeMask> shapes):owner_(owner) {
        for (const auto& shape:shapes) edits_.push_back({shape,shape.value});
    }
    VehicleLifetime owner_;
    std::vector<ShapeEdit> edits_;
};
}
