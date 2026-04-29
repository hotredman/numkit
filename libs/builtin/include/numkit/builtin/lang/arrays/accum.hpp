// libs/builtin/include/numkit/builtin/lang/arrays/accum.hpp
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

#include <numkit/core/allocator.hpp>
#include <numkit/core/value.hpp>

#include <cstddef>

namespace numkit::builtin {

// Built-in reducers we recognise from the function-handle name (@sum,
// @max, ...). Custom function handles are rejected at the adapter
// layer with a clear error — this surface is the 99% MATLAB use case.
enum class AccumReducer { Sum, Max, Min, Prod, Mean, Any, All };

// nOutShape == 0 → derive shape from max(subs) per column.
// `vals` may be a scalar (broadcast to every subscript row) or a
// length-N vector (one value per subscript row).
Value accumarray(Allocator &alloc,
                  const Value &subs,
                  const Value &vals,
                  const size_t *outShape, std::size_t nOutShape,
                  AccumReducer op,
                  double fillVal);

} // namespace numkit::builtin
