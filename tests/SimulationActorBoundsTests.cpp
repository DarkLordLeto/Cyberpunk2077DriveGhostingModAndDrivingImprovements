#include "SimulationActorBounds.hpp"
#include <array>
#include <cstring>
#include <cstdlib>
#include <iostream>
using namespace Dnc;
void Check(bool v,const char* m){if(!v){std::cerr<<m<<'\n';std::exit(1);}}
int main(){
 std::array<unsigned char,0x6000> memory{};
 auto put=[&]<class T>(uintptr_t p,T v){std::memcpy(memory.data()+p,&v,sizeof(v));};
 auto read=[&](uintptr_t p,void* out,std::size_t n){if(p>memory.size() || n>memory.size()-p)return false;std::memcpy(out,memory.data()+p,n);return true;};
 constexpr uintptr_t core=0x100,scene=0x1000,manager=0x3000,entries=0x3100,interaction=0x3200;
 constexpr uintptr_t a=0x3300,b=0x3400,aa=0x3500,bb=0x3600,array=0x3700,data=0x3800,extra=0x3900,callback=0x4000;
 put(core,scene);put(core+0x98,manager);put(scene+0x1078,callback);put(manager,entries);put(manager+8,uint32_t{1});put(entries,interaction);
 put(interaction+0x25,uint8_t{0x20});put(interaction+0x28,a);put(interaction+0x30,b);
 put(a+0x10,aa);put(b+0x10,bb);put(extra+0x10,aa);put(aa+0x48,scene);put(bb+0x48,scene);
 put(a+0x18,uint32_t{0});put(b+0x18,uint32_t{1});put(extra+0x18,uint32_t{2});
 put(aa+0x38,a);put(bb+0x38,b);put(a+8,extra);put(scene+0x818,array);put(array+8,data);
 std::array<SimulationBox,3> boxes{{{{0,0,0},{1,1,1}},{{4,0,0},{5,1,1}},{{4,0,0},{6,1,1}}}};
 SimulationBoundsView view{scene,array,data,boxes};SimulationPairBounds contact{};SimulationActorBounds out{};
 Check(DecodeSimulationPair(read,core,callback,0,view,contact) && CompareBounds(contact.first,contact.second)==BoundsRelation::Clear,"Contact fixture not clear");
 auto decode=[&]{return DecodeSimulationActorBounds(read,core,callback,0,view,out);};
 Check(decode() && out.firstShapeCount==2 && out.secondShapeCount==1 && CompareBounds(out.first,out.second)==BoundsRelation::Overlapping,"Uncontacted overlapping shape was omitted");
 put(extra+8,a);Check(!decode(),"Cycle accepted");put(extra+8,uintptr_t{0});
 put(extra+0x10,bb);Check(!decode(),"Foreign actor link accepted");put(extra+0x10,aa);
 put(extra+0x18,uint32_t{0});Check(!decode(),"Duplicate bounds index accepted");put(extra+0x18,uint32_t{2});
 Check(!DecodeSimulationActorBounds<1>(read,core,callback,0,view,out),"Truncated inventory accepted");
 put(aa+0x38,extra);Check(!decode(),"Missing contact shape accepted");put(aa+0x38,a);
 unsigned reads=0;auto changing=[&](uintptr_t p,void* v,std::size_t n){if(p==a+8 && ++reads==2)put(a+8,uintptr_t{0});return read(p,v,n);};
 out.firstShapeCount=99;Check(!DecodeSimulationActorBounds(changing,core,callback,0,view,out) && out.firstShapeCount==99,"Inventory mutation published partial output");
 std::cout<<"Simulation actor bounds tests passed\n";
}
