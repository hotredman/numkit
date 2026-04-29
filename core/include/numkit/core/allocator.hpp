// include/allocator.hpp
#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <memory_resource>

namespace numkit {

// Single source of heap traffic for the engine. Holds an allocate/deallocate
// pair plus a lazily-constructed bridge to std::pmr::memory_resource so
// pmr-aware containers (ScratchVec, std::pmr::vector, ...) and raw allocate()
// calls share the same tracked path.
//
// Allocator is a value type, immutable after construction. The pmr bridge is
// built eagerly in the ctor — no lazy init, no synchronisation primitive,
// no race possible by construction. allocate / deallocate are const so
// embedders cannot rebind them post-construction; that would silently
// diverge the raw path from the pmr path (which captures the fns by value
// at ctor time).
struct Allocator
{
    using AllocFn = std::function<void *(std::size_t)>;
    using DeallocFn = std::function<void(void *, std::size_t)>;

    // Construct from explicit fns. The pmr bridge is built eagerly here;
    // no later rebinding possible.
    Allocator(AllocFn alloc, DeallocFn dealloc);

    // Default fns: ::operator new / delete.
    static Allocator defaultAllocator();

    const AllocFn  allocate;
    const DeallocFn deallocate;

    // pmr bridge — set once in the ctor, read-only afterwards. Stable for
    // the lifetime of this Allocator. Copies share the same bridge instance
    // (the bridge holds its own copies of the fns by value).
    std::pmr::memory_resource *memoryResource() const noexcept
    {
        return mr_.get();
    }

private:
    std::shared_ptr<std::pmr::memory_resource> mr_;
};

} // namespace numkit
