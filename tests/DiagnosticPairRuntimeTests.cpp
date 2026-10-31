#include "DiagnosticPairRuntime.hpp"
#include <cstdlib>
#include <iostream>
#include <thread>
#include <vector>
using namespace Dnc;
void Check(bool v,const char* m){if(!v){std::cerr<<m<<'\n';std::exit(1);}}
int main(){
    DiagnosticPairRuntime<2> runtime;
    for(unsigned i=0;i<20000;++i){
        Check(runtime.Begin({100,0},0x3e61),"Recycled slot refused");
        Check(!runtime.Next(200),"Update crossed callback scope");
        auto echo=runtime.Next(100);
        Check(echo && echo->pairFlags==0x3e61 && echo->filterFlags==0x0c,"Echo changed collision flags");
        Check(!runtime.Next(100),"Repeated echo");runtime.Lost({100,0});
    }
    auto stats=runtime.Snapshot();
    Check(stats.begun==20000 && stats.lost==20000 && stats.echoed==20000 && stats.active==0 && !stats.fault,"Lifecycle accounting failed");
    runtime.Begin({100,0},1);runtime.Begin({200,0},1);
    Check(!runtime.Begin({300,0},1),"Capacity was not bounded");
    runtime.Lost({100,0});Check(runtime.Begin({300,0},1),"Freed slot unavailable");
    DiagnosticPairRuntime<16> concurrent;
    std::vector<std::thread> workers;
    for(unsigned n=1;n<=4;++n)workers.emplace_back([&,n]{
        for(unsigned i=0;i<10000;++i)if(concurrent.Begin({n,i},0x3e61)){concurrent.Next(n);concurrent.Lost({n,i});}
    });
    for(auto& thread:workers)thread.join();
    const auto concurrentStats=concurrent.Snapshot();
    Check(concurrentStats.active<=16,"Concurrent registry exceeded capacity");
    if(concurrentStats.fault)Check(!concurrent.Begin({10,0},1) && !concurrent.Next(10),"Fault allowed new tracking or updates");
    std::cout<<"Reusable registry accounting and concurrent diagnostic fault handling passed\n";
}
