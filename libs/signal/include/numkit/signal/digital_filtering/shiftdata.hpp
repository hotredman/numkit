// libs/signal/include/numkit/signal/digital_filtering/shiftdata.hpp
//
// MATLAB Signal Toolbox shiftdata + unshiftdata (Phase 4.4):
// utilities for writing dim-aware functions. shiftdata moves the
// chosen dim to dim 1 (leading); unshiftdata reverses.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>
#include <tuple>

namespace numkit::signal {

// shiftdata(x, dim) — move dim to leading; returns (shifted, perm, nshifts).
// dim = 0 (or empty) means "first non-singleton dim"; in that case
// the auto path is used: shifted = shiftdim(x), perm = [] (1×0 row),
// nshifts = number of leading singletons dropped.
// Otherwise: perm = [dim, 1..dim-1, dim+1..ndims], x = permute(x, perm),
// nshifts = empty.
//
// Returns (shifted, perm, nshifts). perm is empty when dim was unspecified
// (or 0); nshifts is empty when dim was specified.
std::tuple<Value, Value, Value>
shiftdata(std::pmr::memory_resource *mr, const Value &x, int dim = 0);

// unshiftdata(x, perm, nshifts) — inverse of shiftdata.
// If perm is empty: y = shiftdim(x, -nshifts).
// Otherwise:        y = ipermute(x, perm).
Value unshiftdata(std::pmr::memory_resource *mr,
                   const Value &x,
                   const Value &perm,
                   const Value &nshifts);

} // namespace numkit::signal
