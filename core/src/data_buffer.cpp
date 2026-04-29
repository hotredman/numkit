// core/src/data_buffer.cpp
#include <numkit/core/data_buffer.hpp>

#include <new>
#include <stdexcept>

namespace numkit {

DataBuffer::DataBuffer(size_t bytes, std::pmr::memory_resource *mr)
    : bytes_(bytes)
    , refCount_(1)
    , mr_(mr ? mr : std::pmr::get_default_resource())
{
    if (bytes > 0) {
        data_ = mr_->allocate(bytes, alignof(std::max_align_t));
        if (!data_)
            throw std::runtime_error("DataBuffer: allocation failed");
    }
}

DataBuffer::~DataBuffer()
{
    if (data_)
        mr_->deallocate(data_, bytes_, alignof(std::max_align_t));
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

} // namespace numkit
