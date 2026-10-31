#pragma once
#include "BoundsClearance.hpp"
#include <span>
#include <cstdint>
#include <algorithm>
namespace Dnc {
struct BodyBoundsSample { uint32_t index{}; WorldBounds bounds{}; };
inline bool CombineBodyBounds(uint32_t declared,std::span<const BodyBoundsSample> samples,WorldBounds& out) {
    if(declared==0 || declared>64 || samples.size()!=declared) return false;
    uint64_t seen=0;WorldBounds result{};bool first=true;
    for(const auto& sample:samples) {
        if(sample.index>=declared || !ValidBounds(sample.bounds)) return false;
        const auto bit=uint64_t{1}<<sample.index;
        if(seen&bit) return false;
        seen|=bit;
        if(first) { result=sample.bounds;first=false; }
        else for(unsigned axis=0;axis<3;++axis) {
            result.minimum[axis]=std::min(result.minimum[axis],sample.bounds.minimum[axis]);
            result.maximum[axis]=std::max(result.maximum[axis],sample.bounds.maximum[axis]);
        }
    }
    out=result;return true;
}
}
