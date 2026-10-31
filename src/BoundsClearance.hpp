#pragma once
#include <cmath>
namespace Dnc {
struct WorldBounds { float minimum[4]{},maximum[4]{}; };
static_assert(sizeof(WorldBounds)==32);
enum class BoundsRelation { Unknown, Overlapping, Clear };
inline bool ValidBounds(const WorldBounds& b) {
    for(unsigned i=0;i<3;++i) if(!std::isfinite(b.minimum[i]) || !std::isfinite(b.maximum[i]) ||
        b.maximum[i]<=b.minimum[i]) return false;
    return b.minimum[3]==1.0f && b.maximum[3]==1.0f;
}
// Observation only: asynchronous bounds do not prove clearance at a later
// physics update. Overlapping AABBs are conservative, not exact mesh overlap.
inline BoundsRelation CompareBounds(const WorldBounds& a,const WorldBounds& b,double margin=0.5) {
    if(!ValidBounds(a) || !ValidBounds(b) || !std::isfinite(margin) || margin<0) return BoundsRelation::Unknown;
    for(unsigned i=0;i<3;++i)
        if(double(a.maximum[i])+margin<double(b.minimum[i]) || double(b.maximum[i])+margin<double(a.minimum[i]))
            return BoundsRelation::Clear;
    return BoundsRelation::Overlapping;
}
}
