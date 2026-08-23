/// @file fading.hpp
/// @ingroup group_comm
// toolboxes/comm/include/numkit/comm/channel/fading.hpp
//
// Frequency-flat fading channels — per-sample iid Rayleigh and
// Rician multiplicative gains.

#pragma once

#include <memory_resource>
#include <numkit/value/value.hpp>

namespace numkit { namespace ops { class RngContext; } }

namespace numkit::comm {

/// @brief Apply iid frequency-flat Rayleigh fading
/// (`y = rayleighchan(x)`).
///
/// Each sample of `x` is multiplied by an independent complex Gaussian
/// `h ~ CN(0, 1)`, giving `E[|h|²] = 1` (unit-power channel). Output
/// is always complex even when `x` is real.
///
/// @param x   Input signal (any shape).
/// @param mr  Memory resource (nullptr → process default).
/// @return    Faded signal of the same shape as `x`, COMPLEX.
/// @see ricianchan
Value rayleighchan(::numkit::ops::RngContext &rng, const Value &x,
                   std::pmr::memory_resource *mr = nullptr);

/// @brief Apply iid frequency-flat Rician fading
/// (`y = ricianchan(x, K)`).
///
/// Each sample is multiplied by
/// `h = √(K/(K+1)) + √(1/(K+1)) · g` with `g ~ CN(0, 1)`.
/// `E[|h|²] = 1` regardless of K.
///
/// @param x   Input signal (any shape).
/// @param K   Linear K-factor (LOS / NLOS power ratio, ≥ 0).
/// @param mr  Memory resource (nullptr → process default).
/// @return    Faded signal of the same shape as `x`, COMPLEX.
/// @see rayleighchan
Value ricianchan(::numkit::ops::RngContext &rng, const Value &x, double K,
                 std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::comm
