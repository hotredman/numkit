// libs/builtin/tests/simd_parity_test.cpp
//
// Parity tests for backend-split functions (Phase 8+). Each test
// computes a reference via a plain scalar loop and compares against
// the public API, which — depending on NUMKIT_WITH_SIMD — resolves
// to either the portable backend (trivially passes, it IS the
// reference) or the Highway-dispatched backend. Same source, same
// assertions, run under both presets.
//
// For abs we demand bit-exact equality: abs on IEEE-754 doubles is
// a deterministic bit-flip of the sign (all SIMD ISAs implement it
// identically). Transcendentals added in later phases will use an
// ULP budget instead.

#include <numkit/m/builtin/MStdMath.hpp>

#include <numkit/m/core/MAllocator.hpp>
#include <numkit/m/core/MTypes.hpp>
#include <numkit/m/core/MValue.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <cstring>
#include <limits>
#include <random>
#include <vector>

using numkit::m::Allocator;
using numkit::m::MType;
using numkit::m::MValue;

namespace {

MValue makeDoubleVector(Allocator &alloc, const std::vector<double> &vals)
{
    MValue v = MValue::matrix(vals.size(), 1, MType::DOUBLE, &alloc);
    double *data = v.doubleDataMut();
    for (size_t i = 0; i < vals.size(); ++i)
        data[i] = vals[i];
    return v;
}

// Bit-exact equality: treat the double as a uint64 so NaN and -0 both
// compare meaningfully. Gtest's EXPECT_DOUBLE_EQ would accept -0 == +0
// and trip over every NaN.
bool bitEquals(double a, double b)
{
    uint64_t ba, bb;
    std::memcpy(&ba, &a, sizeof(ba));
    std::memcpy(&bb, &b, sizeof(bb));
    return ba == bb;
}

} // namespace

// ════════════════════════════════════════════════════════════════════════
// abs — bit-exact against a scalar reference loop
// ════════════════════════════════════════════════════════════════════════

TEST(SimdParity_Abs, MatchesScalarOnLargeRandomVector)
{
    Allocator alloc = Allocator::defaultAllocator();

    // Size chosen to span several SIMD lanes + scalar tail — 1021 is
    // prime, so every vector width leaves a different remainder.
    constexpr size_t N = 1021;
    std::mt19937 rng(123);
    std::uniform_real_distribution<double> dist(-1e6, 1e6);

    std::vector<double> src(N);
    for (auto &v : src) v = dist(rng);

    MValue x = makeDoubleVector(alloc, src);
    MValue y = numkit::m::builtin::abs(alloc, x);

    ASSERT_EQ(y.numel(), N);
    for (size_t i = 0; i < N; ++i) {
        EXPECT_TRUE(bitEquals(y.doubleData()[i], std::fabs(src[i])))
            << "mismatch at i=" << i << ": got " << y.doubleData()[i]
            << ", expected " << std::fabs(src[i]);
    }
}

TEST(SimdParity_Abs, HandlesIeeeEdgeCases)
{
    Allocator alloc = Allocator::defaultAllocator();

    // Every subnormal, zero, infinity, and NaN combo that commonly
    // goes wrong in hand-rolled SIMD (e.g. bit-clear vs subtract-zero
    // strategies diverge on -0.0).
    const double inf   = std::numeric_limits<double>::infinity();
    const double qnan  = std::numeric_limits<double>::quiet_NaN();
    const double denorm = std::numeric_limits<double>::denorm_min();

    std::vector<double> src = {
        0.0, -0.0,
        1.0, -1.0,
        inf, -inf,
        denorm, -denorm,
        std::numeric_limits<double>::min(),
        -std::numeric_limits<double>::min(),
        std::numeric_limits<double>::max(),
        std::numeric_limits<double>::lowest(),
        qnan, -qnan,
    };

    MValue x = makeDoubleVector(alloc, src);
    MValue y = numkit::m::builtin::abs(alloc, x);

    ASSERT_EQ(y.numel(), src.size());
    for (size_t i = 0; i < src.size(); ++i) {
        double got = y.doubleData()[i];
        double ref = std::fabs(src[i]);
        if (std::isnan(ref)) {
            // NaN payload / sign isn't preserved consistently across
            // std::fabs implementations, so only require: output is NaN.
            EXPECT_TRUE(std::isnan(got)) << "i=" << i;
        } else {
            EXPECT_TRUE(bitEquals(got, ref))
                << "mismatch at i=" << i << ": got " << got
                << ", expected " << ref;
        }
    }
}

TEST(SimdParity_Abs, ScalarInputStillWorks)
{
    Allocator alloc = Allocator::defaultAllocator();

    // Scalar / small paths bypass the SIMD loop entirely; included
    // to catch regressions in the public wrapper's dispatch logic.
    MValue x = MValue::scalar(-3.5, &alloc);
    MValue y = numkit::m::builtin::abs(alloc, x);
    EXPECT_TRUE(bitEquals(y.toScalar(), 3.5));
}

TEST(SimdParity_Abs, ComplexFallsBackToScalarImpl)
{
    Allocator alloc = Allocator::defaultAllocator();

    // The SIMD backend shouldn't touch the complex path — it delegates
    // to std::abs(Complex). A quick check that this path still works
    // under the SIMD build.
    auto v = MValue::complexMatrix(1, 3, &alloc);
    v.complexDataMut()[0] = {3.0, 4.0};
    v.complexDataMut()[1] = {-5.0, 12.0};
    v.complexDataMut()[2] = {0.0, 0.0};

    MValue y = numkit::m::builtin::abs(alloc, v);
    ASSERT_EQ(y.numel(), 3u);
    EXPECT_DOUBLE_EQ(y.doubleData()[0], 5.0);
    EXPECT_DOUBLE_EQ(y.doubleData()[1], 13.0);
    EXPECT_DOUBLE_EQ(y.doubleData()[2], 0.0);
}
