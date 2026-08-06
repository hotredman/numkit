// ops/include/numkit/ops/blas1.hpp
#pragma once

#include <cstddef>

namespace numkit::ops {

/// @brief Vector add scaled y = alpha * x + y (real double).
void axpy(std::size_t n, double alpha, const double *x, double *y);

} // namespace numkit::ops
