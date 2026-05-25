// libs/linalg/include/numkit/linalg/page_ops.hpp
//
// Page-wise linalg ops on 3-D arrays.
//
// Currently: pageinv only. The general page operators (pagetranspose,
// pagectranspose, pagemtimes) stay in libs/builtin — they're basic
// tensor ops, not strictly linalg.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

namespace numkit::linalg {

/// @brief Page-wise inverse of a 3-D array (`B = pageinv(A)`).
///
/// Each `m × n` page is independently inverted via LU.
Value pageinv(const Value &A, std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::linalg
