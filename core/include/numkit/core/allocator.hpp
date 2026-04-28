// include/allocator.hpp
#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <memory_resource>

namespace numkit {

struct Allocator
{
    using AllocFn = std::function<void *(size_t)>;
    using DeallocFn = std::function<void(void *, size_t)>;

    AllocFn allocate;
    DeallocFn deallocate;

    static Allocator defaultAllocator();

    // Lazily-constructed bridge to std::pmr::memory_resource.
    // Routes do_allocate/do_deallocate through this Allocator's fn members,
    // so std::pmr::vector<T>(mr) and similar pmr-aware containers go through
    // the same tracked path as Value heap buffers.
    //
    // The returned pointer is stable for the lifetime of this Allocator.
    // Copies of Allocator construct their own bridge on demand.
    std::pmr::memory_resource *memoryResource();

private:
    std::shared_ptr<std::pmr::memory_resource> mr_;
};

} // namespace numkit
