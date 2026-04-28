// libs/stats/include/numkit/m/stats/moments/moments.hpp
//
// Higher moments: skewness, kurtosis. Both follow MATLAB Statistics
// Toolbox semantics — default normFlag = 1 (uncorrected); normFlag = 0
// applies the bias correction (requires n >= 3 for skewness, n >= 4
// for kurtosis).

#pragma once

#include <numkit/m/core/MAllocator.hpp>
#include <numkit/m/core/MValue.hpp>

namespace numkit::m::stats {

/// skewness(X[, normFlag[, dim]]) — sample skewness E[((X-μ)/σ)^3].
///   normFlag = 1 (default): uncorrected y = m3 / m2^1.5
///   normFlag = 0: bias-corrected y *= sqrt(n*(n-1))/(n-2). Requires n ≥ 3.
MValue skewness(Allocator &alloc, const MValue &x, int normFlag = 1, int dim = 0);

/// kurtosis(X[, normFlag[, dim]]) — sample kurtosis (NON-excess; equals
/// 3 for a normal distribution per MATLAB convention).
///   normFlag = 1 (default): uncorrected y = m4 / m2^2
///   normFlag = 0: bias-corrected
///     y = ((n-1)/((n-2)(n-3))) * ((n+1)*g2 - 3*(n-1)) + 3,
///     where g2 = m4/m2^2. Requires n ≥ 4.
MValue kurtosis(Allocator &alloc, const MValue &x, int normFlag = 1, int dim = 0);

} // namespace numkit::m::stats
