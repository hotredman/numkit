// libs/dsp/include/numkit/m/dsp/MDspSgolay.hpp
//
// Savitzky-Golay smoothing filter family.

#pragma once

#include <numkit/m/core/MAllocator.hpp>
#include <numkit/m/core/MValue.hpp>

namespace numkit::m::dsp {

/// sgolay(order, framelen) — Savitzky-Golay filter projection matrix.
/// Returns a (framelen × framelen) DOUBLE matrix B where row r contains
/// the filter coefficients producing the polynomial-fit estimate at
/// the r-th sample of a length-framelen window. Row floor(framelen/2)
/// is the central (symmetric) filter; the other rows are used at the
/// signal edges.
///
/// Constraints (MATLAB-compatible):
///   * framelen must be odd and ≥ order + 1.
///   * 0 ≤ order < framelen.
MValue sgolay(Allocator &alloc, int order, int framelen);

/// sgolayfilt(x, order, framelen) — apply Savitzky-Golay smoothing to
/// a 1-D signal. Interior samples use the central row of sgolay()'s
/// projection matrix; edge samples (where a symmetric window can't
/// fit) use the asymmetric rows. Output has the same length and shape
/// as x.
MValue sgolayfilt(Allocator &alloc, const MValue &x, int order, int framelen);

} // namespace numkit::m::dsp
