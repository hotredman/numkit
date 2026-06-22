// ops/src/iir_filter.cpp
//
// Direct-Form-II-transposed IIR filter recurrence (real + complex), moved
// verbatim from signal's filter.cpp so the kernel lives in the kernel layer
// (ops, core-free). The toolbox keeps the Value-API (filter/filtfilt) and the
// Value-level marshalling helpers (toComplexBuf, firstNonSingletonExtent). The
// recurrence is sequential — no SIMD backend. See iir_filter.hpp.

#include <numkit/ops/iir_filter.hpp>

#include <algorithm>

namespace numkit::ops {

ScratchVec<double> applyFilterDf2t(const double *bn, std::size_t nb, const double *an,
                                   std::size_t na, const double *input, std::size_t len,
                                   std::pmr::memory_resource *mr, const double *zi,
                                   std::size_t ziLen, double *zfOut)
{
    const std::size_t  nfilt = std::max(nb, na);
    ScratchVec<double> out(len, mr);
    ScratchVec<double> z(nfilt, mr);
    for (std::size_t i = 0; i < nfilt; ++i)
        z[i] = (zi && i < ziLen) ? zi[i] : 0.0;
    for (std::size_t n = 0; n < len; ++n) {
        out[n] = (nb > 0 ? bn[0] : 0.0) * input[n] + z[0];
        for (std::size_t i = 1; i < nfilt; ++i) {
            z[i - 1] = (i < nb ? bn[i] : 0.0) * input[n]
                       - (i < na ? an[i] : 0.0) * out[n]
                       + (i < nfilt - 1 ? z[i] : 0.0);
        }
    }
    if (zfOut)
        for (std::size_t i = 0; i + 1 < nfilt; ++i) zfOut[i] = z[i];
    return out;
}

ScratchVec<Complex> applyFilterDf2tComplex(const Complex *bn, std::size_t nb, const Complex *an,
                                           std::size_t na, const Complex *input, std::size_t len,
                                           std::pmr::memory_resource *mr, const Complex *zi,
                                           std::size_t ziLen, Complex *zfOut)
{
    const std::size_t   nfilt = std::max(nb, na);
    ScratchVec<Complex> out(len, mr);
    ScratchVec<Complex> z(nfilt, mr);
    const Complex       zero(0.0, 0.0);
    for (std::size_t i = 0; i < nfilt; ++i)
        z[i] = (zi && i < ziLen) ? zi[i] : zero;
    for (std::size_t n = 0; n < len; ++n) {
        out[n] = (nb > 0 ? bn[0] : zero) * input[n] + z[0];
        for (std::size_t i = 1; i < nfilt; ++i) {
            z[i - 1] = (i < nb ? bn[i] : zero) * input[n]
                       - (i < na ? an[i] : zero) * out[n]
                       + (i < nfilt - 1 ? z[i] : zero);
        }
    }
    if (zfOut)
        for (std::size_t i = 0; i + 1 < nfilt; ++i) zfOut[i] = z[i];
    return out;
}

} // namespace numkit::ops
