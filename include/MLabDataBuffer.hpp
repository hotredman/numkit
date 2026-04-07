// include/MLabDataBuffer.hpp
#pragma once

#include "MLabAllocator.hpp"

#include <atomic>
#include <cstddef>

namespace mlab {

// ============================================================
// DataBuffer — raw memory block with ref-counting
// ============================================================
class DataBuffer
{
public:
    DataBuffer(size_t bytes, Allocator *alloc);
    ~DataBuffer();

    DataBuffer(const DataBuffer &) = delete;
    DataBuffer &operator=(const DataBuffer &) = delete;

    void *data() { return data_; }
    const void *data() const { return data_; }
    size_t bytes() const { return bytes_; }
    Allocator *allocator() const { return allocator_; }

    void addRef();
    bool release();
    int refCount() const;

private:
    void *data_ = nullptr;
    size_t bytes_ = 0;
    std::atomic<int> refCount_{1};
    Allocator *allocator_ = nullptr;
};

} // namespace mlab
