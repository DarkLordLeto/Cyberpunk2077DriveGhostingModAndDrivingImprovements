#include "BodyBoundsInventory.hpp"
#include <array>
#include <cstdlib>
#include <iostream>
using namespace Dnc;
void Check(bool v,const char* m){if(!v){std::cerr<<m<<'\n';std::exit(1);}}
int main(){
    const WorldBounds front{{0,0,0,1},{2,2,2,1}},rear{{-5,-1,0,1},{1,3,4,1}};
    std::array<BodyBoundsSample,2> samples{{{1,rear},{0,front}}};WorldBounds out=front;
    Check(CombineBodyBounds(2,samples,out) && out.minimum[0]==-5 && out.maximum[1]==3 && out.maximum[2]==4,"Unordered full inventory union failed");
    WorldBounds nearby{{-4,0,0,1},{-3,1,1,1}};
    Check(CompareBounds(front,nearby)==BoundsRelation::Clear && CompareBounds(out,nearby)==BoundsRelation::Overlapping,"Additional body coverage counterexample failed");
    out=front;
    Check(!CombineBodyBounds(3,samples,out) && out.minimum[0]==front.minimum[0],"Partial inventory accepted/modified output");
    samples[1].index=1;Check(!CombineBodyBounds(2,samples,out),"Duplicate body accepted");
    samples[1].index=2;Check(!CombineBodyBounds(2,samples,out),"Out-of-range body accepted");
    samples[1]={0,front};samples[0].bounds.maximum[0]=-6;Check(!CombineBodyBounds(2,samples,out),"Malformed body accepted");
    Check(!CombineBodyBounds(0,{},out) && !CombineBodyBounds(65,{},out),"Empty/over-cap inventory accepted");
    std::array<BodyBoundsSample,64> maximum{};for(uint32_t i=0;i<64;++i)maximum[i]={i,front};
    Check(CombineBodyBounds(64,maximum,out),"Maximum complete inventory rejected");
    std::cout<<"Full-body union, missing/duplicate rejection and body-0 counterexample passed\n";
}
