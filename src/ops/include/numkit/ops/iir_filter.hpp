// ops/include/numkit/ops/iir_filter.hpp
//
// Direct-Form-II-transposed IIR filter recurrence — the numerical core of
// signal's filter()/filtfilt() (and the zi/zf initial-condition forms exposed
// through the register layer). Raw-buffer, core-free; the recurrence is
// inherently sequential (each output feeds the next), so there is no SIMD
// backend — the value of living in ops is dedup (one kernel for filter/filtfilt/
// the register half) plus a single native codegen-lowering target for filter().
//
// b/a are expected a0-normalised by the caller (the Value-API divides by a(1)).

#pragma once

#include <numkit/value/value.hpp>    // numkit::Complex
#include <numkit/value/scratch.hpp>  // ScratchVec

#include <cstddef>
#include <memory_resource>

namespace numkit::ops {

// Direct-form-II transposed IIR filter (real), applied to a flat input buffer.
// Optional `zi` (length ziLen) seeds the initial delay state; when `zfOut` is
// non-null the final state (length max(nb,na)-1) is written there — this backs
// MATLAB's filter(b,a,x,zi) and [y,zf] = filter(...).
ScratchVec<double> applyFilterDf2t(const double *bn, std::size_t nb, const double *an,
                                   std::size_t na, const double *input, std::size_t len,
                                   std::pmr::memory_resource *mr,
                                   const double *zi = nullptr, std::size_t ziLen = 0,
                                   double *zfOut = nullptr);

// Complex DF2T variant. filter() is bilinear (the recursive a-part mixes
// terms), so a real/imag split does not apply — the recurrence runs over
// Complex.
ScratchVec<Complex> applyFilterDf2tComplex(const Complex *bn, std::size_t nb,
                                           const Complex *an, std::size_t na,
                                           const Complex *input, std::size_t len,
                                           std::pmr::memory_resource *mr,
                                           const Complex *zi = nullptr,
                                           std::size_t ziLen = 0,
                                           Complex *zfOut = nullptr);

} // namespace numkit::ops
