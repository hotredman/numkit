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
TEST(DspFftPublicApi, InvalidDimThrows)
{
    Allocator alloc = Allocator::defaultAllocator();
    MValue x = makeRealRow(alloc, {1.0, 2.0, 3.0, 4.0});

    EXPECT_THROW(numkit::m::dsp::fft(alloc, x, /*n=*/-1, /*dim=*/3),
                 numkit::m::MError);
    EXPECT_THROW(numkit::m::dsp::fft(alloc, x, /*n=*/-1, /*dim=*/0),
                 numkit::m::MError);
    EXPECT_THROW(numkit::m::dsp::ifft(alloc, x, /*n=*/-1, /*dim=*/3),
                 numkit::m::MError);
}
