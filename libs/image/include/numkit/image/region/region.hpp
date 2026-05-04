// libs/image/include/numkit/image/region/region.hpp
//
// Connected-component labelling and basic region descriptors.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

#include <tuple>

namespace numkit::image {

/// bwlabel(BW[, conn]) — label connected components of a binary image.
/// `conn` ∈ {4, 8} (default 8). Returns (L, num) where L is uint16
/// label image and num is the count of components.
std::tuple<Value, Value>
bwlabel(std::pmr::memory_resource *mr, const Value &BW, int conn);

/// bwconncomp(BW[, conn]) — like bwlabel but returns a struct-like
/// 4-tuple (Connectivity, ImageSize, NumObjects, PixelIdxList). Since
/// numkit avoids OOП, we expose this as a 4-output function and
/// callers can destructure.
std::tuple<Value, Value, Value, Value>
bwconncomp(std::pmr::memory_resource *mr, const Value &BW, int conn);

/// bwarea(BW) — total area (number of foreground pixels with optional
/// quarter-pixel boundary correction). For first cut, returns the
/// integer pixel count.
Value bwarea(std::pmr::memory_resource *mr, const Value &BW);

/// bwperim(BW[, conn]) — perimeter mask. Foreground pixel survives iff
/// at least one of its `conn`-neighbours is background.
Value bwperim(std::pmr::memory_resource *mr, const Value &BW, int conn);

/// bwareaopen(BW, P[, conn]) — remove components with fewer than P
/// pixels.
Value bwareaopen(std::pmr::memory_resource *mr, const Value &BW, int P, int conn);

} // namespace numkit::image
