// libs/signal/include/numkit/signal/transforms/fft.hpp
#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

namespace numkit::signal {

/// 1D discrete Fourier transform along a given dimension.
///
/// Mirrors MATLAB's `fft`:
///   fft(x)         — along first non-singleton dimension
///   fft(x, n)      — zero-pad or truncate to length n
///   fft(x, [], k)  — along dimension k (1 = rows, 2 = cols, 3 = pages)
///   fft(x, n,  k)
///
/// @param mr  memory_resource for the output Value and any intermediate buffers.
/// @param x      Input — real or complex, 1-D / 2-D / 3-D. Empty → returns empty.
/// @param n      Output length per transform. -1 (default) keeps input length.
///               n > input length → zero-pad; n < input → truncate.
/// @param dim    Axis to transform along. 1 = along rows, 2 = along cols,
///               3 = along pages. 0 (default) means "first non-singleton
///               dimension" — matches MATLAB's `fft(x)` behaviour so a
///               row vector stays a row vector without explicit dim.
/// @return       Value of the same shape as x with the chosen axis replaced
///               by the output length. Forward FFT is always complex-typed;
///               inverse FFT may downgrade to real when the imaginary part
///               is within 1e-10 everywhere.
/// @throws       Error on dim outside {0, 1, 2, 3}, or when the requested
///               transform would extend dimensionality (axis length 1, n > 1).
Value fft(std::pmr::memory_resource *mr, const Value &x, int n = -1, int dim = 0);

/// 1D inverse DFT along a given dimension. Same parameter semantics as `fft`.
Value ifft(std::pmr::memory_resource *mr, const Value &X, int n = -1, int dim = 0);

/// 2-D DFT. fft2(X) ≡ fft(fft(X, m, 1), n, 2). m = -1 / n = -1 use size(X).
Value fft2(std::pmr::memory_resource *mr, const Value &X, int m = -1, int n = -1);

/// 2-D inverse DFT. Same shape semantics as fft2.
Value ifft2(std::pmr::memory_resource *mr, const Value &X, int m = -1, int n = -1);

/// N-D forward FFT. Mirrors MATLAB `fftn`:
///   fftn(X)        — FFT along every dimension of X using each axis's
///                    current length.
///   fftn(X, sz)    — same, but axis k is zero-padded or truncated to
///                    sz[k-1] before its FFT. `sz` length must not
///                    exceed ndims(X).
/// Implemented as fft along each axis in turn (commutes, like
/// MATLAB / NumPy / SciPy). Output is always complex-typed.
/// `sz` is passed as a pointer + length; pass `nullptr` / 0 for the
/// no-override form.
Value fftn(std::pmr::memory_resource *mr, const Value &X,
           const std::size_t *sz = nullptr, std::size_t szLen = 0);

/// N-D inverse FFT. Same shape semantics as `fftn`. May downgrade to
/// real output if the imaginary part is within 1e-10 everywhere.
Value ifftn(std::pmr::memory_resource *mr, const Value &X,
            const std::size_t *sz = nullptr, std::size_t szLen = 0);

/// czt(x, m, w, a) — discrete chirp Z-transform.
/// Computes Y[k] = Σ_{n=0..N-1} x[n] · a^(-n) · w^(n·k) for k=0..m-1.
///
/// Defaults match MATLAB:
///   m = N (input length)
///   w = exp(-2·π·j / m)
///   a = 1
///   → czt(x) ≡ fft(x), czt(x, m) ≡ fft(x, m)
///
/// Algorithm: Bluestein decomposition n·k = (n² + k² − (k−n)²)/2 →
/// turns the chirp-z sum into a convolution g ⋆ h evaluated via FFT
/// of length L = nextPow2(N + m − 1).
///
/// For 2-D input, transforms each column independently (MATLAB
/// semantics). 3-D not supported (matches existing fft policy on
/// transforms with explicit length args).
Value czt(std::pmr::memory_resource *mr, const Value &x,
          int m, Complex w, Complex a);

/// interpft(x, n[, dim]) — band-limited (FFT-based) interpolation of `x`
/// to `n` equispaced samples along `dim`. dim=0 means "first non-singleton".
/// Implementation: FFT → zero-pad in frequency domain → IFFT → scale by n/m.
Value interpft(std::pmr::memory_resource *mr, const Value &x, int n, int dim = 0);

} // namespace numkit::signal
