// src/MAllocator.cpp
#include "MAllocator.hpp"

namespace numkit {

Allocator Allocator::defaultAllocator()
{
    return {[](size_t n) -> void * { return ::operator new(n); },
            [](void *p, size_t) { ::operator delete(p); }};
}

} // namespace numkit
