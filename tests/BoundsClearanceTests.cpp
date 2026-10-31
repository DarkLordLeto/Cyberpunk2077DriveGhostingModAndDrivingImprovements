#include "BoundsClearance.hpp"
#include <cstdlib>
#include <iostream>
#include <limits>
using namespace Dnc;
void Check(bool v,const char* m){if(!v){std::cerr<<m<<'\n';std::exit(1);}}
int main(){
    WorldBounds a{{-1,-2,-1,1},{1,2,1,1}},b=a;
    Check(CompareBounds(a,b)==BoundsRelation::Overlapping,"Identical boxes clear");
    b.minimum[0]=1;b.maximum[0]=3;
    Check(CompareBounds(a,b)==BoundsRelation::Overlapping,"Touching boxes clear");
    b.minimum[0]=1.5f;b.maximum[0]=3.5f;
    Check(CompareBounds(a,b)==BoundsRelation::Overlapping,"Exact margin accepted");
    b.minimum[0]=1.6f;
    Check(CompareBounds(a,b)==BoundsRelation::Clear && CompareBounds(b,a)==BoundsRelation::Clear,"Separation/order mismatch");
    for(unsigned axis=0;axis<3;++axis){b=a;b.minimum[axis]=a.maximum[axis]+1;b.maximum[axis]=b.minimum[axis]+2;Check(CompareBounds(a,b)==BoundsRelation::Clear,"Separating axis ignored");}
    b=a;b.maximum[1]=b.minimum[1];Check(CompareBounds(a,b)==BoundsRelation::Unknown,"Degenerate bounds accepted");
    b=a;b.minimum[2]=std::numeric_limits<float>::quiet_NaN();Check(CompareBounds(a,b)==BoundsRelation::Unknown,"NaN bounds accepted");
    b=a;b.maximum[0]=std::numeric_limits<float>::infinity();Check(CompareBounds(a,b)==BoundsRelation::Unknown,"Infinite bounds accepted");
    b=a;b.minimum[3]=0;Check(CompareBounds(a,b)==BoundsRelation::Unknown,"Unexpected output layout accepted");
    Check(CompareBounds(a,a,-1)==BoundsRelation::Unknown,"Negative margin accepted");
    std::cout<<"Bounds validity, touching, margin and symmetric separation cases passed\n";
}
