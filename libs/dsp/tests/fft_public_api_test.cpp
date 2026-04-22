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

// Private kernel-dispatch headers, exposed to this test target via
// libs/dsp/tests/CMakeLists adding libs/dsp as a private include path.
// Used by the DspFftStockham parity tests below to call the radix-2
// + Stockham kernels directly (without going through the public
// dsp::fft wrapper).
#include "src/backends/FftKernels.hpp"
#include "src/MDspHelpers.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <complex>
#include <vector>

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

// ── Radix-4 path correctness at large pow-of-4 sizes ──────────────────
//
// kRadix4Threshold inside MDspFft_simd.cpp is currently 1<<15 (32768);
// pow-of-4 sizes ≥ that route to fftRadix4Pow4Dispatch. Other tests use
// small N (≤ 16) and never trigger r4. This test guards r4 against
// bitrot — if its threshold is later raised to disable r4, the test
// stops exercising the kernel but doesn't fail (it goes through r2),
// so the assertion is structural (DC bin, energy-preservation), not
// algorithm-specific.
TEST(DspFftPublicApi, Radix4PathPowerOfFourSize)
{
    Allocator alloc = Allocator::defaultAllocator();
    constexpr size_t N = 65536;  // = 4^8, well above current r4 threshold

    auto x = MValue::matrix(N, 1, MType::DOUBLE, &alloc);
    double *xd = x.doubleDataMut();
    double sumX = 0.0, sumX2 = 0.0;
    for (size_t i = 0; i < N; ++i) {
        xd[i] = std::sin(0.0123 * double(i)) + 0.5 * std::cos(0.07 * double(i));
        sumX  += xd[i];
        sumX2 += xd[i] * xd[i];
    }

    MValue X = numkit::m::dsp::fft(alloc, x);
    ASSERT_TRUE(X.isComplex());
    ASSERT_EQ(X.numel(), N);
    const Complex *Xd = X.complexData();

    // DC bin equals sum of inputs (within FP tolerance).
    EXPECT_NEAR(Xd[0].real(), sumX, 1e-6);
    EXPECT_NEAR(Xd[0].imag(), 0.0, 1e-6);

    // Parseval: sum |X[k]|^2 == N * sum |x[k]|^2.
    double sumPow = 0.0;
    for (size_t k = 0; k < N; ++k) sumPow += std::norm(Xd[k]);
    EXPECT_NEAR(sumPow, double(N) * sumX2, 1e-3);

    // Round-trip restores input.
    MValue y = numkit::m::dsp::ifft(alloc, X);
    ASSERT_EQ(y.numel(), N);
    const double *yd = y.isComplex() ? nullptr : y.doubleData();
    if (y.isComplex()) {
        const Complex *yc = y.complexData();
        for (size_t i = 0; i < N; ++i)
            EXPECT_NEAR(yc[i].real(), xd[i], 1e-9) << "at i=" << i;
    } else {
        for (size_t i = 0; i < N; ++i)
            EXPECT_NEAR(yd[i], xd[i], 1e-9) << "at i=" << i;
    }
}

// ── Stockham kernel parity vs the in-place radix-2 reference ──────────
//
// The Stockham auto-sort kernel must produce bit-identical results to
// the in-place r2 dispatcher (same butterfly, same twiddles, same
// reduction order), for any pow-of-two N. These tests pin both
// algorithms against a known input set and require their outputs
// match within FP noise.
//
// Direct kernel calls — fftStockhamDispatch and fftRadix2Impl are
// declared in libs/dsp/src/backends/FftKernels.hpp; we link against
// the dsp library which exports them.

// Fill a complex test vector deterministically. Use values with
// non-trivial real+imag parts so any sign/order error in the
// butterfly shows up.
static void fillTestSignal(Complex *out, size_t N)
{
    for (size_t i = 0; i < N; ++i) {
        const double t = double(i) / double(N);
        out[i] = Complex(std::sin(2.0 * M_PI * 3.0 * t)
                       + 0.4 * std::cos(2.0 * M_PI * 7.0 * t),
                         0.5 * std::cos(2.0 * M_PI * 5.0 * t));
    }
}

TEST(DspFftStockham, MatchesRadix2_SmallSizes)
{
    // Cover the small sizes the dispatcher might reach: 4..1024.
    for (size_t N : {size_t{4}, size_t{8}, size_t{16}, size_t{64},
                     size_t{256}, size_t{1024}}) {
        std::vector<Complex> ref(N), test(N), W(N / 2);
        fillTestSignal(ref.data(), N);
        std::copy(ref.begin(), ref.end(), test.begin());
        numkit::m::fillFftTwiddles(W.data(), N, /*dir=*/+1);

        numkit::m::dsp::detail::fftRadix2Impl(ref.data(), N, W.data());
        numkit::m::dsp::detail::fftStockhamDispatch(test.data(), N, W.data());

        for (size_t i = 0; i < N; ++i) {
            EXPECT_NEAR(ref[i].real(), test[i].real(), 1e-10)
                << "N=" << N << " i=" << i;
            EXPECT_NEAR(ref[i].imag(), test[i].imag(), 1e-10)
                << "N=" << N << " i=" << i;
        }
    }
}

