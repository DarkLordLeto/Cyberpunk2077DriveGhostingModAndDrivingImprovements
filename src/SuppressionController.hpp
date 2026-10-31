#pragma once
#include "PairLifetimeRegistry.hpp"
#include <mutex>
#include <type_traits>
namespace Dnc {
// Offline controller. Engine integration must serialize callback completion with
// pair removal, and provide synchronized clearance evidence. No engine calls here.
struct VehiclePairIdentity {
    uint64_t session{}, playerEntity{}, trafficEntity{};
    uint32_t playerProxy{}, trafficProxy{};
    bool operator==(const VehiclePairIdentity&) const = default;
};
struct ClearanceTicket {
    VehiclePairIdentity identity{};
    uint64_t generation{}, revision{};
};
template<std::size_t Capacity> class SuppressionController {
    struct Intent {
        VehiclePairIdentity identity{};
        uint64_t generation{}, revision{};
        bool active{}, stopping{};
    };
    struct Binding { PairToken token{}; std::size_t intent{}; bool active{}; };
    std::array<Intent,Capacity> intents{};
    std::array<Binding,Capacity> bindings{};
    PairLifetimeRegistry<Capacity> registry;
    mutable std::mutex mutex;
    uint64_t nextGeneration{1};
    bool accepting{true};
    static bool Valid(const VehiclePairIdentity& id) {
        return id.session && id.playerEntity && id.trafficEntity && id.playerEntity!=id.trafficEntity &&
            id.playerProxy!=0xffffffff && id.trafficProxy!=0xffffffff && id.playerProxy!=id.trafficProxy;
    }
    void InvalidateClearance(std::size_t index) {
        for(const auto& b:bindings) if(b.active && b.intent==index) registry.RevokeClearance(b.token);
    }
    bool HasBindings(std::size_t index) const {
        for(const auto& b:bindings) if(b.active && b.intent==index) return true;
        return false;
    }
public:
    // All gates must hold for a new intent. Existing intent survives eligibility
    // loss/disable until clearance, but never crosses entity/proxy/session identity.
    std::optional<PairToken> Begin(PairKey key,VehiclePairIdentity id,uint16_t filter,uint16_t flags,
        uint32_t attr0,uint32_t attr1,bool eligible,bool lifecycleReady) {
        std::lock_guard lock(mutex);
        if(!Valid(id)) return {};
        std::size_t index=Capacity,freeIntent=Capacity,freeBinding=Capacity;
        for(std::size_t i=0;i<Capacity;++i) {
            if(intents[i].active && intents[i].identity==id) index=i;
            if(!intents[i].active) freeIntent=i;
            if(!bindings[i].active) freeBinding=i;
        }
        if(freeBinding==Capacity) return {};
        const bool fresh=index==Capacity;
        if(fresh && (!accepting || !eligible || !lifecycleReady || freeIntent==Capacity || !nextGeneration)) return {};
        // Even continuity must reject unsupported native attributes/flags.
        if(!PlanTrafficContact(filter,flags,attr0,attr1,true,true,true,true).changed) return {};
        auto token=registry.Begin(key,filter,flags,attr0,attr1);
        if(!token) return {};
        if(fresh) {
            index=freeIntent;
            intents[index]={id,nextGeneration++,0,true,false};
        }
        auto& intent=intents[index];
        InvalidateClearance(index);
        ++intent.revision;
        bindings[freeBinding]={*token,index,true};
        registry.RequestSuppression(*token,true,true,true,true);
        // A stopping intent still suppresses newly recreated pairs until clear.
        // RequestStop is deferred until clearance to allow this submission.
        return token;
    }
private:
    bool LostLocked(PairToken token) {
        for(auto& b:bindings) if(b.active && b.token==token) {
            if(!registry.Lost(token)) return false;
            InvalidateClearance(b.intent);
            ++intents[b.intent].revision;b.active=false;return true;
        }
        return false;
    }
public:
    bool Lost(PairToken token) {
        std::lock_guard lock(mutex);return LostLocked(token);
    }
    bool LostCurrent(PairKey key) {
        std::lock_guard lock(mutex);
        auto token=registry.Current(key);return token && LostLocked(*token);
    }
    void StopAll() {
        std::lock_guard lock(mutex);
        accepting=false;
        for(std::size_t n=0;n<Capacity;++n) if(intents[n].active) {
            intents[n].stopping=true;++intents[n].revision;InvalidateClearance(n);
        }
    }
    std::optional<ClearanceTicket> SampleTicket(VehiclePairIdentity id) const {
        std::lock_guard lock(mutex);
        for(std::size_t n=0;n<Capacity;++n) {
            const auto& i=intents[n];
            if(i.active && i.identity==id && (i.stopping || !HasBindings(n)))
                return ClearanceTicket{id,i.generation,i.revision};
        }
        return {};
    }
    std::array<std::optional<ClearanceTicket>,Capacity> ClearanceCandidates() const {
        std::lock_guard lock(mutex);
        std::array<std::optional<ClearanceTicket>,Capacity> result{};
        for(std::size_t n=0;n<Capacity;++n) {
            const auto& i=intents[n];
            if(i.active && (i.stopping || !HasBindings(n)))
                result[n]=ClearanceTicket{i.identity,i.generation,i.revision};
        }
        return result;
    }
    // Caller must supply fresh, synchronized complete-vehicle clearance. A ticket
    // rejects samples spanning controller mutations; it cannot prove physics freshness.
    bool ConfirmClear(ClearanceTicket ticket) {
        std::lock_guard lock(mutex);
        for(std::size_t n=0;n<Capacity;++n) {
            auto& i=intents[n];
            if(!i.active || (!i.stopping && HasBindings(n)) || i.identity!=ticket.identity ||
                i.generation!=ticket.generation || i.revision!=ticket.revision) continue;
            for(const auto& b:bindings) if(b.active && b.intent==n) {
                registry.RequestStop(b.token);registry.ConfirmClear(b.token);
            }
            ++i.revision;
            if(!HasBindings(n)) i.active=false;
            return true;
        }
        return false;
    }
    // Default path never emits restoration: a queued game-thread clearance ticket
    // is insufficient. Suppression can proceed without a geometry read.
    bool SubmitNext(uintptr_t callback,PairUpdate& output) {
        return SubmitNextChecked(callback,output,[](VehiclePairIdentity) noexcept {return false;});
    }
    // Called at the consumer phase. Reader runs outside the controller lock;
    // native integration must guarantee stable physics through output consumption.
    // No stored clear result is allowed to bypass this per-submission read.
    template<class ReadClear> bool SubmitNextChecked(uintptr_t callback,PairUpdate& output,ReadClear&& readClear) {
        static_assert(std::is_nothrow_invocable_r_v<bool,ReadClear,VehiclePairIdentity>);
        PairUpdate update{};
        ClearanceTicket sampled{};
        std::size_t index=Capacity;
        {
            std::lock_guard lock(mutex);
            auto prepared=registry.Prepare(callback);
            if(!prepared) return false;
            update=*prepared;
            for(const auto& b:bindings) if(b.active && b.token==update.token) {
                index=b.intent;const auto& i=intents[index];
                sampled={i.identity,i.generation,i.revision};break;
            }
            if(index==Capacity) {registry.Cancel(update);return false;}
            // Clearing SOLVE_CONTACT was the only permitted mutation. A set bit
            // therefore denotes restoration of the original pair flags.
            if(!(update.pairFlags & 1)) {
                if(!registry.Submitted(update)) return false;
                output=update;return true;
            }
        }
        const bool clear=readClear(sampled.identity);
        std::lock_guard lock(mutex);
        const auto& intent=intents[index];
        const auto current=registry.Current(update.token.key);
        if(!clear || !intent.active || intent.identity!=sampled.identity ||
            intent.generation!=sampled.generation || intent.revision!=sampled.revision ||
            !current || *current!=update.token) {
            registry.Cancel(update);
            registry.RevokeClearance(update.token);
            return false;
        }
        if(!registry.Submitted(update)) return false;
        output=update;return true;
    }
    std::size_t ActiveIntents() const {
        std::lock_guard lock(mutex);std::size_t count=0;
        for(const auto& i:intents) count+=i.active;return count;
    }
    bool CanDetach() const {
        std::lock_guard lock(mutex);
        // Conservative: require no live bindings AND no unresolved overlap intent.
        for(const auto& i:intents) if(i.active) return false;
        return registry.Active()==0;
    }
    std::size_t ActivePairs() const { std::lock_guard lock(mutex);return registry.Active(); }
};
}

