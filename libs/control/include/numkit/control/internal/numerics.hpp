// libs/control/include/numkit/control/internal/numerics.hpp
//
// Shared numerical kernels for libs/control. All matrices are stored
// column-major in flat std::vector<double>.
//
// This header is INTERNAL — only libs/control TUs include it. The
// kernels were originally inline-duplicated in response/discretize/
// freq/lyapunov/state, leading to ~4× the LU code, 2× the expm code
// and 2× the Faddeev char-poly code. Cycle 44 of the autonomous
// /loop consolidates them here. No behavioural change.

#pragma once

#include <cstddef>
#include <vector>

namespace numkit::control::internal {

using Mat = std::vector<double>;
using Vec = std::vector<double>;

/// In-place partial-pivot LU decomposition + back-substitution on a
/// column-major n×n matrix `A`, with `nrhs` right-hand-side columns
/// held column-major in `B`. Returns `false` if the matrix is
/// numerically singular (any pivot < 1e-14); the contents of `A`
/// and `B` are unspecified in that case.
bool solveInPlace(Mat &A, Mat &B, std::size_t n, std::size_t nrhs);

/// Matrix exponential via the canonical [6/6] Padé approximant with
/// scaling and squaring. Coefficient set:
///   c_k = (12 − k)! · 6! / (12! · k! · (6 − k)!)
/// Falls back to a 30-term truncated power series if the Padé
/// denominator is numerically singular. O(n³) per matrix multiply.
Mat expm(const Mat &A, std::size_t n);

/// Faddeev–LeVerrier characteristic polynomial of an n×n column-
/// major A. Returns coefficients [1, c1, c2, …, cn] in the MATLAB
/// convention (descending powers, leading 1):
///   det(sI − A) = sⁿ + c1·sⁿ⁻¹ + … + cn.
/// O(n⁴) work — fine for the n ≲ 32 typical of textbook problems.
Vec charPoly(const Mat &A, std::size_t n);

/// Square matrix multiply C = A·B (all n×n column-major).
Mat matmulSq(const Mat &A, const Mat &B, std::size_t n);

} // namespace numkit::control::internal
