#include "PairLifetimeRegistry.hpp"
#include <cstdlib>
#include <iostream>
using namespace Dnc;
void Check(bool v,const char* m){if(!v){std::cerr<<m<<'\n';std::exit(1);}}
int main(){
    PairLifetimeRegistry<2> registry;
    auto a=registry.Begin({100,0},0,0x3e61,1,1);
    auto b=registry.Begin({200,0},0,0x3e61,1,1);
    Check(a && b && registry.Active()==2,"Scoped registration failed");
    Check(!registry.Begin({100,0},0,1,1,1) && !registry.Begin({300,0},0,1,1,1),"Duplicate/capacity rejection failed");
    Check(!registry.RequestSuppression(*a,true,true,true,false),"Missing restoration readiness accepted");
    Check(registry.RequestSuppression(*a,true,true,true,true),"Suppression request rejected");
    Check(!registry.Prepare(200),"Pending update crossed scene scope");
    auto suppress=registry.Prepare(100);
    Check(suppress && suppress->pairFlags==0x3e60 && suppress->filterFlags==0x0c,"Suppression flag preservation failed");
    Check(!registry.Prepare(100),"Duplicate in-flight submission");
    Check(registry.Submitted(*suppress) && !registry.Submitted(*suppress),"Receipt duplicated");
    registry.RequestStop(*a);
    Check(!registry.Prepare(100),"Restored while overlap clearance unknown");
    registry.ConfirmClear(*a);
    auto restore=registry.Prepare(100);
    Check(restore && restore->pairFlags==0x3e61 && restore->filterFlags==0x0c,"Original flags not restored");
    Check(registry.Submitted(*restore) && !registry.Prepare(100),"Restoration did not settle");
    Check(registry.Lost(*a),"Lifetime removal failed");
    auto reused=registry.Begin({100,0},0,0x401,1,1);
    Check(reused && reused->lifetime!=a->lifetime,"Reused pair ID inherited lifetime");
    Check(!registry.Lost(*a) && !registry.Submitted(*restore) && !registry.ConfirmClear(*a),"Stale events modified replacement pair");
    registry.RequestSuppression(*reused,true,true,true,true);
    auto pending=registry.Prepare(100);
    registry.Lost(*reused);
    Check(!registry.Submitted(*pending),"Lost pair accepted delayed submission");
    registry.Reset();
    Check(registry.Active()==0 && !registry.RequestStop(*b),"Reset retained old lifetime");
    auto fresh=registry.Begin({200,0},0,0x3e61,1,1);
    Check(fresh && fresh->lifetime!=b->lifetime,"Reset reused lifetime token");
    registry.RequestSuppression(*fresh,true,true,true,true);
    auto flight=registry.Prepare(200);
    registry.RequestStop(*fresh);registry.ConfirmClear(*fresh);
    registry.Submitted(*flight);
    auto correction=registry.Prepare(200);
    Check(correction && correction->pairFlags==0x3e61,"Stop during in-flight suppression lost restoration");
    registry.Submitted(*correction);
    Check(!registry.Prepare(200),"Correction repeated");
    registry.Reset();fresh=registry.Begin({1,1},0,0x3e61,1,1);
    registry.RequestSuppression(*fresh,true,true,true,true);registry.RequestStop(*fresh);
    Check(!registry.Prepare(1),"Cancelled unsent suppression emitted");
    Check(!registry.Begin({2,2},2,0x3e61,1,1),"Overrode nondefault engine filter");
    registry.Reset();fresh=registry.Begin({1,1},0,0x3e61,1,1);
    registry.RequestSuppression(*fresh,true,true,true,true);
    auto cancelled=registry.Prepare(1);
    Check(registry.Cancel(*cancelled),"Cancel failed");
    auto retry=registry.Prepare(1);
    Check(retry && retry->revision!=cancelled->revision,"Retry reused submission ticket");
    Check(!registry.Submitted(*cancelled) && !registry.Cancel(*cancelled),"Cancelled ticket affected retry");
    auto forged=*retry;forged.pairFlags=0;
    Check(!registry.Submitted(forged) && !registry.Cancel(forged),"Changed payload accepted");
    Check(registry.Submitted(*retry),"Valid retry rejected");
    std::cout<<"Scoped lifetimes, stale receipts, clear-before-restore and in-flight stop cases passed\n";
}
