// libs/signal/include/numkit/signal/filter_design/filter_design.hpp
#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

#include <string>
#include <tuple>

namespace numkit::signal {

/// Butterworth IIR lowpass / highpass filter design.
/// Returns (b, a) — numerator and denominator polynomials.
///
/// @param N     Filter order (integer >= 1).
/// @param Wn    Normalized cutoff frequency in (0, 1) — fraction of Nyquist.
/// @param type  "low" (default) or "high".
/// @param mr    Memory resource (nullptr → process default).
/// @throws      Error if Wn is out of (0, 1) or type is unrecognised.
///
/// @code  auto [b, a] = butter(4, 0.3);  // 4th-order lowpass  @endcode
std::tuple<Value, Value>
butter(int                          N,
       double                       Wn,
       const std::string &          type = "low",
       std::pmr::memory_resource *  mr   = nullptr);

/// FIR filter design via windowed-sinc (Hamming window).
/// Returns the impulse response coefficients b (row vector of length N+1).
///
/// @param N     Filter order (>= 1). Output length is N+1.
/// @param Wn    Normalized cutoff frequency in (0, 1) — fraction of Nyquist.
/// @param type  "low" (default) or "high".
/// @param mr    Memory resource (nullptr → process default).
Value fir1(int                          N,
           double                       Wn,
           const std::string &          type = "low",
           std::pmr::memory_resource *  mr   = nullptr);

/// Least-squares FIR filter design (Type-I linear-phase only).
/// Solves the weighted least-squares problem on a piecewise-linear
/// desired amplitude response specified by frequency-edge / amplitude
/// pairs. Returns the impulse response coefficients b (row vector of
/// length N+1).
///
/// @param N   Filter order (must be even — Type-I only). Length = N+1.
/// @param F   Vector of band-edge frequencies in [0, 1] (Nyquist=1),
///            must have even length and be non-decreasing.
/// @param A   Desired amplitudes at each F point (same length as F).
///            Within each band [F[2k], F[2k+1]] the desired amp is
///            linearly interpolated from A[2k] to A[2k+1].
/// @param mr  Memory resource (nullptr → process default).
/// @throws    Error if N is odd, or F/A invalid.
///
/// @code  Value b = firls(40, {0, 0.4, 0.5, 1.0}, {1, 1, 0, 0});  @endcode
Value firls(int                          N,
            const Value &                F,
            const Value &                A,
            std::pmr::memory_resource *  mr = nullptr);

/// cell2sos(C) — convert cell array of {Bi, Ai} pairs to L×6 SOS matrix.
/// 2-output form [S, G] = cell2sos(C) extracts the leading scalar gain
/// section if present (when C{1} = {scalar_b, scalar_a}).
/// Linear (length-2) sections are zero-padded on the right to length 3.
std::tuple<Value, Value>
cell2sos(const Value &                C,
         std::pmr::memory_resource *  mr = nullptr);

/// fir2(N, F, A) — arbitrary-response FIR via frequency-sampling +
/// inverse FFT + Hamming window. Bit-equal MATLAB R2025b on the
/// 3-arg form (npt=512 default for nn<1024, hamming default window,
/// lap=floor(npt/25) transition smoothing).
///
/// @param N   Filter order. Output length is N+1.
/// @param F   Break frequencies in [0, 1]. F(1)=0, F(end)=1, monotonic.
/// @param A   Desired amplitude at each break frequency.
/// @param mr  Memory resource (nullptr → process default).
///
/// KNOWN GAP: optional npt/lap/wind args deferred.
///
/// @code  Value b = fir2(50, {0,0.3,0.5,0.7,1}, {1,1,0,0,0});  @endcode
Value fir2(int                          N,
           const Value &                F,
           const Value &                A,
           std::pmr::memory_resource *  mr = nullptr);

/// firpm(N, F, A [, W] [, ftype]) — Parks-McClellan optimal equiripple
/// FIR (Remez exchange). Returns (b, err) — `b` is the row vector of
/// length N+1 of filter coefficients, `err` is the peak ripple
/// magnitude |δ|. Supports all four linear-phase FIR types via the
/// Q-factor trick:
///   * Type I   (even N, symmetric)       — Q(ω) = 1
///   * Type II  (odd  N, symmetric)       — Q(ω) = cos(ω/2)
///   * Type III (even N, anti-symmetric)  — Q(ω) = sin(ω)
///   * Type IV  (odd  N, anti-symmetric)  — Q(ω) = sin(ω/2)
/// Type III / IV are selected by passing ftype = "hilbert" (constant
/// amplitude) or "differentiator" (amplitude linear in frequency).
///
/// @param N      Filter order ≥ 3.
/// @param F      Band edges in [0,1] (Nyquist=1), even-length, non-decreasing.
/// @param A      Desired amplitude at each F point — piecewise linear
///               interpolation inside each band.
/// @param W      Optional weight per band (length = F.numel()/2). Pass an
///               empty Value{} for unit weights everywhere.
/// @param ftype  "" / "hilbert" / "differentiator" (case-insensitive).
///               Empty / unset → Type I (even N) or II (odd N).
/// @param mr     Memory resource (nullptr → process default).
///
/// KNOWN GAPS: no `fresp` function-handle form, `lgrid` fixed at
/// MATLAB's default (16), no 3rd `res` output struct.
///
/// @code
/// auto [h, err] = firpm(30, {0,0.4,0.5,1}, {1,1,0,0});            // LP
/// auto [h, err] = firpm(30, {0,0.4,0.5,1}, {1,1,0,0}, {1, 10});   // weighted
/// auto [h, err] = firpm(30, {0.05, 0.95}, {1, 1}, {}, "hilbert"); // Type III
/// @endcode
std::tuple<Value, double>
firpm(int                          N,
      const Value &                F,
      const Value &                A,
      const Value &                W      = {},
      const std::string &          ftype  = "",
      std::pmr::memory_resource *  mr     = nullptr);

} // namespace numkit::signal
