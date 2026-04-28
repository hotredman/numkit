// libs/dsp/include/numkit/m/dsp/MDspSos.hpp
//
// Second-order sections (SOS) — biquad cascade representation of IIR
// filters. Standard MATLAB triple:
//
//   y = sosfilt(sos, x)         — apply biquad cascade to a signal
//   sos = tf2sos(b, a)          — convert (b, a) → SOS matrix
//   sos = zp2sos(z, p, k)       — convert zeros / poles / gain → SOS
//
// SOS matrix layout: L rows × 6 columns. Each row is one biquad
// section [b0 b1 b2 a0 a1 a2] for H(z) = (b0 + b1·z⁻¹ + b2·z⁻²) /
// (a0 + a1·z⁻¹ + a2·z⁻²). a0 is typically 1.

#pragma once

#include <numkit/m/core/MAllocator.hpp>
#include <numkit/m/core/MValue.hpp>

#include <tuple>

namespace numkit::m::dsp {

/// Apply an SOS cascade. `sos` is L×6, `x` is a vector or 2D matrix
/// (columns are filtered independently). Returns y of the same shape
/// as x.
MValue sosfilt(Allocator &alloc, const MValue &sos, const MValue &x);

/// Convert zeros / poles / gain → L×6 SOS matrix. Returns the matrix
/// only; the global gain is distributed across the sections (matches
/// MATLAB's default 1-output form).
MValue zp2sos(Allocator &alloc, const MValue &zeros, const MValue &poles, double gain);

/// Convert zeros / poles / gain → (sos, g). The two-output form keeps
/// the gain factored out (sos has no leading scale; final output is
/// g * cascade(sos)).
std::tuple<MValue, double>
zp2sosWithGain(Allocator &alloc, const MValue &zeros, const MValue &poles, double gain);

/// Convert transfer-function (b, a) → SOS matrix. Internally:
/// roots(b) → zeros, roots(a) → poles, gain = b(1) / a(1), then
/// dispatches to zp2sos.
MValue tf2sos(Allocator &alloc, const MValue &b, const MValue &a);

/// Two-output form of tf2sos.
std::tuple<MValue, double>
tf2sosWithGain(Allocator &alloc, const MValue &b, const MValue &a);

} // namespace numkit::m::dsp
