// libs/stats/include/numkit/stats/mvdist/mvdist.hpp
//
// Multivariate distribution PDFs / PMFs (closed-form members of the
// `stats.mvdist.*` family).

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

namespace numkit::stats {

/// `mvnpdf(X[, mu, Sigma])` — multivariate normal pdf.
///   X      — N×d (each row a point) or 1×d single point.
///   mu     — 1×d mean (default: zeros).
///   Sigma  — d×d covariance (default: I).
/// Returns N×1 column of densities.
/// `mu` may be empty (Value()) → defaults to zeros.
/// `Sigma` may be empty → defaults to identity.
Value mvnpdf(std::pmr::memory_resource *mr, const Value &X,
             const Value &mu, const Value &Sigma);

/// `mvtpdf(X, C, df)` — multivariate t PDF. `C` is treated as a
/// correlation matrix; if the diagonal is not all 1, the input is
/// normalised to a correlation matrix (matches MATLAB R2025b).
/// Returns N×1 column.
Value mvtpdf(std::pmr::memory_resource *mr, const Value &X,
             const Value &C, double df);

/// `mnpdf(X, P)` — multinomial PMF.
///   X — 1×k counts (or N×k batch).
///   P — 1×k probabilities (sum to 1).
/// Returns N×1 column of probabilities.
Value mnpdf(std::pmr::memory_resource *mr, const Value &X, const Value &P);

} // namespace numkit::stats
