/// @file lti.hpp
/// @ingroup group_control
// toolboxes/control/include/numkit/control/lti/lti.hpp
//
// LTI (Linear Time-Invariant) system constructors. Each returns a
// numkit struct value with a `kind` field tag ('tf', 'zpk', or 'ss')
// plus the canonical fields. Optional `Ts` field for discrete systems
// (>0); 0 means continuous-time.

#pragma once

#include <memory_resource>
#include <numkit/value/value.hpp>
#include <tuple>

namespace numkit::control {

/// Transfer-function LTI constructor (`tf(num, den, Ts)`).
///
/// Builds a `tf`-tagged struct value with fields `{kind="tf",
/// num, den, Ts, variable}`. The numerator and denominator are row
/// coefficient vectors with the **leading coefficient first**.
/// `Ts` selects the time domain:
///   - `Ts == 0` → continuous, variable = `"s"`.
///   - `Ts > 0`  → discrete with that sample time, variable = `"z"`.
///   - `Ts == -1`→ discrete "unspecified".
///
/// @param num  Numerator coefficients.
/// @param den  Denominator coefficients (leading coefficient nonzero).
/// @param Ts   Sample time in seconds; 0 for continuous.
/// @param mr   Memory resource (nullptr → process default).
/// @return     Struct Value tagged `kind="tf"`.
///
/// @code
/// auto sys = tf({1, 2}, {1, 3, 2}, 0);   // (s + 2) / (s² + 3s + 2)
/// @endcode
///
/// @see zpk, ss, filt, frd
Value tf(const Value &num, const Value &den, double Ts,
         std::pmr::memory_resource *mr = nullptr);

/// Zero-pole-gain LTI constructor (`zpk(z, p, k, Ts)`).
///
/// Builds a `zpk`-tagged struct value with fields `{kind="zpk",
/// z, p, k, Ts}`. `z` and `p` are vectors of zeros and poles
/// (real or complex); `k` is a real scalar gain.
///
/// @param z   Zero list.
/// @param p   Pole list.
/// @param k   Gain scalar.
/// @param Ts  Sample time (0 for continuous).
/// @param mr  Memory resource (nullptr → process default).
/// @return    Struct Value tagged `kind="zpk"`.
///
/// @see tf, ss
Value zpk(const Value &z, const Value &p, double k, double Ts,
          std::pmr::memory_resource *mr = nullptr);

/// State-space LTI constructor (`ss(A, B, C, D, Ts)`).
///
/// Builds an `ss`-tagged struct value with fields `{kind="ss",
/// A, B, C, D, Ts}`. Dimensions:
///   - A is n×n,
///   - B is n×nin,
///   - C is nout×n,
///   - D is nout×nin (a scalar 0 is broadcast when D is empty).
///
/// @param A    State matrix (n×n).
/// @param B    Input matrix (n×nin).
/// @param C    Output matrix (nout×n).
/// @param D    Feedthrough matrix (nout×nin); empty → broadcast 0.
/// @param Ts   Sample time (0 for continuous).
/// @param mr   Memory resource (nullptr → process default).
/// @return         Struct Value tagged `kind="ss"`.
///
/// @see tf, zpk
Value ss(const Value &A, const Value &B, const Value &C, const Value &D,
        double Ts, std::pmr::memory_resource *mr = nullptr);

/// Discrete tf with `z^-1` variable convention (`filt(num, den, Ts)`).
///
/// Equivalent to @ref tf except:
///   - default `Ts` is −1 ("unspecified discrete"),
///   - `variable` field is set to `"z^-1"`,
///   - coefficient order is that of `filt`: num/den as in
///     `Y(z)/U(z) = (b0 + b1 z^-1 + …) / (a0 + a1 z^-1 + …)`.
///
/// @param num  Numerator coefficients (z^-1 ascending powers).
/// @param den  Denominator coefficients.
/// @param Ts   Sample time (−1 = unspecified, > 0 = explicit).
/// @param mr   Memory resource (nullptr → process default).
/// @return     Struct Value tagged `kind="tf"` with variable = "z^-1".
///
/// @see tf
Value filt(const Value &num, const Value &den, double Ts,
           std::pmr::memory_resource *mr = nullptr);

/// Frequency-response data model (`frd(response, frequency, Ts)`).
///
/// Builds a struct `{kind="frd", resp, freq, Ts}`. `response` may be
/// complex; `frequency` is a real vector (rad/s). Both are stored as
/// column vectors.
///
/// @param response   Complex response samples.
/// @param frequency  Real frequency grid (rad/s).
/// @param Ts         Sample time (0 = continuous).
/// @param mr         Memory resource (nullptr → process default).
/// @return           Struct Value tagged `kind="frd"`.
///
/// @see frdata
Value frd(const Value &response, const Value &frequency, double Ts,
          std::pmr::memory_resource *mr = nullptr);

/// Extract `(resp, freq)` from an frd model.
///
/// @param sys  frd struct.
/// @param mr   Memory resource (nullptr → process default).
/// @return     `(resp, freq)`; bind via `auto [r, f] = frdata(sys);`.
/// @throws     Error if `kind != "frd"`.
///
/// @see frd
std::tuple<Value, Value>
frdata(const Value &sys, std::pmr::memory_resource *mr = nullptr);

/// Extract numerator / denominator from any LTI form (`tfdata`).
///
/// With `asVector = true` returns row vectors padded so num and den
/// have equal length (leading zeros on num).
/// With `asVector = false` wraps each row vector in a 1×1 cell
/// (the default).
///
/// Accepts tf inputs directly; zpk and ss inputs are converted via
/// @ref zp2tf and @ref ss2tf.
///
/// @param sys       LTI struct (tf / zpk / ss).
/// @param asVector  `true` for raw row vectors, `false` for 1×1 cells.
/// @param mr        Memory resource (nullptr → process default).
/// @return          `(num, den)`.
/// @throws          Error on unrecognised kind.
///
/// @see zpkdata, ssdata
std::tuple<Value, Value>
tfdata(const Value &sys, bool asVector, std::pmr::memory_resource *mr = nullptr);

/// Extract zeros, poles, and gain (`zpkdata`).
///
/// `z` and `p` are column vectors (or 1×1 cells without `asVector`).
/// `k` is always a numeric scalar.
///
/// @param sys       LTI struct (tf / zpk / ss).
/// @param asVector  `true` for raw vectors, `false` for cell-wrapped.
/// @param mr        Memory resource (nullptr → process default).
/// @return          `(z, p, k)`.
///
/// @see tfdata, ssdata
std::tuple<Value, Value, Value>
zpkdata(const Value &sys, bool asVector, std::pmr::memory_resource *mr = nullptr);

/// Extract A, B, C, D from any LTI form (`ssdata`).
///
/// tf / zpk inputs are first converted via @ref tf2ss / @ref zp2tf+@ref tf2ss.
///
/// @param sys  LTI struct (tf / zpk / ss).
/// @param mr   Memory resource (nullptr → process default).
/// @return     `(A, B, C, D)`.
///
/// @see tfdata, zpkdata
std::tuple<Value, Value, Value, Value>
ssdata(const Value &sys, std::pmr::memory_resource *mr = nullptr);

/// Similarity transform of a state-space model (`ss2ss(sys, T)`).
///
/// Applies the change of basis `x' = T · x`:
/// @f$ A' = T A T^{-1},\ B' = T B,\ C' = C T^{-1},\ D' = D @f$.
/// Returns a fresh ss struct; the original is untouched.
///
/// @param sys  Input ss struct.
/// @param T    n×n invertible similarity matrix.
/// @param mr   Memory resource (nullptr → process default).
/// @return     New ss-tagged struct with transformed (A,B,C,D).
/// @throws     Error if T is singular.
Value ss2ss(const Value &sys, const Value &T,
            std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::control
