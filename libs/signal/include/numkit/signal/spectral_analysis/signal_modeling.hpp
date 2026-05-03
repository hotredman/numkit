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
levinson(std::pmr::memory_resource *mr, const Value &r, int n = -1);

/// rlevinson(a, e) — inverse of levinson: from AR poly + prediction
/// error to autocorrelation sequence + reflection coefficients.
/// Returns (R, k).
std::tuple<Value, Value>
rlevinson(std::pmr::memory_resource *mr, const Value &a, double efinal);

// ── Autoregressive estimation ──────────────────────────────────────

/// aryule(x, p) — Yule-Walker estimate of AR(p) parameters.
/// Returns (a, e, k): a is length p+1, e is prediction error variance,
/// k is reflection coefficients (length p).
std::tuple<Value, Value, Value>
aryule(std::pmr::memory_resource *mr, const Value &x, int p);

/// arburg(x, p) — Burg's method. Same return tuple as aryule.
std::tuple<Value, Value, Value>
arburg(std::pmr::memory_resource *mr, const Value &x, int p);

/// lpc(x, p) — linear-prediction coefficients (alias of aryule for
/// the AR poly + sqrt of the error variance as a gain). Returns (a, g).
std::tuple<Value, Value>
lpc(std::pmr::memory_resource *mr, const Value &x, int p);

// ── Representation conversions (poly ↔ rc ↔ autocorr) ─────────────

/// ac2poly(R) — autocorrelation → AR poly.
/// Composes levinson on the input autocorrelation; returns (a, e).
std::tuple<Value, Value>
ac2poly(std::pmr::memory_resource *mr, const Value &R);

/// poly2ac(a, e) — AR poly + prediction error → autocorrelation.
/// Inverse of ac2poly. Returns the length-n+1 autocorrelation sequence.
Value poly2ac(std::pmr::memory_resource *mr, const Value &a, double e);

/// ac2rc(R) — autocorrelation → reflection coefficients (and predictor
/// error variance). Returns (k, r0).
std::tuple<Value, Value>
ac2rc(std::pmr::memory_resource *mr, const Value &R);

/// rc2ac(k, r0) — reflection coefficients + r[0] → autocorrelation.
Value rc2ac(std::pmr::memory_resource *mr, const Value &k, double r0);

/// poly2rc(a) — Step-down recursion from AR poly to reflection coeffs.
Value poly2rc(std::pmr::memory_resource *mr, const Value &a);

/// rc2poly(k) — Step-up recursion from reflection coeffs to AR poly.
Value rc2poly(std::pmr::memory_resource *mr, const Value &k);

/// is2rc(is) — inverse-sine parameterisation → reflection coefficients.
Value is2rc(std::pmr::memory_resource *mr, const Value &is);

/// rc2is(k) — reflection coefficients → inverse-sine parameterisation.
Value rc2is(std::pmr::memory_resource *mr, const Value &k);

/// lar2rc(g) — log-area-ratio → reflection coefficients.
Value lar2rc(std::pmr::memory_resource *mr, const Value &g);

/// rc2lar(k) — reflection coefficients → log-area-ratio.
Value rc2lar(std::pmr::memory_resource *mr, const Value &k);

} // namespace numkit::signal
