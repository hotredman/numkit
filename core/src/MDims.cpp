// src/MDims.cpp
#include <numkit/m/core/MDims.hpp>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>

namespace numkit::m {

Dims::Dims() : heap_(nullptr), nd_(2)
{
    inline_[0] = 0;
    inline_[1] = 0;
    inline_[2] = 1;
    inline_[3] = 1;
}

Dims::Dims(size_t r, size_t c) : heap_(nullptr), nd_(2)
{
    inline_[0] = r;
    inline_[1] = c;
    inline_[2] = 1;
    inline_[3] = 1;
}

Dims::Dims(size_t r, size_t c, size_t p) : heap_(nullptr), nd_(3)
{
    inline_[0] = r;
    inline_[1] = c;
    inline_[2] = p;
    inline_[3] = 1;
}

Dims::Dims(const size_t *dims, int nd) : heap_(nullptr)
{
    setND(dims, nd);
}

void Dims::setND(const size_t *dims, int nd)
{
    if (nd < 1) nd = 1;
    nd_ = static_cast<uint8_t>(nd);
    for (int i = 0; i < kInlineCap; ++i)
        inline_[i] = (i < nd) ? dims[i] : 1;
    if (nd > kInlineCap) {
        const int extra = nd - kInlineCap;
        heap_ = static_cast<size_t *>(std::malloc(extra * sizeof(size_t)));
        std::memcpy(heap_, dims + kInlineCap, extra * sizeof(size_t));
    }
}

void Dims::freeHeap() noexcept
{
    if (heap_) {
        std::free(heap_);
        heap_ = nullptr;
    }
}

Dims::Dims(const Dims &o) : heap_(nullptr), nd_(o.nd_)
{
    std::memcpy(inline_, o.inline_, sizeof(inline_));
    if (o.heap_) {
        const int extra = nd_ - kInlineCap;
        heap_ = static_cast<size_t *>(std::malloc(extra * sizeof(size_t)));
        std::memcpy(heap_, o.heap_, extra * sizeof(size_t));
    }
}

Dims::Dims(Dims &&o) noexcept : heap_(o.heap_), nd_(o.nd_)
{
    std::memcpy(inline_, o.inline_, sizeof(inline_));
    o.heap_ = nullptr;
    // o keeps its inline_ + nd_; not strictly necessary to reset since
    // its dtor on a null heap_ is a no-op.
}

Dims &Dims::operator=(const Dims &o)
{
    if (this == &o) return *this;
    freeHeap();
    nd_ = o.nd_;
    std::memcpy(inline_, o.inline_, sizeof(inline_));
    if (o.heap_) {
        const int extra = nd_ - kInlineCap;
        heap_ = static_cast<size_t *>(std::malloc(extra * sizeof(size_t)));
        std::memcpy(heap_, o.heap_, extra * sizeof(size_t));
    }
    return *this;
}

Dims &Dims::operator=(Dims &&o) noexcept
{
    if (this == &o) return *this;
    freeHeap();
    nd_  = o.nd_;
    std::memcpy(inline_, o.inline_, sizeof(inline_));
    heap_     = o.heap_;
    o.heap_   = nullptr;
    return *this;
}

Dims::~Dims()
{
    freeHeap();
}

size_t Dims::sub2indChecked(size_t r, size_t c) const
{
    if (r >= inline_[0] || c >= inline_[1])
        throw std::runtime_error("Index (" + std::to_string(r + 1) + ","
                                 + std::to_string(c + 1)
                                 + ") exceeds dimensions");
    return c * inline_[0] + r;
}

size_t Dims::sub2indChecked(size_t r, size_t c, size_t p) const
{
    if (r >= inline_[0] || c >= inline_[1] || p >= inline_[2])
        throw std::runtime_error("Index exceeds dimensions");
    return p * inline_[0] * inline_[1] + c * inline_[0] + r;
}

bool Dims::operator==(const Dims &o) const noexcept
{
    // Semantic equality: compare with implicit trailing 1s. Inline slots
    // [0, kInlineCap) are always populated correctly (1 in unused slots),
    // so a direct compare suffices for the common case. For ndim > kInlineCap
    // either side, fall through to dim() which returns 1 past ndim.
    if (std::memcmp(inline_, o.inline_, sizeof(inline_)) != 0)
        return false;
    const int n = std::max(static_cast<int>(nd_), static_cast<int>(o.nd_));
    for (int i = kInlineCap; i < n; ++i)
        if (dim(i) != o.dim(i)) return false;
    return true;
}

} // namespace numkit::m
