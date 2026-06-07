// libs/image/src/geom/geom_detail.hpp
//
// Private (src-only) helpers shared between the engine-free compute in
// geom.cpp and its CallContext register half in geom_reg.cpp. NOT part of the
// public image API.
//
//   writeNative      — inline native-clipping element writer used by every
//                      resampler (compute) and by imrotate3 (register).
//   imresize3_core   — the lower-level 3-D resize entry (defined in geom.cpp),
//                      called both by imresize3() (compute) and imresize3_reg.
//
// Phase 2b compute/register split — see project_layering_refactor memory.
#pragma once

#include <numkit/value/value.hpp>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory_resource>
#include <string>

namespace numkit::image {

// Write a sampled value `v` (native units) into output element idx i, clipping
// integer types to their natural range.
inline void writeNative(Value &out, std::size_t i, double v, ValueType t) {
    switch (t) {
        case ValueType::DOUBLE:
            out.doubleDataMut()[i] = v; break;
        case ValueType::SINGLE:
            out.singleDataMut()[i] = static_cast<float>(v); break;
        case ValueType::UINT8: {
            if (v < 0.0) v = 0.0;
            if (v > 255.0) v = 255.0;
            out.uint8DataMut()[i] = static_cast<std::uint8_t>(std::lround(v));
            break;
        }
        case ValueType::UINT16: {
            if (v < 0.0) v = 0.0;
            if (v > 65535.0) v = 65535.0;
            out.uint16DataMut()[i] = static_cast<std::uint16_t>(std::lround(v));
            break;
        }
        case ValueType::INT16: {
            if (v < -32768.0) v = -32768.0;
            if (v > 32767.0)  v = 32767.0;
            out.int16DataMut()[i] = static_cast<std::int16_t>(std::lround(v));
            break;
        }
        case ValueType::LOGICAL:
            out.logicalDataMut()[i] = (v != 0.0) ? 1u : 0u; break;
        default:
            // Fall back: store as double (the buffer is large enough).
            out.doubleDataMut()[i] = v; break;
    }
}

// Lower-level 3-D resize entry (scale = outLen/inLen per axis). Defined in
// geom.cpp.
Value imresize3_core(const Value &V,
                     std::size_t outR, std::size_t outC, std::size_t outD,
                     double scaleY, double scaleX, double scaleZ,
                     const std::string &method, bool antialias,
                     std::pmr::memory_resource *mr);

} // namespace numkit::image
