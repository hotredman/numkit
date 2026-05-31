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

// ─────────────────────────────────────────────────────────────────────
// Foundation: Levinson-Durbin
// ─────────────────────────────────────────────────────────────────────

/// Levinson-Durbin recursion: solve a Hermitian Toeplitz system.
///
/// Given the first row `r` (length `n+1`) of a Hermitian Toeplitz
/// matrix, computes the AR(n) coefficients via the order-recursive
/// Levinson algorithm. Returns the AR poly, the residual prediction
/// error variance, and the reflection coefficients.
///
/// @param r   First row of the Toeplitz matrix (autocorrelation sequence).
/// @param n   AR order. `-1` (default) → `numel(r) - 1`.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Tuple `(a, e, k)`:
///              * `a` — AR coefficient vector of length `n+1`, with `a[0] = 1`.
///              * `e` — prediction error power (scalar).
///              * `k` — reflection coefficients of length `n`.
///
/// @see rlevinson, aryule, poly2rc
std::tuple<Value, Value, Value>
levinson(const Value &                r,
         int                          n  = -1,
         std::pmr::memory_resource *  mr = nullptr);

/// Inverse Levinson recursion: recover autocorrelation from AR poly.
///
/// @param a       AR coefficient vector (`a[0]` = 1).
/// @param efinal  Final prediction error variance.
/// @param mr      Memory resource (nullptr → process default).
/// @return        Tuple `(R, k)` — autocorrelation sequence and
///                reflection coefficients.
///
/// @see levinson
std::tuple<Value, Value>
rlevinson(const Value &                a,
          double                       efinal,
          std::pmr::memory_resource *  mr = nullptr);

// ─────────────────────────────────────────────────────────────────────
// Autoregressive estimation
// ─────────────────────────────────────────────────────────────────────

/// Yule-Walker AR(p) parameter estimation.
///
/// Solves the normal equations on the biased autocorrelation sequence
/// via Levinson-Durbin.
///
/// @param x   Real 1-D signal.
/// @param p   AR model order.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Tuple `(a, e, k)` (see `levinson`).
///
/// @see arburg, levinson, pyulear
std::tuple<Value, Value, Value>
aryule(const Value &                x,
       int                          p,
       std::pmr::memory_resource *  mr = nullptr);

/// @brief Burg's AR(p) parameter estimation.
///
/// Minimises the sum of forward + backward prediction error variances
/// in an order-recursive fashion. Numerically stable for short signals
/// where Yule-Walker autocorrelation estimates are biased.
///
/// @param x   Real 1-D signal.
/// @param p   AR model order.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Tuple `(a, e, k)` — AR poly, error variance, reflection
///            coefficients (see @ref levinson).
/// @see aryule, pburg
std::tuple<Value, Value, Value>
arburg(const Value &x, int p,
       std::pmr::memory_resource *mr = nullptr);

/// Linear-prediction coefficients (`aryule` alias + sqrt-error gain).
///
/// @param x   Real 1-D signal.
/// @param p   Prediction order.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Tuple `(a, g)` — AR poly and `sqrt(e)` gain factor.
///
/// @see aryule
std::tuple<Value, Value>
lpc(const Value &                x,
    int                          p,
    std::pmr::memory_resource *  mr = nullptr);

// ─────────────────────────────────────────────────────────────────────
// Representation conversions (poly ↔ rc ↔ autocorr)
// ─────────────────────────────────────────────────────────────────────

/// Autocorrelation → AR poly (composes `levinson`).
/// @param R   Autocorrelation sequence (length n+1).
/// @param mr  Memory resource (nullptr → process default).
/// @return    Tuple `(a, e)`.
std::tuple<Value, Value>
ac2poly(const Value &                R,
        std::pmr::memory_resource *  mr = nullptr);

