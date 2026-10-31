#include "FilterSnapshot.hpp"
#include <array>
#include <cstdlib>
#include <iostream>
using namespace Dnc;
void Check(bool condition, const char* message) {
    if (!condition) { std::cerr << message << '\n'; std::exit(1); }
}
int main() {
    const VehicleLifetime car{9021325,1,0x1047c};
    const SimulationMask main{0x10,0x8000000000008cf0ULL}, bottom{0,0x58};
    // Test values exercise restoration; they are NOT an approved ghosting policy.
    const SimulationMask changed{0x10,0x8000000000008ce0ULL}, again{0x10,0x8000000000008cc0ULL};
    std::array shapes{ShapeMask{0,0,bottom},ShapeMask{0,1,main},ShapeMask{0,2,main}};
    Check(!FilterSnapshot::Capture(car,shapes,false),"Incomplete inventory accepted");
    auto duplicate=shapes;duplicate[2]=duplicate[1];
    Check(!FilterSnapshot::Capture(car,duplicate,true),"Duplicate shape accepted");
    auto wildcard=shapes;wildcard[1].shape=255;
    Check(!FilterSnapshot::Capture(car,wildcard,true),"Native wildcard accepted");
    wildcard[1].shape=0xffffffff;
    Check(!FilterSnapshot::Capture(car,wildcard,true),"Wrapper wildcard accepted");
    Check(!FilterSnapshot::Capture({car.entity,0,car.proxy},shapes,true),"Missing epoch accepted");
    auto snapshot=FilterSnapshot::Capture(car,shapes,true);
    Check(snapshot.has_value(),"Valid snapshot rejected");
    Check(snapshot->PlanRestore(car,shapes).empty(),"Unchanged shapes scheduled");
    Check(!snapshot->ConfirmApplied(car,0,1,main,changed,main),"Failed readback accepted");
    Check(snapshot->ConfirmApplied(car,0,1,main,changed,changed),"Applied edit rejected");
    shapes[1].value=changed;
    auto plan=snapshot->PlanRestore(car,shapes);
    Check(plan.size()==1 && plan[0].action==RestoreAction::RestoreOriginal && plan[0].write==main,"Original 128 bits not restored");
    Check(plan[0].shape==1,"Bottom or unrelated shape scheduled");
    Check(!snapshot->ConfirmApplied(car,0,1,main,again,again),"Stale preimage accepted");
    Check(snapshot->ConfirmApplied(car,0,1,changed,again,again),"Second edit rejected");
    shapes[1].value=again;
    Check(snapshot->PlanRestore(car,shapes)[0].write==main,"Repeated edit overwrote original");
    shapes[1].value={0x20,0x4000000000008cf0ULL};
    plan=snapshot->PlanRestore(car,shapes);
    Check(plan[0].action==RestoreAction::Conflict && !plan[0].write,"External change overwritten");
    shapes[1].value=main;
    Check(snapshot->PlanRestore(car,shapes)[0].action==RestoreAction::AlreadyOriginal,"Already restored state not recognized");
    for (const auto other:std::array{VehicleLifetime{car.entity,2,car.proxy},VehicleLifetime{car.entity,1,0x2047c},VehicleLifetime{9021330,1,car.proxy}}) {
        plan=snapshot->PlanRestore(other,shapes);
        Check(plan[0].action==RestoreAction::WrongLifetime && !plan[0].write,"Stale identity allowed restoration");
    }
    std::array missing{shapes[0],shapes[2]};
    Check(snapshot->PlanRestore(car,missing)[0].action==RestoreAction::MissingShape,"Missing shape allowed restoration");
    duplicate[1].value=again;
    Check(snapshot->PlanRestore(car,duplicate)[0].action==RestoreAction::Conflict,"Ambiguous current inventory allowed restoration");
    shapes[1].value=again;
    Check(snapshot->ConfirmApplied(car,0,1,again,main,main),"Restoration readback rejected");
    Check(snapshot->PlanRestore(car,shapes).empty(),"Confirmed restoration retains ownership");
    // Both verified contact-registration flags must survive exact restoration.
    for (uint64_t flags=0; flags<4; ++flags) {
        const SimulationMask before{0x10,(flags<<62)|0x8cf0};
        const SimulationMask after{0x10,(flags<<62)|0x8ce0};
        std::array one{ShapeMask{0,2,before}};
        auto flagged=FilterSnapshot::Capture(car,one,true);
        Check(flagged.has_value(),"Flagged capture rejected");
        Check(flagged->ConfirmApplied(car,0,2,before,after,after),"Flagged edit rejected");
        one[0].value=after;
        Check(flagged->PlanRestore(car,one)[0].write==before,"Contact-registration flags lost");
    }
    std::cout << "Snapshot lifecycle, conflicts, sentinel rejection and exact restoration passed\n";
}

