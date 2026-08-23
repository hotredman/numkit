/// @file random_source.hpp
/// @ingroup group_comm
// toolboxes/comm/include/numkit/comm/source/random_source.hpp
//
// Random data sources (randsrc / randerr).

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory_resource>
#include <numkit/value/value.hpp>

namespace numkit {

/// @addtogroup group_comm
/// @{
 namespace ops { class RngContext; } }

namespace numkit::comm {

/// @brief Generate an `m`×`n` random matrix from a finite alphabet
/// (`out = randsrc(m, n, alphabet, state)`).
///
/// `alphabet` is either:
/// - a row vector (uniform probability over its entries), or
/// - a 2-row matrix `[symbols; probabilities]` where probabilities
///   lie in [0, 1] and sum to 1 (within `sqrt(eps)`).
///
/// When `have_state` is true, draws come from an isolated MT19937
/// seeded with `state` (equivalent to
/// `RandStream('mt19937ar','Seed', state)`); otherwise the shared
/// engine is used.
///
/// @param m           Row count.
/// @param n           Column count.
/// @param alphabet    Alphabet specification (see above). Default
///                    `Value::Empty` → `[-1, 1]` uniform.
/// @param have_state  If true, use the supplied seed.
/// @param state       Seed for the isolated MT19937.
/// @param mr          Memory resource (nullptr → process default).
/// @return            `m`×`n` matrix of drawn symbols.
/// @see randerr
Value randsrc(::numkit::ops::RngContext &rng, size_t m, size_t n, const Value &alphabet,
              bool have_state, uint32_t state,
              std::pmr::memory_resource *mr = nullptr);

/// @brief Generate an `m`×`n` binary error pattern
/// (`out = randerr(m, n, errors, state)`).
///
/// Each row contains a controlled number of 1s ("errors") at random
/// column positions. `errspec` is either:
/// - a scalar (exact count per row),
/// - a row vector of possible counts (uniform), or
/// - a 2-row matrix `[counts; probabilities]`.
///
/// @param m           Row count.
/// @param n           Column count.
/// @param errspec     Error count specification (see above).
/// @param have_state  If true, use the supplied seed.
/// @param state       Seed for the isolated MatlabMT19937 instance.
/// @param mr          Memory resource (nullptr → process default).
/// @return            `m`×`n` binary error matrix.
/// @see randsrc
Value randerr(::numkit::ops::RngContext &rng, size_t m, size_t n, const Value &errspec,
              bool have_state, uint32_t state,
              std::pmr::memory_resource *mr = nullptr);


/// @}
} // namespace numkit::comm
