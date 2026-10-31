#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

namespace Dnc {
enum class Fact : uint8_t { Unknown, No, Yes };
struct VehicleIdentity {
    uint64_t entity{};
    uint32_t proxy{0xffffffff}; // Includes the engine generation bits.
    bool operator==(const VehicleIdentity&) const = default;
    bool Valid() const { return entity!=0 && proxy!=0xffffffff; }
};
struct VehicleObservation {
    VehicleIdentity id{};
    Fact vehicle{}, playerOwned{}, traffic{}, playerDriver{};
    uint64_t observedAt{};
    bool present{};
};
enum class PairAssessment { KeepOriginal, TrafficCandidate };

// Value snapshot: mutable only on the game thread. A future runtime publisher must
// provide readers an immutable lifetime-safe copy; this class is NOT a concurrent container.
// TrafficCandidate is diagnostic classification, never permission to suppress contacts.
template<std::size_t Capacity=128>
class VehicleLifecycle {
    static_assert(Capacity>0);
    std::array<VehicleObservation,Capacity> observations{};
    VehicleIdentity driver{};
    uint64_t driverAt{}, epoch{1}, clock{};
    bool enabled{};
    uint64_t lifetime;
    bool Fresh(uint64_t at,uint64_t now) const {
        return now>=clock && now>=at && now-at<lifetime;
    }
public:
    explicit VehicleLifecycle(uint64_t freshnessTicks):lifetime(freshnessTicks) {}
    uint64_t Epoch() const { return epoch; }
    void Reset() {
        observations={}; driver={}; driverAt=0; enabled=false; clock=0; ++epoch;
    }
    void SetEnabled(bool value) { enabled=value; }
    bool Update(uint64_t sourceEpoch,VehicleObservation value,uint64_t now) {
        if(sourceEpoch!=epoch || now<clock || !value.id.Valid()) return false;
        clock=now;
        // Replace the entire observation, including unknown/negative facts.
        // Both entity-to-proxy changes and proxy-to-entity changes retire old mappings.
        for(auto& old:observations) {
            if(old.present && (old.id.entity==value.id.entity || old.id.proxy==value.id.proxy)) {
                if(old.id==driver && !(old.id==value.id)) driver={};
                old.present=false;
            }
        }
        for(auto& slot:observations) {
            if(!slot.present || !Fresh(slot.observedAt,now)) {
                value.present=true; value.observedAt=now; slot=value; return true;
            }
        }
        // No eviction of fresh observations. Unknown entries keep original collisions.
        return false;
    }
    bool Retire(uint64_t sourceEpoch,VehicleIdentity id,uint64_t now) {
        if(sourceEpoch!=epoch || now<clock) return false;
        clock=now;
        if(driver==id) driver={};
        for(auto& item:observations) {
            if(item.present && item.id==id) { item.present=false; return true; }
        }
        return false;
    }
    bool SetDriver(uint64_t sourceEpoch,VehicleIdentity id,Fact verified,uint64_t now) {
        if(sourceEpoch!=epoch || now<clock) return false;
        clock=now; driver={}; driverAt=now;
        if(verified==Fact::Yes && id.Valid()) driver=id;
        return true;
    }
    const VehicleObservation* Find(VehicleIdentity id,uint64_t now) const {
        for(const auto& item:observations)
            if(item.present && item.id==id && Fresh(item.observedAt,now)) return &item;
        return nullptr;
    }
    VehicleIdentity IdentityForProxy(uint32_t proxy,uint64_t now) const {
        for(const auto& item:observations)
            if(item.present && item.id.proxy==proxy && Fresh(item.observedAt,now)) return item.id;
        return {};
    }
    PairAssessment Assess(uint64_t sourceEpoch,VehicleIdentity a,VehicleIdentity b,uint64_t now) const {
        if(sourceEpoch!=epoch || !enabled || a==b || !driver.Valid() || !Fresh(driverAt,now))
            return PairAssessment::KeepOriginal;
        if(b==driver) { auto temp=a; a=b; b=temp; }
        if(!(a==driver)) return PairAssessment::KeepOriginal;
        const auto* player=Find(a,now); const auto* other=Find(b,now);
        if(!player || !other || player->vehicle!=Fact::Yes || player->playerDriver!=Fact::Yes ||
           other->vehicle!=Fact::Yes || other->playerOwned!=Fact::No || other->traffic!=Fact::Yes)
            return PairAssessment::KeepOriginal;
        return PairAssessment::TrafficCandidate;
    }
};
}
