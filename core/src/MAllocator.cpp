// src/MAllocator.cpp
#include <numkit/m/core/MAllocator.hpp>

namespace numkit::m {

Allocator Allocator::defaultAllocator()
{
    return {[](size_t n) -> void * { return ::operator new(n); },
            [](void *p, size_t) { ::operator delete(p); }};
}

} // namespace numkit::m
