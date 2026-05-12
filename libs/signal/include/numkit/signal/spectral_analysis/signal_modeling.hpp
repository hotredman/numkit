// libs/signal/include/numkit/signal/spectral_analysis/signal_modeling.hpp
//
// Parametric signal modelling: Yule-Walker / Burg / linear-prediction
// AR models + the conversion utilities that move between AR poly,
// reflection coefficients, autocorrelation, line-spectral frequencies,
// inverse-sine and log-area-ratio representations.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

#include <tuple>

namespace numkit::signal {

// ── Foundation: Levinson-Durbin ────────────────────────────────────

/// levinson(r[, n]) — solve a Hermitian Toeplitz system whose first
/// row is r (length n+1) for the AR(n) coefficient vector. Returns
/// (a, e, k) where a is length n+1 (a[0] = 1), e is the prediction
/// error power (scalar), and k is the reflection coefficients (length n).
std::tuple<Value, Value, Value>
levinson(const Value &r, int n = -1, std::pmr::memory_resource *mr = nullptr);

/// rlevinson(a, e) — inverse of levinson: from AR poly + prediction
/// error to autocorrelation sequence + reflection coefficients.
/// Returns (R, k).
std::tuple<Value, Value>
rlevinson(const Value &a, double efinal, std::pmr::memory_resource *mr = nullptr);

// ── Autoregressive estimation ──────────────────────────────────────

/// aryule(x, p) — Yule-Walker estimate of AR(p) parameters.
/// Returns (a, e, k): a is length p+1, e is prediction error variance,
/// k is reflection coefficients (length p).
std::tuple<Value, Value, Value>
aryule(const Value &x, int p, std::pmr::memory_resource *mr = nullptr);

/// arburg(x, p) — Burg's method. Same return tuple as aryule.
std::tuple<Value, Value, Value>
arburg(const Value &x, int p, std::pmr::memory_resource *mr = nullptr);

/// lpc(x, p) — linear-prediction coefficients (alias of aryule for
/// the AR poly + sqrt of the error variance as a gain). Returns (a, g).
std::tuple<Value, Value>
lpc(const Value &x, int p, std::pmr::memory_resource *mr = nullptr);

// ── Representation conversions (poly ↔ rc ↔ autocorr) ─────────────

/// ac2poly(R) — autocorrelation → AR poly.
/// Composes levinson on the input autocorrelation; returns (a, e).
std::tuple<Value, Value>
ac2poly(const Value &R, std::pmr::memory_resource *mr = nullptr);

/// poly2ac(a, e) — AR poly + prediction error → autocorrelation.
/// Inverse of ac2poly. Returns the length-n+1 autocorrelation sequence.
Value poly2ac(const Value &a, double e, std::pmr::memory_resource *mr = nullptr);

/// ac2rc(R) — autocorrelation → reflection coefficients (and predictor
/// error variance). Returns (k, r0).
std::tuple<Value, Value>
ac2rc(const Value &R, std::pmr::memory_resource *mr = nullptr);

/// schurrc(R) — Schur reflection coefficients from autocorrelation.
/// Equivalent to the first output of ac2rc; matches MATLAB's schurrc.
/// Returns a column vector of length numel(R) - 1.
Value schurrc(const Value &R, std::pmr::memory_resource *mr = nullptr);

/// rc2ac(k, r0) — reflection coefficients + r[0] → autocorrelation.
Value rc2ac(const Value &k, double r0, std::pmr::memory_resource *mr = nullptr);

/// poly2rc(a) — Step-down recursion from AR poly to reflection coeffs.
Value poly2rc(const Value &a, std::pmr::memory_resource *mr = nullptr);

/// rc2poly(k) — Step-up recursion from reflection coeffs to AR poly.
Value rc2poly(const Value &k, std::pmr::memory_resource *mr = nullptr);

/// is2rc(is) — inverse-sine parameterisation → reflection coefficients.
Value is2rc(const Value &is, std::pmr::memory_resource *mr = nullptr);

/// rc2is(k) — reflection coefficients → inverse-sine parameterisation.
Value rc2is(const Value &k, std::pmr::memory_resource *mr = nullptr);

/// lar2rc(g) — log-area-ratio → reflection coefficients.
Value lar2rc(const Value &g, std::pmr::memory_resource *mr = nullptr);

/// rc2lar(k) — reflection coefficients → log-area-ratio.
Value rc2lar(const Value &k, std::pmr::memory_resource *mr = nullptr);

// ── Covariance / modified-covariance AR + Prony + corrmtx ─────────

/// arcov(x, p) — covariance AR: minimise the forward prediction
/// error energy over n = p .. N-1. Returns (a, e).
std::tuple<Value, Value>
arcov(const Value &x, int p, std::pmr::memory_resource *mr = nullptr);

/// armcov(x, p) — modified-covariance AR: average forward + backward
/// prediction error energies. Returns (a, e).
std::tuple<Value, Value>
armcov(const Value &x, int p, std::pmr::memory_resource *mr = nullptr);

/// prony(h, nb, na) — given impulse response h, identify a (nb, na)
/// IIR filter b(z)/a(z). Returns (b, a). Solves a denominator system
/// by least squares then convolves a with h to recover b.
std::tuple<Value, Value>
prony(const Value &h, int nb, int na, std::pmr::memory_resource *mr = nullptr);

/// corrmtx(x, m) — generate the (n+m) × (m+1) data matrix X such
/// that X'·X is the autocorrelation matrix Rxx (default
/// 'autocorrelation' method). Other methods not yet supported.
Value corrmtx(const Value &x, int m, std::pmr::memory_resource *mr = nullptr);

// ── LSF ↔ AR poly ─────────────────────────────────────────────────

/// poly2lsf(a) — line spectral frequencies (in rad). Returns a column
/// vector of N angles in (0, π) for AR poly of order N.
Value poly2lsf(const Value &a, std::pmr::memory_resource *mr = nullptr);

/// lsf2poly(lsf) — inverse: LSF angles → AR coefficient vector
/// length N+1.
Value lsf2poly(const Value &lsf, std::pmr::memory_resource *mr = nullptr);

// ── invfreqs / invfreqz — frequency-response least-squares fit ────

/// invfreqs(H, w, nb, na) — fit analog filter b(s)/a(s) of orders
/// (nb, na) to a desired complex frequency response H sampled at
/// angular frequencies w (rad/s). Levi-style equation-error LSQ.
/// Returns (b, a).
std::tuple<Value, Value>
invfreqs(const Value &H, const Value &w, int nb, int na, std::pmr::memory_resource *mr = nullptr);

/// invfreqz(H, w, nb, na) — same as invfreqs for digital filters.
/// w in [0, π], polynomial in z⁻¹.
std::tuple<Value, Value>
invfreqz(const Value &H, const Value &w, int nb, int na, std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::signal
