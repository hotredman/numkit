// libs/stats/include/numkit/stats/qmc/qmc.hpp
//
// Quasi-random / low-discrepancy sequences.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

namespace numkit::stats {

/// `p = haltonset(d[, 'Skip', s, 'Leap', l])` — Halton quasi-random
/// stream descriptor. Returns a struct {kind='halton', dim, skip, leap}.
Value haltonset(std::pmr::memory_resource *mr, int d, long long skip, long long leap);

/// `X = net(p, n)` — extract the next n points from a stream `p`.
/// Indices used: skip+1, skip+1+(leap+1), …; output is n × dim.
/// Currently only Halton streams are supported.
Value net(std::pmr::memory_resource *mr, const Value &stream, long long n);

} // namespace numkit::stats
