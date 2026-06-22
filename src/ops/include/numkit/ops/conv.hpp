// ops/include/numkit/ops/conv.hpp
//
// Direct (time-domain) linear convolution of two real double sequences — the
// numerical FIR kernel behind signal's conv()/xcorr(). Raw-buffer, core-free;
// lives in the kernel layer so the VM/codegen can lower a conv() call to it and
// a SIMD backend can replace the inner loop without touching the toolbox.
//
// The FFT-based path (convFFT) stays in the signal toolbox for now: it is built
// on the signal-local radix-2 FFT helpers, not ops::fft, so it is not a verbatim
// move.

#pragma once

#include <numkit/value/scratch.hpp>

#include <cstddef>
#include <memory_resource>

namespace numkit::ops {

// Full linear convolution: out[k] = Σ_{i+j=k} a[i]·b[j], length na+nb-1.
// Both inputs are real double buffers; the result is returned on the arena `mr`.
// O(na·nb) — the caller (conv/xcorr) picks direct vs FFT by cost.
ScratchVec<double> convDirect(const double *a, std::size_t na,
                              const double *b, std::size_t nb,
                              std::pmr::memory_resource *mr);

} // namespace numkit::ops
