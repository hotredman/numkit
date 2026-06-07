// libs/image/src/color/color_extras_detail.hpp
//
// Private (src-only) declarations shared between the engine-free compute in
// color_extras.cpp and its CallContext register half in color_extras_reg.cpp.
// NOT part of the public image API. The colormap/label2rgb cluster these
// belong to was a single in-file anonymous namespace; it is promoted to
// numkit::image scope (external linkage) so the register TU can reuse it
// without duplicating the colormap tables.
//
// Phase 2b compute/register split — see project_layering_refactor memory.
#pragma once

#include <numkit/value/value.hpp>

#include <memory_resource>
#include <string>

namespace numkit::image {

// MATLAB-canonical jet(N) colormap (N x 3, double in [0,1]).
Value jet_colormap(int m, std::pmr::memory_resource *mr);

// Dispatch a named MATLAB colormap (jet/hsv/parula/...). Throws on unknown.
Value resolve_named_colormap(const std::string &name, int N,
                             std::pmr::memory_resource *mr);

// Lower-case a string (ASCII).
std::string lower(const std::string &s);

// Parse a MATLAB ColorSpec (single letter or full name) into RGB in [0,1].
// Returns false if unrecognised.
bool parseColorSpec(const std::string &name, double rgb[3]);

} // namespace numkit::image
