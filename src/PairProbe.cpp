#include "PairProbe.hpp"
#include "VerifiedBuild.hpp"
#include "VehicleQueries.hpp"
#include "VehicleLifecycle.hpp"
#include "TrySnapshot.hpp"
#include "TryQueue.hpp"
#include "DriverGate.hpp"
#include "PairSuppressionPolicy.hpp"
#include "VerifiedPhysX.hpp"
#include <vector>
#include <RED4ext/Scripting/Natives/entEntity.hpp>
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstring>
namespace {
// Explicit MSVC x64 hidden return-buffer ABI observed in the installed function:
// RCX=this, RDX=return buffer, R8D=pairId, R9D=attributes0; remaining args on stack.
using PairFound = uint16_t* (*)(void*, uint16_t*, uint32_t, uint32_t, const void*,
    const void*, const void*, uint32_t, const void*, const void*, const void*, uint16_t*);
PairFound original=nullptr;
void* target=nullptr;
std::atomic<uint64_t> total{0}, matching{0};
struct Record {
    uint32_t pair{}, actor0{}, actor1{}, observedPlayer{}, attr0{}, attr1{};
    uint16_t filterFlags{}, pairFlags{};
    std::atomic<bool> ready{false};
    bool logged=false; // Running callback only.
};
std::array<Record,16> samples;
// Reserve independent budgets for up to eight mounted intervals. No slot reuse.
std::array<Record,512> matches;
std::array<std::atomic<uint32_t>,8> mountSamples{};
std::atomic<uint64_t> mounted{0xffffffff};
std::atomic<uint64_t> publishedEpoch{1}, ghostedContacts{0};
uint32_t nextMount=0; // Running callback only.
bool ownerReaderReady=false;
Dnc::VehicleLifecycle<> lifecycle(1500); // milliseconds; diagnostic only
Dnc::TrySnapshot<Dnc::VehicleLifecycle<>> published(lifecycle);
std::atomic<uint64_t> candidates{0}, snapshotBusy{0}, driverDenied{0};
Dnc::DriverGate driverGate;
struct Discovery { uint64_t mount{}; uint32_t first{},second{}; };
Dnc::TryQueue<Discovery,512> discovery;
std::atomic<uint64_t> queued{0}, queueDrops{0}, missingOwners{0}, ruleRejected{0};
uint64_t resolvedOwners=0, unresolvedOwners=0, refreshCalls=0, publishOk=0, publishSkipped=0;
size_t replaceCursor=0;
std::array<uint64_t,256> retryAt{};
std::array<unsigned,256> failures{};
std::vector<uint32_t> knownProxies; // Running callback only; capped at 256.
uint64_t NowMs() {
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
}
Dnc::Fact FactOf(int value) { return value<0 ? Dnc::Fact::Unknown : value ? Dnc::Fact::Yes : Dnc::Fact::No; }
static_assert(std::atomic<uint32_t>::is_always_lock_free);
static_assert(std::atomic<uint64_t>::is_always_lock_free);
uint16_t* OnPair(void* self,uint16_t* result,uint32_t pair,uint32_t attr0,const void* data0,
    const void* actor0,const void* shape0,uint32_t attr1,const void* data1,const void* actor1,
    const void* shape1,uint16_t* pairFlags) {
    auto* returned=original(self,result,pair,attr0,data0,actor0,shape0,attr1,data1,actor1,shape1,pairFlags);
    // Original function reads both actors at +0x10. Observe the same scalar only;
    // its relationship to chassis proxy IDs is the hypothesis tested by this probe.
    uint32_t id0=0,id1=0;
    if (actor0) std::memcpy(&id0,static_cast<const unsigned char*>(actor0)+0x10,4);
    if (actor1) std::memcpy(&id1,static_cast<const unsigned char*>(actor1)+0x10,4);
    const auto observation=mounted.load(std::memory_order_relaxed);
    const auto player=static_cast<uint32_t>(observation);
    const auto bank=static_cast<uint32_t>(observation>>32);
    bool trackingCandidate=false;
    Dnc::VehicleIdentity authorizedDriver{};
    uint64_t decisionEpoch=0;
    if(player!=0xffffffff && (id0==player || id1==player)) {
        if(discovery.Push({observation,id0,id1})) queued.fetch_add(1,std::memory_order_relaxed);
        else queueDrops.fetch_add(1,std::memory_order_relaxed);
        const auto now=NowMs();
        if(!published.Read([&](const auto& state) {
            const auto a=state.IdentityForProxy(id0,now), b=state.IdentityForProxy(id1,now);
            if(!a.Valid() || !b.Valid()) { missingOwners.fetch_add(1,std::memory_order_relaxed); return; }
            const auto driverIdentity=a.proxy==player ? a : b;
            if(!driverGate.Allows(driverIdentity,observation,now)) { driverDenied.fetch_add(1,std::memory_order_relaxed); return; }
            if(state.Assess(state.Epoch(),a,b,now)==Dnc::PairAssessment::TrafficCandidate)
                {
                    candidates.fetch_add(1,std::memory_order_relaxed); trackingCandidate=true;
                    authorizedDriver=driverIdentity;
                    decisionEpoch=state.Epoch();
                }
            else ruleRejected.fetch_add(1,std::memory_order_relaxed);
        })) snapshotBusy.fetch_add(1,std::memory_order_relaxed);
    }
    const auto sequence=total.fetch_add(1,std::memory_order_relaxed);
    const bool playerMatch=player!=0xffffffff && (id0==player || id1==player);
    uint64_t matchSequence=matches.size();
    if (playerMatch) {
        matching.fetch_add(1,std::memory_order_relaxed);
        if (bank<mountSamples.size()) {
            const auto local=mountSamples[bank].fetch_add(1,std::memory_order_relaxed);
            if (local<64) matchSequence=uint64_t(bank)*64+local;
        }
    }
    const auto save=[&](Record& r) {
        r.pair=pair;r.actor0=id0;r.actor1=id1;r.observedPlayer=player;r.attr0=attr0;r.attr1=attr1;
        if (result) r.filterFlags=*result;
        if (pairFlags) r.pairFlags=*pairFlags;
        r.ready.store(true,std::memory_order_release);
    };
    if (sequence<samples.size()) save(samples[static_cast<size_t>(sequence)]);
    if (matchSequence<matches.size()) save(matches[static_cast<size_t>(matchSequence)]);
    // Write to this invocation's shape pair only. PhysX owns its lifetime and
    // runs filtering again for a new pair. Never retain a pair ID for mutation.
    if (trackingCandidate && result && returned==result && pairFlags && *pairFlags==0x3e61 &&
        actor0 && actor1 && shape0 && shape1 && mounted.load(std::memory_order_acquire)==observation &&
        publishedEpoch.load(std::memory_order_acquire)==decisionEpoch &&
        driverGate.Allows(authorizedDriver,observation,NowMs())) {
        const auto decision=Dnc::PlanNewTrafficContact(*result,*pairFlags,attr0,attr1);
        if (decision.changed) {
            *pairFlags=decision.pairFlags;
            ghostedContacts.fetch_add(1,std::memory_order_relaxed);
        }
    }
    return returned;
}
bool MatchesBuild(const unsigned char* base) {
    const auto* dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (!base || dos->e_magic!=IMAGE_DOS_SIGNATURE || dos->e_lfanew<=0 || dos->e_lfanew>4096) return false;
    const auto* pe=reinterpret_cast<const IMAGE_NT_HEADERS64*>(base+dos->e_lfanew);
    if (pe->Signature!=IMAGE_NT_SIGNATURE || pe->FileHeader.Machine!=IMAGE_FILE_MACHINE_AMD64 ||
        pe->FileHeader.TimeDateStamp!=DncBuild::Timestamp || pe->OptionalHeader.SizeOfImage!=DncBuild::ImageSize) return false;
    if (std::memcmp(base+DncBuild::PairFoundRva,DncBuild::PairFoundBytes.data(),DncBuild::PairFoundBytes.size())) return false;
    uintptr_t registered=0;
    std::memcpy(&registered,base+DncBuild::PairCallbackTableRva,sizeof(registered));
    return registered==reinterpret_cast<uintptr_t>(base+DncBuild::PairFoundRva);
}
}
void ObservePlayerProxy(uint32_t proxy) {
    const auto previous=mounted.load(std::memory_order_relaxed);
    if(proxy==0xffffffff) driverGate.Revoke();
    if (static_cast<uint32_t>(previous)==proxy) return;
    driverGate.Revoke();
    lifecycle.Reset();
    publishedEpoch.store(lifecycle.Epoch(),std::memory_order_release);
    lifecycle.SetEnabled(proxy!=0xffffffff);
    published.Publish(lifecycle); // A missed publication expires; no collision writes exist.
    if (proxy==0xffffffff) mounted.store(0xffffffff,std::memory_order_relaxed);
    else mounted.store((uint64_t(nextMount++)<<32)|proxy,std::memory_order_relaxed);
}
void ObserveDriver(uint64_t entity,uint32_t proxy,int verifiedDriver) {
    const auto observation=mounted.load(std::memory_order_relaxed);
    if(proxy==0xffffffff || static_cast<uint32_t>(observation)!=proxy) { driverGate.Revoke(); return; }
    driverGate.Observe({entity,proxy},observation,FactOf(verifiedDriver),NowMs());
}
void StartPairProbe(RED4ext::v1::PluginHandle h,const RED4ext::v1::Sdk* sdk) {
    const auto* base=reinterpret_cast<const unsigned char*>(GetModuleHandleW(nullptr));
    const auto* phys=reinterpret_cast<const unsigned char*>(GetModuleHandleW(L"PhysX3_x64.dll"));
    if (!phys || !sdk->hooking || !MatchesBuild(base)) {
        sdk->logger->Warn(h,"DNC_GHOST_SKIPPED: unsupported build");return;
    }
    const auto* dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(phys);
    if(dos->e_magic!=IMAGE_DOS_SIGNATURE || dos->e_lfanew<=0 || dos->e_lfanew>4096) return;
    const auto* pe=reinterpret_cast<const IMAGE_NT_HEADERS64*>(phys+dos->e_lfanew);
    if(pe->Signature!=IMAGE_NT_SIGNATURE || pe->FileHeader.TimeDateStamp!=DncPhysX::Timestamp ||
       pe->OptionalHeader.SizeOfImage!=DncPhysX::ImageSize ||
       std::memcmp(phys+DncPhysX::FilterRva,DncPhysX::FilterBytes.data(),DncPhysX::FilterBytes.size())) {
        sdk->logger->Warn(h,"DNC_PAIR_SKIPPED: hook API or verified callback bytes/table unavailable");return;
    }
    auto* address=const_cast<unsigned char*>(base)+DncBuild::PairFoundRva;
    if (!sdk->hooking->Attach(h,address,reinterpret_cast<void*>(&OnPair),reinterpret_cast<void**>(&original))) {
        sdk->logger->Warn(h,"DNC_PAIR_SKIPPED: hook attach failed");return;
    }
    target=address;
    ownerReaderReady=std::memcmp(base+DncBuild::ProxyOwnerRva,DncBuild::ProxyOwnerBytes.data(),DncBuild::ProxyOwnerBytes.size())==0 &&
        std::memcmp(base+DncBuild::ResolveSharedProxyRva,DncBuild::ResolveSharedProxyBytes.data(),DncBuild::ResolveSharedProxyBytes.size())==0;
    sdk->logger->InfoF(h,"DNC_OWNER_READER ready=%u",unsigned(ownerReaderReady));
    sdk->logger->Info(h,"DNC_GHOST_READY mode=creationOnly originalFlags=3e61 suppressedFlags=3e60");
}
namespace {
bool LogProxyOwner(uint32_t proxy,RED4ext::v1::PluginHandle h,const RED4ext::v1::Sdk* sdk,bool log=true) {
    if (!ownerReaderReady) return false;
    bool ownerResolved=false;
    ++refreshCalls;
    const auto* base=reinterpret_cast<const unsigned char*>(GetModuleHandleW(nullptr));
    using GetOwner=void* (*)(RED4ext::WeakHandle<RED4ext::IScriptable>*,uint32_t,uint32_t);
    auto getOwner=reinterpret_cast<GetOwner>(const_cast<unsigned char*>(base)+DncBuild::ProxyOwnerRva);
    auto* entityType=RED4ext::CRTTISystem::Get()->GetClass("entEntity");
    auto* vehicleType=RED4ext::CRTTISystem::Get()->GetClass("vehicleBaseObject");
    if (!entityType || !vehicleType || entityType->GetSize()!=sizeof(RED4ext::ent::Entity)) return false;
    for (uint32_t slot=0;slot<2;++slot) {
        RED4ext::WeakHandle<RED4ext::IScriptable> weak;
        getOwner(&weak,proxy,slot); // Native helper increments the copied weak reference.
        auto owner=weak.Lock();
        if (!owner) {
            if(slot==0) {
                ++unresolvedOwners;
                const auto now=NowMs();
                lifecycle.Retire(lifecycle.Epoch(),lifecycle.IdentityForProxy(proxy,now),now);
            }
            if(log) sdk->logger->InfoF(h,"DNC_PAIR_OWNER proxy=%08x slot=%u resolved=0",proxy,slot);continue;
        }
        if(slot==0) { ++resolvedOwners; ownerResolved=true; }
        const auto* type=owner->GetType();
        const bool entity=type->IsA(entityType),vehicle=type->IsA(vehicleType);
        uint64_t id=entity ? static_cast<RED4ext::ent::Entity*>(owner.GetPtr())->entityID.hash : 0;
        const int playerVehicle=vehicle ? ReadVehicleBool(owner.GetPtr(),"IsPlayerVehicle") : -1;
        const int traffic=vehicle ? ReadVehicleBool(owner.GetPtr(),"HasTrafficSlot") : -1;
        if(slot==0 && entity) {
            const auto now=NowMs();
            const auto attached=static_cast<RED4ext::ent::Entity*>(owner.GetPtr())->status==RED4ext::EntityStatus::Attached;
            const Dnc::VehicleIdentity identity{id,proxy};
            if(attached) {
                const int driver=vehicle ? ReadVehicleBool(owner.GetPtr(),"IsPlayerDriver") : -1;
                lifecycle.Update(lifecycle.Epoch(),{identity,vehicle ? Dnc::Fact::Yes : Dnc::Fact::No,
                    FactOf(playerVehicle),FactOf(traffic),FactOf(driver)},now);
                if(proxy==static_cast<uint32_t>(mounted.load(std::memory_order_relaxed)))
                    lifecycle.SetDriver(lifecycle.Epoch(),identity,FactOf(driver),now);
            } else lifecycle.Retire(lifecycle.Epoch(),identity,now);
        }
        if(slot==0 && !entity) {
            const auto now=NowMs();
            lifecycle.Retire(lifecycle.Epoch(),lifecycle.IdentityForProxy(proxy,now),now);
        }
        if(log) sdk->logger->InfoF(h,"DNC_PAIR_OWNER proxy=%08x slot=%u resolved=1 class=%s entity=%llu vehicle=%u playerVehicle=%d trafficSlot=%d",
            proxy,slot,type->GetName().ToString(),static_cast<unsigned long long>(id),unsigned(vehicle),playerVehicle,traffic);
    }
    return ownerResolved;
}
}
void ObserveNearbyProxy(uint32_t proxy,RED4ext::v1::PluginHandle h,const RED4ext::v1::Sdk* sdk,bool nearby) {
    if(!ownerReaderReady || proxy==0xffffffff) return;
    if(std::find(knownProxies.begin(),knownProxies.end(),proxy)!=knownProxies.end()) return;
    size_t index=knownProxies.size();
    if(index<256) knownProxies.push_back(proxy);
    else {
        index=replaceCursor;
        const auto now=NowMs();
        lifecycle.Retire(lifecycle.Epoch(),lifecycle.IdentityForProxy(knownProxies[index],now),now);
        knownProxies[index]=proxy;
        replaceCursor=(replaceCursor+1)%knownProxies.size();
    }
    failures[index]=0;retryAt[index]=0;
    sdk->logger->InfoF(h,"DNC_DISCOVERED proxy=%08x source=%s",proxy,nearby ? "nearby-query" : "contact");
    LogProxyOwner(proxy,h,sdk);
}
void DrainPairProbe(RED4ext::v1::PluginHandle h,const RED4ext::v1::Sdk* sdk) {
    const auto drain=[&](auto& records,const char* kind) {
        for (auto& r:records) {
            if (r.logged || !r.ready.load(std::memory_order_acquire)) continue;
            sdk->logger->InfoF(h,"DNC_PAIR_%s pair=%u actor0=%08x actor1=%08x observedPlayer=%08x attributes=%08x:%08x filterFlags=%04x pairFlags=%04x",
                kind,r.pair,r.actor0,r.actor1,r.observedPlayer,r.attr0,r.attr1,unsigned(r.filterFlags),unsigned(r.pairFlags));
            r.logged=true;

        }
    };
    drain(samples,"SAMPLE");drain(matches,"PLAYER_MATCH");
    Discovery item;
    for(unsigned i=0;ownerReaderReady && i<128 && discovery.Pop(item);++i) {
        if(item.mount!=mounted.load(std::memory_order_relaxed)) continue;
        for(const auto proxy:{item.first,item.second}) {
            ObserveNearbyProxy(proxy,h,sdk,false);
        }
    }

    static uint64_t refreshAt=0;
    static size_t refreshCursor=0;
    const auto tick=NowMs();
    if(ownerReaderReady && tick>=refreshAt) {
        refreshAt=tick+50;
        const auto count=knownProxies.size()<16 ? knownProxies.size() : size_t{16};
        for(size_t i=0;i<count;++i) {
            if(refreshCursor>=knownProxies.size()) refreshCursor=0;
            const auto index=refreshCursor++;
            if(tick<retryAt[index]) continue;
            if(LogProxyOwner(knownProxies[index],h,sdk,false)) { failures[index]=0; retryAt[index]=0; }
            else {
                if(failures[index]<6) ++failures[index];
                retryAt[index]=tick+(uint64_t{100}<<failures[index]);
            }
        }
        if(published.Publish(lifecycle)) ++publishOk; else ++publishSkipped;
    }
    using Clock=std::chrono::steady_clock;
    static auto next=Clock::time_point{};
    if (target && Clock::now()>=next) {
        next=Clock::now()+std::chrono::seconds(15);
        sdk->logger->InfoF(h,"DNC_DISCOVERY queued=%llu dropped=%llu missingOwners=%llu ruleRejected=%llu refreshCalls=%llu resolved=%llu unresolved=%llu publishOk=%llu publishSkipped=%llu",
            static_cast<unsigned long long>(queued.load()),static_cast<unsigned long long>(queueDrops.load()),
            static_cast<unsigned long long>(missingOwners.load()),static_cast<unsigned long long>(ruleRejected.load()),
            static_cast<unsigned long long>(refreshCalls),static_cast<unsigned long long>(resolvedOwners),
            static_cast<unsigned long long>(unresolvedOwners),static_cast<unsigned long long>(publishOk),static_cast<unsigned long long>(publishSkipped));
        sdk->logger->InfoF(h,"DNC_LIFECYCLE candidates=%llu snapshotBusy=%llu knownProxies=%u creationOnly=1 driverDenied=%llu",
            static_cast<unsigned long long>(candidates.load()),static_cast<unsigned long long>(snapshotBusy.load()),unsigned(knownProxies.size()),static_cast<unsigned long long>(driverDenied.load()));
        sdk->logger->InfoF(h,"DNC_PAIR_COUNTS total=%llu playerMatches=%llu",
            static_cast<unsigned long long>(total.load()),static_cast<unsigned long long>(matching.load()));
    }
}
bool StopPairProbe(RED4ext::v1::PluginHandle h,const RED4ext::v1::Sdk* sdk) {
    ObservePlayerProxy(0xffffffff);
    if (target && !sdk->hooking->Detach(h,target)) {
        sdk->logger->Error(h,"DNC_PAIR_DETACH_FAILED");return false;
    }
    if (target) sdk->logger->Info(h,"DNC_PAIR_DETACHED");
    target=nullptr;
    sdk->logger->InfoF(h,"DNC_GHOST_END contacts=%llu",static_cast<unsigned long long>(ghostedContacts.load()));
    ownerReaderReady=false; // Do not resolve entities during shutdown.
    DrainPairProbe(h,sdk);
    return true;
}
