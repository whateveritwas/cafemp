#include "utils/atomic.hpp"

std::mutex atomic_mutex;

extern "C" {

void __atomic_store_8(volatile void* ptr, unsigned long long val, int memorder) {
    std::lock_guard<std::mutex> lock(atomic_mutex);
    *reinterpret_cast<volatile unsigned long long*>(ptr) = val;
}

unsigned long long __atomic_load_8(const volatile void* ptr, int memorder) {
    std::lock_guard<std::mutex> lock(atomic_mutex);
    return *reinterpret_cast<const volatile unsigned long long*>(ptr);
}

}
