#pragma once
#include "FilterSnapshot.hpp"
namespace Dnc {
// Offline model of RVA 0x320794's ordinary (non-trigger) branch.
// This is not a runtime hook or an authorization to modify either mask.
struct OrdinaryPairResult { uint16_t filterFlags{}, pairFlags{}; };
constexpr OrdinaryPairResult EvaluateOrdinaryPair(SimulationMask a,SimulationMask b,uint16_t priorPairFlags=0) {
    if (!((a.category & b.contacts) | (b.category & a.contacts))) return {2,priorPairFlags};
    const bool callbacks=((a.contacts | b.contacts) & 0xc000000000000000ULL)!=0;
    return {uint16_t(callbacks ? 4 : 0),uint16_t(priorPairFlags | 0xc01)};
}
}
