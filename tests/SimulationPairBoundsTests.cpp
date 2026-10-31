#include "SimulationPairBounds.hpp"
#include <array>
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <limits>
using namespace Dnc;
void Check(bool v,const char* m){if(!v){std::cerr<<m<<'\n';std::exit(1);}}
int main(){
    std::array<unsigned char,0x6000> memory{};
    auto put=[&]<class T>(uintptr_t p,T value){std::memcpy(memory.data()+p,&value,sizeof(value));};
    auto read=[&](uintptr_t p,void* out,std::size_t size){if(p>memory.size() || size>memory.size()-p)return false;std::memcpy(out,memory.data()+p,size);return true;};
    constexpr uintptr_t core=0x100,scene=0x1000,manager=0x3000,entries=0x3100,interaction=0x3200;
    constexpr uintptr_t shapeA=0x3300,shapeB=0x3400,actorA=0x3500,actorB=0x3600,array=0x3700,data=0x3800,callback=0x4000;
    put(core,scene);put(core+0x98,manager);put(scene+0x1078,callback);put(manager,entries);put(manager+8,uint32_t{1});
    put(entries,interaction);put(interaction+0x24,uint8_t{0});put(interaction+0x25,uint8_t{0x20});put(interaction+0x38,uint32_t{0});
    put(interaction+0x28,shapeA);put(interaction+0x30,shapeB);put(shapeA+0x10,actorA);put(shapeB+0x10,actorB);
    put(actorA+0x48,scene);put(actorB+0x48,scene);put(shapeA+0x18,uint32_t{0x80000000});put(shapeB+0x18,uint32_t{0x80000001});
    put(scene+0x818,array);put(array+8,data);
    std::array<SimulationBox,2> boxes{{{{0,0,0},{1,1,1}},{{3,0,0},{4,1,1}}}};
    SimulationBoundsView view{scene,array,data,boxes};SimulationPairBounds out{};
    auto decode=[&]{return DecodeSimulationPair(read,core,callback,0,view,out);};
    Check(decode() && out.firstIndex==0 && out.secondIndex==1 && CompareBounds(out.first,out.second)==BoundsRelation::Clear,"Valid simulation pair failed");
    put(entries,interaction|uintptr_t{2});Check(!decode(),"Invalid tagged pair accepted");put(entries,interaction);
    put(manager+8,uint32_t{0});Check(!decode(),"Out-of-range pair accepted");put(manager+8,uint32_t{1});
    put(actorB+0x48,scene+8);Check(!decode(),"Cross-scene shape accepted");put(actorB+0x48,scene);
    put(shapeB+0x18,uint32_t{2});Check(!decode(),"Out-of-range bounds index accepted");put(shapeB+0x18,uint32_t{1});
    put(interaction+0x38,uint32_t{9});Check(!decode(),"Reused pair mismatch accepted");put(interaction+0x38,uint32_t{0});
    put(shapeB+0x18,uint32_t{0x20000001});Check(!decode(),"Non-shape element accepted");put(shapeB+0x18,uint32_t{1});
    boxes[1].minimum[0]=std::numeric_limits<float>::quiet_NaN();Check(!decode(),"NaN bounds accepted");boxes[1].minimum[0]=3;
    unsigned reads=0;auto changing=[&](uintptr_t p,void* value,std::size_t size){
        if(p==entries && ++reads==2)put(entries,uintptr_t{0});return read(p,value,size);
    };
    out.firstIndex=99;Check(!DecodeSimulationPair(changing,core,callback,0,view,out) && out.firstIndex==99,"Changed pair published partial output");
    Check(!DecodeSimulationPair(read,std::numeric_limits<uintptr_t>::max()-1,callback,0,view,out),"Overflow address accepted");
    std::cout<<"Simulation pair bounds tests passed\n";
}
