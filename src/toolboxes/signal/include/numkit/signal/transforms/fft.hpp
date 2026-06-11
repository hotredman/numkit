// toolboxes/signal/include/numkit/signal/transforms/fft.hpp
#pragma once

#include <memory_resource>
#include <numkit/value/span.hpp>
#include <numkit/value/value.hpp>

namespace numkit::signal {

/// 1-D discrete Fourier transform along a given dimension.
///
/// Call forms:
///   * `fft(x)`         — along the first non-singleton dimension
///   * `fft(x, n)`      — zero-pad or truncate to length n first
///   * `fft(x, -1, k)`  — along dimension k (1=rows, 2=cols, 3=pages)
///   * `fft(x, n,  k)`  — both n and dim explicit
///
/// Forward transform is always complex-typed; an inverse transform that
/// happens to produce a real result (imaginary part ≤ 1e-10 everywhere)
/// is automatically downgraded to DOUBLE.
///
/// @param x      Input — real or complex, 1-D / 2-D / 3-D. Empty → empty.
/// @param n      Output length per transform along `dim`. `-1` (default) keeps
///               the input length; `n > axis_len` zero-pads; `n < axis_len`
///               truncates.
/// @param dim    Axis to transform along. `0` (default) = first non-singleton
///               dimension (a row vector therefore stays a row vector).
///               `1` = rows, `2` = columns, `3` = pages.
/// @param mr     Memory resource (nullptr → process default).
/// @return       Value of the same shape as `x` with the chosen axis replaced
///               by the output length; COMPLEX-typed.
///
/// @throws  numkit::Error  if `dim` is outside `{0, 1, 2, 3}`, or the
///                         request would extend a singleton axis.
///
/// @code
/// Value X = fft({1.0, 2.0, 3.0, 4.0});       // 4-pt FFT
/// Value Y = fft(x, 8);                       // zero-pad to 8 samples
/// Value Z = fft(matrix3d, -1, 2);            // transform along columns
/// @endcode
///
/// @see ifft, fft2, fftn, czt
Value fft(const Value &                x,
          int                          n   = -1,
          int                          dim = 0,
          std::pmr::memory_resource *  mr  = nullptr);

/// 1-D inverse discrete Fourier transform along a given dimension.
///
/// Same parameter semantics as `fft`. The result may downgrade to
/// DOUBLE when the spectrum is conjugate-symmetric (imag ≤ 1e-10).
///
/// @param X    Input spectrum (real or complex).
/// @param n    Output length, `-1` keeps input length.
/// @param dim  Transform axis (see `fft`).
/// @param mr   Memory resource (nullptr → process default).
/// @return     Inverse-transformed Value.
///
/// @see fft
Value ifft(const Value &                X,
           int                          n   = -1,
           int                          dim = 0,
           std::pmr::memory_resource *  mr  = nullptr);

/// Inverse DFT with conjugate-symmetric input (MATLAB's `ifft(X,...,
/// 'symmetric')`). Treats `X` as conjugate-symmetric along `dim`: the lower
/// half `X[0..floor(L/2)]` is authoritative (the DC and, for even `L`, the
/// Nyquist bin are forced real), the upper half is reconstructed as
/// `conj(X[k])`, and the inverse transform is returned as an exactly real
/// (DOUBLE) result. Differs from `real(ifft(X))`, which instead averages the
/// conjugate-symmetric part of `X`.
///
/// @param X    Input spectrum (real or complex), vector or matrix.
/// @param n    Output length, `-1` keeps input length (pads/truncates first).
/// @param dim  Transform axis (`0` = first non-singleton).
/// @param mr   Memory resource (nullptr → process default).
/// @return     Real (DOUBLE) inverse transform.
///
/// @see ifft
Value ifftSymmetric(const Value &                X,
                    int                          n   = -1,
                    int                          dim = 0,
                    std::pmr::memory_resource *  mr  = nullptr);

/// 2-D forward FFT.
///
/// Equivalent to `fft(fft(X, m, 1), n, 2)`. `m == -1` and / or `n == -1`
/// keep the corresponding dimension at its current length.
///
/// @param X   2-D input (real or complex).
/// @param m   Number of rows for the FFT, `-1` keeps `size(X, 1)`.
/// @param n   Number of columns, `-1` keeps `size(X, 2)`.
/// @param mr  Memory resource (nullptr → process default).
/// @return    m × n COMPLEX matrix.
///
/// @see ifft2, fftn
Value fft2(const Value &                X,
           int                          m  = -1,
           int                          n  = -1,
           std::pmr::memory_resource *  mr = nullptr);

/// @brief 2-D inverse FFT (`Y = ifft2(X, m, n)`).
///
/// Inverse of @ref fft2; same shape semantics. May downgrade output
/// to DOUBLE when the result is real within tolerance (imag ≤ 1e-10).
///
/// @param X   2-D input spectrum.
/// @param m   Number of rows, `-1` keeps `size(X, 1)`.
/// @param n   Number of columns, `-1` keeps `size(X, 2)`.
/// @param mr  Memory resource (nullptr → process default).
/// @return    `m × n` array (COMPLEX or DOUBLE).
/// @see fft2, ifft, ifftn
Value ifft2(const Value &X, int m = -1, int n = -1,
            std::pmr::memory_resource *mr = nullptr);

/// 2-D inverse FFT with conjugate-symmetric input (MATLAB's
/// `ifft2(X,'symmetric')`). Treats `X` as conjugate-symmetric so the inverse
/// transform is exactly real (DOUBLE). Decomposes into the 1-D
/// @ref ifftSymmetric over each dimension of length > 1. 2-D only; the resize
/// form `ifft2(X,m,n,'symmetric')` is a deferred gap.
///
/// @param X   2-D input spectrum (real or complex).
/// @param mr  Memory resource (nullptr → process default).
/// @return    Real (DOUBLE) 2-D inverse transform.
/// @see ifft2, ifftSymmetric
Value ifft2Symmetric(const Value &X, std::pmr::memory_resource *mr = nullptr);

/// @brief N-D forward FFT.
///
/// Call forms:
/// - `fftn(X)`     — FFT along every dimension of X at its current
///   length.
/// - `fftn(X, sz)` — same, but axis `k` is zero-padded or truncated
///   to `sz[k]` before its FFT. `sz.size()` must be ≤ `ndims(X)`.
///
/// Implemented as `fft` along each axis in turn (axis order does
/// not affect the result).
///
/// @param X    N-D input.
/// @param sz   Per-axis target sizes (empty Span → use X's current
///             shape).
/// @param mr   Memory resource (nullptr → process default).
/// @return     Same-shape (or `sz`-shape) COMPLEX array.
/// @see fft, fft2, ifftn
Value fftn(const Value &              X,
           Span<const std::size_t>    sz = {},
           std::pmr::memory_resource *mr = nullptr);

/// @brief N-D inverse FFT (`Y = ifftn(X, sz)`).
///
/// Inverse of @ref fftn; same shape semantics. May downgrade to
/// DOUBLE when `imag(Y) ≤ 1e-10` everywhere.
///
/// @param X    N-D input spectrum.
/// @param sz   Per-axis target sizes (empty Span → use X's current
///             shape).
/// @param mr   Memory resource (nullptr → process default).
/// @return     Same-shape (or `sz`-shape) array (COMPLEX or DOUBLE).
/// @see fftn, ifft, ifft2
Value ifftn(const Value &              X,
            Span<const std::size_t>    sz = {},
            std::pmr::memory_resource *mr = nullptr);

/// Chirp Z-transform (Bluestein).
///
/// Computes
/// \f$ Y[k] = \sum_{n=0}^{N-1} x[n] \cdot a^{-n} \cdot w^{n k} \f$
/// for k = 0..m-1. Generalises `fft` to arbitrary contour points on the
/// z-plane (zoom FFT, fractional-rate FFT, etc.).
///
/// Defaults that recover `fft`:
///   * m = N (input length)
///   * w = `exp(-2πj / m)`
///   * a = 1.0
///
/// Algorithm: Bluestein decomposition `n·k = (n² + k² − (k−n)²)/2`
/// converts the sum into a convolution `g ⋆ h` evaluated via an FFT of
/// length `L = nextPow2(N + m − 1)`.
///
/// For 2-D input, transforms each column independently.
/// 3-D input is not supported.
///
/// @param x   Input (real or complex), 1-D or 2-D.
/// @param m   Number of output points.
/// @param w   Ratio between consecutive contour points (complex).
/// @param a   Starting contour point (complex).
/// @param mr  Memory resource (nullptr → process default).
/// @return    m-length (or m × Ncols) COMPLEX result.
///
/// @code
/// // Zoom around a narrow band [f1, f2] of an N-sample sequence,
/// // sampling m points:
/// Complex w = std::polar(1.0, -2 * M_PI * (f2 - f1) / m);
/// Complex a = std::polar(1.0, -2 * M_PI * f1);
/// Value Y = czt(x, m, w, a);
/// @endcode
Value czt(const Value &                x,
          int                          m,
          Complex                      w,
          Complex                      a,
          std::pmr::memory_resource *  mr = nullptr);

/// FFT-based band-limited interpolation of `x` to `n` equispaced samples.
///
/// Algorithm: forward FFT → zero-pad in the frequency domain → inverse
/// FFT → scale by `n / m`. The result is the trigonometric interpolant
/// of `x` evaluated at n uniform points.
///
/// @param x    Input signal.
/// @param n    Number of output samples along `dim`.
/// @param dim  Axis to interpolate along. `0` → first non-singleton.
/// @param mr   Memory resource (nullptr → process default).
/// @return     Same shape as `x` with `dim` axis replaced by length `n`.
///
/// @see fft
Value interpft(const Value &                x,
               int                          n,
               int                          dim = 0,
               std::pmr::memory_resource *  mr  = nullptr);

} // namespace numkit::signal
