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

/// @brief In-place partial-pivot LU decomposition + back-substitution.
///
/// Operates on a column-major n×n matrix `A`, with `nrhs`
/// right-hand-side columns held column-major in `B`.
///
/// @param A     Column-major n×n matrix; overwritten with LU.
/// @param B     Column-major n×nrhs RHS; overwritten with solution.
/// @param n     Matrix dimension.
/// @param nrhs  Number of RHS columns.
/// @return      `false` if numerically singular (any pivot < 1e-14);
///              the contents of `A` and `B` are unspecified in that
///              case. `true` on success.
/// @see expm, charPoly
bool solveInPlace(Mat &A, Mat &B, std::size_t n, std::size_t nrhs);

/// @brief Matrix exponential via the canonical [6/6] Padé approximant
/// with scaling and squaring.
///
/// Coefficient set: `c_k = (12 − k)! · 6! / (12! · k! · (6 − k)!)`.
/// Falls back to a 30-term truncated power series if the Padé
/// denominator is numerically singular. O(n³) per matrix multiply.
///
/// @param A  Column-major n×n input.
/// @param n  Matrix dimension.
/// @return   Column-major n×n matrix exponential.
/// @see solveInPlace, matmulSq
Mat expm(const Mat &A, std::size_t n);

/// @brief Faddeev–LeVerrier characteristic polynomial of an n×n
/// column-major matrix.
///
/// Returns coefficients `[1, c1, c2, …, cn]` in MATLAB convention
/// (descending powers, leading 1):
/// `det(sI − A) = sⁿ + c1·sⁿ⁻¹ + … + cn`.
/// O(n⁴) work — fine for the n ≲ 32 typical of textbook problems.
///
/// @param A  Column-major n×n input.
/// @param n  Matrix dimension.
/// @return   Coefficient vector of length n+1.
/// @see expm
Vec charPoly(const Mat &A, std::size_t n);

/// @brief Square matrix multiply `C = A·B` (all n×n column-major).
///
/// @param A  Column-major n×n left operand.
/// @param B  Column-major n×n right operand.
/// @param n  Matrix dimension.
/// @return   Column-major n×n product.
Mat matmulSq(const Mat &A, const Mat &B, std::size_t n);

} // namespace numkit::control::internal
