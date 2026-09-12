// toolboxes/signal/src/digital_filtering/filter_detail.hpp
//
// Private (src-only) helpers shared between the engine-free compute in
// filter.cpp and its CallContext register half in filter_reg.cpp. NOT part of
// the public signal API (library_api.md forbids raw double*/Complex* on the
// public surface). The register wrapper reuses these to honour the zi/zf
// initial-condition forms of filter().
//
// The raw-buffer IIR recurrence kernels (applyFilterDf2t / ...Complex) moved to
// the kernel layer — nk::ops::applyFilterDf2t{,Complex} (nk/ops/
// iir_filter.hpp). They are re-exported here as signal-scope aliases so the
// existing unqualified call sites (filter.cpp, filtfilt, filter_reg.cpp) need
// no churn. The Value-level marshalling helpers stay in signal.
//
// Phase 2b compute/register split — see project_layering_refactor memory.
#pragma once

#include <nk/ops/iir_filter.hpp>
#include <nk/value/value.hpp>
#include <nk/value/scratch.hpp>

#include <cstddef>
#include <memory_resource>

namespace nk::signal {

// IIR recurrence kernels now live in ops; re-export under the signal namespace.
using nk::ops::applyFilterDf2t;
using nk::ops::applyFilterDf2tComplex;

// Marshal a Value's first n elements into a complex scratch buffer.
ScratchVec<Complex> toComplexBuf(const Value &v, size_t n,
                                 std::pmr::memory_resource *mr);

// Length of x along its first non-singleton dimension.
size_t firstNonSingletonExtent(const Value &x);

} // namespace nk::signal
