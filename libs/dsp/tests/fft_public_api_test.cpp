// libs/dsp/tests/fft_public_api_test.cpp
//
// Direct-call tests for the public C++ API of numkit::m::dsp::fft / ifft.
// These exercise the algorithm without going through Engine, Parser, VM, or
// the registration adapter — so any breakage here is in the math, not the
// MATLAB calling glue. The Engine-routed tests live alongside in
// libs/dsp/tests/signal_core_test.cpp and remain in place.

#include <numkit/m/core/MAllocator.hpp>
#include <numkit/m/core/MTypes.hpp>  // MError
#include <numkit/m/core/MValue.hpp>
#include <numkit/m/dsp/MDspFft.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <complex>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using numkit::m::Allocator;
using numkit::m::MType;
using numkit::m::MValue;
using Complex = std::complex<double>;

namespace {

MValue makeRealRow(Allocator &alloc, std::initializer_list<double> vals)
{
    auto v = MValue::matrix(1, vals.size(), MType::DOUBLE, &alloc);
    double *data = v.doubleDataMut();
    size_t i = 0;
    for (double x : vals)
        data[i++] = x;
    return v;
}

} // namespace

// ── Basic round-trip: x -> fft -> ifft ≈ x ──────────────────────────────
TEST(DspFftPublicApi, RoundTripRealVector)
{
    Allocator alloc = Allocator::defaultAllocator();
    MValue x = makeRealRow(alloc, {1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0});

    MValue X = numkit::m::dsp::fft(alloc, x);
    MValue y = numkit::m::dsp::ifft(alloc, X);

    ASSERT_EQ(y.numel(), x.numel());
    const double *xData = x.doubleData();
    // ifft should restore the real vector within numerical tolerance
    if (y.isComplex()) {
        const Complex *yData = y.complexData();
        for (size_t i = 0; i < y.numel(); ++i)
            EXPECT_NEAR(yData[i].real(), xData[i], 1e-10) << "at i=" << i;
    } else {
        const double *yData = y.doubleData();
        for (size_t i = 0; i < y.numel(); ++i)
            EXPECT_NEAR(yData[i], xData[i], 1e-10) << "at i=" << i;
    }
}

// ── DC of a constant vector is N, everything else is ~0 ────────────────
TEST(DspFftPublicApi, DcBinOfConstant)
{
    Allocator alloc = Allocator::defaultAllocator();
    MValue x = makeRealRow(alloc, {1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0});

    MValue X = numkit::m::dsp::fft(alloc, x);
    ASSERT_TRUE(X.isComplex());
    const Complex *Xd = X.complexData();
    EXPECT_NEAR(Xd[0].real(), 8.0, 1e-10);  // DC = sum = N
    EXPECT_NEAR(Xd[0].imag(), 0.0, 1e-10);
    for (size_t i = 1; i < X.numel(); ++i) {
        EXPECT_NEAR(std::abs(Xd[i]), 0.0, 1e-10) << "at i=" << i;
    }
}

// ── Single-frequency cosine: peak at known bin ─────────────────────────
TEST(DspFftPublicApi, CosinePeakBin)
{
    Allocator alloc = Allocator::defaultAllocator();
    constexpr size_t N = 16;
    constexpr int k = 3;  // frequency bin to test
    auto x = MValue::matrix(1, N, MType::DOUBLE, &alloc);
    double *xd = x.doubleDataMut();
    for (size_t i = 0; i < N; ++i)
        xd[i] = std::cos(2.0 * M_PI * k * i / static_cast<double>(N));

    MValue X = numkit::m::dsp::fft(alloc, x);
    const Complex *Xd = X.complexData();
    // Peak of a real cosine at bin k should be N/2 in magnitude at both k and N-k
    EXPECT_NEAR(std::abs(Xd[k]), N / 2.0, 1e-9);
    EXPECT_NEAR(std::abs(Xd[N - k]), N / 2.0, 1e-9);
}

// ── n argument: zero-pad extends output length ─────────────────────────
TEST(DspFftPublicApi, ZeroPadExtendsOutput)
{
    Allocator alloc = Allocator::defaultAllocator();
    MValue x = makeRealRow(alloc, {1.0, 2.0, 3.0, 4.0});

    MValue X = numkit::m::dsp::fft(alloc, x, /*n=*/8);
    EXPECT_EQ(X.numel(), 8u);
}

