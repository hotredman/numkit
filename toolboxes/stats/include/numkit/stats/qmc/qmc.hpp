// toolboxes/stats/include/numkit/stats/qmc/qmc.hpp
//
// Quasi-random / low-discrepancy sequences.

#pragma once

#include <memory_resource>
#include <numkit/value/value.hpp>

namespace numkit::stats {

/// @brief Build a Halton stream descriptor (`p = haltonset(d, 'Skip', s, 'Leap', l)`).
///
/// Returns an opaque stream value (`struct {kind='halton', dim, skip, leap}`)
/// suitable for use with @ref net. Halton uses prime-base radical-inverse
/// per dimension.
///
/// @param d     Dimensionality (`d >= 1`; primes 2, 3, 5, … used in order).
/// @param skip  Number of initial points to discard.
/// @param leap  Stride between successive points (0 → no leap).
/// @param mr    Memory resource (nullptr → process default).
/// @return      Stream descriptor.
/// @see net
Value haltonset(int d, long long skip, long long leap,
                std::pmr::memory_resource *mr = nullptr);

/// @brief Extract points from a quasi-random stream (`X = net(p, n)`).
///
/// Returns the next `n` points of the stream `p`. Indices walked:
/// `skip+1, skip+1+(leap+1), …`. Output is `n × dim`.
/// Only Halton streams supported in this revision.
///
/// @param stream  Stream descriptor produced by @ref haltonset.
/// @param n       Number of points to extract (`n >= 0`).
/// @param mr      Memory resource (nullptr → process default).
/// @return        `n × dim` matrix of quasi-random points in `[0, 1)^d`.
/// @throws Error  Unknown stream kind (`m:net:badStream`).
/// @see haltonset
Value net(const Value &stream, long long n,
          std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::stats
