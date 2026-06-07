// libs/image/src/io/io_detail.hpp
//
// Private (src-only) declarations shared between the engine-free compute in
// io.cpp and its CallContext register half in io_reg.cpp. NOT part of the
// public image API — both the native imread() and the VFS-backed imread_reg
// funnel pixel decoding through imreadFromBytes(), so it must be visible to
// both translation units.
//
// Phase 2b compute/register split — see project_layering_refactor memory.
#pragma once

#include <numkit/value/value.hpp>

#include <algorithm>
#include <cctype>
#include <memory_resource>
#include <string>

namespace numkit::image {

// Lower-cased file extension (without the dot), or "" if none. Used by both
// the native and VFS imread/imwrite paths to dispatch on format.
inline std::string lowerExt(const std::string &path) {
    auto dot = path.find_last_of('.');
    if (dot == std::string::npos) return {};
    std::string e = path.substr(dot + 1);
    std::transform(e.begin(), e.end(), e.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return e;
}

// True if the byte buffer carries a TIFF magic header (II*. or MM.*, classic
// or BigTIFF). stb_image can't decode TIFF, so callers dispatch on this.
bool isTiffBytes(const std::string &b);

// Decode an image from in-memory bytes — the single place pixels are produced.
Value imreadFromBytes(const std::string &bytes, std::pmr::memory_resource *mr);

} // namespace numkit::image
