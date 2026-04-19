// src/MAllocator.cpp
#include "MAllocator.hpp"

namespace numkit::m::m {

Allocator Allocator::defaultAllocator()
{
    return {[](size_t n) -> void * { return ::operator new(n); },
            [](void *p, size_t) { ::operator delete(p); }};
}

} // namespace numkit::m::m
