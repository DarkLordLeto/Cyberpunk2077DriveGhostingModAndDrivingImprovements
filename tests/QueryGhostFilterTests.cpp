#include "QueryGhostFilter.hpp"
#include <atomic>
#include <cstdlib>
#include <iostream>
#include <thread>
#include <vector>
void Check(bool v,const char* m){if(!v){std::cerr<<m<<'\n';std::exit(1);}}
struct Actor { unsigned char bytes[0x18]{}; explicit Actor(uint32_t id){std::memcpy(bytes+0x10,&id,4);} };
struct Game final : Dnc::QueryFilterCallback {
    int pre=Dnc::QueryHitTouch, post=Dnc::QueryHitNone; unsigned preCalls=0, postCalls=0;
    int preFilter(const void*,const void*,const void*,uint16_t* flags) override { ++preCalls; if(flags) *flags=7; return pre; }
    int postFilter(const void*,const void*) override { ++postCalls; return post; }
};
int main(){
    Dnc::GhostProxySet set;
    Check(!set.Active(100) && !set.Contains(5,100),"Empty set reported an entry");
    Check(!set.Add(0,100,50) && !set.Add(0xffffffff,100,50) && !set.Add(5,100,0),"Invalid entry accepted");
    Check(set.Add(5,100,50) && set.Active(149) && set.Contains(5,149),"Entry missing before expiry");
    Check(!set.Contains(5,150) && !set.Active(150) && !set.Contains(6,120),"Expiry or identity failed");
    Check(set.Add(5,120,100) && set.Contains(5,219) && !set.Contains(5,220),"Renewal failed");
    Check(set.Add(5,130,10) && set.Contains(5,219),"Shorter renewal shortened the entry");
    for(uint32_t i=0;i<24;++i) Check(set.Add(1000+i,300,100),"Capacity entry rejected");
    Check(!set.Add(2000,300,100) && set.Add(2000,400,100) && set.Contains(2000,450) && !set.Contains(1000,450),"Slot reuse failed");
    set.Clear();
    Check(!set.Active(401) && !set.Contains(2000,401),"Clear left an entry");

    // Plans mirror the installed PhysX default hit type and never invent a postfilter.
    auto p=Dnc::PlanGhostQuery(0x0003,false,0);
    Check(p.flags==0x0007 && !p.forwardPre && !p.forwardPost && p.fallback==Dnc::QueryHitBlock,"Plain query plan wrong");
    p=Dnc::PlanGhostQuery(0x0003,false,4);
    Check(p.fallback==Dnc::QueryHitTouch,"Touch buffer default wrong");
    p=Dnc::PlanGhostQuery(0x8003,false,0);
    Check(p.fallback==Dnc::QueryHitTouch,"Reserved flag default wrong");
    p=Dnc::PlanGhostQuery(0x000b,false,0);
    Check(p.flags==0x0007 && !p.forwardPost,"Postfilter kept without a callback");
    p=Dnc::PlanGhostQuery(0x000f,true,0);
    Check(p.flags==0x000f && p.forwardPre && p.forwardPost,"Game callback plan wrong");
    p=Dnc::PlanGhostQuery(0x0003,true,0);
    Check(p.flags==0x0007 && !p.forwardPre,"Unrequested game prefilter forwarded");

    Check(set.Add(77,500,100),"Ghost add failed");
    Actor ghost(77), other(78);
    Game game; uint16_t flags=1;
    {
        Dnc::GhostQueryFilter filter(set,&game,Dnc::PlanGhostQuery(0x000f,true,0),550);
        Check(filter.preFilter(nullptr,nullptr,&ghost,&flags)==Dnc::QueryHitNone && game.preCalls==0 && flags==1 && filter.rejected==1,"Ghost actor not dropped");
        Check(filter.preFilter(nullptr,nullptr,&other,&flags)==Dnc::QueryHitTouch && game.preCalls==1 && flags==7,"Game prefilter not forwarded");
        Check(filter.postFilter(nullptr,nullptr)==Dnc::QueryHitNone && game.postCalls==1,"Game postfilter not forwarded");
        Check(filter.preFilter(nullptr,nullptr,nullptr,&flags)==Dnc::QueryHitTouch,"Null actor not forwarded");
    }
    {
        Dnc::GhostQueryFilter filter(set,nullptr,Dnc::PlanGhostQuery(0x0003,false,0),550);
        Check(filter.preFilter(nullptr,nullptr,&other,&flags)==Dnc::QueryHitBlock,"Default block missing");
        Check(filter.preFilter(nullptr,nullptr,&ghost,&flags)==Dnc::QueryHitNone,"Ghost actor not dropped without game callback");
    }
    {
        Dnc::GhostQueryFilter filter(set,nullptr,Dnc::PlanGhostQuery(0x0003,false,8),600);
        Check(filter.preFilter(nullptr,nullptr,&ghost,&flags)==Dnc::QueryHitTouch,"Expired ghost still dropped");
    }
    // Concurrent writers and readers: no torn state may report a proxy never added.
    Dnc::GhostProxySet shared; std::atomic<bool> stop{false}; std::atomic<unsigned> bad{0};
    std::vector<std::thread> threads;
    for(unsigned n=0;n<4;++n) threads.emplace_back([&,n]{ for(uint64_t t=1;t<200000;++t) shared.Add(100+(t+n)%40,t,3); });
    for(unsigned n=0;n<2;++n) threads.emplace_back([&]{ while(!stop.load()) if(shared.Contains(9999,1) || shared.Contains(0,1)) ++bad; });
    for(unsigned n=0;n<4;++n) threads[n].join();
    stop=true; threads[4].join(); threads[5].join();
    Check(bad.load()==0,"Reader saw a proxy that was never added");
    std::cout<<"QueryGhostFilterTests passed\n";
}