/// AR poly + prediction error → autocorrelation.
/// @param a   AR coefficient vector.
/// @param e   Prediction error variance.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Autocorrelation column of length `numel(a)`.
Value poly2ac(const Value &                a,
              double                       e,
              std::pmr::memory_resource *  mr = nullptr);

/// Autocorrelation → reflection coefficients.
/// @param R   Autocorrelation sequence.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Tuple `(k, r0)` — reflection coefficients and `r[0]`.
std::tuple<Value, Value>
ac2rc(const Value &                R,
      std::pmr::memory_resource *  mr = nullptr);

/// Schur reflection coefficients from autocorrelation.
///
/// Equivalent to the first output of `ac2rc`.
/// @param R   Autocorrelation sequence.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Column vector of length `numel(R) - 1`.
Value schurrc(const Value &                R,
              std::pmr::memory_resource *  mr = nullptr);

/// @brief Reflection coefficients + r[0] → autocorrelation.
///
/// Inverse of @ref ac2rc.
///
/// @param k   Reflection coefficients.
/// @param r0  Zeroth autocorrelation sample.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Autocorrelation column of length `numel(k) + 1`.
/// @see ac2rc
Value rc2ac(const Value &k, double r0,
            std::pmr::memory_resource *mr = nullptr);

/// AR poly → reflection coefficients (step-down recursion).
/// @param a       AR coefficient vector.
/// @param efinal  Final prediction error (for the second output R0).
/// @param mr      Memory resource (nullptr → process default).
/// @return        Tuple `(k, R0)`: reflection coefficients of length
///                `numel(a) - 1`, and the zero-lag autocorrelation
///                `R0 = efinal / prod(1 - k.^2)`.
std::tuple<Value, Value>
poly2rc(const Value &                a,
        double                       efinal = 0.0,
        std::pmr::memory_resource *  mr     = nullptr);

/// Reflection coefficients → AR poly (step-up recursion).
/// @param k   Reflection coefficients.
/// @param r0  Zero-lag autocorrelation (for the second output efinal).
/// @param mr  Memory resource (nullptr → process default).
/// @return    Tuple `(a, efinal)`: AR coefficient vector of length
///            `numel(k) + 1`, and the final prediction error
///            `efinal = r0 * prod(1 - k.^2)`.
std::tuple<Value, Value>
rc2poly(const Value &                k,
        double                       r0 = 1.0,
        std::pmr::memory_resource *  mr = nullptr);

/// @brief Inverse-sine parameterisation → reflection coefficients.
///
/// @param is  Inverse-sine parameter vector.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Reflection-coefficient vector.
/// @see rc2is
Value is2rc(const Value &                is,
            std::pmr::memory_resource *  mr = nullptr);

/// @brief Reflection coefficients → inverse-sine parameterisation.
///
/// @param k   Reflection-coefficient vector.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Inverse-sine parameter vector.
/// @see is2rc
Value rc2is(const Value &                k,
            std::pmr::memory_resource *  mr = nullptr);

/// @brief Log-area-ratio → reflection coefficients.
///
/// @param g   Log-area-ratio vector.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Reflection-coefficient vector.
/// @see rc2lar
Value lar2rc(const Value &                g,
             std::pmr::memory_resource *  mr = nullptr);

/// @brief Reflection coefficients → log-area-ratio.
///
/// @param k   Reflection-coefficient vector.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Log-area-ratio vector.
/// @see lar2rc
Value rc2lar(const Value &                k,
             std::pmr::memory_resource *  mr = nullptr);

// ─────────────────────────────────────────────────────────────────────
// Covariance / modified-covariance AR + Prony + corrmtx
// ─────────────────────────────────────────────────────────────────────

/// Covariance method AR estimation.
///
/// Minimises the forward prediction error energy over `n = p..N-1`.
/// Avoids the bias of Yule-Walker but lacks the BIBO guarantee of Burg.
///
/// @param x   Real 1-D signal.
/// @param p   AR order.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Tuple `(a, e)`.
///
/// @see armcov, arburg
std::tuple<Value, Value>
arcov(const Value &                x,
      int                          p,
      std::pmr::memory_resource *  mr = nullptr);

