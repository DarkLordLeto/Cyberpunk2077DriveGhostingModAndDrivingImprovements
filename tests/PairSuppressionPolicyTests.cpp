#include "PairSuppressionPolicy.hpp"
#include <cstdlib>
#include <iostream>
using namespace Dnc;
void Check(bool v,const char* m){if(!v){std::cerr<<m<<'\n';std::exit(1);}}
int main(){
    for(uint32_t bits=0;bits<=0xffff;++bits){
        const auto flags=static_cast<uint16_t>(bits);
        auto r=PlanTrafficContact(0,flags,1,1,true,true,true,true);
        Check(r.filterFlags==0 && r.pairFlags==(flags & 0xfffe) && r.changed==bool(flags&1),"Unrelated flag changed");
        for(unsigned gates=0;gates<15;++gates){
            r=PlanTrafficContact(0,flags,1,1,bool(gates&1),bool(gates&2),bool(gates&4),bool(gates&8));
            Check(!r.changed && r.pairFlags==flags,"Missing eligibility/lifecycle gate changed pair");
        }
    }
    for(uint32_t filter=1;filter<=0xffff;++filter){
        const auto r=PlanTrafficContact(static_cast<uint16_t>(filter),0x3e61,1,1,true,true,true,true);
        Check(!r.changed && r.filterFlags==filter && r.pairFlags==0x3e61,"Engine filter decision overridden");
    }
    for(uint32_t attr:{0u,2u,0x11u,0x21u,0xffffffffu}){
        Check(!PlanTrafficContact(0,0x3e61,attr,1,true,true,true,true).changed,"Unsupported first actor accepted");
        Check(!PlanTrafficContact(0,0x3e61,1,attr,true,true,true,true).changed,"Unsupported second actor accepted");
    }
    Check(PlanTrafficContact(0,0x3e61,1,1,true,true,true,true).pairFlags==0x3e60,"Captured pair conversion incorrect");
    std::cout<<"Suppression policy gates and exhaustive flag preservation passed\n";
}
