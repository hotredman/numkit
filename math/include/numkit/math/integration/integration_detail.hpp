// math/include/numkit/math/integration/integration_detail.hpp
//
// Internal (numkit::math::detail) entry points of the integration compute,
// exposed only so the Engine-coupled *_reg adapters in bundle can reuse the
// exact same machinery as the public gradient/del2/cumtrapz/trapz/integral.
// Not a stable API — these are the lower-level dispatch helpers + the
// Gauss-Kronrod node tables; lifted out of integration.cpp's anonymous
// namespace in Phase E so the adapters could move to the bundle layer.

#pragma once

#include <numkit/value/span.hpp>
#include <numkit/value/value.hpp>

#include <cstddef>
#include <memory_resource>
#include <vector>

namespace numkit::math::detail {

// 15-point Gauss-Kronrod nodes (symmetric about 0) and weights, plus the
// embedded 7-point Gauss weights. Source: Davis & Rabinowitz, "Methods of
// Numerical Integration". Shared by the kernel (gaussKronrod15) and the
// __gk15_nodes primitive that feeds the pausable .m integral wrapper.
inline constexpr double kKronrodX[15] = {
    -0.991455371120813, -0.949107912342759, -0.864864423359769,
    -0.741531185599394, -0.586087235467691, -0.405845151377397,
    -0.207784955007898,  0.0,                0.207784955007898,
     0.405845151377397,  0.586087235467691,  0.741531185599394,
     0.864864423359769,  0.949107912342759,  0.991455371120813,
};
inline constexpr double kKronrodW[15] = {
    0.022935322010529, 0.063092092629979, 0.104790010322250,
    0.140653259715525, 0.169004726639267, 0.190350578064785,
    0.204432940075298, 0.209482141084728, 0.204432940075298,
    0.190350578064785, 0.169004726639267, 0.140653259715525,
    0.104790010322250, 0.063092092629979, 0.022935322010529,
};
inline constexpr double kGaussW[7] = {
    0.129484966168870, 0.279705391489277, 0.381830050505119,
    0.417959183673469, 0.381830050505119, 0.279705391489277,
    0.129484966168870,
};

// Copy any numeric/logical Value to a fresh DOUBLE Value (used by every
// cumtrapz/trapz path that needs a real double buffer).
Value toDoubleCopy(const Value &x, std::pmr::memory_resource *mr);

// N-D gradient: one gradient per dimension, up to `nout`, spacings in `hs`.
std::pmr::vector<Value> gradientND(const Value &f, const double *hs, size_t nh,
                                   size_t nout, std::pmr::memory_resource *mr);

// cumtrapz down columns / along rows, real and complex variants.
Value cumtrapzMatrixCols(const double *src, const double *xData, size_t rows,
                         size_t cols, std::pmr::memory_resource *mr);
Value cumtrapzMatrixRows(const double *src, const double *xData, size_t rows,
                         size_t cols, std::pmr::memory_resource *mr);
Value cumtrapzMatrixColsC(const Complex *src, const double *xData, size_t rows,
                          size_t cols, std::pmr::memory_resource *mr);
Value cumtrapzMatrixRowsC(const Complex *src, const double *xData, size_t rows,
                          size_t cols, std::pmr::memory_resource *mr);

// trapz along `dim`; `xData` null → unit spacing.
Value trapzImpl(const Value &y, int dim, const double *xData,
                std::pmr::memory_resource *mr);

} // namespace numkit::math::detail