// ── n argument: truncate shortens output ───────────────────────────────
TEST(DspFftPublicApi, TruncateShortensOutput)
{
    Allocator alloc = Allocator::defaultAllocator();
    MValue x = makeRealRow(alloc, {1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0});

    MValue X = numkit::m::dsp::fft(alloc, x, /*n=*/4);
    EXPECT_EQ(X.numel(), 4u);
}

// ── Invalid dim throws MError ──────────────────────────────────────────
// 0 now means "first non-singleton" (valid); 4+ and negative are invalid.
TEST(DspFftPublicApi, InvalidDimThrows)
{
    Allocator alloc = Allocator::defaultAllocator();
    MValue x = makeRealRow(alloc, {1.0, 2.0, 3.0, 4.0});

    EXPECT_THROW(numkit::m::dsp::fft(alloc, x, /*n=*/-1, /*dim=*/4),
                 numkit::m::MError);
    EXPECT_THROW(numkit::m::dsp::fft(alloc, x, /*n=*/-1, /*dim=*/-1),
                 numkit::m::MError);
    EXPECT_THROW(numkit::m::dsp::ifft(alloc, x, /*n=*/-1, /*dim=*/99),
                 numkit::m::MError);
}

// ── fft(x, [], dim) on a singleton axis is identity ────────────────────
TEST(DspFftPublicApi, Dim3OnVectorIsIdentity)
{
    Allocator alloc = Allocator::defaultAllocator();
    MValue x = makeRealRow(alloc, {1.0, 2.0, 3.0, 4.0});
    // dim=3 on a 1x4 row vector: the page axis has length 1, so the
    // per-slice FFT is of length 1 (identity). Result shape matches input.
    MValue X = numkit::m::dsp::fft(alloc, x, /*n=*/-1, /*dim=*/3);
    EXPECT_EQ(X.dims().rows(), 1u);
    EXPECT_EQ(X.dims().cols(), 4u);
    ASSERT_EQ(X.numel(), 4u);
    // Each "page slice" has length 1, so FFT is identity (forward
    // keeps complex; values reproduce the input).
    ASSERT_TRUE(X.isComplex());
    for (size_t i = 0; i < 4; ++i) {
        EXPECT_NEAR(X.complexData()[i].real(), double(i + 1), 1e-12);
        EXPECT_NEAR(X.complexData()[i].imag(), 0.0,           1e-12);
    }
}

// ── dim=0 explicitly picks first non-singleton (auto) ──────────────────
TEST(DspFftPublicApi, Dim0AutoOnRowVector)
{
    Allocator alloc = Allocator::defaultAllocator();
    MValue x = makeRealRow(alloc, {1.0, 1.0, 1.0, 1.0});
    MValue X = numkit::m::dsp::fft(alloc, x, /*n=*/-1, /*dim=*/0);
    // Row vector: non-singleton is cols — FFT along the row → DC = 4.
    ASSERT_TRUE(X.isComplex());
    EXPECT_NEAR(X.complexData()[0].real(), 4.0, 1e-10);
    for (size_t i = 1; i < 4; ++i)
        EXPECT_NEAR(std::abs(X.complexData()[i]), 0.0, 1e-10);
}

// ── 3-D input, dim=1: FFT along rows for every (col, page) slice ───────
TEST(DspFftPublicApi, Fft3DDim1PreservesShape)
{
    Allocator alloc = Allocator::defaultAllocator();
    constexpr size_t R = 4, C = 3, P = 2;
    MValue x = MValue::matrix3d(R, C, P, MType::DOUBLE, &alloc);
    for (size_t i = 0; i < R * C * P; ++i)
        x.doubleDataMut()[i] = double(i + 1);

    MValue X = numkit::m::dsp::fft(alloc, x, /*n=*/-1, /*dim=*/1);
    ASSERT_TRUE(X.dims().is3D());
    EXPECT_EQ(X.dims().rows(), R);
    EXPECT_EQ(X.dims().cols(), C);
    EXPECT_EQ(X.dims().pages(), P);
    EXPECT_EQ(X.numel(), R * C * P);

    // Spot-check: the DC bin of each column-per-page is the sum of that
    // column's input values (since FFT bin 0 = Σ x[k]).
    ASSERT_TRUE(X.isComplex());
    const Complex *Xd = X.complexData();
    for (size_t p = 0; p < P; ++p)
        for (size_t c = 0; c < C; ++c) {
            double sum = 0.0;
            for (size_t r = 0; r < R; ++r)
                sum += x.doubleData()[c * R + p * R * C + r];
            Complex dcBin = Xd[c * R + p * R * C];  // row 0
            EXPECT_NEAR(dcBin.real(), sum, 1e-10);
            EXPECT_NEAR(dcBin.imag(), 0.0, 1e-10);
        }
}

