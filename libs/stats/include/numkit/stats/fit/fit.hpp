// libs/stats/include/numkit/stats/fit/fit.hpp
//
// Distribution fitters (function-form). Each returns scalar parameter
// estimate(s) plus 1×2 confidence intervals at level (1 − alpha).
// Default alpha = 0.05 (95% CI).

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

#include <tuple>

namespace numkit::stats {

/// `[muhat, sigmahat, muci, sigmaci] = normfit(x[, alpha[, cens[, freq]]])`.
/// muhat = mean(x); sigmahat = sample std (N−1). CIs: t-based for mu,
/// chi² for sigma in the basic+freq case; Wald (analytic Fisher info)
/// + log-σ transform for the censored case.
std::tuple<Value, Value, Value, Value>
normfit(const Value &x, double alpha, std::pmr::memory_resource *mr = nullptr);
std::tuple<Value, Value, Value, Value>
normfit(const Value &x, double alpha, const Value *cens, const Value *freq, std::pmr::memory_resource *mr = nullptr);

/// `[lhat, lci] = poissfit(x[, alpha])` — lambda = mean(x); exact CI
/// from chi² inversion of cumulative Poisson tail.
std::tuple<Value, Value>
poissfit(const Value &x, double alpha, std::pmr::memory_resource *mr = nullptr);

/// `[muhat, muci] = expfit(x[, alpha[, censoring[, freq]]])` —
/// MLE μ = Σ(freq·x) / Σ(freq·(1-cens)); exact CI via χ²(2D) where
/// D = event count = Σ(freq·(1-cens)). cens / freq may be passed as
/// nullptr (default: no censoring, freq=1) or empty Value to mean
/// "use defaults". Lengths must match x.numel() when non-empty.
std::tuple<Value, Value>
expfit(const Value &x, double alpha, const Value *cens = nullptr, const Value *freq = nullptr, std::pmr::memory_resource *mr = nullptr);

/// `[ahat, bhat, aci, bci] = unifit(x[, alpha])` — uniform U(a,b)
/// MLE: a=min, b=max. CI based on (b-a) · (alpha^(-1/n) − 1).
std::tuple<Value, Value, Value, Value>
unifit(const Value &x, double alpha, std::pmr::memory_resource *mr = nullptr);

/// `[parm, pci] = lognfit(x[, alpha[, cens[, freq]]])` — lognormal MLE
/// of muhat / sigmahat of log(x). Closed-form weighted moments when
/// freq is supplied without censoring; EM-iterated MLE on log(x) with
/// numeric Hessian for CIs when right-censored. Returns parm = 1×2
/// row, pci = 2×2 (column 1 = mu CI, column 2 = sigma CI; row 1 =
/// lower, row 2 = upper).
std::tuple<Value, Value>
lognfit(const Value &x, double alpha, std::pmr::memory_resource *mr = nullptr);
std::tuple<Value, Value>
lognfit(const Value &x, double alpha, const Value *cens, const Value *freq, std::pmr::memory_resource *mr = nullptr);

/// `[phat, pci] = binofit(x, n[, alpha])` — Clopper–Pearson exact
/// binomial CI for `x` successes out of `n` trials. Vector inputs
/// (same length) produce a column vector phat and Nx2 pci.
std::tuple<Value, Value>
binofit(const Value &x, const Value &n, double alpha, std::pmr::memory_resource *mr = nullptr);

/// `[shat, sci] = raylfit(x[, alpha])` — Rayleigh MLE:
/// σ̂ = √(Σx² / (2N)); CI from chi² inversion of 2N·σ̂² ~ σ²·χ²(2N).
std::tuple<Value, Value>
raylfit(const Value &x, double alpha, std::pmr::memory_resource *mr = nullptr);

// ── Negative log-likelihoods ───────────────────────────────────────────
// Each *like(params, data) returns the scalar nLogL = −Σ log f(x_i; θ).
// `params` is a 1×k row vector; `data` is the sample vector. The second
// MATLAB output `avar` (inverse observed-Fisher information) is wired up
// for `normlike` only; the other *like fitters return only nLogL today.
//
// `normlike` additionally honours MATLAB's `(censoring, freq)` optional
// args:
//   * `censoring[i] != 0` → element i is right-censored; contribute
//     -log(1 - F(z_i)) = -log(0.5·erfc(z_i/sqrt(2))) instead of -log(f).
//   * `freq[i]` (default 1) → multiplies the element's contribution.
//     `freq[i] == 0` removes the element from the sum (matches MATLAB).
// Boundary handling: σ ≤ 0 → NaN; any NaN in x → NaN; empty x → 0.

double normlike (double mu, double sigma, const Value &x, const Value &cens, const Value &freq, std::pmr::memory_resource *mr = nullptr);
double explike  (double mu, const Value &x, std::pmr::memory_resource *mr = nullptr);
double lognlike (double mu, double sigma, const Value &x, std::pmr::memory_resource *mr = nullptr);
double gamlike  (double a, double b, const Value &x, std::pmr::memory_resource *mr = nullptr);
double betalike (double a, double b, const Value &x, std::pmr::memory_resource *mr = nullptr);
double wbllike  (double a, double b, const Value &x, std::pmr::memory_resource *mr = nullptr);
double evlike   (double mu, double sigma, const Value &x, std::pmr::memory_resource *mr = nullptr);

/// gevlike([k, sigma, mu], x) — Generalised Extreme Value nLogL.
double gevlike (double k, double sigma, double mu, const Value &x, std::pmr::memory_resource *mr = nullptr);

/// gplike([k, sigma], x) — Generalised Pareto nLogL (theta=0).
double gplike  (double k, double sigma, const Value &x, std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::stats
