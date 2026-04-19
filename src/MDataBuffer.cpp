// src/MDataBuffer.cpp
#include "MDataBuffer.hpp"

#include <new>
#include <stdexcept>

namespace numkit::m {

DataBuffer::DataBuffer(size_t bytes, Allocator *alloc)
    : bytes_(bytes)
    , refCount_(1)
    , allocator_(alloc)
{
    if (bytes > 0) {
        if (alloc && alloc->allocate)
            data_ = alloc->allocate(bytes);
        else
            data_ = ::operator new(bytes);
        if (!data_)
            throw std::runtime_error("DataBuffer: allocation failed");
    }
}

DataBuffer::~DataBuffer()
{
    if (data_) {
        if (allocator_ && allocator_->deallocate)
            allocator_->deallocate(data_, bytes_);
        else
            ::operator delete(data_);
    }
}

void DataBuffer::addRef()
{
    refCount_.fetch_add(1, std::memory_order_relaxed);
}

bool DataBuffer::release()
{
    return refCount_.fetch_sub(1, std::memory_order_acq_rel) == 1;
}

int DataBuffer::refCount() const
{
    return refCount_.load(std::memory_order_acquire);
}

} // namespace numkit::m
