#include "CarQuery.hpp"
#include "TrySnapshot.hpp"
#include "TryQueue.hpp"
#include "VerifiedBuild.hpp"
#include "QueryGhostFilter.hpp"
#include <array>
#include <atomic>
#include <intrin.h>
#include <cstring>
namespace CarQuery {
namespace {
struct Vec { float x,y,z; };
using Ptr=const void*;
using Ray=bool(*)(Ptr,Ptr,const Vec*,float,void*,Ptr,Ptr,Ptr,Ptr);
using Sweep=bool(*)(Ptr,Ptr,Ptr,const Vec*,float,void*,Ptr,Ptr,Ptr,Ptr,float);
Ray ray{}; Sweep sweep{};
void* rayEntry{}; void* sweepEntry{}; void* batchEntry{};
using ExecuteBatch=void(*)(void*); ExecuteBatch executeBatch{};
std::atomic<uint64_t> batchCalls{}, batchMatches{};
struct Target { uint64_t mount{}, expires{}; uintptr_t actor{}; uint32_t proxy{}; };
Dnc::TrySnapshot<Target> latest(Target{});
std::atomic<uint64_t> mount{UINT64_MAX}, until{}, checked{}, matched{};
struct Sample {uint32_t proxy{}, caller{}; bool swept{}; float distance{}, z{};};
Dnc::TryQueue<Sample,128> samples;
uintptr_t gameBase{};
// Ghosted traffic is removed from ray and sweep results so wheel and suspension
// probes cannot stand on a car whose chassis contacts are already suppressed.
Dnc::GhostProxySet ghosts;
struct FilterData { uint32_t words[4]; uint16_t flags; uint8_t client; }; // PxQueryFilterData
static_assert(sizeof(FilterData)==20);
std::atomic<uint64_t> ghostQueries{}, ghostRejects{};
struct CallerStat { std::atomic<uint64_t> key{}, hits{}; std::atomic<uint32_t> detail{}; };
std::array<CallerStat,16> callerStats;
void RecordGhost(Ptr caller,bool swept,uint32_t hits,uint16_t flags,bool callback,bool touches) {
    ghostRejects.fetch_add(hits,std::memory_order_relaxed);
    const auto addr=reinterpret_cast<uintptr_t>(caller);
    const uint64_t rva=addr>=gameBase && addr-gameBase<DncBuild::ImageSize ? addr-gameBase : 0;
    const uint64_t key=(rva<<4)|8|(swept?1:0);
    for(auto& stat:callerStats) {
        uint64_t expected=stat.key.load(std::memory_order_acquire);
        if(expected==0 && stat.key.compare_exchange_strong(expected,key)) expected=key;
        if(expected!=key) continue;
        stat.hits.fetch_add(hits,std::memory_order_relaxed);
        stat.detail.store(uint32_t(flags)|(callback?0x10000u:0)|(touches?0x20000u:0),std::memory_order_relaxed);
        return;
    }
}
// Runs one scene query with ghosted actors filtered out. PhysX uses the filter
// data and callback only for the duration of the call.
template<class Invoke> bool GhostAware(void* hits,Ptr filter,Ptr callback,Ptr cache,Ptr caller,bool swept,Invoke invoke) {
    const auto now=GetTickCount64();
    if(!hits || !filter || !ghosts.Active(now) || mount.load(std::memory_order_acquire)==UINT64_MAX)
        return invoke(filter,callback,cache);
    FilterData local;
    std::memcpy(&local,filter,sizeof(local));
    uint32_t maxTouches{};
    std::memcpy(&maxTouches,static_cast<const unsigned char*>(hits)+0x58,sizeof(maxTouches));
    const uint16_t flags=local.flags;
    const auto plan=Dnc::PlanGhostQuery(flags,callback!=nullptr,maxTouches);
    local.flags=plan.flags;
    Dnc::GhostQueryFilter wrapper(ghosts,static_cast<Dnc::QueryFilterCallback*>(const_cast<void*>(callback)),plan,now);
    // PhysX skips filtering for a cached shape, so drop a cache entry that names a ghost.
    if(cache) {
        uintptr_t cached{};
        std::memcpy(&cached,static_cast<const unsigned char*>(cache)+8,sizeof(cached));
        if(cached && ghosts.Contains(Dnc::ActorProxy(reinterpret_cast<const void*>(cached)),now)) cache=nullptr;
    }
    ghostQueries.fetch_add(1,std::memory_order_relaxed);
    const bool ok=invoke(static_cast<Ptr>(&local),static_cast<Ptr>(&wrapper),cache);
    if(wrapper.rejected) RecordGhost(caller,swept,wrapper.rejected,flags,callback!=nullptr,maxTouches!=0);
    return ok;
}
void Trace(bool swept, void* buffer, const Vec* direction, float distance, Ptr caller) {
    if(!buffer || !direction || direction->z > -0.5f || distance<=0 || distance>16) return;
    const auto now=GetTickCount64();
    if(now>=until.load(std::memory_order_relaxed)) return;
    Target target;
    if(!latest.Read([&](const Target& value){target=value;}) || now>=target.expires ||
       target.mount!=mount.load(std::memory_order_acquire)) return;
    ++checked;
    const auto* hit=static_cast<const unsigned char*>(buffer);
    if(!hit[0x48]) return;
    uintptr_t actor{};
    std::memcpy(&actor,hit+8,sizeof(actor));
    if(actor!=target.actor) return;
    ++matched;
    const auto addr=reinterpret_cast<uintptr_t>(caller);
    uint32_t rva=addr>=gameBase && addr-gameBase<DncBuild::ImageSize ? uint32_t(addr-gameBase) : 0;
    samples.Push({target.proxy,rva,swept,distance,direction->z});
}
bool RayHook(Ptr scene,Ptr origin,const Vec* dir,float dist,void* hits,Ptr flags,Ptr filter,Ptr callback,Ptr cache) {
    const bool ok=GhostAware(hits,filter,callback,cache,_ReturnAddress(),false,[&](Ptr f,Ptr c,Ptr k){return ray(scene,origin,dir,dist,hits,flags,f,c,k);});
    if(ok) Trace(false,hits,dir,dist,_ReturnAddress());
    return ok;
}
bool SweepHook(Ptr scene,Ptr geometry,Ptr pose,const Vec* dir,float dist,void* hits,Ptr flags,Ptr filter,Ptr callback,Ptr cache,float inflation) {
    const bool ok=GhostAware(hits,filter,callback,cache,_ReturnAddress(),true,[&](Ptr f,Ptr c,Ptr k){return sweep(scene,geometry,pose,dir,dist,hits,flags,f,c,k,inflation);});
    if(ok) Trace(true,hits,dir,dist,_ReturnAddress());
    return ok;
}
}
namespace {
void BatchHook(void* self) {
    // Verified NpBatchQuery layout: pending ray/sweep counts and result buffers.
    auto* object=static_cast<unsigned char*>(self);
    uint32_t rayCount{},sweepCount{};
    unsigned char* rays{}; unsigned char* sweeps{};
    std::memcpy(&rayCount,object+0x30,4);
    std::memcpy(&sweepCount,object+0x38,4);
    std::memcpy(&rays,object+0x68,8);
    std::memcpy(&sweeps,object+0x78,8);
    executeBatch(self);
    ++batchCalls;
    const auto now=GetTickCount64();
    if(now>=until.load(std::memory_order_relaxed)) return;
    Target target;
    if(!latest.Read([&](const Target& value){target=value;}) || now>=target.expires ||
       target.mount!=mount.load(std::memory_order_acquire)) return;
    const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress());
    const uint32_t rva=caller>=gameBase && caller-gameBase<DncBuild::ImageSize ? uint32_t(caller-gameBase) : 0;
    const auto inspect=[&](unsigned char* results,uint32_t count,bool swept) {
        if(!results || count>4096) return;
        for(uint32_t i=0;i<count;++i) {
            const auto* result=results+i*0x60;
            if(result[0x58]!=1 || !result[0x59]) continue;
            uintptr_t actor{};std::memcpy(&actor,result,8);
            if(actor!=target.actor) continue;
            ++batchMatches;
            // Negative range identifies an executed batch result without direction data.
            samples.Push({target.proxy,rva,swept,-1.0f,0.0f});
        }
    };
    inspect(rays,rayCount,false);inspect(sweeps,sweepCount,true);
}
}
void Mount(uint64_t token){
    if(mount.exchange(token,std::memory_order_acq_rel)!=token) ghosts.Clear();
}
void Ghost(uint64_t token,uint32_t proxy,uint64_t lifetimeMs){
    if(token==UINT64_MAX || token!=mount.load(std::memory_order_acquire)) return;
    ghosts.Add(proxy,GetTickCount64(),lifetimeMs);
}
void Contact(uint64_t token,uint32_t proxy,Ptr actor){
    if(token!=mount.load(std::memory_order_acquire)) return;
    const auto end=GetTickCount64()+1500;
    // The contact actor can be a secondary body with its own proxy ID.
    ghosts.Add(proxy,end-1500,1500);
    ghosts.Add(Dnc::ActorProxy(actor),end-1500,1500);
    if(latest.Publish({token,end,reinterpret_cast<uintptr_t>(actor),proxy})) until.store(end,std::memory_order_relaxed);
}
void Start(RED4ext::v1::PluginHandle h,const RED4ext::v1::Sdk* sdk,const unsigned char* phys){
    gameBase=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    auto* base=const_cast<unsigned char*>(phys);
    // Caller verifies the image fingerprint. Check both public scene vtable slots as well.
    auto hook=[&](size_t slot,size_t rva,void* handler,void** original,void*& entry){
        void* fn{};std::memcpy(&fn,base+slot,sizeof(fn));
        if(fn==base+rva && sdk->hooking->Attach(h,fn,handler,original)) entry=fn;
    };
    hook(0x20cde0,0x60890,reinterpret_cast<void*>(&RayHook),reinterpret_cast<void**>(&ray),rayEntry);
    hook(0x20cde8,0x60960,reinterpret_cast<void*>(&SweepHook),reinterpret_cast<void**>(&sweep),sweepEntry);
    hook(0x208c48,0x2e020,reinterpret_cast<void*>(&BatchHook),reinterpret_cast<void**>(&executeBatch),batchEntry);
    sdk->logger->InfoF(h,"DNC_BATCH_READY enabled=%u",unsigned(batchEntry!=nullptr));
    sdk->logger->InfoF(h,"DNC_CAR_QUERY_READY ray=%u sweep=%u",unsigned(rayEntry!=nullptr),unsigned(sweepEntry!=nullptr));
}
void Drain(RED4ext::v1::PluginHandle h,const RED4ext::v1::Sdk* sdk){
    static unsigned logged=0;Sample s;
    while(samples.Pop(s)) if(logged<256){
        ++logged;
        sdk->logger->InfoF(h,"DNC_CAR_QUERY_HIT proxy=%08x type=%s callerGameRva=%08x range=%.2f dirZ=%.2f",s.proxy,s.swept?"sweep":"ray",s.caller,s.distance,s.z);
    }
}
bool Stop(RED4ext::v1::PluginHandle h,const RED4ext::v1::Sdk* sdk){
    Mount(UINT64_MAX);
    if(rayEntry && sdk->hooking->Detach(h,rayEntry)) rayEntry=nullptr;
    if(sweepEntry && sdk->hooking->Detach(h,sweepEntry)) sweepEntry=nullptr;
    sdk->logger->InfoF(h,"DNC_CAR_QUERY_END shortDownHits=%llu matchedTraffic=%llu",checked.load(),matched.load());
    if(batchEntry && sdk->hooking->Detach(h,batchEntry)) batchEntry=nullptr;
    sdk->logger->InfoF(h,"DNC_BATCH_END calls=%llu matchedTraffic=%llu",batchCalls.load(),batchMatches.load());
    sdk->logger->InfoF(h,"DNC_QUERY_GHOST_END filteredQueries=%llu droppedHits=%llu",ghostQueries.load(),ghostRejects.load());
    for(const auto& stat:callerStats) {
        const auto key=stat.key.load();
        if(!key) continue;
        const auto detail=stat.detail.load();
        sdk->logger->InfoF(h,"DNC_QUERY_GHOST_CALLER callerGameRva=%08x type=%s droppedHits=%llu flags=%04x callback=%u touches=%u",
            unsigned(key>>4),(key&1)?"sweep":"ray",stat.hits.load(),unsigned(detail&0xffff),unsigned((detail>>16)&1),unsigned((detail>>17)&1));
    }
    return !rayEntry && !sweepEntry && !batchEntry;
}
}