// ── 3-D input, dim=2: FFT along cols for every (row, page) slice ───────
TEST(DspFftPublicApi, Fft3DDim2PreservesShape)
{
    Allocator alloc = Allocator::defaultAllocator();
    constexpr size_t R = 4, C = 4, P = 2;
    MValue x = MValue::matrix3d(R, C, P, MType::DOUBLE, &alloc);
    for (size_t i = 0; i < R * C * P; ++i)
        x.doubleDataMut()[i] = double(i + 1);

    MValue X = numkit::m::dsp::fft(alloc, x, /*n=*/-1, /*dim=*/2);
    ASSERT_TRUE(X.dims().is3D());
    EXPECT_EQ(X.dims().rows(), R);
    EXPECT_EQ(X.dims().cols(), C);
    EXPECT_EQ(X.dims().pages(), P);

    // DC bin of each row-per-page: sum of row values.
    ASSERT_TRUE(X.isComplex());
    const Complex *Xd = X.complexData();
    for (size_t p = 0; p < P; ++p)
        for (size_t r = 0; r < R; ++r) {
            double sum = 0.0;
            for (size_t c = 0; c < C; ++c)
                sum += x.doubleData()[c * R + p * R * C + r];
            Complex dcBin = Xd[r + p * R * C];  // col 0 at this (row, page)
            EXPECT_NEAR(dcBin.real(), sum, 1e-10);
            EXPECT_NEAR(dcBin.imag(), 0.0, 1e-10);
        }
}

// ── 3-D input, dim=3: FFT along pages for every (row, col) slice ───────
TEST(DspFftPublicApi, Fft3DDim3PreservesShape)
{
    Allocator alloc = Allocator::defaultAllocator();
    constexpr size_t R = 3, C = 2, P = 4;
    MValue x = MValue::matrix3d(R, C, P, MType::DOUBLE, &alloc);
    for (size_t i = 0; i < R * C * P; ++i)
        x.doubleDataMut()[i] = double(i + 1);

    MValue X = numkit::m::dsp::fft(alloc, x, /*n=*/-1, /*dim=*/3);
    ASSERT_TRUE(X.dims().is3D());
    EXPECT_EQ(X.dims().rows(), R);
    EXPECT_EQ(X.dims().cols(), C);
    EXPECT_EQ(X.dims().pages(), P);

    // DC bin of each page-per-(row,col): sum across pages.
    ASSERT_TRUE(X.isComplex());
    const Complex *Xd = X.complexData();
    for (size_t c = 0; c < C; ++c)
        for (size_t r = 0; r < R; ++r) {
            double sum = 0.0;
            for (size_t p = 0; p < P; ++p)
                sum += x.doubleData()[r + c * R + p * R * C];
            Complex dcBin = Xd[r + c * R];  // page 0
            EXPECT_NEAR(dcBin.real(), sum, 1e-10);
            EXPECT_NEAR(dcBin.imag(), 0.0, 1e-10);
        }
}

// ── Round-trip: x → fft → ifft ≈ x for 3-D input on each dim ──────────
TEST(DspFftPublicApi, RoundTrip3DOnEachDim)
{
    Allocator alloc = Allocator::defaultAllocator();
    constexpr size_t R = 4, C = 2, P = 8;
    MValue x = MValue::matrix3d(R, C, P, MType::DOUBLE, &alloc);
    for (size_t i = 0; i < R * C * P; ++i)
        x.doubleDataMut()[i] = std::sin(0.37 * double(i));

    for (int dim = 1; dim <= 3; ++dim) {
        MValue X = numkit::m::dsp::fft(alloc, x, /*n=*/-1, dim);
        MValue y = numkit::m::dsp::ifft(alloc, X, /*n=*/-1, dim);
        ASSERT_EQ(y.numel(), R * C * P) << "dim=" << dim;
        ASSERT_TRUE(y.dims().is3D()) << "dim=" << dim;
        // ifft should downgrade to real (imag < 1e-10 everywhere).
        for (size_t i = 0; i < y.numel(); ++i) {
            double ref = x.doubleData()[i];
            double got = y.isComplex() ? y.complexData()[i].real()
                                       : y.doubleData()[i];
            EXPECT_NEAR(got, ref, 1e-10)
                << "dim=" << dim << " at i=" << i;
        }
    }
}
