#pragma once
#include "VehicleLifecycle.hpp"
namespace Dnc {
struct BikeBodyBinding {
    uint32_t raw{0xffffffff}; VehicleIdentity vehicle{};uint64_t expires{};bool character{};
};
struct BikeBodyMap {
    uint64_t mount{UINT64_MAX};std::array<BikeBodyBinding,64> entries{};
    BikeBodyBinding Lookup(uint64_t current,uint32_t raw,uint64_t now) const {
        if(current==mount && raw!=0xffffffff) for(const auto& e:entries) if(e.raw==raw && e.vehicle.Valid() && now<e.expires) return e;
        return {};
    }
};
inline uint32_t DrivingAttribute(uint32_t actual,const BikeBodyBinding& binding) {
    return actual==0x11 && binding.vehicle.Valid() && binding.character ? 1 : actual;
}
}
