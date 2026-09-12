// ops/include/nk/ops/blas1.hpp
#pragma once

#include <cstddef>

namespace nk::ops {

/// @brief Vector add scaled y = alpha * x + y (real double).
void axpy(std::size_t n, double alpha, const double *x, double *y);

} // namespace nk::ops
