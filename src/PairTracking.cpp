#include "PairTracking.hpp"
#include "PhysicsPhaseProbe.hpp"
#include "VerifiedBuild.hpp"
#include "VerifiedPhysX.hpp"
#include "TrackedPairLog.hpp"
#include "DiagnosticPairRuntime.hpp"
#include "NativeProbe.hpp"
#include <chrono>
#include <atomic>
#include <array>
#include <cstring>
namespace {
using Lost=void(*)(void*,uint32_t,uint32_t,const void*,uint32_t,const void*,bool);
Lost originalLost=nullptr;
using Status=bool(*)(void*,uint32_t*,uint16_t*,uint16_t*);
Status originalStatus=nullptr;
void** statusSlot=nullptr;
std::atomic<uint64_t> statusCalls{0}, statusUpdates{0};
void** slot=nullptr;
std::atomic<bool> enabled{false};
Dnc::TrackedPairLog<8> records; // Bounded audit only; does not limit live tracking.
Dnc::DiagnosticPairRuntime<128> livePairs;
// Shadow integration: proposed flags are consumed only into a private local value.
// The engine still receives only the existing unchanged-flags diagnostic updates.
Dnc::SuppressionController<128> shadow;
std::atomic<uint64_t> shadowBegun{0},shadowLost{0},shadowSubmitted{0},shadowCleared{0},shadowRejected{0};
void OnLost(void* self,uint32_t pair,uint32_t attr0,const void* data0,uint32_t attr1,const void* data1,bool removed) {
    originalLost(self,pair,attr0,data0,attr1,data1,removed);
    if(shadow.LostCurrent({reinterpret_cast<uintptr_t>(self),pair})) ++shadowLost;
    livePairs.Lost({reinterpret_cast<uintptr_t>(self),pair});
    records.MarkLost(reinterpret_cast<uintptr_t>(self),pair);
}
bool OnStatus(void* self,uint32_t* pair,uint16_t* pairFlags,uint16_t* filterFlags) {
    statusCalls.fetch_add(1,std::memory_order_relaxed);
    if(originalStatus(self,pair,pairFlags,filterFlags)) return true;
    if(!enabled.load(std::memory_order_acquire) || !pair || !pairFlags || !filterFlags) return false;
    Dnc::PairUpdate proposed{};
    if(shadow.SubmitNext(reinterpret_cast<uintptr_t>(self),proposed)) ++shadowSubmitted;
    auto update=livePairs.Next(reinterpret_cast<uintptr_t>(self));
    if(!update) return false;
    SamplePhysicsPhasePair(reinterpret_cast<uintptr_t>(self),update->token.key.pair);
    // Diagnostic IDs are scoped to plugin bookkeeping. The engine may have retired or reused the slot.
    // Only originalStatus may supply an engine update. Sampling needs no output writes.
    return false;
}
bool Replace(void** where,void* expected,void* desired) {
    DWORD old=0;
    if(!VirtualProtect(where,sizeof(void*),PAGE_READWRITE,&old)) return false;
    auto* actual=InterlockedCompareExchangePointer(where,desired,expected);
    DWORD unused=0;
    const bool protectedAgain=VirtualProtect(where,sizeof(void*),old,&unused)!=0;
    if(!protectedAgain) {
        if(actual==expected) InterlockedCompareExchangePointer(where,expected,desired);
        VirtualProtect(where,sizeof(void*),old,&unused);
        return false;
    }
    return actual==expected;
}
}
bool ReserveTrackedPair(void* callback,uint32_t pair,uint32_t first,uint32_t second,uint16_t flags,Dnc::VehiclePairIdentity identity) {
    if(!enabled.load(std::memory_order_acquire) || pair==0xffffffff) return false;
    if(!livePairs.Begin({reinterpret_cast<uintptr_t>(callback),pair},flags)) return false;
    if(shadow.Begin({reinterpret_cast<uintptr_t>(callback),pair},identity,0,flags,1,1,true,true)) ++shadowBegun;
    else ++shadowRejected;
    records.Reserve(reinterpret_cast<uintptr_t>(callback),pair,first,second,flags);
    return true;
}
bool StartPairTracking(RED4ext::v1::PluginHandle h,const RED4ext::v1::Sdk* sdk) {
    auto* game=reinterpret_cast<unsigned char*>(GetModuleHandleW(nullptr));
    auto* phys=reinterpret_cast<unsigned char*>(GetModuleHandleW(L"PhysX3_x64.dll"));
    if(!phys) return false;
    const auto* dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(phys);
    if(dos->e_magic!=IMAGE_DOS_SIGNATURE || dos->e_lfanew<=0 || dos->e_lfanew>4096) return false;
    const auto* pe=reinterpret_cast<const IMAGE_NT_HEADERS64*>(phys+dos->e_lfanew);
    if(pe->Signature!=IMAGE_NT_SIGNATURE || pe->FileHeader.TimeDateStamp!=DncPhysX::Timestamp ||
       pe->OptionalHeader.SizeOfImage!=DncPhysX::ImageSize ||
       std::memcmp(phys+DncPhysX::FilterRva,DncPhysX::FilterBytes.data(),DncPhysX::FilterBytes.size()) ||
       std::memcmp(phys+DncPhysX::StatusRva,DncPhysX::StatusBytes.data(),DncPhysX::StatusBytes.size())) return false;
    // Caller has already checked the executable and callback-table identity.
    auto** candidate=reinterpret_cast<void**>(game+DncBuild::PairCallbackTableRva+8);
    auto* expected=game+0x1ff2b0;
    const unsigned char stub[]={0xc2,0,0};
    if(*candidate!=expected || std::memcmp(expected,stub,sizeof(stub))) return false;
    auto** statusCandidate=reinterpret_cast<void**>(game+DncBuild::PairCallbackTableRva+16);
    auto* statusExpected=game+0x664f00;
    const unsigned char statusStub[]={0x32,0xc0,0xc3};
    if(*statusCandidate!=statusExpected || std::memcmp(statusExpected,statusStub,sizeof(statusStub))) return false;
    originalStatus=reinterpret_cast<Status>(statusExpected);
    originalLost=reinterpret_cast<Lost>(expected);
    slot=candidate;
    if(!Replace(slot,expected,reinterpret_cast<void*>(&OnLost))) {
        if(*slot==reinterpret_cast<void*>(&OnLost)) {
            if(!Replace(slot,reinterpret_cast<void*>(&OnLost),expected)) {
                sdk->logger->Error(h,"DNC_TRACK_PATCH_ROLLBACK_FAILED");return false;
            }
        }
        slot=nullptr;return false;
    }
    statusSlot=statusCandidate;
    if(!Replace(statusSlot,statusExpected,reinterpret_cast<void*>(&OnStatus))) {
        if(*statusSlot==statusExpected) statusSlot=nullptr;
        StopPairTracking(h,sdk);return false;
    }
    if(!StartPhysicsPhaseProbe(h,sdk)) sdk->logger->Warn(h,"DNC_PHASE_SKIPPED: dispatch hook unavailable");
    enabled.store(true,std::memory_order_release);
    sdk->logger->Info(h,"DNC_TRACK_READY capacity=128 auditLimit=8 reusable=1 collisionResponseUnchanged=1 statusEcho=0");return true;
}
void DrainPairTracking(RED4ext::v1::PluginHandle h,const RED4ext::v1::Sdk* sdk) {
    DrainPhysicsPhaseProbe(h,sdk);
    // Game-thread diagnostics only. Retain sampled proxy IDs, never actor pointers.
    // Re-query generation-checked proxies even after pairLost to distinguish
    // refilter loss from later geometric clearance. No restoration is authorized.
    using Clock=std::chrono::steady_clock;
    static auto nextBounds=Clock::time_point{};
    static auto nextStats=Clock::time_point{};
    static bool reportedFault=false;
    const auto stats=livePairs.Snapshot();
    if(stats.fault && !reportedFault) {sdk->logger->Error(h,"DNC_REGISTRY_FAULT: updates stopped; collision response unchanged");reportedFault=true;}
    if(Clock::now()>=nextStats || !enabled.load(std::memory_order_acquire)) {
        nextStats=Clock::now()+std::chrono::seconds(5);
        sdk->logger->InfoF(h,"DNC_SHADOW begun=%llu lost=%llu proposed=%llu cleared=%llu rejected=%llu active=%llu collisionWrites=0",
            shadowBegun.load(),shadowLost.load(),shadowSubmitted.load(),shadowCleared.load(),shadowRejected.load(),
            static_cast<unsigned long long>(shadow.ActivePairs()));
        sdk->logger->InfoF(h,"DNC_REGISTRY begun=%llu lost=%llu echoed=%llu rejected=%llu contention=%llu active=%llu fault=%u",
            static_cast<unsigned long long>(stats.begun),static_cast<unsigned long long>(stats.lost),static_cast<unsigned long long>(stats.echoed),
            static_cast<unsigned long long>(stats.rejected),static_cast<unsigned long long>(stats.contention),static_cast<unsigned long long>(stats.active),unsigned(stats.fault));
    }
    static std::array<int,8> previousRelation{-1,-1,-1,-1,-1,-1,-1,-1};
    if(enabled.load(std::memory_order_acquire) && Clock::now()>=nextBounds) {
        nextBounds=Clock::now()+std::chrono::milliseconds(200);
        // Diagnostic evidence only; never authorizes a real engine restoration.
        const auto tickets=shadow.ClearanceCandidates();
        static std::size_t clearanceCursor=0;
        unsigned sampled=0;
        for(std::size_t scanned=0;scanned<tickets.size() && sampled<4;++scanned) {
            const auto& ticket=tickets[clearanceCursor++ % tickets.size()];
            if(!ticket) continue;
            ++sampled;
            Dnc::WorldBounds a,b;uint32_t countA=0,countB=0;
            if(ReadVerifiedProxyBounds(ticket->identity.playerProxy,a,countA) &&
               ReadVerifiedProxyBounds(ticket->identity.trafficProxy,b,countB) &&
               Dnc::CompareBounds(a,b)==Dnc::BoundsRelation::Clear && shadow.ConfirmClear(*ticket)) ++shadowCleared;
        }
        auto& entries=records.Entries();
        for(size_t i=0;i<entries.size();++i) {
            auto& r=entries[i];
            if(!r.ready.load(std::memory_order_acquire)) continue;
            bool duplicate=false;
            for(size_t j=0;j<i;++j) {
                const auto& old=entries[j];
                if(old.ready.load(std::memory_order_acquire) && old.callback==r.callback &&
                   ((old.first==r.first && old.second==r.second)||(old.first==r.second && old.second==r.first))) duplicate=true;
            }
            if(duplicate) continue;
            Dnc::WorldBounds a,b;
            uint32_t bodiesA=0,bodiesB=0;
            const bool valid=ReadVerifiedProxyBounds(r.first,a,bodiesA) && ReadVerifiedProxyBounds(r.second,b,bodiesB);
            const auto relation=valid ? Dnc::CompareBounds(a,b) : Dnc::BoundsRelation::Unknown;
            if(previousRelation[i]==static_cast<int>(relation)) continue;
            previousRelation[i]=static_cast<int>(relation);
            const char* name=relation==Dnc::BoundsRelation::Clear ? "clear" : relation==Dnc::BoundsRelation::Overlapping ? "overlapping" : "unknown";
            sdk->logger->InfoF(h,"DNC_BOUNDS_RELATION actor0=%08x actor1=%08x relation=%s pairLost=%u diagnosticOnly=1 bodies=%u:%u inventory=proxy-all",
                r.first,r.second,name,unsigned(r.lost.load(std::memory_order_acquire)),bodiesA,bodiesB);
            if(valid) {
                sdk->logger->InfoF(h,"DNC_BOUNDS actor=%08x min=(%.3f,%.3f,%.3f) max=(%.3f,%.3f,%.3f)",r.first,a.minimum[0],a.minimum[1],a.minimum[2],a.maximum[0],a.maximum[1],a.maximum[2]);
                sdk->logger->InfoF(h,"DNC_BOUNDS actor=%08x min=(%.3f,%.3f,%.3f) max=(%.3f,%.3f,%.3f)",r.second,b.minimum[0],b.minimum[1],b.minimum[2],b.maximum[0],b.maximum[1],b.maximum[2]);
            }
        }
    }
    for(auto& r:records.Entries()) {
        if(!r.ready.load(std::memory_order_acquire)) continue;
        if(!r.logged) {
            sdk->logger->InfoF(h,"DNC_TRACK_FOUND pair=%u actor0=%08x actor1=%08x originalPairFlags=%04x",r.pair,r.first,r.second,unsigned(r.flags));
            r.logged=true;
        }
        if(r.echoSent.load(std::memory_order_acquire) && !r.echoLogged) {
            sdk->logger->InfoF(h,"DNC_STATUS_ECHO pair=%u actor0=%08x actor1=%08x pairFlags=%04x filterFlags=000c",r.pair,r.first,r.second,unsigned(r.flags));
            r.echoLogged=true;
        }
        if(r.lost.load(std::memory_order_acquire) && !r.lostLogged) {
            sdk->logger->InfoF(h,"DNC_TRACK_LOST pair=%u actor0=%08x actor1=%08x",r.pair,r.first,r.second);
            r.lostLogged=true;
        }
    }
}
bool StopPairTracking(RED4ext::v1::PluginHandle h,const RED4ext::v1::Sdk* sdk) {
    enabled.store(false,std::memory_order_release);
    if(!StopPhysicsPhaseProbe(h,sdk)) return false;
    if(statusSlot) {
        if(!Replace(statusSlot,reinterpret_cast<void*>(&OnStatus),reinterpret_cast<void*>(originalStatus))) {
            sdk->logger->Error(h,"DNC_STATUS_RESTORE_FAILED");return false;
        }
        statusSlot=nullptr;sdk->logger->Info(h,"DNC_STATUS_RESTORED");
    }
    if(slot) {
        if(!Replace(slot,reinterpret_cast<void*>(&OnLost),reinterpret_cast<void*>(originalLost))) {
            sdk->logger->Error(h,"DNC_TRACK_RESTORE_FAILED");return false;
        }
        slot=nullptr;sdk->logger->Info(h,"DNC_TRACK_RESTORED");
    }
    sdk->logger->InfoF(h,"DNC_STATUS_COUNTS calls=%llu updates=%llu",
        static_cast<unsigned long long>(statusCalls.load()),static_cast<unsigned long long>(statusUpdates.load()));
    DrainPairTracking(h,sdk);return true;
}


