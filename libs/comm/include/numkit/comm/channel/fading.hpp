// libs/comm/include/numkit/comm/channel/fading.hpp
//
// Frequency-flat fading channels — per-sample iid Rayleigh and
// Rician multiplicative gains.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

namespace numkit::comm {

/// `y = rayleighchan(x)` — apply iid frequency-flat Rayleigh fading.
/// Each sample of x is multiplied by an independent complex Gaussian
/// h ~ CN(0, 1), giving E[|h|²] = 1 (unit-power channel). The output
/// is always complex even when x is real.
Value rayleighchan(std::pmr::memory_resource *mr, const Value &x);

/// `y = ricianchan(x, K)` — apply iid frequency-flat Rician fading.
/// `K` is the linear K-factor (LOS / NLOS power ratio). Each sample
/// is multiplied by h = √(K / (K+1)) + √(1 / (K+1)) · g where
/// g ~ CN(0, 1). E[|h|²] = 1 regardless of K.
Value ricianchan(std::pmr::memory_resource *mr, const Value &x, double K);

} // namespace numkit::comm
