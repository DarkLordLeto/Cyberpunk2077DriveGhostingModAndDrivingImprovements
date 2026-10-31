#pragma once
#include "TrySnapshot.hpp"
#include "VehicleLifecycle.hpp"
#include <atomic>
namespace Dnc {
class DriverGate {
    struct Lease { VehicleIdentity id{}; uint64_t mount{},at{},revision{}; bool valid{}; };
    TrySnapshot<Lease> snapshot{Lease{}};
    std::atomic<uint64_t> revision{0};
public:
    // Single game-thread publisher. Revision changes invalidate an old snapshot
    // even when publishing its replacement skips due to a reader holding the slot.
    void Revoke() { revision.fetch_add(1,std::memory_order_acq_rel); }
    bool Observe(VehicleIdentity id,uint64_t mount,Fact driver,uint64_t now) {
        const auto rev=revision.fetch_add(1,std::memory_order_acq_rel)+1;
        return snapshot.Publish({id,mount,now,rev,driver==Fact::Yes && id.Valid()});
    }
    bool Allows(VehicleIdentity id,uint64_t mount,uint64_t now) const {
        const auto before=revision.load(std::memory_order_acquire);
        bool allowed=false;
        if(!snapshot.Read([&](const Lease& lease) {
            allowed=lease.valid && lease.revision==before && lease.id==id && lease.mount==mount &&
                now>=lease.at && now-lease.at<250;
        })) return false;
        return allowed && before==revision.load(std::memory_order_acquire);
    }
};
}