TEST(DspFftStockham, MatchesRadix2_LargeSizes)
{
    // Sizes spanning the FFT cliff region the dispatcher cares about.
    for (size_t N : {size_t{8192}, size_t{16384}, size_t{32768},
                     size_t{65536}}) {
        std::vector<Complex> ref(N), test(N), W(N / 2);
        fillTestSignal(ref.data(), N);
        std::copy(ref.begin(), ref.end(), test.begin());
        numkit::m::fillFftTwiddles(W.data(), N, /*dir=*/+1);

        numkit::m::dsp::detail::fftRadix2Impl(ref.data(), N, W.data());
        numkit::m::dsp::detail::fftStockhamDispatch(test.data(), N, W.data());

        for (size_t i = 0; i < N; ++i) {
            ASSERT_NEAR(ref[i].real(), test[i].real(), 1e-7)
                << "N=" << N << " i=" << i;
            ASSERT_NEAR(ref[i].imag(), test[i].imag(), 1e-7)
                << "N=" << N << " i=" << i;
        }
    }
}

TEST(DspFftR2SoA, MatchesRadix2_AllSizes)
{
    // Parity vs the AoS in-place reference across small + large sizes.
    // SoA path goes through AoS↔SoA conversion, bit-reverse on SoA,
    // and butterflies on split re/im arrays — any error in any of
    // those phases shows up here.
    for (size_t N : {size_t{4},   size_t{8},   size_t{16},   size_t{64},
                     size_t{256}, size_t{1024}, size_t{8192}, size_t{16384},
                     size_t{32768}, size_t{65536}}) {
        std::vector<Complex> ref(N), test(N), W(N / 2);
        fillTestSignal(ref.data(), N);
        std::copy(ref.begin(), ref.end(), test.begin());
        numkit::m::fillFftTwiddles(W.data(), N, /*dir=*/+1);

        numkit::m::dsp::detail::fftRadix2Impl(ref.data(), N, W.data());
        numkit::m::dsp::detail::fftRadix2SoaDispatch(test.data(), N, W.data());

        for (size_t i = 0; i < N; ++i) {
            ASSERT_NEAR(ref[i].real(), test[i].real(), 1e-7)
                << "N=" << N << " i=" << i;
            ASSERT_NEAR(ref[i].imag(), test[i].imag(), 1e-7)
                << "N=" << N << " i=" << i;
        }
    }
}

TEST(DspFftR2SoA, EdgeCase_N1)
{
    std::vector<Complex> x = {Complex(7.0, -3.5)};
    std::vector<Complex> W;
    numkit::m::dsp::detail::fftRadix2SoaDispatch(x.data(), 1, W.data());
    EXPECT_DOUBLE_EQ(x[0].real(), 7.0);
    EXPECT_DOUBLE_EQ(x[0].imag(), -3.5);
}

TEST(DspFftR4SoA, MatchesRadix2_PowerOfFourSizes)
{
    // r4 SoA only handles powers of 4. Compare against AoS r2 which
    // handles arbitrary pow-of-2 (and is the established reference).
    for (size_t N : {size_t{4}, size_t{16}, size_t{64}, size_t{256},
                     size_t{1024}, size_t{4096}, size_t{16384},
                     size_t{65536}}) {
        std::vector<Complex> ref(N), test(N), W(N / 2);
        fillTestSignal(ref.data(), N);
        std::copy(ref.begin(), ref.end(), test.begin());
        numkit::m::fillFftTwiddles(W.data(), N, /*dir=*/+1);

        numkit::m::dsp::detail::fftRadix2Impl(ref.data(), N, W.data());
        numkit::m::dsp::detail::fftRadix4Pow4SoaDispatch(test.data(), N, W.data());

        for (size_t i = 0; i < N; ++i) {
            ASSERT_NEAR(ref[i].real(), test[i].real(), 1e-7)
                << "N=" << N << " i=" << i;
            ASSERT_NEAR(ref[i].imag(), test[i].imag(), 1e-7)
                << "N=" << N << " i=" << i;
        }
    }
}

TEST(DspFftStockham, EdgeCase_N1)
{
    // N=1 is identity — should leave the input unchanged.
    std::vector<Complex> x = {Complex(3.14, 2.71)};
    std::vector<Complex> W;  // empty, not used at N<=1
    numkit::m::dsp::detail::fftStockhamDispatch(x.data(), 1, W.data());
    EXPECT_DOUBLE_EQ(x[0].real(), 3.14);
    EXPECT_DOUBLE_EQ(x[0].imag(), 2.71);
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
