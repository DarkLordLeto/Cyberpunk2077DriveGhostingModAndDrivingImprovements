#pragma once
#include <cstdint>
namespace Dnc {
struct PairDecision { uint16_t filterFlags{},pairFlags{}; bool changed{}; };
// Caller has classified the two actors and verified the driver.
inline PairDecision PlanNewTrafficContact(uint16_t filter,uint16_t pair,uint32_t attr0,uint32_t attr1) {
    PairDecision result{filter,pair,false};
    // Preserve non-default engine decisions, triggers, kinematic/static bodies,
    // and attributes outside the observed rigid-dynamic layout (exact value 1).
    if(filter!=0 || attr0!=1 || attr1!=1)
        return result;
    // eSOLVE_CONTACT alone controls solver response; keep reporting/detection bits.
    // Game-side damage/audio from retained reports requires separate validation.
    result.pairFlags=static_cast<uint16_t>(pair & ~uint16_t{1});
    result.changed=result.pairFlags!=pair;
    return result;
}
// Require an ordinary detected contact. Contact modification delegates response
// to a separate game callback, so those pairs keep the engine decision.
inline bool SupportedDrivingContact(uint16_t flags) {
    constexpr uint16_t solve=0x1, modify=0x2, detect=0x400|0x800;
    return (flags&solve) && !(flags&modify) && (flags&detect);
}
inline PairDecision PlanDrivingPair(uint16_t filter,uint16_t pair,uint32_t attr0,uint32_t attr1,bool contactFree) {
    if(!SupportedDrivingContact(pair)) return {filter,pair,false};
    auto decision=PlanNewTrafficContact(filter,pair,attr0,attr1);
    // For verified bike/borrowed-driver traffic only, eSUPPRESS prevents a
    // contact interaction (including reports) for this shape-pair lifetime.
    if(decision.changed && contactFree) decision.filterFlags=2;
    return decision;
}
inline PairDecision PlanTrafficContact(uint16_t filter,uint16_t pair,uint32_t attr0,uint32_t attr1,
    bool enabled,bool authorizedDriver,bool eligibleTraffic,bool lifecycleReady) {
    if(!enabled || !authorizedDriver || !eligibleTraffic || !lifecycleReady) return {filter,pair,false};
    return PlanNewTrafficContact(filter,pair,attr0,attr1);
}
}
