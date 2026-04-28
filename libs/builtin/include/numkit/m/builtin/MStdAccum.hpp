// libs/builtin/include/numkit/m/builtin/MStdAccum.hpp
//
// accumarray — group-by reduction. MATLAB:
//   A = accumarray(subs, vals)
//   A = accumarray(subs, vals, sz)
//   A = accumarray(subs, vals, sz, fn)
//   A = accumarray(subs, vals, sz, fn, fillval)
//
// Each row of `subs` is a 1-based index into the output `A`. `vals(i)`
// is contributed to `A(subs(i, :))` via the reduction `fn` (default
// @sum). Output shape is `sz`, or auto-derived from max(subs) if `sz`
// is omitted/empty. Cells with no contributions get `fillval`
// (default 0).

#pragma once

#include <numkit/m/core/MAllocator.hpp>
#include <numkit/m/core/MValue.hpp>

#include <cstddef>
#include <vector>

namespace numkit::m::builtin {

// Built-in reducers we recognise from the function-handle name (@sum,
// @max, ...). Custom function handles are rejected at the adapter
// layer with a clear error — this surface is the 99% MATLAB use case.
enum class AccumReducer { Sum, Max, Min, Prod, Mean, Any, All };

// `outShape` is empty (size 0) → derive from max(subs) per column.
// `vals` may be a scalar (broadcast to every subscript row) or a
// length-N vector (one value per subscript row).
MValue accumarray(Allocator &alloc,
                  const MValue &subs,
                  const MValue &vals,
                  const std::vector<size_t> &outShape,
                  AccumReducer op,
                  double fillVal);

} // namespace numkit::m::builtin
