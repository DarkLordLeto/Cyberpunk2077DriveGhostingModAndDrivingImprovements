#pragma once
#include <atomic>
#include <type_traits>
namespace Dnc {
// Fixed storage. Readers and writer try once and skip on contention. No allocation,
// spinning, exposed references, or unprotected access to the stored snapshot.
template<class T> class TrySnapshot {
    mutable std::atomic_flag busy=ATOMIC_FLAG_INIT;
    T value;
    struct Release { std::atomic_flag& flag; ~Release(){ flag.clear(std::memory_order_release); } };
public:
    explicit TrySnapshot(const T& initial):value(initial) {}
    bool Publish(const T& next) {
        static_assert(std::is_nothrow_copy_assignable_v<T>);
        if(busy.test_and_set(std::memory_order_acquire)) return false;
        Release unlock{busy}; value=next; return true;
    }
    template<class F> bool Read(F&& use) const {
        if(busy.test_and_set(std::memory_order_acquire)) return false;
        Release unlock{busy}; use(static_cast<const T&>(value)); return true;
    }
};
}
