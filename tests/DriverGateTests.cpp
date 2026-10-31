#include "DriverGate.hpp"
#include <cstdlib>
#include <iostream>
using namespace Dnc;
void Check(bool value,const char* text){if(!value){std::cerr<<text<<'\n';std::exit(1);}}
int main(){
    DriverGate gate; VehicleIdentity car{10,0x10001},other{11,0x10001};
    Check(!gate.Allows(car,1,100),"Default gate allowed driver");
    Check(gate.Observe(car,1,Fact::Yes,100),"Initial publication failed");
    Check(gate.Allows(car,1,100) && gate.Allows(car,1,349),"Valid lease rejected");
    Check(!gate.Allows(car,1,350) && !gate.Allows(car,1,99),"Expired/future lease accepted");
    Check(!gate.Allows(car,2,100) && !gate.Allows(other,1,100),"Mount/identity mismatch accepted");
    gate.Revoke();
    Check(!gate.Allows(car,1,100),"Revoked snapshot still allowed");
    gate.Observe(car,2,Fact::Unknown,200);
    Check(!gate.Allows(car,2,200),"Unknown driver accepted");
    gate.Observe(car,2,Fact::No,200);
    Check(!gate.Allows(car,2,200),"Passenger accepted");
    gate.Observe(car,2,Fact::Yes,200);
    Check(gate.Allows(car,2,200) && !gate.Allows(car,1,200),"Remount reused old authorization");
    gate.Observe({},2,Fact::Yes,200);
    Check(!gate.Allows({},2,200),"Invalid identity accepted");
    std::cout<<"Driver lease expiry, revocation, identity and remount checks passed\n";
}
