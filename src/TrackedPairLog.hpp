#pragma once
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
namespace Dnc {
template<std::size_t Capacity> class TrackedPairLog {
    static_assert(Capacity>0);
public:
    struct Entry {
        uintptr_t callback{};uint32_t pair{},first{},second{};uint16_t flags{};
        std::atomic<bool> ready{false},lost{false},echoClaimed{false},echoSent{false};
        bool logged=false,lostLogged=false,echoLogged=false; // Consumer only.
    };
private:
    std::array<Entry,Capacity> entries{};
    std::atomic<std::size_t> claimed{0};
public:
    bool Reserve(uintptr_t callback,uint32_t pair,uint32_t first,uint32_t second,uint16_t flags) {
        if(!callback || pair==0xffffffff) return false;
        auto index=claimed.load(std::memory_order_relaxed);
        if(index>=Capacity || !claimed.compare_exchange_strong(index,index+1,std::memory_order_relaxed)) return false;
        auto& r=entries[index];r.callback=callback;r.pair=pair;r.first=first;r.second=second;r.flags=flags;
        r.ready.store(true,std::memory_order_release);return true;
    }
    void MarkLost(uintptr_t callback,uint32_t pair) {
        for(auto& r:entries)
            if(r.ready.load(std::memory_order_acquire) && r.callback==callback && r.pair==pair)
                r.lost.store(true,std::memory_order_release);
    }
    void MarkEcho(uintptr_t callback,uint32_t pair) {
        for(auto& r:entries)
            if(r.ready.load(std::memory_order_acquire) && r.callback==callback && r.pair==pair && !r.lost.load(std::memory_order_acquire))
                r.echoSent.store(true,std::memory_order_release);
    }
    struct Echo { uint32_t pair{}; uint16_t pairFlags{},filterFlags{}; };
    bool NextEcho(uintptr_t callback,Echo& out) {
        for(auto& r:entries) {
            if(!r.ready.load(std::memory_order_acquire) || r.callback!=callback || r.lost.load(std::memory_order_acquire)) continue;
            bool expected=false;
            if(!r.echoClaimed.compare_exchange_strong(expected,true,std::memory_order_acq_rel)) continue;
            // Engine dispatches status updates after simulation. Do not queue this
            // ID on the game thread or retain it beyond the callback invocation.
            if(r.lost.load(std::memory_order_acquire)) continue;
            out={r.pair,r.flags,0x0c}; r.echoSent.store(true,std::memory_order_release); return true;
        }
        return false;
    }
    auto& Entries(){return entries;}
};
}
