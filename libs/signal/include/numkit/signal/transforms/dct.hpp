// libs/signal/include/numkit/signal/transforms/dct.hpp
#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

namespace numkit::signal {

/// dct(x) — Type-II discrete cosine transform (MATLAB default).
/// 1-D entry point. For matrices, use dct(mr, x, n, dim) below.
Value dct(std::pmr::memory_resource *mr, const Value &x);

/// idct(x) — inverse Type-II DCT.
Value idct(std::pmr::memory_resource *mr, const Value &x);

/// dct(x, n, dim) — DCT-II with length override and explicit dim.
///   `n  <= 0` means "use native extent along `dim`".
///   `dim == 0` means "first non-singleton dim" (MATLAB default).
/// Matrix input: per-column (dim=1) or per-row (dim=2) transform.
Value dct(std::pmr::memory_resource *mr, const Value &x, int n, int dim);

/// idct(x, n, dim) — same conventions as dct above.
Value idct(std::pmr::memory_resource *mr, const Value &x, int n, int dim);

} // namespace numkit::signal
