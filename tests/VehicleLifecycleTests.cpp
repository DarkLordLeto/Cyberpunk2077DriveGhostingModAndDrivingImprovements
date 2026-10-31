#include "VehicleLifecycle.hpp"
#include <cstdlib>
#include <iostream>
using namespace Dnc;
void Check(bool value,const char* message) { if(!value) { std::cerr<<message<<'\n'; std::exit(1); } }
int main() {
    VehicleLifecycle<4> state(10);
    const VehicleIdentity player{100,0x10001}, npc{200,0x10002}, world{300,0x10003};
    auto epoch=state.Epoch();
    const auto put=[&](VehicleIdentity id,Fact vehicle,Fact owned,Fact traffic,Fact driver,uint64_t now) {
        return state.Update(epoch,{id,vehicle,owned,traffic,driver},now);
    };
    Check(put(player,Fact::Yes,Fact::Yes,Fact::No,Fact::Yes,100),"Player insert failed");
    Check(put(npc,Fact::Yes,Fact::No,Fact::Yes,Fact::No,100),"Traffic insert failed");
    Check(put(world,Fact::No,Fact::Unknown,Fact::Unknown,Fact::Unknown,100),"World insert failed");
    state.SetDriver(epoch,player,Fact::Yes,100);
    const auto candidate=[&](VehicleIdentity a,VehicleIdentity b,uint64_t now) {
        return state.Assess(epoch,a,b,now)==PairAssessment::TrafficCandidate;
    };
    Check(!candidate(player,npc,100),"Enabled by default");
    state.SetEnabled(true);
    Check(candidate(player,npc,100) && candidate(npc,player,100),"Pair order mismatch");
    Check(!candidate(player,world,100) && !candidate(npc,world,100) && !candidate(player,player,100),"World/NPC/self pair accepted");
    Check(!candidate(player,npc,99) && !candidate(player,npc,110),"Future or expired data accepted");
    state.SetDriver(epoch,player,Fact::Unknown,101);
    Check(!candidate(player,npc,101),"Unknown driver retained");
    state.SetDriver(epoch,player,Fact::Yes,101);
    put(npc,Fact::Yes,Fact::No,Fact::Unknown,Fact::No,101);
    Check(!candidate(player,npc,101),"Unknown traffic retained positive state");
    put(npc,Fact::Yes,Fact::Yes,Fact::Yes,Fact::No,101);
    Check(!candidate(player,npc,101),"Player-owned target accepted");
    put(npc,Fact::Yes,Fact::No,Fact::Yes,Fact::No,102);
    const VehicleIdentity moved{200,0x20002};
    put(moved,Fact::Yes,Fact::No,Fact::No,Fact::No,103);
    Check(!state.Find(npc,103) && !candidate(player,moved,103),"Entity migration retained stale proxy/traffic");
    put(moved,Fact::Yes,Fact::No,Fact::Yes,Fact::No,104);
    Check(candidate(player,moved,104),"Refreshed migration rejected");
    Check(!state.Retire(epoch,npc,104) && candidate(player,moved,104),"Old proxy removal deleted migrated owner");
    Check(state.Retire(epoch,moved,104) && !candidate(player,moved,104),"Despawn did not invalidate owner");
    put(moved,Fact::Yes,Fact::No,Fact::Yes,Fact::No,104);
    const VehicleIdentity reused{400,0x20002};
    put(reused,Fact::No,Fact::Unknown,Fact::Unknown,Fact::No,105);
    Check(!state.Find(moved,105) && !candidate(player,reused,105),"Proxy reassignment retained old owner");
    Check(!put(npc,Fact::Yes,Fact::No,Fact::Yes,Fact::No,104),"Out-of-order observation accepted");
    state.Reset();
    Check(!state.Update(epoch,{npc,Fact::Yes,Fact::No,Fact::Yes},106),"Previous-session observation accepted");
    Check(!state.SetDriver(epoch,player,Fact::Yes,106),"Previous-session driver accepted");
    Check(!candidate(player,npc,106),"Previous-session pair accepted");
    VehicleLifecycle<1> bounded(2); const auto e=bounded.Epoch();
    Check(bounded.Update(e,{npc},1),"Bounded insertion failed");
    Check(!bounded.Update(e,{player},2) && bounded.Find(npc,2),"Fresh entry evicted on overflow");
    Check(bounded.Update(e,{player},3) && !bounded.Find(npc,3),"Expired slot not reclaimed");
    VehicleLifecycle<2> driverLife(10); const auto d=driverLife.Epoch();
    driverLife.SetEnabled(true);
    driverLife.Update(d,{player,Fact::Yes,Fact::Yes,Fact::No,Fact::Yes},0);
    driverLife.Update(d,{npc,Fact::Yes,Fact::No,Fact::Yes},0);
    driverLife.SetDriver(d,player,Fact::Yes,0);
    driverLife.Update(d,{player,Fact::Yes,Fact::Yes,Fact::No,Fact::Yes},9);
    driverLife.Update(d,{npc,Fact::Yes,Fact::No,Fact::Yes},9);
    Check(driverLife.Assess(d,player,npc,10)==PairAssessment::KeepOriginal,"Fresh owners extended expired driver authorization");
    std::cout<<"Lifecycle identity, freshness, migration, driver and capacity cases passed\n";
}