/// @brief Modified-covariance AR estimation.
///
/// Averages forward + backward prediction error energies. Tighter peak
/// detection than the basic covariance method.
///
/// @param x   Real 1-D signal.
/// @param p   AR order.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Tuple `(a, e)`.
/// @see arcov, arburg
std::tuple<Value, Value>
armcov(const Value &x, int p,
       std::pmr::memory_resource *mr = nullptr);

/// Prony IIR identification from impulse response.
///
/// Given an impulse response `h`, identifies the `(nb, na)` IIR filter
/// `b(z)/a(z)` whose impulse response matches `h` in a least-squares
/// sense. Solves the denominator equation by LS, then convolves `a`
/// with `h` to recover `b`.
///
/// @param h   Impulse response.
/// @param nb  Numerator order.
/// @param na  Denominator order.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Tuple `(b, a)`.
std::tuple<Value, Value>
prony(const Value &                h,
      int                          nb,
      int                          na,
      std::pmr::memory_resource *  mr = nullptr);

/// Generate the autocorrelation data matrix.
///
/// Returns the `(n+m) × (m+1)` matrix X such that X'·X is the biased
/// autocorrelation matrix Rxx (default `'autocorrelation'` method).
/// Other methods (`'covariance'`, `'prewindowed'`, etc.) are deferred.
///
/// @param x   Real 1-D signal.
/// @param m   Order parameter (X has `m+1` columns).
/// @param mr  Memory resource (nullptr → process default).
/// @return    `(n+m) × (m+1)` data matrix.
Value corrmtx(const Value &                x,
              int                          m,
              std::pmr::memory_resource *  mr = nullptr);

// ─────────────────────────────────────────────────────────────────────
// LSF ↔ AR poly
// ─────────────────────────────────────────────────────────────────────

/// AR poly → line-spectral frequencies (radians).
///
/// @param a   AR coefficient vector of length N+1.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Column vector of N angles in `(0, π)`.
Value poly2lsf(const Value &                a,
               std::pmr::memory_resource *  mr = nullptr);

/// Line-spectral frequencies → AR poly.
///
/// @param lsf   LSF angles in `(0, π)`.
/// @param mr    Memory resource (nullptr → process default).
/// @return      AR coefficient vector of length `numel(lsf) + 1`.
Value lsf2poly(const Value &                lsf,
               std::pmr::memory_resource *  mr = nullptr);

// ─────────────────────────────────────────────────────────────────────
// invfreqs / invfreqz — frequency-response least-squares fit
// ─────────────────────────────────────────────────────────────────────

/// Fit analog filter to a frequency response (Levi equation-error LSQ).
///
/// Solves for `(b, a)` of orders `(nb, na)` minimising
/// \f$ \sum_k |A(j\omega_k) H_k - B(j\omega_k)|^2 \f$.
///
/// @param H   Desired complex response samples.
/// @param w   Angular frequencies in rad/s, same length as H.
/// @param nb  Numerator order.
/// @param na  Denominator order.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Tuple `(b, a)`.
///
/// @see invfreqz, freqs
std::tuple<Value, Value>
invfreqs(const Value &                H,
         const Value &                w,
         int                          nb,
         int                          na,
         std::pmr::memory_resource *  mr = nullptr);

/// @brief Digital counterpart of @ref invfreqs.
///
/// `w` is in `[0, π]`; the polynomial is in `z^{-1}`.
///
/// @param H   Desired complex response samples.
/// @param w   Normalised digital frequencies in `[0, π]`, length matches `H`.
/// @param nb  Numerator order.
/// @param na  Denominator order.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Tuple `(b, a)`.
/// @see invfreqs, freqz
std::tuple<Value, Value>
invfreqz(const Value &H, const Value &w, int nb, int na,
         std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::signal
