// toolboxes/signal/src/digital_filtering/filter_detail.hpp
//
// Private (src-only) raw-buffer kernels shared between the engine-free compute
// in filter.cpp and its CallContext register half in filter_reg.cpp. NOT part
// of the public signal API (LIBRARY_API.md forbids raw double*/Complex* on the
// public surface). The register wrapper reuses these to honour the zi/zf
// initial-condition forms of filter().
//
// Phase 2b compute/register split — see project_layering_refactor memory.
#pragma once

#include <numkit/value/value.hpp>
#include <numkit/value/scratch.hpp>

#include <cstddef>
#include <memory_resource>

namespace numkit::signal {

// Direct-form-II transposed IIR filter (real), optional zi state + zf out.
ScratchVec<double> applyFilterDf2t(const double *bn, size_t nb, const double *an,
                                   size_t na, const double *input, size_t len,
                                   std::pmr::memory_resource *mr,
                                   const double *zi = nullptr, size_t ziLen = 0,
                                   double *zfOut = nullptr);

// Complex DF2T variant.
ScratchVec<Complex> applyFilterDf2tComplex(const Complex *bn, size_t nb,
                                           const Complex *an, size_t na,
                                           const Complex *input, size_t len,
                                           std::pmr::memory_resource *mr,
                                           const Complex *zi = nullptr,
                                           size_t ziLen = 0,
                                           Complex *zfOut = nullptr);

// Marshal a Value's first n elements into a complex scratch buffer.
ScratchVec<Complex> toComplexBuf(const Value &v, size_t n,
                                 std::pmr::memory_resource *mr);

// Length of x along its first non-singleton dimension.
size_t firstNonSingletonExtent(const Value &x);

} // namespace numkit::signal
