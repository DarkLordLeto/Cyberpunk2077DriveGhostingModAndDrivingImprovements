#include "CollisionFilterModel.hpp"
#include <cstdlib>
#include <iostream>
using namespace Dnc;
void Check(bool value,const char* message){if(!value){std::cerr<<message<<'\n';std::exit(1);}}
int main(){
    const SimulationMask chassis{0x10,0x8000000000008cf0ULL},bottom{0,0x58};
    const auto original=EvaluateOrdinaryPair(chassis,chassis);
    Check(original.filterFlags==4 && original.pairFlags==0xc01,"Captured chassis pair not routed to callback");
    Check(EvaluateOrdinaryPair(chassis,bottom).filterFlags==4,"Chassis/bottom pair incorrectly suppressed");
    Check(EvaluateOrdinaryPair(bottom,bottom).filterFlags==2,"Zero-category bottom pair incorrectly accepted");
    auto clearContact=chassis;clearContact.contacts&=~uint64_t{0x10};
    Check(EvaluateOrdinaryPair(clearContact,chassis).filterFlags==4,"One-sided clearing counterexample lost");
    // Synthetic world mask: allowed entirely by the world's response to Vehicle.
    // Demonstrates why removing the category is not generally world-safe.
    const SimulationMask world{4,0x10};
    Check(EvaluateOrdinaryPair(chassis,world).filterFlags==4,"One-sided world contact should pass");
    auto clearBoth=clearContact;clearBoth.category&=~uint64_t{0x10};
    Check(EvaluateOrdinaryPair(clearBoth,world).filterFlags==2,"Category clearing counterexample lost");
    for(uint64_t f=0;f<4;++f){
        SimulationMask a{1,(f<<62)|2},b{2,0};
        auto ab=EvaluateOrdinaryPair(a,b,0x100),ba=EvaluateOrdinaryPair(b,a,0x100);
        Check(ab.filterFlags==ba.filterFlags && ab.pairFlags==ba.pairFlags,"Ordinary rule not symmetric");
        Check(ab.filterFlags==(f?4:0) && ab.pairFlags==0xd01,"Registration/flag preservation mismatch");
    }
    for(uint64_t ac=0;ac<4;++ac)for(uint64_t am=0;am<4;++am)
    for(uint64_t bc=0;bc<4;++bc)for(uint64_t bm=0;bm<4;++bm){
        const auto result=EvaluateOrdinaryPair({ac,am},{bc,bm});
        const bool left=(ac&bm)!=0,right=(bc&am)!=0;
        Check((result.filterFlags!=2)==(left||right),"Mask truth table mismatch");
    }
    Check(EvaluateOrdinaryPair({0,0},{0,0},0x1234).pairFlags==0x1234,"Suppressed branch changed pair flags");
    std::cout<<"Ordinary filter truth table and traffic/world counterexamples passed\n";
}
