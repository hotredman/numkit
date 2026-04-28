// src/allocator.cpp
#include <numkit/core/allocator.hpp>

#include <new>

namespace numkit {

Allocator Allocator::defaultAllocator()
{
    // Explicit field-by-field init rather than braced — Allocator is no longer
    // an aggregate (mr_ is private) since the pmr bridge was added.
    Allocator a;
    a.allocate = [](size_t n) -> void * { return ::operator new(n); };
    a.deallocate = [](void *p, size_t) { ::operator delete(p); };
    return a;
}

// ── pmr::memory_resource bridge ─────────────────────────────────────────
//
// Adapter that forwards do_allocate/do_deallocate to an Allocator's std::function
// members. Alignment is ignored on the assumption the underlying allocator
// returns at least alignof(std::max_align_t)-aligned blocks (which the default
// ::operator new does). If embedders register a custom allocator that produces
// less-aligned blocks, pmr containers holding over-aligned types could misbehave —
// not a concern for double/complex<double>/int used by Value.
namespace {

class AllocatorBridge : public std::pmr::memory_resource
{
public:
    AllocatorBridge(Allocator::AllocFn alloc, Allocator::DeallocFn dealloc)
        : allocate_(std::move(alloc)), deallocate_(std::move(dealloc))
    {}

protected:
    void *do_allocate(std::size_t bytes, std::size_t /*alignment*/) override
    {
        return allocate_(bytes);
    }

    void do_deallocate(void *p, std::size_t bytes, std::size_t /*alignment*/) override
    {
        deallocate_(p, bytes);
    }

    bool do_is_equal(const std::pmr::memory_resource &other) const noexcept override
    {
        return this == &other;
    }

private:
    Allocator::AllocFn allocate_;
    Allocator::DeallocFn deallocate_;
};

} // anonymous namespace

std::pmr::memory_resource *Allocator::memoryResource()
{
    if (!mr_) {
        mr_ = std::make_shared<AllocatorBridge>(allocate, deallocate);
    }
    return mr_.get();
}

} // namespace numkit
