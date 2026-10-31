#pragma once
#include "BoundsClearance.hpp"
#include <span>
#include <cstdint>
#include <limits>
namespace Dnc {
struct SimulationBox { float minimum[3],maximum[3]; };
static_assert(sizeof(SimulationBox)==24);
// Caller supplies an owned/correctly borrowed snapshot and independently verified
// length. No bounds-array size field is guessed from its in-memory header.
template<class Boxes> struct BasicSimulationBoundsView {
    uintptr_t scene{}, arrayObject{}, dataAddress{};
    Boxes boxes;
};
using SimulationBoundsView=BasicSimulationBoundsView<std::span<const SimulationBox>>;
struct SimulationPairBounds {
    uint32_t firstIndex{},secondIndex{};
    WorldBounds first{},second{};
};
// Pure checked decoder, not connected to live native memory. Read must copy from
// a coherent snapshot. Repeated headers reject observable changes, not ABA races.
// Only two shape bounds are returned; this is NOT complete-vehicle clearance.
template<class Read,class Boxes> bool DecodeSimulationPair(Read&& read,uintptr_t nphase,uintptr_t callback,
    uint32_t pair,const BasicSimulationBoundsView<Boxes>& view,SimulationPairBounds& output) {
    auto field=[&]<class T>(uintptr_t address,std::size_t offset,T& value) {
        if(!address || offset>std::numeric_limits<uintptr_t>::max()-address) return false;
        address+=offset;
        if(sizeof(T)>std::numeric_limits<uintptr_t>::max()-address) return false;
        return read(address,&value,sizeof(T));
    };
    if(!nphase || !callback || pair==0xffffffff || !view.scene || !view.arrayObject ||
       !view.dataAddress || view.boxes.empty()) return false;
    uintptr_t scene=0,registered=0,manager=0,entries=0,array=0,data=0,tag=0;
    uint32_t count=0;
    if(!field(nphase,0,scene) || scene!=view.scene || !field(scene,0x1078,registered) || registered!=callback ||
       !field(nphase,0x98,manager) || !field(manager,0,entries) || !field(manager,8,count) || pair>=count ||
       !field(entries,std::size_t(pair)*8,tag) || !tag || (tag&3)!=0 ||
       !field(scene,0x818,array) || array!=view.arrayObject || !field(array,8,data) || data!=view.dataAddress) return false;
    uint8_t type=0,flags=0;uint32_t retainedId=0;
    if(!field(tag,0x24,type) || type!=0 || !field(tag,0x25,flags) || !(flags&0x20) ||
       !field(tag,0x38,retainedId) || retainedId!=pair) return false;
    uintptr_t shapes[2]{},actors[2]{};uint32_t packed[2]{};
    SimulationPairBounds result{};
    WorldBounds* boxes[2]{&result.first,&result.second};
    for(unsigned i=0;i<2;++i) {
        uintptr_t ownerScene=0;
        if(!field(tag,0x28+i*8,shapes[i]) || !field(shapes[i],0x10,actors[i]) ||
           !field(actors[i],0x48,ownerScene) || ownerScene!=scene ||
           !field(shapes[i],0x18,packed[i]) || (packed[i]&0x60000000)!=0) return false;
        const auto index=packed[i]&0x1fffffff;
        if(index>=view.boxes.size()) return false;
        const auto& source=view.boxes[index];
        for(unsigned axis=0;axis<3;++axis){boxes[i]->minimum[axis]=source.minimum[axis];boxes[i]->maximum[axis]=source.maximum[axis];}
        boxes[i]->minimum[3]=boxes[i]->maximum[3]=1;
        if(!ValidBounds(*boxes[i])) return false;
        (i ? result.secondIndex : result.firstIndex)=index;
    }
    uintptr_t again=0;uint32_t countAgain=0;
    if(!field(nphase,0,again) || again!=scene || !field(scene,0x1078,again) || again!=callback ||
       !field(nphase,0x98,again) || again!=manager || !field(manager,0,again) || again!=entries ||
       !field(manager,8,countAgain) || countAgain!=count || !field(entries,std::size_t(pair)*8,again) || again!=tag ||
       !field(scene,0x818,again) || again!=array || !field(array,8,again) || again!=data) return false;
    for(unsigned i=0;i<2;++i) {
        uint32_t current=0;
        if(!field(tag,0x28+i*8,again) || again!=shapes[i] || !field(shapes[i],0x10,again) || again!=actors[i] ||
           !field(actors[i],0x48,again) || again!=scene || !field(shapes[i],0x18,current) || current!=packed[i]) return false;
    }
    output=result;return true;
}
}
