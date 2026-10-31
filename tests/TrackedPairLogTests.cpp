#include "TrackedPairLog.hpp"
#include <cstdlib>
#include <iostream>
void Check(bool v,const char* m){if(!v){std::cerr<<m<<'\n';std::exit(1);}}
int main(){
    Dnc::TrackedPairLog<4> log;
    Check(!log.Reserve(0,0,1,2,3) && !log.Reserve(1,0xffffffff,1,2,3),"Invalid tracking identity accepted");
    Check(log.Reserve(100,0,1,2,0x3e61) && log.Reserve(200,0,3,4,0x3e61),"Separate callback scopes rejected");
    log.MarkLost(100,0);
    auto& e=log.Entries();
    Check(e[0].lost.load() && !e[1].lost.load(),"Pair ID crossed callback scope");
    Check(log.Reserve(100,0,5,6,0x3e61),"Released ID reuse rejected");
    Check(!e[2].lost.load(),"Reused pair inherited old loss");
    log.MarkLost(100,0);
    Check(e[2].lost.load() && e[0].lost.load() && !e[1].lost.load(),"Reused pair loss mismatch");
    Check(log.Reserve(100,1,7,8,0x3e61) && !log.Reserve(100,2,9,10,0x3e61),"Bounded log capacity failed");
    Check(e[0].flags==0x3e61 && e[2].first==5 && e[2].second==6,"Audit record overwritten");
    Dnc::TrackedPairLog<3> echoes;
    Dnc::TrackedPairLog<3>::Echo update;
    echoes.Reserve(10,0,1,2,0x3e61);
    echoes.Reserve(20,0,3,4,0x3e61);
    Check(!echoes.NextEcho(30,update),"Update crossed callback scope");
    Check(echoes.NextEcho(10,update) && update.pair==0 && update.pairFlags==0x3e61 && update.filterFlags==0x0c,"Echo changed original response flags");
    Check(!echoes.NextEcho(10,update),"Repeated update would loop forever");
    echoes.MarkLost(20,0);
    Check(!echoes.NextEcho(20,update),"Lost pair was updated");
    echoes.Reserve(20,0,5,6,0x3e61);
    Check(echoes.NextEcho(20,update),"Reused ID failed to get new lifetime update");
    std::cout<<"Scoped pair identities, ID reuse, immutable audit records and capacity passed\n";
}
