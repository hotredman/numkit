/// @file multivariate.hpp
/// @ingroup group_stats
// toolboxes/stats/include/numkit/stats/distributions/multivariate.hpp
//
// Multivariate distribution primitives.

#pragma once

#include <cstddef>
#include <memory_resource>
#include <numkit/value/value.hpp>

namespace numkit {

/// @addtogroup group_stats
/// @{
 namespace ops { class RngContext; } }

namespace numkit::stats {

/// @brief Multivariate normal random samples
/// (`R = mvnrnd(mu, Sigma [, n])`).
///
/// Draws `n` independent samples from `N(mu, Sigma)` via Cholesky:
/// `R = mu + Z · L'` where `L = chol(Sigma, 'lower')` and `Z` is a
/// standard-normal `n × d` matrix.
///
/// Supports three calling conventions for `mu`:
///   - `1 × d` row vector — same mu for every sample
///   - `d × 1` column vector — same mu for every sample
///   - `n × d` matrix — per-sample location (`n` must match)
///
/// Sigma must be `d × d`, symmetric positive-definite. Non-PD inputs
/// throw `m:mvnrnd:notPD` at the Cholesky step. Sigma-as-diagonal-vector
/// shorthand (`mvnrnd(mu, sigmaVec, n)`) is a v1 KNOWN GAP.
///
/// Uses the shared MT19937 stream so `rng(seed)` makes draws reproducible.
///
/// @param mu     Location parameter (`1×d`, `d×1`, or `n×d`).
/// @param Sigma  Covariance matrix (`d×d`, symmetric PD).
/// @param n      Sample count. `0` → infer from `mu` (1 if `mu` is a vector,
///               `rows(mu)` if `mu` is a matrix).
/// @param mr     Memory resource (nullptr → process default).
/// @return       `n × d` matrix of samples.
/// @throws Error on shape mismatch or non-PD Sigma.
Value mvnrnd(::numkit::ops::RngContext &rng, const Value &mu, const Value &Sigma, std::size_t n = 0,
             std::pmr::memory_resource *mr = nullptr);

/// @brief Multivariate normal CDF (`p = mvncdf(X, mu, Sigma)`).
///
/// Returns the probability `P(Y ≤ X)` for `Y ~ N(mu, Sigma)`,
/// row-wise (each row of `X` is one evaluation point).
///
/// Algorithm by dimension:
///   - `d = 1` → direct `normcdf`.
///   - `d = 2` → Owen's tetrachoric / Drezner-Wesolowsky numerical
///               integration (16-point Gauss-Legendre).
///   - `d ≥ 3` → Monte Carlo with antithetic sampling (10000 draws by
///               default). KNOWN GAP: Genz separation-of-variables
///               quasi-MC is more accurate but not yet in v1.
///
/// Mu may be omitted (defaults to zero vector). Sigma defaults to
/// identity. The standard-normal one-arg form `mvncdf(X)` is the
/// most common usage in practice.
///
/// @param X      `n × d` evaluation points (each row one query).
/// @param mu     `1 × d` mean (may be empty → zero).
/// @param Sigma  `d × d` covariance (may be empty → identity).
/// @param mr     Memory resource (nullptr → process default).
/// @return       `n × 1` column of cumulative probabilities.
Value mvncdf(const Value &X, const Value &mu, const Value &Sigma,
              std::pmr::memory_resource *mr = nullptr);

/// @brief Multivariate Student's-t random samples
/// (`R = mvtrnd(C, df, n)`).
///
/// Draws n samples from `t_df(0, C)` via the canonical algorithm:
/// `T = X / sqrt(χ² / df)` where `X ~ N(0, C)` and `χ² ~ χ²(df)`.
///
/// `C` is a `d × d` **correlation** matrix (diagonal = 1). df is the
/// degrees of freedom (positive scalar).
///
/// KNOWN GAP: location parameter not supported (MATLAB also defaults
/// to zero); pass `mu + mvtrnd(...)` at the call site if needed.
///
/// @param C    `d × d` correlation matrix.
/// @param df   Degrees of freedom (`df > 0`).
/// @param n    Sample count.
/// @param mr   Memory resource (nullptr → process default).
/// @return     `n × d` matrix of samples.
Value mvtrnd(::numkit::ops::RngContext &rng, const Value &C, double df, std::size_t n,
              std::pmr::memory_resource *mr = nullptr);

/// @brief Multinomial random samples (`R = mnrnd(N, P, m)`).
///
/// Draws m samples from `Multinomial(N, P)` (k categories with
/// probabilities P, total trials N per sample). Returns an `m × k`
/// matrix where each row sums to N.
///
/// `P` must be a length-`k` probability vector (non-negative, sums
/// to 1; renormalised if not). Implementation: per-trial categorical
/// sampling via CDF lookup — O(m · N · k).
///
/// KNOWN GAP: `P` matrix form (per-sample probability rows) not yet
/// supported in v1.
///
/// @param N   Trials per sample (positive integer).
/// @param P   Length-k probability vector.
/// @param m   Number of samples (default 1).
/// @param mr  Memory resource (nullptr → process default).
/// @return    `m × k` count matrix.
Value mnrnd(::numkit::ops::RngContext &rng, std::size_t N, const Value &P, std::size_t m = 1,
              std::pmr::memory_resource *mr = nullptr);

/// @brief Wishart random matrix (`W = wishrnd(Sigma, df)`).
///
/// Draws one `p × p` positive-definite matrix from the Wishart
/// distribution `W_p(Sigma, df)` via Bartlett decomposition:
/// - Factor `Sigma = L · L'` (lower Cholesky).
/// - Sample `B` lower triangular with
///   `B(i,i) = sqrt(χ²(df - i))` and `B(i,j) ~ N(0,1)` for `i > j`.
/// - Then `M = L · B` and `W = M · M'`.
///
/// `df` must satisfy `df > p - 1`.
///
/// KNOWN GAPs: third-argument `D` (pre-computed Cholesky) and the
/// two-output `[W, D] = wishrnd(...)` form are deferred.
///
/// @param Sigma  `p × p` symmetric positive-definite scale matrix.
/// @param df     Degrees of freedom (> p - 1).
/// @param mr     Memory resource (nullptr → process default).
/// @return       Single `p × p` Wishart draw.
/// @see iwishrnd, mvnrnd
Value wishrnd(::numkit::ops::RngContext &rng, const Value &Sigma, double df,
              std::pmr::memory_resource *mr = nullptr);

/// @brief Inverse-Wishart random matrix (`W = iwishrnd(Tau, df)`).
///
/// Draws one `p × p` positive-definite matrix from the inverse Wishart
/// `W ~ IW_p(Tau, df)`. Implementation: sample
/// `Y ~ W_p(inv(Tau), df)` via Bartlett, then return `inv(Y)`.
///
/// `df` must satisfy `df > p - 1`.
///
/// KNOWN GAPs: third-argument `DI` (pre-computed Cholesky of `inv(Tau)`)
/// and the two-output `[W, DI] = iwishrnd(...)` form are deferred.
///
/// @param Tau  `p × p` symmetric positive-definite scale matrix.
/// @param df   Degrees of freedom (> p - 1).
/// @param mr   Memory resource (nullptr → process default).
/// @return     Single `p × p` inverse Wishart draw.
/// @see wishrnd
Value iwishrnd(::numkit::ops::RngContext &rng, const Value &Tau, double df,
               std::pmr::memory_resource *mr = nullptr);

/// @brief Wishart sample with pre-computed Cholesky factor
/// (`W = wishrnd(Sigma, df, D)`).
///
/// If `D` is empty, internally compute `D = chol(Sigma, 'upper')`;
/// otherwise reuse the supplied factor (which must satisfy
/// `D' * D == Sigma`). The 2-output form returns the Cholesky factor.
///
/// @param Sigma  `p × p` covariance / scale matrix.
/// @param df     Degrees of freedom (`df > p - 1`).
/// @param D      Pre-computed `chol(Sigma, 'upper')`, or `Value::Empty`.
/// @param mr     Memory resource.
/// @return       `{W, D}` — Wishart draw and the upper-Cholesky factor.
std::tuple<Value, Value>
wishrnd_factor(::numkit::ops::RngContext &rng, const Value &Sigma, double df,
               const Value &D, std::pmr::memory_resource *mr = nullptr);

/// @brief Inverse-Wishart sample with pre-computed Cholesky factor
/// (`W = iwishrnd(Tau, df, DI)`).
///
/// `DI = chol(inv(Tau), 'lower')`: a lower-triangular factor with
/// `DI · DI' = inv(Tau)`. When provided, skips the inverse-Cholesky
/// computation. (Note: MATLAB's `iwishrnd` returns a different but
/// numerically equivalent factor `inv(chol(Tau, 'upper'))'`, which
/// satisfies `DI' · DI = inv(Tau)` instead. Both produce statistically
/// identical Inv-Wishart draws; the numerical orientation differs.)
///
/// @return  `{W, DI}` — Inv-Wishart draw and the lower-Cholesky of inv(Tau).
std::tuple<Value, Value>
iwishrnd_factor(::numkit::ops::RngContext &rng, const Value &Tau, double df,
                const Value &DI, std::pmr::memory_resource *mr = nullptr);

/// @brief Multivariate t cdf upper-tail (`p = mvtcdf(X, C, df)`).
///
/// Returns the probability `P(Y ≤ X)` for `Y ~ MVT(0, C, df)`,
/// row-wise (each row of `X` is one evaluation point).
///
/// Algorithm by dimension:
///   - `d = 1` → direct `tcdf` (bit-exact).
///   - `d ≥ 2` → deterministic Monte Carlo on the
///               `Y = Z / sqrt(W/df)` representation with a
///               fixed seed (`12345`), `N = ceil(1/tol²) ≥ 10000`
///               draws, antithetic.
///
/// @param X    Evaluation points (`n × d` or length-`d` row).
/// @param C    `d × d` correlation/scale matrix (symmetric PD).
/// @param df   Degrees of freedom (`df > 0`).
/// @param tol  Target MC absolute error (default 0.01).
/// @param mr   Memory resource (nullptr → process default).
/// @return     `n × 1` column of cdf values.
/// @see mvtpdf, mvtrnd
Value mvtcdf(const Value &X, const Value &C, double df,
             double tol = 0.01,
             std::pmr::memory_resource *mr = nullptr);

/// @brief Multivariate t box cdf (`p = mvtcdf(L, U, C, df)`).
///
/// Returns `P(L ≤ Y ≤ U)` for `Y ~ MVT(0, C, df)`, row-wise.
/// Each row of `L` (and `U`) is a length-`d` lower (upper) bound;
/// entries may be `-Inf` / `+Inf`. Implemented via the same
/// deterministic Monte Carlo path as the upper-tail form.
///
/// @param L    Lower bounds (`n × d` or length-`d` row); `-Inf` allowed.
/// @param U    Upper bounds (same shape as `L`); `+Inf` allowed.
/// @param C    Correlation/scale matrix.
/// @param df   Degrees of freedom.
/// @param tol  Target MC absolute error (default 0.01).
/// @param mr   Memory resource.
/// @return     `n × 1` column of box probabilities.
Value mvtcdf_box(const Value &L, const Value &U, const Value &C, double df,
                 double tol = 0.01,
                 std::pmr::memory_resource *mr = nullptr);


/// @}
} // namespace numkit::stats
