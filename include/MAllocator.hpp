// include/MAllocator.hpp
#pragma once

#include <cstddef>
#include <functional>

namespace numkit::m::m {

struct Allocator
{
    using AllocFn = std::function<void *(size_t)>;
    using DeallocFn = std::function<void(void *, size_t)>;

    AllocFn allocate;
    DeallocFn deallocate;

    static Allocator defaultAllocator();
};

} // namespace numkit::m::m
