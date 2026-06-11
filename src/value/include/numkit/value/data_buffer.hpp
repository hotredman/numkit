// core/include/numkit/value/data_buffer.hpp
#pragma once

#include <atomic>
#include <cstddef>
#include <memory_resource>

namespace numkit {

// ============================================================
// DataBuffer — raw memory block with ref-counting
//
// Allocated via a std::pmr::memory_resource passed in at construction.
// The same resource is used for deallocation in the destructor — caller
// must keep it alive while the DataBuffer (and any cloned chain) lives.
//
// nullptr `mr` is mapped to std::pmr::get_default_resource() so the
// "engine-free" Value path (Value::matrix(..., nullptr)) keeps working
// without callers having to look up the default resource themselves.
//
// Non-owning mode: construct via the ViewTag ctor to wrap an external
// pointer without ownership; destructor will not deallocate. Used by
// Value::view() for zero-copy borrowing of caller-managed buffers.
// ============================================================
class DataBuffer
{
public:
    struct ViewTag {};

    DataBuffer(size_t bytes, std::pmr::memory_resource *mr);
    /// Non-owning ctor: wraps `external` for `bytes` bytes. Destructor
    /// will not free; caller manages lifetime.
    DataBuffer(ViewTag, void *external, size_t bytes) noexcept;
    ~DataBuffer();

    DataBuffer(const DataBuffer &) = delete;
    DataBuffer &operator=(const DataBuffer &) = delete;

    void *data() { return data_; }
    const void *data() const { return data_; }
    size_t bytes() const { return bytes_; }
    std::pmr::memory_resource *resource() const { return mr_; }
    bool owns() const { return owns_; }

    void addRef();
    bool release();
    int refCount() const;

private:
    void *data_ = nullptr;
    size_t bytes_ = 0;
    std::atomic<int> refCount_{1};
    std::pmr::memory_resource *mr_ = nullptr;
    bool owns_ = true;
};

} // namespace numkit
