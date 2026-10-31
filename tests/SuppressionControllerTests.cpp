#include "SuppressionController.hpp"
#include <cstdlib>
#include <iostream>
#include <thread>
#include <vector>
using namespace Dnc;
void Check(bool v,const char* m){if(!v){std::cerr<<m<<'\n';std::exit(1);}}
int main(){
    const VehiclePairIdentity id{1,10,20,100,200};
    SuppressionController<4> c;
    Check(!c.Begin({1,0},id,0,0x3e61,1,1,false,true),"Ineligible intent accepted");
    auto first=c.Begin({1,0},id,0,0x3e61,1,1,true,true);
    Check(first.has_value(),"Initial pair rejected");
    PairUpdate output{};
    Check(c.SubmitNext(1,output) && output.pairFlags==0x3e60 && output.filterFlags==12,"Suppression flags wrong");
    c.StopAll();
    auto stale=c.SampleTicket(id);
    Check(stale.has_value() && !c.CanDetach(),"Stop abandoned overlap");
    Check(c.Lost(*first),"Loss rejected");
    Check(!c.ConfirmClear(*stale),"Pre-loss sample accepted");
    auto second=c.Begin({1,0},id,0,0x3e61,1,1,false,false);
    Check(second && second->lifetime!=first->lifetime,"Refilter continuity lost");
    Check(!c.Lost(*first),"Stale loss removed replacement");
    Check(c.SubmitNext(1,output) && output.pairFlags==0x3e60,"Disable restored overlapping refilter");
    auto clear=c.SampleTicket(id);
    Check(clear && c.ConfirmClear(*clear) && !c.ConfirmClear(*clear),"Clearance ticket replay accepted");
    Check(c.SubmitNextChecked(1,output,[](VehiclePairIdentity) noexcept {return true;}) && output.pairFlags==0x3e61,"Original flags not restored");
    Check(c.Lost(*second) && !c.CanDetach(),"Loss incorrectly proved clearance");
    clear=c.SampleTicket(id);
    Check(clear && c.ConfirmClear(*clear) && c.CanDetach(),"Clear detached intent retained");
    auto changed=id;changed.session++;
    Check(!c.Begin({1,0},changed,0,0x3e61,1,1,true,true),"Disabled controller accepted new session");
    SuppressionController<4> delayed;
    auto a=delayed.Begin({1,1},id,0,0x401,1,1,true,true);
    Check(a && delayed.SubmitNext(1,output),"Delayed setup failed");
    delayed.StopAll();auto evidence=delayed.SampleTicket(id);
    Check(evidence && delayed.ConfirmClear(*evidence),"Delayed clearance failed");
    auto b=delayed.Begin({1,2},id,0,0x401,1,1,false,false);
    Check(b && delayed.SubmitNext(1,output) && output.token==*b && output.pairFlags==0x400,
        "New contact did not invalidate queued restoration");
    Check(!delayed.SubmitNext(1,output),"Old restoration escaped after new contact");
    SuppressionController<1> recycle;
    for(uint32_t n=0;n<1000;++n) {
        auto identity=id;identity.trafficEntity+=n;identity.trafficProxy+=n;
        auto token=recycle.Begin({1,n},identity,0,0x401,1,1,true,true);
        Check(token.has_value(),"Enabled intent capacity leaked");
        Check(recycle.LostCurrent({1,n}),"Native key loss failed");
        auto ticket=recycle.SampleTicket(identity);
        Check(ticket && recycle.ConfirmClear(*ticket),"Enabled intent retirement failed");
    }
    // Real simultaneous loss/submission/reuse; no try-lock loss may disappear.
    SuppressionController<16> concurrent;
    std::vector<std::thread> workers;
    for(uint32_t t=0;t<4;++t) workers.emplace_back([&,t]{
        auto identity=id;identity.trafficEntity+=t;identity.trafficProxy+=t;
        for(uint32_t n=0;n<5000;++n){
            auto token=concurrent.Begin({t+1,n},identity,0,0x401,1,1,true,true);
            Check(token.has_value(),"Concurrent registration lost");
            PairUpdate value{};concurrent.SubmitNext(t+1,value);
            Check(concurrent.Lost(*token),"Concurrent loss dropped");
        }
    });
    for(auto& worker:workers) worker.join();
    Check(concurrent.ActivePairs()==0,"Concurrent bindings leaked");
    concurrent.StopAll();
    for(uint32_t t=0;t<4;++t){auto identity=id;identity.trafficEntity+=t;identity.trafficProxy+=t;
        auto ticket=concurrent.SampleTicket(identity);Check(ticket && concurrent.ConfirmClear(*ticket),"Intent cleanup failed");}
    Check(concurrent.CanDetach(),"Concurrent detach blocked after clearance");
    std::cout<<"Suppression controller tests passed\n";
}

