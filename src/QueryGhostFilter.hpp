#pragma once
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
namespace Dnc {
// Short-lived set of physics proxy IDs that scene queries should pass through.
// Writers try once and never wait; readers are lock-free. A reader racing a
// writer can at worst miss or briefly keep one entry, which the next contact
// or expiry corrects.
class GhostProxySet {
    static constexpr std::size_t Capacity=24;
    std::array<std::atomic<uint32_t>,Capacity> ids{};
    std::array<std::atomic<uint64_t>,Capacity> expires{};
    std::atomic<uint64_t> latest{0};
    std::atomic_flag writing=ATOMIC_FLAG_INIT;
public:
    bool Add(uint32_t proxy,uint64_t now,uint64_t lifetime) {
        if(proxy==0 || proxy==0xffffffff || lifetime==0) return false;
        if(writing.test_and_set(std::memory_order_acquire)) return false;
        const uint64_t end=now+lifetime;
        std::size_t slot=Capacity;
        for(std::size_t i=0;i<Capacity;++i) {
            const auto until=expires[i].load(std::memory_order_relaxed);
            if(until>now && ids[i].load(std::memory_order_relaxed)==proxy) { slot=i; break; }
            if(until<=now && slot==Capacity) slot=i;
        }
        const bool ok=slot<Capacity;
        if(ok) {
            if(ids[slot].load(std::memory_order_relaxed)!=proxy) {
                expires[slot].store(0,std::memory_order_release);
                ids[slot].store(proxy,std::memory_order_release);
            }
            if(expires[slot].load(std::memory_order_relaxed)<end) expires[slot].store(end,std::memory_order_release);
            if(latest.load(std::memory_order_relaxed)<end) latest.store(end,std::memory_order_release);
        }
        writing.clear(std::memory_order_release);
        return ok;
    }
    // Cheap gate for the query hooks: false means no entry can be live.
    bool Active(uint64_t now) const { return now<latest.load(std::memory_order_acquire); }
    bool Contains(uint32_t proxy,uint64_t now) const {
        if(proxy==0 || proxy==0xffffffff) return false;
        for(std::size_t i=0;i<Capacity;++i)
            if(ids[i].load(std::memory_order_acquire)==proxy && now<expires[i].load(std::memory_order_acquire)) return true;
        return false;
    }
    // Game thread only. Writers hold the flag for a few instructions.
    void Clear() {
        while(writing.test_and_set(std::memory_order_acquire)) {}
        latest.store(0,std::memory_order_release);
        for(std::size_t i=0;i<Capacity;++i) { expires[i].store(0,std::memory_order_release); ids[i].store(0,std::memory_order_release); }
        writing.clear(std::memory_order_release);
    }
};
// PxQueryHitType values and PxQueryFlag bits of the installed PhysX 3.4 build.
enum : int { QueryHitNone=0, QueryHitTouch=1, QueryHitBlock=2 };
enum : uint16_t { QueryPreFilter=4, QueryPostFilter=8, QueryReserved=0x8000 };
// Same virtual order as physx::PxQueryFilterCallback: preFilter, postFilter, destructor.
struct QueryFilterCallback {
    virtual int preFilter(const void* filterData,const void* shape,const void* actor,uint16_t* hitFlags)=0;
    virtual int postFilter(const void* filterData,const void* hit)=0;
    virtual ~QueryFilterCallback()=default;
};
struct GhostQueryPlan {
    uint16_t flags{};       // flags to pass to PhysX
    bool forwardPre{}, forwardPost{};
    int fallback{QueryHitBlock}; // hit type PhysX would use with no prefilter
};
// Verified in PhysX3_x64.dll at 0x61c27..0x61cc1: without a prefilter a shape
// is a touch when the hit buffer accepts touches (or the reserved flag is set),
// otherwise a block. eNO_BLOCK is applied by PhysX afterwards.
inline GhostQueryPlan PlanGhostQuery(uint16_t flags,bool hasCallback,uint32_t maxTouches) {
    GhostQueryPlan plan;
    plan.forwardPre=hasCallback && (flags&QueryPreFilter);
    plan.forwardPost=hasCallback && (flags&QueryPostFilter);
    plan.fallback=(maxTouches!=0 || (flags&QueryReserved)) ? QueryHitTouch : QueryHitBlock;
    plan.flags=static_cast<uint16_t>(flags|QueryPreFilter);
    if(!plan.forwardPost) plan.flags=static_cast<uint16_t>(plan.flags&~QueryPostFilter);
    return plan;
}
// PxActor::userData holds the engine proxy ID in its low 32 bits (same field the pair callback reads).
inline uint32_t ActorProxy(const void* actor) {
    uint32_t id=0;
    if(actor) std::memcpy(&id,static_cast<const unsigned char*>(actor)+0x10,sizeof(id));
    return id;
}
// Lives on the stack for one query. Drops ghosted actors, defers everything else.
class GhostQueryFilter final : public QueryFilterCallback {
    const GhostProxySet& set;
    QueryFilterCallback* original;
    GhostQueryPlan plan;
    uint64_t now;
public:
    uint32_t rejected{};
    GhostQueryFilter(const GhostProxySet& ghosts,QueryFilterCallback* callback,GhostQueryPlan queryPlan,uint64_t tick)
        : set(ghosts),original(callback),plan(queryPlan),now(tick) {}
    int preFilter(const void* filterData,const void* shape,const void* actor,uint16_t* hitFlags) override {
        if(set.Contains(ActorProxy(actor),now)) { ++rejected; return QueryHitNone; }
        return plan.forwardPre ? original->preFilter(filterData,shape,actor,hitFlags) : plan.fallback;
    }
    int postFilter(const void* filterData,const void* hit) override {
        return plan.forwardPost ? original->postFilter(filterData,hit) : QueryHitBlock;
    }
};
}
