#include "PairSuppressionPolicy.hpp"
#include "GhostShellPart.hpp"
#include "BikeBodyMap.hpp"
#include <cstdlib>
#include <iostream>
using namespace Dnc;
void Check(bool v,const char* m){if(!v){std::cerr<<m<<'\n';std::exit(1);}}
int main(){
    Check(SupportedDrivingContact(0x3e61) && SupportedDrivingContact(0x401) && SupportedDrivingContact(0x801),"Ordinary traffic flag variants rejected");
    Check(!SupportedDrivingContact(1) && !SupportedDrivingContact(0x402) && !SupportedDrivingContact(0x403),"Unsupported contact admitted");
    for(auto name:{"body_player_02","bumper_f_unremovable_01","Door_FL","wheel_br_01","tire_fl_01","fuelcap_01"}) Check(GhostShellPart(name),"Exterior shell excluded");
    for(auto name:{"interior_01","seat_fl_01","steering_wheel","wheel_brake","door_fl_stickers","door_inner","window_f","body_shadow","chassis","engine","AppearanceProxyMesh"}) Check(!GhostShellPart(name),"Internal or accessory mesh exposed");
    Check(PlanDrivingPair(0,0x3e61,1,1,true).filterFlags==2,"Borrowed/bike traffic contact still reported");
    Check(PlanDrivingPair(0,0x3e61,1,1,false).filterFlags==0,"Owned car filter changed");
    Check(!PlanDrivingPair(0,0x3e61,1,0,true).changed && !PlanDrivingPair(4,0x3e61,1,1,true).changed && !PlanDrivingPair(0,0x403,1,1,true).changed,"Protected pair suppressed");
    BikeBodyMap bodies;bodies.mount=123;bodies.entries[0]={7,{11,8},100,true};
    Check(DrivingAttribute(0x11,bodies.Lookup(123,7,99))==1,"Verified mounted character rejected");
    Check(DrivingAttribute(0x11,bodies.Lookup(124,7,99))==0x11 && DrivingAttribute(0x11,bodies.Lookup(123,7,100))==0x11,"Expired or different mount authorized");
    Check(DrivingAttribute(0,bodies.Lookup(123,7,99))==0 && DrivingAttribute(0x21,bodies.Lookup(123,7,99))==0x21,"Static or trigger character changed");
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
