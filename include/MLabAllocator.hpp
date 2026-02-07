// include/MLabAllocator.hpp
#pragma once

#include <cstddef>
#include <functional>

namespace mlab {

struct Allocator
{
    using AllocFn = std::function<void *(size_t)>;
    using DeallocFn = std::function<void(void *, size_t)>;

    AllocFn allocate;
    DeallocFn deallocate;

    static Allocator defaultAllocator();
};

} // namespace mlab
