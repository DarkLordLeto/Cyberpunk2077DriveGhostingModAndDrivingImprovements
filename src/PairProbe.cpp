#include "PairProbe.hpp"
#include "BikeBodyMap.hpp"
#include "LiveProbe.hpp"
#include "NativeProbe.hpp"
#include "FadeContacts.hpp"
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
std::atomic<uint64_t> contactFreeMount{UINT64_MAX}, contactFreePairs{0};
uint32_t nextMount=0; // Running callback only.
bool ownerReaderReady=false;
Dnc::VehicleLifecycle<> lifecycle(1500); // milliseconds; diagnostic only
Dnc::TrySnapshot<Dnc::VehicleLifecycle<>> published(lifecycle);
std::atomic<uint64_t> candidates{0}, snapshotBusy{0}, driverDenied{0};
Dnc::DriverGate driverGate;
struct Discovery { uint64_t mount{}; uint32_t first{},second{}; bool ordinary{},known{},ghosted{}; };
Dnc::TryQueue<Discovery,512> discovery;
std::atomic<uint64_t> queued{0}, queueDrops{0}, missingOwners{0}, ruleRejected{0};
uint64_t resolvedOwners=0, unresolvedOwners=0, refreshCalls=0, publishOk=0, publishSkipped=0;
size_t replaceCursor=0;
std::array<uint64_t,256> retryAt{};
std::array<unsigned,256> failures{};
std::vector<uint32_t> knownProxies; // Running callback only; capped at 256.
std::atomic<uint64_t> activeBike{UINT64_MAX},extraPairs{0},characterPairs{0};
uint64_t bikeEntity{},riderEntity{}; // game thread only
Dnc::BikeBodyMap aliases;
Dnc::TrySnapshot<Dnc::BikeBodyMap> bikeBodies(aliases);
struct BodyRequest {uint64_t mount;uint32_t proxy;};
Dnc::TryQueue<BodyRequest,512> bodyRequests;
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
    Dnc::BikeBodyBinding extra0{},extra1{};
    const bool riding=activeBike.load(std::memory_order_acquire)==observation && player!=0xffffffff;
    if(riding && (attr0==1 || attr0==0x11) && (attr1==1 || attr1==0x11)) {
        const auto tick=NowMs();
        bikeBodies.Read([&](const auto& map){extra0=map.Lookup(observation,id0,tick);extra1=map.Lookup(observation,id1,tick);});
        if(!extra0.vehicle.Valid() && id0!=player) bodyRequests.Push({observation,id0});
        if(!extra1.vehicle.Valid() && id1!=player) bodyRequests.Push({observation,id1});
    }
    const auto canonical0=extra0.vehicle.Valid()?extra0.vehicle.proxy:id0;
    const auto canonical1=extra1.vehicle.Valid()?extra1.vehicle.proxy:id1;
    bool trackingCandidate=false;
    Dnc::VehicleIdentity authorizedDriver{},authorizedTraffic{};
    uint64_t decisionEpoch=0;
    if(player!=0xffffffff && (canonical0==player || canonical1==player)) {
        const auto now=NowMs();
        if(!published.Read([&](const auto& state) {
            const auto a=state.IdentityForProxy(canonical0,now), b=state.IdentityForProxy(canonical1,now);
            if(!a.Valid() || !b.Valid()) { missingOwners.fetch_add(1,std::memory_order_relaxed); return; }
            if((extra0.vehicle.Valid() && extra0.vehicle!=a) || (extra1.vehicle.Valid() && extra1.vehicle!=b)) return;
            const auto driverIdentity=a.proxy==player ? a : b;
            if(!driverGate.Allows(driverIdentity,observation,now)) { driverDenied.fetch_add(1,std::memory_order_relaxed); return; }
            if(state.Assess(state.Epoch(),a,b,now)==Dnc::PairAssessment::TrafficCandidate)
                {
                    candidates.fetch_add(1,std::memory_order_relaxed); trackingCandidate=true;
                    authorizedDriver=driverIdentity;
                    authorizedTraffic=a.proxy==player ? b : a;
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
    const bool ordinary=attr0==1 && attr1==1 && result && returned==result && *result==0 && pairFlags && Dnc::SupportedDrivingContact(*pairFlags);
    bool ghosted=false;
    if (trackingCandidate && result && returned==result && pairFlags && Dnc::SupportedDrivingContact(*pairFlags) &&
        actor0 && actor1 && shape0 && shape1 && mounted.load(std::memory_order_acquire)==observation &&
        publishedEpoch.load(std::memory_order_acquire)==decisionEpoch &&
        driverGate.Allows(authorizedDriver,observation,NowMs())) {
        const bool full=contactFreeMount.load(std::memory_order_acquire)==observation;
        const auto decision=Dnc::PlanDrivingPair(*result,*pairFlags,Dnc::DrivingAttribute(attr0,extra0),Dnc::DrivingAttribute(attr1,extra1),full);
        if (decision.changed) {
            *pairFlags=decision.pairFlags;
            *result=decision.filterFlags;
            if(full) ++contactFreePairs;
            if(extra0.vehicle.Valid() || extra1.vehicle.Valid()) ++extraPairs;
            if(extra0.character || extra1.character) ++characterPairs;
            ghosted=true;
            QueueFadeContact({observation,authorizedDriver,authorizedTraffic,NowMs()});
            ghostedContacts.fetch_add(1,std::memory_order_relaxed);
        }
    }
    if(playerMatch) {
        if(discovery.Push({observation,id0,id1,ordinary,trackingCandidate,ghosted})) ++queued;
        else ++queueDrops;
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
void ObserveNearbyVehicle(uint64_t entity,uint32_t proxy,int owned,int traffic,int driver) {
    if(!ownerReaderReady || !entity || proxy==0xffffffff || proxy==static_cast<uint32_t>(mounted.load())) return;
    lifecycle.Update(lifecycle.Epoch(),{{entity,proxy},Dnc::Fact::Yes,FactOf(owned),FactOf(traffic),FactOf(driver)},NowMs());
}
void ObserveDriver(uint64_t entity,uint32_t proxy,int verifiedDriver,int owned,bool bike,uint64_t rider) {
    const auto observation=mounted.load(std::memory_order_acquire);
    if(proxy==0xffffffff || static_cast<uint32_t>(observation)!=proxy) { driverGate.Revoke(); return; }
    const auto now=NowMs();
    const Dnc::VehicleIdentity id{entity,proxy};
    driverGate.Observe(id,observation,FactOf(verifiedDriver),now);
    bikeEntity=entity;riderEntity=rider;
    activeBike.store(verifiedDriver==1 && bike ? observation : UINT64_MAX,std::memory_order_release);
    contactFreeMount.store(verifiedDriver==1 && (bike || owned==0) ? observation : UINT64_MAX,std::memory_order_release);
    lifecycle.Update(lifecycle.Epoch(),{id,Dnc::Fact::Yes,FactOf(owned),Dnc::Fact::Unknown,FactOf(verifiedDriver)},now);
    lifecycle.SetDriver(lifecycle.Epoch(),id,FactOf(verifiedDriver),now);
    PublishNearbyObservations();
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
    sdk->logger->Info(h,"DNC_GHOST_READY mode=creationOnly detectedDynamicPairs=1");
}
namespace {
bool LogProxyOwner(uint32_t proxy,RED4ext::v1::PluginHandle h,const RED4ext::v1::Sdk* sdk,bool log=true) {
    if (!ownerReaderReady) return false;
    // The mounted entity and its seat are refreshed directly by PollLiveVehicle.
    if(proxy==static_cast<uint32_t>(mounted.load())) return true;
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
                if(vehicle && traffic==1 && playerVehicle==0) {
                    InspectFadeTarget(identity,static_cast<RED4ext::ent::Entity*>(owner.GetPtr()),h,sdk);
                }
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
void PublishNearbyObservations() {
    if(published.Publish(lifecycle)) ++publishOk; else ++publishSkipped;
}
namespace {
Dnc::BikeBodyBinding ResolveBody(uint32_t raw,uint64_t now) {
    using namespace RED4ext;
    using GetOwner=void*(*)(WeakHandle<IScriptable>*,uint32_t,uint32_t);
    auto fn=reinterpret_cast<GetOwner>(reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr))+DncBuild::ProxyOwnerRva);
    WeakHandle<IScriptable> weak;fn(&weak,raw,0);auto owner=weak.Lock();
    auto* rtti=CRTTISystem::Get();auto* entityType=rtti->GetClass("entEntity");auto* vehicle=rtti->GetClass("vehicleBaseObject");auto* character=rtti->GetClass("ScriptedPuppet");auto* chassis=rtti->GetClass("vehicleChassisComponent");
    if(!owner || !entityType || !vehicle || !chassis || !owner->GetType()->IsA(entityType)) return {};
    auto* entity=static_cast<ent::Entity*>(owner.GetPtr());if(entity->status!=EntityStatus::Attached) return {};
    const bool puppet=character && owner->GetType()->IsA(character);
    if(entity->entityID.hash==bikeEntity || (puppet && entity->entityID.hash==riderEntity))
        return {raw,{bikeEntity,static_cast<uint32_t>(activeBike.load())},now+300,puppet};
    if(puppet) {owner=TrafficOccupantMount(owner);if(!owner) return {};}
    if(!owner->GetType()->IsA(vehicle) || ReadVehicleBool(owner.GetPtr(),"IsPlayerVehicle")!=0 || ReadVehicleBool(owner.GetPtr(),"HasTrafficSlot")!=1) return {};
    entity=static_cast<ent::Entity*>(owner.GetPtr());if(entity->status!=EntityStatus::Attached || entity->components.size()>512) return {};
    for(const auto& c:entity->components) if(c && c->GetType()->IsA(chassis)) {
        uint32_t proxy=0xffffffff;if(!ReadVerifiedChassisProxy(c.GetPtr(),proxy) || proxy==raw) return {};
        return {raw,{entity->entityID.hash,proxy},now+300,puppet};
    }
    return {};
}
void RefreshBikeBodies() {
    const auto mount=activeBike.load();const auto now=NowMs();
    static std::array<BodyRequest,128> recent{};static size_t cursor=0;
    if(mount!=aliases.mount){aliases={};aliases.mount=mount;recent={};bikeBodies.Publish(aliases);}
    if(!ownerReaderReady || mount==UINT64_MAX || mount!=mounted.load()) return;
    static uint64_t checkAt=0;if(now<checkAt) return;checkAt=now+25;
    for(auto& binding:aliases.entries) if(binding.vehicle.Valid() && binding.expires<=now+100) binding=ResolveBody(binding.raw,now);
    BodyRequest request{};unsigned resolved=0;
    for(unsigned i=0;i<96 && resolved<12 && bodyRequests.Pop(request);++i) {
        if(request.mount!=mount || !request.proxy || request.proxy==0xffffffff || aliases.Lookup(mount,request.proxy,now).vehicle.Valid()) continue;
        if(std::any_of(recent.begin(),recent.end(),[&](const auto& old){return old.proxy==request.proxy && old.mount>now;})) continue;
        recent[cursor++%recent.size()]={now+200,request.proxy};++resolved;
        const auto body=ResolveBody(request.proxy,now);if(!body.vehicle.Valid()) continue;
        for(auto& slot:aliases.entries) if(!slot.vehicle.Valid()){slot=body;break;}
    }
    bikeBodies.Publish(aliases);
}
}
void DrainPairProbe(RED4ext::v1::PluginHandle h,const RED4ext::v1::Sdk* sdk) {
    RefreshBikeBodies();
    UpdateFadeContacts(mounted.load(std::memory_order_acquire),NowMs(),h,sdk);
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
        static uint64_t auditMount=0;
        static std::array<uint32_t,256> audited{};
        static size_t used=0;
        if(auditMount!=item.mount){auditMount=item.mount;used=0;}
        const auto other=item.first==static_cast<uint32_t>(item.mount) ? item.second : item.first;
        if(item.ordinary && used<audited.size() && std::find(audited.begin(),audited.begin()+used,other)==audited.begin()+used) {
            audited[used++]=other;
            sdk->logger->InfoF(h,"DNC_FIRST_CONTACT proxy=%08x classifiedTraffic=%u suppressed=%u",other,unsigned(item.known),unsigned(item.ghosted));
        }
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
    sdk->logger->InfoF(h,"DNC_GHOST_END contacts=%llu contactFree=%llu",static_cast<unsigned long long>(ghostedContacts.load()),static_cast<unsigned long long>(contactFreePairs.load()));
    sdk->logger->InfoF(h,"DNC_BIKE_BODIES extraPairs=%llu characterPairs=%llu",static_cast<unsigned long long>(extraPairs.load()),static_cast<unsigned long long>(characterPairs.load()));
    ownerReaderReady=false; // Do not resolve entities during shutdown.
    DrainPairProbe(h,sdk);
    return true;
}

