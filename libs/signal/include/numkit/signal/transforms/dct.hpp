// libs/signal/include/numkit/signal/transforms/dct.hpp
#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

namespace numkit::signal {

/// dct(x) — Type-II discrete cosine transform (MATLAB default).
/// 1-D entry point. For matrices, use dct(mr, x, n, dim) below.
Value dct(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// idct(x) — inverse Type-II DCT.
Value idct(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// dct(x, n, dim) — DCT-II with length override and explicit dim.
///   `n  <= 0` means "use native extent along `dim`".
///   `dim == 0` means "first non-singleton dim" (MATLAB default).
/// Matrix input: per-column (dim=1) or per-row (dim=2) transform.
Value dct(const Value &x, int n, int dim, std::pmr::memory_resource *mr = nullptr);

/// idct(x, n, dim) — same conventions as dct above.
Value idct(const Value &x, int n, int dim, std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::signal
