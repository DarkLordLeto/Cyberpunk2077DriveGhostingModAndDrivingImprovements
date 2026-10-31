#include "PhysicsPhaseProbe.hpp"
#include "SimulationActorBounds.hpp"
#include "VerifiedPhysX.hpp"
#include "VerifiedPhaseLayout.hpp"
#include <atomic>
#include <chrono>
#include <cstring>
namespace {
using Dispatch=void(*)(void*,void*,bool);
Dispatch originalDispatch=nullptr;
void* dispatchTarget=nullptr;
thread_local uintptr_t phaseOwner=0;
std::atomic<bool> probing{false};
std::atomic<uint64_t> calls{0};
struct Record {
    std::atomic<bool> ready{false};
    uint32_t pair{},length{},firstCount{},secondCount{};
    uint64_t elapsedUs{};
    bool phase{},header{},pairRead{},actorRead{},logged{};
    Dnc::BoundsRelation pairRelation{Dnc::BoundsRelation::Unknown},actorRelation{Dnc::BoundsRelation::Unknown};
};
std::array<Record,16> records;
std::atomic<uint32_t> reserved{0};
void OnDispatch(void* owner,void* outputs,bool adaptiveForce) {
    const auto previous=phaseOwner;
    phaseOwner=reinterpret_cast<uintptr_t>(owner);
    struct Restore {uintptr_t previous;~Restore(){phaseOwner=previous;}} restore{previous};
    ++calls;originalDispatch(owner,outputs,adaptiveForce);
}
bool Copy(uintptr_t address,void* destination,std::size_t size) noexcept {
    if(!address || size>UINTPTR_MAX-address) return false;
    SIZE_T read=0;
    return ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<const void*>(address),destination,size,&read) && read==size;
}
template<class T> bool Field(uintptr_t address,std::size_t offset,T& result) noexcept {
    return address && offset<=UINTPTR_MAX-address && Copy(address+offset,&result,sizeof(result));
}
struct NativeBoxes {
    uintptr_t address;uint32_t count;
    std::size_t size() const noexcept {return count;}
    bool empty() const noexcept {return count==0;}
    Dnc::SimulationBox operator[](std::size_t index) const noexcept {
        Dnc::SimulationBox box{};
        if(index<count && index<=UINTPTR_MAX/sizeof(box)) {
            if(!Field(address,index*sizeof(box),box)) box={};
        }
        return box; // Zero box is invalid and rejected by the decoder.
    }
};
}
bool StartPhysicsPhaseProbe(RED4ext::v1::PluginHandle h,const RED4ext::v1::Sdk* sdk) {
    auto* base=reinterpret_cast<unsigned char*>(GetModuleHandleW(L"PhysX3_x64.dll"));
    if(!base || !sdk->hooking || dispatchTarget) return false;
    // PairTracking has already checked this module's PE identity before calling.
    if(std::memcmp(base+DncPhysX::StatusRva,DncPhysX::StatusBytes.data(),DncPhysX::StatusBytes.size())) return false;
    if(std::memcmp(base+DncPhaseBuild::BoundsSizeLoopRva,DncPhaseBuild::BoundsSizeLoopBytes.data(),DncPhaseBuild::BoundsSizeLoopBytes.size()) ||
       std::memcmp(base+DncPhaseBuild::ElementInitRva,DncPhaseBuild::ElementInitBytes.data(),DncPhaseBuild::ElementInitBytes.size()) ||
       std::memcmp(base+DncPhaseBuild::ActorShapesRva,DncPhaseBuild::ActorShapesBytes.data(),DncPhaseBuild::ActorShapesBytes.size()) ||
       std::memcmp(base+DncPhaseBuild::BoundsUpdateRva,DncPhaseBuild::BoundsUpdateBytes.data(),DncPhaseBuild::BoundsUpdateBytes.size())) return false;
    auto* target=base+DncPhysX::StatusRva;
    if(!sdk->hooking->Attach(h,target,reinterpret_cast<void*>(&OnDispatch),reinterpret_cast<void**>(&originalDispatch))) return false;
    dispatchTarget=target;probing.store(true,std::memory_order_release);
    sdk->logger->Info(h,"DNC_PHASE_READY samples=16 scope=dispatch collisionWrites=0");return true;
}
void SamplePhysicsPhasePair(uintptr_t callback,uint32_t pair) {
    if(!probing.load(std::memory_order_acquire)) return;
    auto index=reserved.load(std::memory_order_relaxed);
    do {if(index>=records.size()) return;} while(!reserved.compare_exchange_weak(index,index+1));
    auto& record=records[index];record.pair=pair;record.phase=phaseOwner!=0;
    const auto start=std::chrono::steady_clock::now();
    uintptr_t scene=0,array=0,data=0;uint32_t count=0,capacity=0;
    if(record.phase && Field(phaseOwner,0,scene) && Field(scene,0x818,array) && Field(array,8,data) &&
       Field(array,0x10,count) && Field(array,0x14,capacity) && data && count && count<=1048576 &&
       count<=(capacity&0x7fffffff)) {
        record.header=true;record.length=count;
        Dnc::BasicSimulationBoundsView<NativeBoxes> view{scene,array,data,{data,count}};
        Dnc::SimulationPairBounds pairBounds;
        record.pairRead=Dnc::DecodeSimulationPair(Copy,phaseOwner,callback,pair,view,pairBounds);
        if(record.pairRead) record.pairRelation=Dnc::CompareBounds(pairBounds.first,pairBounds.second);
        Dnc::SimulationActorBounds actors;
        record.actorRead=Dnc::DecodeSimulationActorBounds(Copy,phaseOwner,callback,pair,view,actors);
        if(record.actorRead){record.actorRelation=Dnc::CompareBounds(actors.first,actors.second);
            record.firstCount=actors.firstShapeCount;record.secondCount=actors.secondShapeCount;}
        uintptr_t current=0;uint32_t currentCount=0,currentCapacity=0;
        if(!Field(scene,0x818,current) || current!=array || !Field(array,8,current) || current!=data ||
           !Field(array,0x10,currentCount) || currentCount!=count || !Field(array,0x14,currentCapacity) || currentCapacity!=capacity) {
            record.pairRead=record.actorRead=false;
            record.pairRelation=record.actorRelation=Dnc::BoundsRelation::Unknown;
        }
    }
    record.elapsedUs=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-start).count();
    record.ready.store(true,std::memory_order_release);
}
void DrainPhysicsPhaseProbe(RED4ext::v1::PluginHandle h,const RED4ext::v1::Sdk* sdk) {
    for(auto& r:records) {
        if(!r.ready.load(std::memory_order_acquire) || r.logged) continue;
        sdk->logger->InfoF(h,"DNC_PHASE_SAMPLE pair=%u phase=%u header=%u length=%u pairRead=%u actorRead=%u shapes=%u:%u pairRelation=%u actorRelation=%u elapsedUs=%llu collisionWrites=0",
            r.pair,unsigned(r.phase),unsigned(r.header),r.length,unsigned(r.pairRead),unsigned(r.actorRead),r.firstCount,r.secondCount,
            unsigned(r.pairRelation),unsigned(r.actorRelation),static_cast<unsigned long long>(r.elapsedUs));
        r.logged=true;
    }
}
bool StopPhysicsPhaseProbe(RED4ext::v1::PluginHandle h,const RED4ext::v1::Sdk* sdk) {
    probing.store(false,std::memory_order_release);
    if(dispatchTarget) {
        if(!sdk->hooking->Detach(h,dispatchTarget)) {sdk->logger->Error(h,"DNC_PHASE_DETACH_FAILED");return false;}
        dispatchTarget=nullptr;sdk->logger->InfoF(h,"DNC_PHASE_RESTORED calls=%llu",static_cast<unsigned long long>(calls.load()));
    }
    DrainPhysicsPhaseProbe(h,sdk);return true;
}

