// toolboxes/signal/src/measurements/findpeaks_detail.hpp
//
// Private (src-only) helpers shared between the engine-free compute in
// findpeaks.cpp and its CallContext register half in findpeaks_reg.cpp. The
// register wrapper parses the Name-Value options into a PeakOpts and calls
// findpeaksImpl (a raw-buffer core LIBRARY_API.md keeps off the public
// surface — the public entry is findpeaks()). NOT part of the public API.
//
// Phase 2b compute/register split — see project_layering_refactor memory.
#pragma once

#include <numkit/value/value.hpp>

#include <cstddef>
#include <limits>
#include <memory_resource>
#include <tuple>

namespace numkit::signal {

// Parsed findpeaks options (filled by findpeaks_reg from Name-Value pairs).
struct PeakOpts {
    double minHeight  = -std::numeric_limits<double>::infinity(); // keep if v > minHeight
    double threshold  = 0.0;     // keep if min(v-left, v-right) >= threshold
    double minDist    = 0.0;     // in location units; <=0 => disabled
    bool   useMinDist = false;
    double minProm    = 0.0;     // MinPeakProminence: keep if prominence >= minProm
    bool   useMinProm = false;
    long   nPeaks     = -1;      // -1 => unlimited
    int    sortMode   = 0;       // 0 none(location), 1 ascend(height), 2 descend(height)
};

// Core detection + filtering. locArr == nullptr => locations are 1-based
// sample indices; otherwise locArr[i] is the location of sample i.
// Returns (heights, locations, widths, prominences).
std::tuple<Value, Value, Value, Value>
findpeaksImpl(const Value &x, const double *locArr, const PeakOpts &opt,
              std::pmr::memory_resource *mr);

} // namespace numkit::signal
