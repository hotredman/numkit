// libs/signal/include/numkit/signal/transforms/fft.hpp
#pragma once

#include <numkit/core/allocator.hpp>
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
/// @param alloc  Allocator for the output Value and any intermediate buffers.
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
Value fft(Allocator &alloc, const Value &x, int n = -1, int dim = 0);

/// 1D inverse DFT along a given dimension. Same parameter semantics as `fft`.
Value ifft(Allocator &alloc, const Value &X, int n = -1, int dim = 0);

} // namespace numkit::signal
