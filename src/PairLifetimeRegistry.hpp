#pragma once
#include "PairSuppressionPolicy.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
namespace Dnc {
struct PairKey {
    uintptr_t callback{}; uint32_t pair{0xffffffff};
    bool operator==(const PairKey&) const = default;
};
struct PairToken {
    PairKey key{}; uint64_t lifetime{};
    bool operator==(const PairToken&) const = default;
};
struct PairUpdate {
    PairToken token{}; uint64_t revision{}; uint16_t filterFlags{},pairFlags{};
};
// Serial state machine, not a concurrently shared container. Native integration
// must serialize pair lifetime events and status submission. No engine writes here.
template<std::size_t Capacity> class PairLifetimeRegistry {
    static_assert(Capacity>0);
    struct Entry {
        PairToken token{};
        uint16_t originalFilter{}, originalPair{}, currentPair{}, desiredPair{};
        uint32_t attr0{},attr1{};
        uint64_t revision{};
        bool active{},pending{},inFlight{},stopping{},clearForRestore{};
        PairUpdate flight{};
    };
    std::array<Entry,Capacity> entries{};
    uint64_t nextLifetime{1},nextSubmission{1};
    Entry* Find(PairToken token) {
        for(auto& entry:entries) if(entry.active && entry.token==token) return &entry;
        return nullptr;
    }
    static void Queue(Entry& entry,uint16_t flags) {
        if(entry.desiredPair!=flags) { entry.desiredPair=flags; ++entry.revision; }
        entry.pending=entry.currentPair!=entry.desiredPair;
    }
public:
    // Accept only pairs whose original default filter permits tracking. Caller
    // must already have retained the engine pair ID using eNOTIFY.
    std::optional<PairToken> Begin(PairKey key,uint16_t filter,uint16_t flags,uint32_t attr0,uint32_t attr1) {
        if(!key.callback || key.pair==0xffffffff || filter!=0 || nextLifetime==0) return {};
        for(const auto& entry:entries) if(entry.active && entry.token.key==key) return {};
        for(auto& entry:entries) if(!entry.active) {
            entry={}; entry.active=true; entry.token={key,nextLifetime++};
            entry.originalFilter=filter; entry.originalPair=flags;
            entry.currentPair=flags;entry.desiredPair=flags;entry.attr0=attr0;entry.attr1=attr1;
            return entry.token;
        }
        return {};
    }
    std::optional<PairToken> Current(PairKey key) const {
        for(const auto& entry:entries) if(entry.active && entry.token.key==key) return entry.token;
        return {};
    }
    bool RequestEcho(PairToken token) {
        auto* entry=Find(token);
        if(!entry || entry->stopping || entry->inFlight) return false;
        ++entry->revision;entry->pending=true;return true;
    }
    bool RequestSuppression(PairToken token,bool suppressionEnabled,bool driver,bool traffic,bool lifecycleReady) {
        auto* entry=Find(token);
        if(!entry || entry->stopping) return false;
        const auto decision=PlanTrafficContact(entry->originalFilter,entry->originalPair,entry->attr0,entry->attr1,
            suppressionEnabled,driver,traffic,lifecycleReady);
        if(!decision.changed) return false;
        Queue(*entry,decision.pairFlags);return true;
    }
    bool RequestStop(PairToken token) {
        auto* entry=Find(token);if(!entry) return false;
        entry->stopping=true;
        // Cancel suppression which has not yet been submitted. An in-flight
        // submission may still land; its receipt will recompute pending state.
        if(!entry->inFlight && entry->currentPair==entry->originalPair) Queue(*entry,entry->originalPair);
        return true;
    }
    bool ConfirmClear(PairToken token) {
        auto* entry=Find(token);if(!entry || !entry->stopping) return false;
        entry->clearForRestore=true;Queue(*entry,entry->originalPair);return true;
    }
    bool RevokeClearance(PairToken token) {
        auto* entry=Find(token);if(!entry || entry->inFlight) return false;
        entry->clearForRestore=false;return true;
    }
    std::optional<PairUpdate> Prepare(uintptr_t callback) {
        for(auto& entry:entries) {
            if(!entry.active || entry.token.key.callback!=callback || !entry.pending || entry.inFlight) continue;
            if(entry.stopping && !entry.clearForRestore) continue;
            if(nextSubmission==0) return {};
            entry.flight={entry.token,nextSubmission++,static_cast<uint16_t>(entry.originalFilter|0x0c),entry.desiredPair};
            entry.inFlight=true;return entry.flight;
        }
        return {};
    }
    // Records callback submission, NOT independent confirmation of engine state.
    bool Submitted(const PairUpdate& update) {
        auto* entry=Find(update.token);
        if(!entry || !entry->inFlight || entry->flight.revision!=update.revision ||
           entry->flight.pairFlags!=update.pairFlags || entry->flight.filterFlags!=update.filterFlags) return false;
        entry->inFlight=false;entry->currentPair=update.pairFlags;
        if(entry->stopping && entry->clearForRestore) entry->desiredPair=entry->originalPair;
        entry->pending=entry->currentPair!=entry->desiredPair;return true;
    }
    bool Cancel(const PairUpdate& update) {
        auto* entry=Find(update.token);
        if(!entry || !entry->inFlight || entry->flight.revision!=update.revision ||
           entry->flight.pairFlags!=update.pairFlags || entry->flight.filterFlags!=update.filterFlags) return false;
        entry->inFlight=false;
        if(entry->stopping && entry->currentPair==entry->originalPair) Queue(*entry,entry->originalPair);
        return true;
    }
    bool Lost(PairToken token) {
        auto* entry=Find(token);if(!entry) return false;
        entry->active=false;return true;
    }
    void Reset() { for(auto& entry:entries) entry.active=false; } // Never rewind lifetime IDs.
    std::size_t Active() const { std::size_t count=0;for(const auto& entry:entries) count+=entry.active;return count; }
};
}


