#pragma once
#include <array>
#include <atomic>
#include <cstddef>
#include <type_traits>
namespace Dnc {
// Bounded reusable FIFO. Each call attempts once; contention/full returns false.
// All payload/index accesses are protected by the atomic flag. No spinning.
template<class T,std::size_t Capacity> class TryQueue {
    static_assert(Capacity>0 && std::is_nothrow_copy_assignable_v<T>);
    std::array<T,Capacity> data{};
    std::size_t head{},tail{},count{};
    std::atomic_flag busy=ATOMIC_FLAG_INIT;
public:
    bool Push(const T& value) {
        if(busy.test_and_set(std::memory_order_acquire)) return false;
        const bool ok=count<Capacity;
        if(ok) { data[tail]=value; tail=(tail+1)%Capacity; ++count; }
        busy.clear(std::memory_order_release); return ok;
    }
    bool Pop(T& value) {
        if(busy.test_and_set(std::memory_order_acquire)) return false;
        const bool ok=count!=0;
        if(ok) { value=data[head]; head=(head+1)%Capacity; --count; }
        busy.clear(std::memory_order_release); return ok;
    }
};
}
