#include <numkit/core/stack_guard.hpp>
#include <stdexcept>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <processthreadsapi.h>

namespace numkit {

void StackGuard::check(const char *stage)
{
    ULONG_PTR lowLimit = 0;
    ULONG_PTR highLimit = 0;
    GetCurrentThreadStackLimits(&lowLimit, &highLimit);
    volatile char dummy = 0;
    ULONG_PTR currentStack = reinterpret_cast<ULONG_PTR>(&dummy);
    if (lowLimit != 0 && currentStack < lowLimit + SAFETY_MARGIN_BYTES) {
        throw std::runtime_error("Stack exhaustion limit reached during " +
                                 std::string(stage ? stage : "operation"));
    }
}

} // namespace numkit

#elif defined(__EMSCRIPTEN__)
#include <emscripten/stack.h>

namespace numkit {

void StackGuard::check(const char *stage)
{
    uintptr_t freeStack = emscripten_stack_get_free();
    if (freeStack < SAFETY_MARGIN_BYTES) {
        throw std::runtime_error("Stack exhaustion limit reached during " +
                                 std::string(stage ? stage : "operation"));
    }
}

} // namespace numkit

#elif defined(__APPLE__)
#include <pthread.h>

namespace numkit {

void StackGuard::check(const char *stage)
{
    pthread_t self = pthread_self();
    void *stackAddr = pthread_get_stackaddr_np(self);
    size_t stackSize = pthread_get_stacksize_np(self);
    uintptr_t lowLimit = reinterpret_cast<uintptr_t>(stackAddr) - stackSize;
    volatile char dummy = 0;
    uintptr_t currentStack = reinterpret_cast<uintptr_t>(&dummy);
    if (currentStack < lowLimit + SAFETY_MARGIN_BYTES) {
        throw std::runtime_error("Stack exhaustion limit reached during " +
                                 std::string(stage ? stage : "operation"));
    }
}

} // namespace numkit

#elif defined(__linux__) || defined(__unix__) || defined(_POSIX_VERSION)
#include <pthread.h>

namespace numkit {

void StackGuard::check(const char *stage)
{
    pthread_attr_t attr;
    if (pthread_getattr_np(pthread_self(), &attr) == 0) {
        void *stackAddr = nullptr;
        size_t stackSize = 0;
        pthread_attr_getstack(&attr, &stackAddr, &stackSize);
        pthread_attr_destroy(&attr);
        uintptr_t lowLimit = reinterpret_cast<uintptr_t>(stackAddr);
        volatile char dummy = 0;
        uintptr_t currentStack = reinterpret_cast<uintptr_t>(&dummy);
        if (lowLimit != 0 && currentStack < lowLimit + SAFETY_MARGIN_BYTES) {
            throw std::runtime_error("Stack exhaustion limit reached during " +
                                     std::string(stage ? stage : "operation"));
        }
    }
}

} // namespace numkit

#else

namespace numkit {

void StackGuard::check(const char *stage)
{
    (void)stage;
}

} // namespace numkit

#endif
