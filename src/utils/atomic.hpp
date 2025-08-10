#ifndef ATOMIC_H
#define ATOMIC_H

#include <mutex>

extern std::mutex atomic_mutex;

extern "C" {
    void __atomic_store_8(volatile void* ptr, unsigned long long val, int memorder);
    unsigned long long __atomic_load_8(const volatile void* ptr, int memorder);
}

#endif
