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
/// @throws      Error if Wn is out of (0, 1) or type is unrecognized.
std::tuple<Value, Value>
butter(std::pmr::memory_resource *mr, int N, double Wn, const std::string &type = "low");

/// FIR filter design via windowed-sinc (Hamming window).
/// Returns the impulse response coefficients b (row vector of length N+1).
///
/// @param N     Filter order (integer >= 1). Output length is N+1.
/// @param Wn    Normalized cutoff frequency in (0, 1) — fraction of Nyquist.
/// @param type  "low" (default) or "high".
Value fir1(std::pmr::memory_resource *mr, int N, double Wn, const std::string &type = "low");

/// Least-squares FIR filter design (Type-I linear-phase only).
/// Solves the weighted least-squares problem on a piecewise-linear
/// desired amplitude response specified by frequency-edge / amplitude
/// pairs. Returns the impulse response coefficients b (row vector of
/// length N+1).
///
/// @param N      Filter order (must be even — Type-I only). Length = N+1.
/// @param F      Vector of band-edge frequencies in [0, 1] (Nyquist=1),
///               must have even length and be non-decreasing.
/// @param A      Desired amplitudes at each F point (same length as F).
///               Within each band [F[2k], F[2k+1]] the desired amp is
///               linearly interpolated from A[2k] to A[2k+1].
/// @throws       Error if N is odd, or F/A invalid.
Value firls(std::pmr::memory_resource *mr, int N,
            const double *F, std::size_t Fn,
            const double *A, std::size_t An);

/// cell2sos(C) — convert cell array of {Bi, Ai} pairs to L×6 SOS matrix.
/// 2-output form [S, G] = cell2sos(C) extracts the leading scalar gain
/// section if present (when C{1} = {scalar_b, scalar_a}).
/// Linear (length-2) sections are zero-padded on the right to length 3.
std::tuple<Value, Value>
cell2sos(std::pmr::memory_resource *mr, const Value &C);

/// fir2(N, F, A) — arbitrary-response FIR via frequency-sampling +
/// inverse FFT + Hamming window. Bit-equal MATLAB R2025b on the
/// 3-arg form (npt=512 default for nn<1024, hamming default window,
/// lap=floor(npt/25) transition smoothing).
/// F must satisfy F(1)=0, F(end)=1, monotonically nondecreasing.
/// A is the desired amplitude at each break frequency.
/// KNOWN GAP: optional npt/lap/wind args deferred.
Value fir2(std::pmr::memory_resource *mr, int N,
           const double *F, std::size_t Fn,
           const double *A, std::size_t An);

/// firpm(N, F, A[, W][, ftype]) — Parks-McClellan optimal equiripple
/// FIR (Remez exchange). Returns (b, err) — `b` is the row vector of
/// length N+1 of filter coefficients, `err` is the peak ripple
/// magnitude |δ|. Supports all four linear-phase FIR types via the
/// Q-factor trick:
///   • Type I   (even N, symmetric)       — Q(ω) = 1
///   • Type II  (odd  N, symmetric)       — Q(ω) = cos(ω/2)
///   • Type III (even N, anti-symmetric)  — Q(ω) = sin(ω)
///   • Type IV  (odd  N, anti-symmetric)  — Q(ω) = sin(ω/2)
/// Type III / IV are selected by passing ftype = "hilbert" (constant
/// amplitude) or "differentiator" (amplitude linear in frequency).
///
/// @param N      Filter order ≥ 3.
/// @param F      Band edges in [0,1] (Nyquist=1), even-length, non-decreasing.
/// @param A      Desired amplitude at each F point — piecewise linear
///               interpolation inside each band.
/// @param W      Optional weight per band (length = numBands = Fn/2).
///               When nullptr, all weights are 1.0.
/// @param ftype  "" / "hilbert" / "differentiator" (case-insensitive).
///               Empty / unset → Type I (even N) or II (odd N).
///
/// KNOWN GAPS:
///   - No `fresp` function-handle form.
///   - `lgrid` fixed at the MATLAB default (16).
///   - No 3rd `res` output struct.
std::tuple<Value, double>
firpm(std::pmr::memory_resource *mr, int N,
      const double *F, std::size_t Fn,
      const double *A, std::size_t An,
      const double *W, std::size_t Wn,
      const std::string &ftype = "");

} // namespace numkit::signal
