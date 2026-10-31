#include "SuppressionController.hpp"
#include <cstdlib>
#include <iostream>
using namespace Dnc;
void Check(bool v,const char* m){if(!v){std::cerr<<m<<'\n';std::exit(1);}}
int main(){
    VehiclePairIdentity id{1,10,20,100,200};
    SuppressionController<4> c;
    auto token=c.Begin({1,0},id,0,0x3e61,1,1,true,true);
    PairUpdate result{};
    Check(token && c.SubmitNext(1,result),"Suppression setup failed");
    c.StopAll();
    auto queueRestore=[&]{auto t=c.SampleTicket(id);Check(t && c.ConfirmClear(*t),"Restore queue failed");};
    queueRestore();
    const auto unchanged=result;
    Check(!c.SubmitNext(1,result) && result.revision==unchanged.revision,"Default path emitted restoration");
    queueRestore();
    unsigned reads=0;
    Check(!c.SubmitNextChecked(1,result,[&](VehiclePairIdentity value) noexcept {
        ++reads;Check(value==id,"Wrong identity sampled");return false;
    }) && reads==1,"Re-overlap without lifecycle event was ignored");
    queueRestore();
    std::optional<PairToken> replacement;
    Check(!c.SubmitNextChecked(1,result,[&](VehiclePairIdentity) noexcept {
        // Reentrancy proves no controller lock is held during the native reader.
        Check(c.Lost(*token),"Loss during read failed");
        replacement=c.Begin({1,0},id,0,0x3e61,1,1,false,false);
        Check(replacement.has_value(),"Replacement during read failed");
        return true;
    }),"Stale read restored a reused numeric pair ID");
    Check(c.SubmitNext(1,result) && result.token==*replacement && result.pairFlags==0x3e60,
        "Replacement did not retain suppression");
    queueRestore();
    Check(!c.SubmitNextChecked(1,result,[&](VehiclePairIdentity) noexcept {
        c.StopAll();return true;
    }),"State mutation during read was ignored");
    queueRestore();
    Check(c.SubmitNextChecked(1,result,[](VehiclePairIdentity) noexcept {return true;}) &&
        result.pairFlags==0x3e61 && result.filterFlags==12,"Fresh restoration failed");
    Check(!c.SubmitNextChecked(1,result,[](VehiclePairIdentity) noexcept {return true;}),"Restore repeated");
    Check(c.Lost(*replacement),"Final loss failed");
    auto clear=c.SampleTicket(id);Check(clear && c.ConfirmClear(*clear),"Intent retirement failed");
    Check(c.ActiveIntents()==0 && c.CanDetach(),"Cleanup incomplete");
    std::cout<<"Restoration submission tests passed\n";
}
