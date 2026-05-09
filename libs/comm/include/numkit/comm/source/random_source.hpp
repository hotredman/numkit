// libs/comm/include/numkit/comm/source/random_source.hpp
//
// Random data sources (randsrc; randerr planned).

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory_resource>
#include <numkit/core/value.hpp>

namespace numkit::comm {

/// `out = randsrc(m, n [, alphabet [, state]])` — generate an
/// `m`-by-`n` random matrix whose entries are drawn from a finite
/// alphabet.
///
///   alphabet : either a row vector (uniform probability) or a
///              2-row matrix `[symbols; probabilities]`.
///              Probabilities must lie in [0, 1] and sum to 1
///              (within sqrt(eps)). Default: [-1, 1] uniform.
///   state    : if provided, seeds an isolated MT19937 (matching
///              MATLAB's `RandStream('mt19937ar', 'Seed', state)`).
///              Otherwise the shared engine is used.
///
/// Output preserves m × n shape.
Value randsrc(std::pmr::memory_resource *mr, size_t m, size_t n,
              const Value &alphabet, bool have_state, uint32_t state);

} // namespace numkit::comm
