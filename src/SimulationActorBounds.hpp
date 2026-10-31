#pragma once
#include "SimulationPairBounds.hpp"
#include <array>
#include <algorithm>
namespace Dnc {
struct SimulationActorBounds {
    WorldBounds first{},second{};
    uint32_t firstShapeCount{},secondShapeCount{};
};
// Snapshot-only union of every shape linked to EACH OF TWO ACTORS. Other actors
// in the same vehicle are not covered. Native lifetime/phase ownership is external.
template<std::size_t MaxElements=128,class Read,class Boxes> bool DecodeSimulationActorBounds(Read&& read,
    uintptr_t nphase,uintptr_t callback,uint32_t pair,const BasicSimulationBoundsView<Boxes>& view,SimulationActorBounds& output) {
    static_assert(MaxElements>0);
    SimulationPairBounds contact;
    if(!DecodeSimulationPair(read,nphase,callback,pair,view,contact)) return false;
    auto field=[&]<class T>(uintptr_t p,std::size_t offset,T& value) {
        if(!p || offset>std::numeric_limits<uintptr_t>::max()-p) return false;
        p+=offset;if(sizeof(T)>std::numeric_limits<uintptr_t>::max()-p) return false;
        return read(p,&value,sizeof(T));
    };
    uintptr_t manager=0,entries=0,interaction=0;
    if(!field(nphase,0x98,manager) || !field(manager,0,entries) ||
       !field(entries,std::size_t(pair)*8,interaction) || !interaction || (interaction&3)) return false;
    SimulationActorBounds result{};
    WorldBounds* unions[]{&result.first,&result.second};
    uint32_t* counts[]{&result.firstShapeCount,&result.secondShapeCount};
    for(unsigned side=0;side<2;++side) {
        uintptr_t contactShape=0,actor=0,head=0;
        if(!field(interaction,0x28+side*8,contactShape) || !field(contactShape,0x10,actor) ||
           !field(actor,0x38,head) || !head) return false;
        std::array<uintptr_t,MaxElements> visited{};
        std::array<uint32_t,MaxElements> indices{};
        std::size_t used=0;bool containsContact=false;
        for(auto shape=head;shape;) {
            if(used==MaxElements || std::find(visited.begin(),visited.begin()+used,shape)!=visited.begin()+used) return false;
            uintptr_t owner=0,next=0,scene=0;uint32_t packed=0;
            if(!field(shape,0x10,owner) || owner!=actor || !field(owner,0x48,scene) || scene!=view.scene ||
               !field(shape,8,next) || !field(shape,0x18,packed) || (packed&0x60000000)) return false;
            const auto index=packed&0x1fffffff;
            if(index>=view.boxes.size() || std::find(indices.begin(),indices.begin()+used,index)!=indices.begin()+used) return false;
            visited[used]=shape;indices[used++]=index;containsContact|=shape==contactShape;
            const auto& source=view.boxes[index];WorldBounds bounds{};
            for(unsigned axis=0;axis<3;++axis){bounds.minimum[axis]=source.minimum[axis];bounds.maximum[axis]=source.maximum[axis];}
            bounds.minimum[3]=bounds.maximum[3]=1;
            if(!ValidBounds(bounds)) return false;
            if(used==1) *unions[side]=bounds;
            else for(unsigned axis=0;axis<3;++axis){
                unions[side]->minimum[axis]=std::min(unions[side]->minimum[axis],bounds.minimum[axis]);
                unions[side]->maximum[axis]=std::max(unions[side]->maximum[axis],bounds.maximum[axis]);
            }
            shape=next;
        }
        if(!containsContact) return false;
        uintptr_t currentHead=0;
        if(!field(actor,0x38,currentHead) || currentHead!=head) return false;
        // Check the entire inventory again, including links and packed type/ID.
        for(std::size_t i=0;i<used;++i) {
            uintptr_t next=0,owner=0;uint32_t packed=0;
            if(!field(visited[i],8,next) || next!=(i+1<used ? visited[i+1] : 0) ||
               !field(visited[i],0x10,owner) || owner!=actor || !field(visited[i],0x18,packed) ||
               (packed&0x60000000) || (packed&0x1fffffff)!=indices[i]) return false;
        }
        *counts[side]=static_cast<uint32_t>(used);
    }
    SimulationPairBounds after;
    if(!DecodeSimulationPair(read,nphase,callback,pair,view,after) ||
       after.firstIndex!=contact.firstIndex || after.secondIndex!=contact.secondIndex) return false;
    output=result;return true;
}
}
