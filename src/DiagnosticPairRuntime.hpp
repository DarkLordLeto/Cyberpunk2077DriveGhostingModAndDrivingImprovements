#pragma once
#include "PairLifetimeRegistry.hpp"
#include <atomic>
namespace Dnc {
// Diagnostic integration only: exposes no suppression API. A lost-event lock
// conflict latches a fault and stops all subsequent updates; normal response
// remains enabled. This fallback is NOT suitable for already-suppressed pairs.
template<std::size_t Capacity> class DiagnosticPairRuntime {
    PairLifetimeRegistry<Capacity> registry;
    std::atomic_flag busy=ATOMIC_FLAG_INIT;
    std::atomic<bool> fault{false};
    std::atomic<uint64_t> begun{0},lost{0},echoed{0},rejected{0},contention{0},active{0};
    struct Release { std::atomic_flag& flag;~Release(){flag.clear(std::memory_order_release);} };
public:
    struct Stats { uint64_t begun,lost,echoed,rejected,contention,active;bool fault; };
    bool Begin(PairKey key,uint16_t flags) {
        if(fault.load(std::memory_order_acquire)) return false;
        if(busy.test_and_set(std::memory_order_acquire)) {++contention;return false;}
        Release unlock{busy};
        if(fault.load(std::memory_order_acquire)) return false;
        auto token=registry.Begin(key,0,flags,1,1);
        if(!token) {++rejected;return false;}
        registry.RequestEcho(*token);++begun;active.store(registry.Active());return true;
    }
    void Lost(PairKey key) {
        if(fault.load(std::memory_order_acquire)) return;
        if(busy.test_and_set(std::memory_order_acquire)) {++contention;fault.store(true,std::memory_order_release);return;}
        Release unlock{busy};
        if(auto token=registry.Current(key)) {registry.Lost(*token);++lost;active.store(registry.Active());}
    }
    std::optional<PairUpdate> Next(uintptr_t callback) {
        if(fault.load(std::memory_order_acquire)) return {};
        if(busy.test_and_set(std::memory_order_acquire)) {++contention;return {};}
        Release unlock{busy};
        if(fault.load(std::memory_order_acquire)) return {};
        auto update=registry.Prepare(callback);
        if(!update) return {};
        if(!registry.Submitted(*update)) {fault.store(true,std::memory_order_release);return {};}
        ++echoed;return update;
    }
    Stats Snapshot() const {return {begun.load(),lost.load(),echoed.load(),rejected.load(),contention.load(),active.load(),fault.load()};}
};
}
