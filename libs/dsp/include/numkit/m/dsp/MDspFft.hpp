// libs/dsp/include/numkit/m/dsp/MDspFft.hpp
#pragma once

#include <numkit/m/core/MAllocator.hpp>
#include <numkit/m/core/MValue.hpp>

namespace numkit::m::dsp {

/// 1D discrete Fourier transform along a given dimension.
///
/// Mirrors MATLAB's `fft`:
///   fft(x)         — along first non-singleton dimension
///   fft(x, n)      — zero-pad or truncate to length n
///   fft(x, [], k)  — along dimension k (k=1 columns, k=2 rows)
///   fft(x, n,  k)
///
/// @param alloc  Allocator for the output MValue and any intermediate buffers
///               allocated via its pmr bridge.
/// @param x      Input — real or complex, vector or matrix. Empty → returns empty.
/// @param n      Output length per transform. -1 (default) keeps input length.
///               n > input length → zero-pad; n < input → truncate.
/// @param dim    Dimension to transform along. 1 = columns (default), 2 = rows.
/// @return       Complex-typed MValue of shape determined by n and dim.
/// @throws       MError on invalid dim.
MValue fft(Allocator &alloc, const MValue &x, int n = -1, int dim = 1);

/// 1D inverse DFT along a given dimension. Same parameter semantics as `fft`.
///
/// When the IFFT result is real-valued (within numerical tolerance), returns a
/// real-typed MValue; otherwise returns complex-typed. Matches MATLAB's `ifft`
/// "symmetric-input → real-output" heuristic.
MValue ifft(Allocator &alloc, const MValue &X, int n = -1, int dim = 1);

} // namespace numkit::m::dsp
