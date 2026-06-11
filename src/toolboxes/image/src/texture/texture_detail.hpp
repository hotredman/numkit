// toolboxes/image/src/texture/texture_detail.hpp
//
// Private (src-only) helpers shared between the engine-free compute in
// texture.cpp and its CallContext register half in texture_reg.cpp. NOT part
// of the public image API — these are graycomatrix GrayLimits helpers the
// register wrapper needs when parsing the 'GrayLimits' name-value pair.
//
// Phase 2b compute/register split — see project_layering_refactor memory.
#pragma once

#include <numkit/value/value.hpp>

#include <cstddef>

namespace numkit::image {

// Default GrayLimits per MATLAB:
//   uint8 -> [0,255], uint16 -> [0,65535], int16 -> [-32768,32767],
//   logical -> [0,1], single/double -> [min(I), max(I)].
inline void default_gray_limits(const Value &I, double &lo, double &hi)
{
    switch (I.type()) {
        case ValueType::UINT8:   lo = 0.0;     hi = 255.0;    return;
        case ValueType::UINT16:  lo = 0.0;     hi = 65535.0;  return;
        case ValueType::INT16:   lo = -32768;  hi = 32767;    return;
        case ValueType::LOGICAL: lo = 0.0;     hi = 1.0;      return;
        default: break;
    }
    // single / double: scan for min/max.
    const std::size_t N = I.numel();
    if (N == 0) { lo = 0.0; hi = 1.0; return; }
    double mn = I.elemAsDouble(0);
    double mx = mn;
    for (std::size_t i = 1; i < N; ++i) {
        const double v = I.elemAsDouble(i);
        if (v < mn) mn = v;
        if (v > mx) mx = v;
    }
    lo = mn; hi = mx;
}

// Data-range limits = [min(I(:)) max(I(:))] over the ACTUAL pixel values,
// regardless of class. What MATLAB's graycomatrix uses for 'GrayLimits', [].
inline void data_gray_limits(const Value &I, double &lo, double &hi)
{
    const std::size_t N = I.numel();
    if (N == 0) { lo = 0.0; hi = 1.0; return; }
    double mn = I.elemAsDouble(0);
    double mx = mn;
    for (std::size_t i = 1; i < N; ++i) {
        const double v = I.elemAsDouble(i);
        if (v < mn) mn = v;
        if (v > mx) mx = v;
    }
    lo = mn; hi = mx;
}

} // namespace numkit::image
