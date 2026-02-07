// src/MLabAllocator.cpp
#include "MLabAllocator.hpp"

namespace mlab {

Allocator Allocator::defaultAllocator()
{
    return {[](size_t n) -> void * { return ::operator new(n); },
            [](void *p, size_t) { ::operator delete(p); }};
}

} // namespace mlab
