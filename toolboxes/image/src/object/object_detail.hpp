// toolboxes/image/src/object/object_detail.hpp
//
// Private (src-only) declaration shared between the engine-free compute in
// object.cpp and its CallContext register half in object_reg.cpp. NOT part of
// the public image API — imgradient_mag is the magnitude-only gradient helper
// the imgradient register wrapper reuses.
//
// Phase 2b compute/register split — see project_layering_refactor memory.
#pragma once

#include <numkit/value/value.hpp>

#include <memory_resource>
#include <string>

namespace numkit::image {

// Gradient magnitude only (skips the per-pixel atan2 direction). Bit-identical
// to imgradient()'s first output.
Value imgradient_mag(const Value &I, const std::string &method,
                     std::pmr::memory_resource *mr);

} // namespace numkit::image
