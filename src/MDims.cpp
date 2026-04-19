// src/MLabDims.cpp
#include "MDims.hpp"

#include <stdexcept>
#include <string>

namespace numkit {

Dims::Dims()
{
    d[0] = 0;
    d[1] = 0;
    d[2] = 1;
    nd = 2;
}
Dims::Dims(size_t r, size_t c)
{
    d[0] = r;
    d[1] = c;
    d[2] = 1;
    nd = 2;
}
Dims::Dims(size_t r, size_t c, size_t p)
{
    d[0] = r;
    d[1] = c;
    d[2] = p;
    nd = 3;
}
int Dims::ndims() const { return nd; }
size_t Dims::rows() const { return d[0]; }
size_t Dims::cols() const { return d[1]; }
size_t Dims::pages() const { return d[2]; }
size_t Dims::numel() const { return d[0] * d[1] * d[2]; }
bool Dims::isScalar() const { return numel() == 1; }
bool Dims::isEmpty() const { return numel() == 0; }
bool Dims::isVector() const { return nd == 2 && (d[0] == 1 || d[1] == 1); }
bool Dims::is3D() const { return nd == 3 && d[2] > 1; }
size_t Dims::dimSize(int dim) const { return (dim >= 0 && dim < 3) ? d[dim] : 1; }

size_t Dims::sub2ind(size_t r, size_t c) const
{
    return c * d[0] + r;
}
size_t Dims::sub2ind(size_t r, size_t c, size_t p) const
{
    return p * d[0] * d[1] + c * d[0] + r;
}
size_t Dims::sub2indChecked(size_t r, size_t c) const
{
    if (r >= d[0] || c >= d[1])
        throw std::runtime_error("Index (" + std::to_string(r + 1) + "," + std::to_string(c + 1)
                                 + ") exceeds dimensions");
    return c * d[0] + r;
}
size_t Dims::sub2indChecked(size_t r, size_t c, size_t p) const
{
    if (r >= d[0] || c >= d[1] || p >= d[2])
        throw std::runtime_error("Index exceeds dimensions");
    return p * d[0] * d[1] + c * d[0] + r;
}
bool Dims::operator==(const Dims &o) const
{
    return d[0] == o.d[0] && d[1] == o.d[1] && d[2] == o.d[2];
}
bool Dims::operator!=(const Dims &o) const
{
    return !(*this == o);
}

} // namespace numkit
