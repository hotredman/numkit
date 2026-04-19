// include/MLabDims.hpp
#pragma once

#include <cstddef>
#include <cstdint>

namespace numkit {

// ============================================================
// Dims — array dimensions
// ============================================================
struct Dims
{
    size_t d[3] = {0, 0, 1};
    int nd = 2;

    Dims();
    Dims(size_t r, size_t c);
    Dims(size_t r, size_t c, size_t p);

    int ndims() const;
    size_t rows() const;
    size_t cols() const;
    size_t pages() const;
    size_t numel() const;
    bool isScalar() const;
    bool isEmpty() const;
    bool isVector() const;
    bool is3D() const;

    size_t dimSize(int dim) const;
    size_t sub2ind(size_t r, size_t c) const;
    size_t sub2ind(size_t r, size_t c, size_t p) const;
    size_t sub2indChecked(size_t r, size_t c) const;
    size_t sub2indChecked(size_t r, size_t c, size_t p) const;

    bool operator==(const Dims &o) const;
    bool operator!=(const Dims &o) const;
};

} // namespace numkit
