// toolboxes/builtin/tests/simd_parity_test.cpp
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

#include <numkit/builtin/ops.hpp>
#include <numkit/builtin/elfun.hpp>
#include <numkit/builtin/elfun.hpp>
#include <numkit/builtin/elfun.hpp>
#include <numkit/builtin/specfun.hpp>

#include <memory_resource>
#include <numkit/core/types.hpp>
#include <numkit/value/value.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <cstring>
#include <limits>
#include <random>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using numkit::ValueType;
using numkit::Value;

namespace {

Value makeDoubleVector(std::pmr::memory_resource *mr, const std::vector<double> &vals)
{
    Value v = Value::matrix(vals.size(), 1, ValueType::DOUBLE, mr);
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
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();

    // Size chosen to span several SIMD lanes + scalar tail — 1021 is
    // prime, so every vector width leaves a different remainder.
    constexpr size_t N = 1021;
    std::mt19937 rng(123);
    std::uniform_real_distribution<double> dist(-1e6, 1e6);

    std::vector<double> src(N);
    for (auto &v : src) v = dist(rng);

    Value x = makeDoubleVector(mr, src);
    Value y = numkit::builtin::abs(x, mr);

    ASSERT_EQ(y.numel(), N);
    for (size_t i = 0; i < N; ++i) {
        EXPECT_TRUE(bitEquals(y.doubleData()[i], std::fabs(src[i])))
            << "mismatch at i=" << i << ": got " << y.doubleData()[i]
            << ", expected " << std::fabs(src[i]);
    }
}

TEST(SimdParity_Abs, HandlesIeeeEdgeCases)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();

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

    Value x = makeDoubleVector(mr, src);
    Value y = numkit::builtin::abs(x, mr);

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
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();

    // Scalar / small paths bypass the SIMD loop entirely; included
    // to catch regressions in the public wrapper's dispatch logic.
    Value x = Value::scalar(-3.5, mr);
    Value y = numkit::builtin::abs(x, mr);
    EXPECT_TRUE(bitEquals(y.toScalar(), 3.5));
}

TEST(SimdParity_Abs, ComplexFallsBackToScalarImpl)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();

    // The SIMD backend shouldn't touch the complex path — it delegates
    // to std::abs(Complex). A quick check that this path still works
    // under the SIMD build.
    auto v = Value::complexMatrix(1, 3, mr);
    v.complexDataMut()[0] = {3.0, 4.0};
    v.complexDataMut()[1] = {-5.0, 12.0};
    v.complexDataMut()[2] = {0.0, 0.0};

    Value y = numkit::builtin::abs(v, mr);
    ASSERT_EQ(y.numel(), 3u);
    EXPECT_DOUBLE_EQ(y.doubleData()[0], 5.0);
    EXPECT_DOUBLE_EQ(y.doubleData()[1], 13.0);
    EXPECT_DOUBLE_EQ(y.doubleData()[2], 0.0);
}

// ════════════════════════════════════════════════════════════════════════
// Transcendentals — ULP-tolerance checks
//
// SIMD transcendental approximations (Highway's hwy/contrib/math is
// SLEEF-derived) aren't bit-exact vs std::sin / std::cos / etc.
// Highway documents ULP <= 4 across all targets; we bound at 8 here to
// leave some slack for tail-loop edge cases. Any larger drift means
// something is genuinely wrong.
// ════════════════════════════════════════════════════════════════════════

namespace {

// Unsigned ULP distance between two finite doubles. Converts sign-magnitude
// to a biased two's-complement representation so adjacent representable
// values (across ±0) are 1 ULP apart.
uint64_t ulpDistance(double a, double b)
{
    // NaN/NaN → max distance; caller should special-case these.
    if (std::isnan(a) || std::isnan(b))
        return UINT64_MAX;
    int64_t ia, ib;
    std::memcpy(&ia, &a, sizeof(ia));
    std::memcpy(&ib, &b, sizeof(ib));
    auto biased = [](int64_t i) -> int64_t {
        return (i < 0) ? (INT64_MIN - i) : i;
    };
    int64_t ba = biased(ia);
    int64_t bb = biased(ib);
    return static_cast<uint64_t>(ba > bb ? ba - bb : bb - ba);
}

template <typename SimdFn, typename ScalarFn>
void checkTranscendentalParity(SimdFn simdFn, ScalarFn scalarFn,
                               double lo, double hi, const char *name,
                               uint64_t ulpBudget = 8)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();

    constexpr size_t N = 1021;
    std::mt19937 rng(271828);
    std::uniform_real_distribution<double> dist(lo, hi);

    std::vector<double> src(N);
    for (auto &v : src) v = dist(rng);

    Value x = makeDoubleVector(mr, src);
    Value y = simdFn(mr, x);

    ASSERT_EQ(y.numel(), N) << name;
    uint64_t worst = 0;
    size_t worstIdx = 0;
    for (size_t i = 0; i < N; ++i) {
        double got = y.doubleData()[i];
        double ref = scalarFn(src[i]);
        uint64_t dist = ulpDistance(got, ref);
        if (dist > worst) { worst = dist; worstIdx = i; }
    }
    EXPECT_LE(worst, ulpBudget)
        << name << " drifted " << worst << " ULP at index " << worstIdx
        << " (in=" << src[worstIdx] << ", got=" << y.doubleData()[worstIdx]
        << ", ref=" << scalarFn(src[worstIdx]) << ")";
}

} // namespace

TEST(SimdParity_Sin, WithinUlpBudget)
{
    checkTranscendentalParity(
        [](std::pmr::memory_resource *a, const Value &x) { return numkit::builtin::sin(x, a); },
        [](double x) { return std::sin(x); },
        -10.0, 10.0, "sin");
}

TEST(SimdParity_Cos, WithinUlpBudget)
{
    checkTranscendentalParity(
        [](std::pmr::memory_resource *a, const Value &x) { return numkit::builtin::cos(x, a); },
        [](double x) { return std::cos(x); },
        -10.0, 10.0, "cos");
}

TEST(SimdParity_Exp, WithinUlpBudget)
{
    // Clamp to a range where exp() doesn't overflow — past ~709 it
    // becomes Inf and ULP distance is undefined / infinite.
    checkTranscendentalParity(
        [](std::pmr::memory_resource *a, const Value &x) { return numkit::builtin::exp(x, a); },
        [](double x) { return std::exp(x); },
        -5.0, 5.0, "exp");
}

TEST(SimdParity_Log, WithinUlpBudget)
{
    // Strictly positive inputs — negatives produce NaN, whose ULP
    // distance doesn't compare meaningfully.
    checkTranscendentalParity(
        [](std::pmr::memory_resource *a, const Value &x) { return numkit::builtin::log(x, a); },
        [](double x) { return std::log(x); },
        0.01, 100.0, "log");
}

TEST(SimdParity_Expm1, WithinUlpBudget)
{
    checkTranscendentalParity(
        [](std::pmr::memory_resource *a, const Value &x) { return numkit::builtin::expm1(x, a); },
        [](double x) { return std::expm1(x); },
        -10.0, 10.0, "expm1");
}

TEST(SimdParity_Log1p, WithinUlpBudget)
{
    // log1p domain is x > -1; sample away from the -1 pole.
    checkTranscendentalParity(
        [](std::pmr::memory_resource *a, const Value &x) { return numkit::builtin::log1p(x, a); },
        [](double x) { return std::log1p(x); },
        -0.9, 100.0, "log1p");
}

TEST(SimdParity_Log2, WithinUlpBudget)
{
    // Strictly positive inputs (negatives -> NaN).
    checkTranscendentalParity(
        [](std::pmr::memory_resource *a, const Value &x) { return numkit::builtin::log2(x, a); },
        [](double x) { return std::log2(x); },
        0.01, 100.0, "log2");
}

TEST(SimdParity_Erf, WithinUlpBudget)
{
    // [-3, 3] exercises both the vectorised SLEEF dd kernel (|x| <= 2.5)
    // and the scalar std::erf fixup (|x| > 2.5).
    checkTranscendentalParity(
        [](std::pmr::memory_resource *a, const Value &x) { return numkit::builtin::erf(x, a); },
        [](double x) { return std::erf(x); },
        -3.0, 3.0, "erf");
}

TEST(SimdParity_Erf, WideRangeAndSpecialLanes)
{
    // Mix in the small-x, large-x, zero and sign branches.
    checkTranscendentalParity(
        [](std::pmr::memory_resource *a, const Value &x) { return numkit::builtin::erf(x, a); },
        [](double x) { return std::erf(x); },
        -6.0, 6.0, "erf-wide");
}

TEST(SimdParity_Transcendental, NegativeLogScalarStillComplex)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    // MATLAB contract (preserved in both backends): scalar log(-1) → i·π.
    Value y = numkit::builtin::log(Value::scalar(-1.0, mr), mr);
    EXPECT_TRUE(y.isComplex());
    auto c = y.toComplex();
    EXPECT_NEAR(c.real(), 0.0, 1e-12);
    EXPECT_NEAR(c.imag(), M_PI, 1e-12);
}

// ════════════════════════════════════════════════════════════════════════
// Binary ops (plus / minus / times / rdivide) — bit-exact
//
// IEEE-754 add / sub / mul / div are deterministic, so SIMD and scalar
// must produce bit-identical results. Anything else means a broken
// reduction or lane mis-alignment.
// ════════════════════════════════════════════════════════════════════════

namespace {

template <typename SimdFn, typename ScalarOp>
void checkBinaryParity(SimdFn simdFn, ScalarOp op, const char *name)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();

    constexpr size_t N = 1021;
    std::mt19937 rng(65537);
    std::uniform_real_distribution<double> dist(-1000.0, 1000.0);

    std::vector<double> av(N), bv(N);
    for (size_t i = 0; i < N; ++i) {
        av[i] = dist(rng);
        // Keep b away from 0 so rdivide doesn't run through Inf — the
        // bit-exact check still holds near Inf, but Inf complicates
        // messages on mismatch.
        bv[i] = dist(rng);
        if (std::fabs(bv[i]) < 1.0) bv[i] += std::copysign(1.0, bv[i]);
    }

    Value A = makeDoubleVector(mr, av);
    Value B = makeDoubleVector(mr, bv);
    Value Y = simdFn(mr, A, B);

    ASSERT_EQ(Y.numel(), N) << name;
    for (size_t i = 0; i < N; ++i) {
        double got = Y.doubleData()[i];
        double ref = op(av[i], bv[i]);
        EXPECT_TRUE(bitEquals(got, ref))
            << name << " mismatch at i=" << i
            << ": got " << got << ", expected " << ref;
    }
}

} // namespace

TEST(SimdParity_Plus,    MatchesScalar)
{
    checkBinaryParity(
        [](std::pmr::memory_resource *a, const Value &x, const Value &y) { return numkit::builtin::plus(x, y, a); },
        [](double x, double y) { return x + y; }, "plus");
}

TEST(SimdParity_Minus,   MatchesScalar)
{
    checkBinaryParity(
        [](std::pmr::memory_resource *a, const Value &x, const Value &y) { return numkit::builtin::minus(x, y, a); },
        [](double x, double y) { return x - y; }, "minus");
}

TEST(SimdParity_Times,   MatchesScalar)
{
    checkBinaryParity(
        [](std::pmr::memory_resource *a, const Value &x, const Value &y) { return numkit::builtin::times(x, y, a); },
        [](double x, double y) { return x * y; }, "times");
}

TEST(SimdParity_Rdivide, MatchesScalar)
{
    checkBinaryParity(
        [](std::pmr::memory_resource *a, const Value &x, const Value &y) { return numkit::builtin::rdivide(x, y, a); },
        [](double x, double y) { return x / y; }, "rdivide");
}

// ════════════════════════════════════════════════════════════════════════
// Matrix multiply — loose tolerance
//
// SIMD matmul uses fused multiply-add (MulAdd), scalar uses mul + add;
// reduction order is identical but FMA skips one rounding step, so
// results diverge by <=0.5 ULP per accumulated term. Over K=128 inner
// products the worst-case drift is ~1e-13 relative — we bound at 1e-10
// to give headroom on IEEE edge cases. Any larger drift means a genuine
// algorithm mismatch.
// ════════════════════════════════════════════════════════════════════════

TEST(SimdParity_Mtimes, MatchesScalarSquareMatrix)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();

    // 128 is large enough to accumulate FMA/non-FMA divergence but
    // small enough to compute a naive reference loop in the test.
    constexpr size_t N = 128;
    std::mt19937 rng(97);
    std::uniform_real_distribution<double> dist(-1.0, 1.0);

    Value A = Value::matrix(N, N, ValueType::DOUBLE, mr);
    Value B = Value::matrix(N, N, ValueType::DOUBLE, mr);
    for (size_t i = 0; i < N * N; ++i) {
        A.doubleDataMut()[i] = dist(rng);
        B.doubleDataMut()[i] = dist(rng);
    }

    Value C = numkit::builtin::mtimes(A, B, mr);
    ASSERT_EQ(C.numel(), N * N);

    // Independent scalar reference with the same (j,k,i) order so the
    // only divergence vs the backend-under-test is FMA rounding.
    std::vector<double> ref(N * N, 0.0);
    for (size_t j = 0; j < N; ++j) {
        for (size_t k = 0; k < N; ++k) {
            const double bkj = B.doubleData()[j * N + k];
            for (size_t i = 0; i < N; ++i)
                ref[j * N + i] += bkj * A.doubleData()[k * N + i];
        }
    }

    double maxRelErr = 0.0;
    size_t worstIdx = 0;
    for (size_t i = 0; i < N * N; ++i) {
        double got = C.doubleData()[i];
        double r   = ref[i];
        double denom = std::max(1.0, std::fabs(r));
        double err = std::fabs(got - r) / denom;
        if (err > maxRelErr) { maxRelErr = err; worstIdx = i; }
    }
    EXPECT_LT(maxRelErr, 1e-10)
        << "worst relative error " << maxRelErr << " at index " << worstIdx
        << " (got=" << C.doubleData()[worstIdx] << ", ref=" << ref[worstIdx] << ")";
}

// K-blocked matmul kernel correctness. K must exceed the KC=256
// internal block size so the kernel runs multiple k-blocks per
// (i0, j0) tile, exercising the load+accumulate path on subsequent
// blocks (vs zero-init only on the first). M and N are kept at MR
// and NR multiples (8, 4) so the body kernel — not the saxpy tail —
// is what's under test. K=300 gives one full kb=256 block plus one
// partial kb=44 block; both initialization paths run.
TEST(SimdParity_Mtimes, MatchesScalarMultiKBlock)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();

    constexpr size_t M = 16;
    constexpr size_t K = 300;
    constexpr size_t N = 12;

    std::mt19937 rng(123);
    std::uniform_real_distribution<double> dist(-1.0, 1.0);

    Value A = Value::matrix(M, K, ValueType::DOUBLE, mr);
    Value B = Value::matrix(K, N, ValueType::DOUBLE, mr);
    for (size_t i = 0; i < M * K; ++i) A.doubleDataMut()[i] = dist(rng);
    for (size_t i = 0; i < K * N; ++i) B.doubleDataMut()[i] = dist(rng);

    Value C = numkit::builtin::mtimes(A, B, mr);
    ASSERT_EQ(C.numel(), M * N);

    // Reference matches the kernel's (j, k, i) reduction order — the
    // K-block split changes WHICH k indices are summed in WHICH register
    // pass, but not the (j, k, i) sequence per output element, so FMA
    // rounding stays bit-identical.
    std::vector<double> ref(M * N, 0.0);
    for (size_t j = 0; j < N; ++j) {
        for (size_t k = 0; k < K; ++k) {
            const double bkj = B.doubleData()[j * K + k];
            for (size_t i = 0; i < M; ++i)
                ref[j * M + i] += bkj * A.doubleData()[k * M + i];
        }
    }

    double maxRelErr = 0.0;
    size_t worstIdx = 0;
    for (size_t i = 0; i < M * N; ++i) {
        double got = C.doubleData()[i];
        double r   = ref[i];
        double denom = std::max(1.0, std::fabs(r));
        double err = std::fabs(got - r) / denom;
        if (err > maxRelErr) { maxRelErr = err; worstIdx = i; }
    }
    EXPECT_LT(maxRelErr, 1e-10)
        << "worst relative error " << maxRelErr << " at index " << worstIdx;
}

// Same as above but K is exactly KC*3 (3 full blocks, no partial).
// Catches off-by-one in the (k0 + KC < K) tail-handling branch.
TEST(SimdParity_Mtimes, MatchesScalarExactKBlockMultiple)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();

    constexpr size_t M = 16;
    constexpr size_t K = 768;   // = 3 * 256, exact multiple of KC
    constexpr size_t N = 8;

    std::mt19937 rng(456);
    std::uniform_real_distribution<double> dist(-1.0, 1.0);

    Value A = Value::matrix(M, K, ValueType::DOUBLE, mr);
    Value B = Value::matrix(K, N, ValueType::DOUBLE, mr);
    for (size_t i = 0; i < M * K; ++i) A.doubleDataMut()[i] = dist(rng);
    for (size_t i = 0; i < K * N; ++i) B.doubleDataMut()[i] = dist(rng);

    Value C = numkit::builtin::mtimes(A, B, mr);

    std::vector<double> ref(M * N, 0.0);
    for (size_t j = 0; j < N; ++j)
        for (size_t k = 0; k < K; ++k) {
            const double bkj = B.doubleData()[j * K + k];
            for (size_t i = 0; i < M; ++i)
                ref[j * M + i] += bkj * A.doubleData()[k * M + i];
        }

    double maxRelErr = 0.0;
    for (size_t i = 0; i < M * N; ++i) {
        double err = std::fabs(C.doubleData()[i] - ref[i])
                     / std::max(1.0, std::fabs(ref[i]));
        if (err > maxRelErr) maxRelErr = err;
    }
    EXPECT_LT(maxRelErr, 1e-10);
}

// ════════════════════════════════════════════════════════════════════════
// Dimensionality coverage — 1D (row + column), 2D, 3D
//
// The SIMD fast paths in Phase 8 are keyed on different shape
// predicates: unary ops (abs/sin/cos/exp/log) just need a flat
// double buffer and work for any shape; binary ops (plus/minus/
// times/rdivide) currently fast-path only 2D same-shape and fall
// back to elementwiseDouble() for 3D and broadcast. These tests
// pin that contract so a future refactor can't silently regress.
// ════════════════════════════════════════════════════════════════════════

TEST(SimdParity_Dim, AbsOn1DRow)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    auto x = Value::matrix(1, 256, ValueType::DOUBLE, mr);
    for (size_t i = 0; i < 256; ++i) x.doubleDataMut()[i] = -double(i);
    auto y = numkit::builtin::abs(x, mr);
    EXPECT_EQ(y.dims().rows(), 1u);
    EXPECT_EQ(y.dims().cols(), 256u);
    for (size_t i = 0; i < 256; ++i)
        EXPECT_TRUE(bitEquals(y.doubleData()[i], double(i)));
}

TEST(SimdParity_Dim, AbsOn1DColumn)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    auto x = Value::matrix(256, 1, ValueType::DOUBLE, mr);
    for (size_t i = 0; i < 256; ++i) x.doubleDataMut()[i] = -double(i);
    auto y = numkit::builtin::abs(x, mr);
    EXPECT_EQ(y.dims().rows(), 256u);
    EXPECT_EQ(y.dims().cols(), 1u);
    for (size_t i = 0; i < 256; ++i)
        EXPECT_TRUE(bitEquals(y.doubleData()[i], double(i)));
}

TEST(SimdParity_Dim, AbsOn3D)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    auto x = Value::matrix3d(3, 4, 5, ValueType::DOUBLE, mr);
    for (size_t i = 0; i < x.numel(); ++i) x.doubleDataMut()[i] = -double(i);
    auto y = numkit::builtin::abs(x, mr);
    ASSERT_TRUE(y.dims().is3D());
    EXPECT_EQ(y.dims().rows(), 3u);
    EXPECT_EQ(y.dims().cols(), 4u);
    EXPECT_EQ(y.dims().pages(), 5u);
    EXPECT_EQ(y.numel(), 60u);
    for (size_t i = 0; i < y.numel(); ++i)
        EXPECT_TRUE(bitEquals(y.doubleData()[i], double(i)));
}

TEST(SimdParity_Dim, SinOn3D)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    auto x = Value::matrix3d(2, 3, 4, ValueType::DOUBLE, mr);
    for (size_t i = 0; i < x.numel(); ++i) x.doubleDataMut()[i] = 0.1 * double(i);
    auto y = numkit::builtin::sin(x, mr);
    ASSERT_TRUE(y.dims().is3D());
    EXPECT_EQ(y.numel(), 24u);
    for (size_t i = 0; i < y.numel(); ++i)
        EXPECT_LE(ulpDistance(y.doubleData()[i], std::sin(0.1 * double(i))), 8u);
}

namespace {

// Parametric 3D binary-op check. After the Phase 8c+ fast-path
// extension, every 3D same-shape DOUBLE op routes through the same
// SIMD loop as 2D — so bit-exactness vs scalar is the right bound.
template <typename BinaryFn, typename ScalarOp>
void checkBinaryOn3D(BinaryFn fn, ScalarOp op, const char *name)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    auto a = Value::matrix3d(3, 5, 7, ValueType::DOUBLE, mr);  // 105 elems, all odd
    auto b = Value::matrix3d(3, 5, 7, ValueType::DOUBLE, mr);

    std::mt19937 rng(65537);
    std::uniform_real_distribution<double> dist(-100.0, 100.0);
    for (size_t i = 0; i < a.numel(); ++i) {
        a.doubleDataMut()[i] = dist(rng);
        double bv = dist(rng);
        if (std::fabs(bv) < 1.0) bv += std::copysign(1.0, bv);  // avoid /0 for rdivide
        b.doubleDataMut()[i] = bv;
    }

    auto c = fn(mr, a, b);
    ASSERT_TRUE(c.dims().is3D()) << name;
    EXPECT_EQ(c.dims().rows(), 3u) << name;
    EXPECT_EQ(c.dims().cols(), 5u) << name;
    EXPECT_EQ(c.dims().pages(), 7u) << name;
    ASSERT_EQ(c.numel(), a.numel()) << name;
    for (size_t i = 0; i < c.numel(); ++i)
        EXPECT_TRUE(bitEquals(c.doubleData()[i], op(a.doubleData()[i], b.doubleData()[i])))
            << name << " mismatch at i=" << i;
}

} // namespace

TEST(SimdParity_Dim, PlusOn3D)
{
    checkBinaryOn3D(
        [](std::pmr::memory_resource *a, const Value &x, const Value &y) { return numkit::builtin::plus(x, y, a); },
        [](double x, double y) { return x + y; }, "plus");
}

TEST(SimdParity_Dim, MinusOn3D)
{
    checkBinaryOn3D(
        [](std::pmr::memory_resource *a, const Value &x, const Value &y) { return numkit::builtin::minus(x, y, a); },
        [](double x, double y) { return x - y; }, "minus");
}

TEST(SimdParity_Dim, TimesOn3D)
{
    checkBinaryOn3D(
        [](std::pmr::memory_resource *a, const Value &x, const Value &y) { return numkit::builtin::times(x, y, a); },
        [](double x, double y) { return x * y; }, "times");
}

TEST(SimdParity_Dim, RdivideOn3D)
{
    checkBinaryOn3D(
        [](std::pmr::memory_resource *a, const Value &x, const Value &y) { return numkit::builtin::rdivide(x, y, a); },
        [](double x, double y) { return x / y; }, "rdivide");
}

TEST(SimdParity_Dim, PlusOn1DRowAndColumn)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    // Row vector: cols > 1, rows = 1 — NOT excluded by the fast path,
    // so goes through SIMD when NUMKIT_WITH_SIMD=ON.
    auto aRow = Value::matrix(1, 256, ValueType::DOUBLE, mr);
    auto bRow = Value::matrix(1, 256, ValueType::DOUBLE, mr);
    for (size_t i = 0; i < 256; ++i) {
        aRow.doubleDataMut()[i] = double(i);
        bRow.doubleDataMut()[i] = 3.0;
    }
    auto cRow = numkit::builtin::plus(aRow, bRow, mr);
    EXPECT_EQ(cRow.dims().rows(), 1u);
    EXPECT_EQ(cRow.dims().cols(), 256u);
    for (size_t i = 0; i < 256; ++i)
        EXPECT_TRUE(bitEquals(cRow.doubleData()[i], double(i) + 3.0));

    // Column vector: rows > 1, cols = 1 — also SIMD-eligible.
    auto aCol = Value::matrix(256, 1, ValueType::DOUBLE, mr);
    auto bCol = Value::matrix(256, 1, ValueType::DOUBLE, mr);
    for (size_t i = 0; i < 256; ++i) {
        aCol.doubleDataMut()[i] = double(i);
        bCol.doubleDataMut()[i] = 3.0;
    }
    auto cCol = numkit::builtin::plus(aCol, bCol, mr);
    EXPECT_EQ(cCol.dims().rows(), 256u);
    EXPECT_EQ(cCol.dims().cols(), 1u);
    for (size_t i = 0; i < 256; ++i)
        EXPECT_TRUE(bitEquals(cCol.doubleData()[i], double(i) + 3.0));
}

TEST(SimdParity_Mtimes, ThrowsOn3DInput)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();

    // 3D * 3D — matrix multiply is undefined here; must throw, not
    // silently strip pages and produce garbage. Pre-Phase 8 this
    // quietly used (rows, cols) and ignored pages.
    {
        auto A = Value::matrix3d(3, 4, 2, ValueType::DOUBLE, mr);
        auto B = Value::matrix3d(4, 3, 2, ValueType::DOUBLE, mr);
        for (size_t i = 0; i < A.numel(); ++i) A.doubleDataMut()[i] = 1.0;
        for (size_t i = 0; i < B.numel(); ++i) B.doubleDataMut()[i] = 1.0;
        EXPECT_THROW({ (void)numkit::builtin::mtimes(A, B, mr); },
                     numkit::Error);
    }

    // 3D * 2D — also undefined.
    {
        auto A = Value::matrix3d(3, 4, 2, ValueType::DOUBLE, mr);
        auto B = Value::matrix(4, 3, ValueType::DOUBLE, mr);
        for (size_t i = 0; i < A.numel(); ++i) A.doubleDataMut()[i] = 1.0;
        for (size_t i = 0; i < B.numel(); ++i) B.doubleDataMut()[i] = 1.0;
        EXPECT_THROW({ (void)numkit::builtin::mtimes(A, B, mr); },
                     numkit::Error);
    }
}

TEST(SimdParity_Mtimes, ScalarTimes3DArrayStillWorks)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();

    // scalar * 3D — MATLAB degenerates this to elementwise scaling;
    // our code routes it through elementwiseDouble() and the result
    // preserves the 3D shape with every element scaled.
    auto A = Value::matrix3d(2, 3, 4, ValueType::DOUBLE, mr);
    for (size_t i = 0; i < A.numel(); ++i) A.doubleDataMut()[i] = double(i);
    auto s = Value::scalar(2.5, mr);

    auto C = numkit::builtin::mtimes(s, A, mr);
    ASSERT_TRUE(C.dims().is3D());
    EXPECT_EQ(C.dims().pages(), 4u);
    EXPECT_EQ(C.numel(), 24u);
    for (size_t i = 0; i < C.numel(); ++i)
        EXPECT_DOUBLE_EQ(C.doubleData()[i], 2.5 * double(i));
}

TEST(SimdParity_Mtimes, HandlesNonSquareDimensions)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();

    // M=37, K=51, N=43 — all odd, none a multiple of typical SIMD
    // widths (2/4/8). Exercises the scalar tail on every loop.
    constexpr size_t M = 37, K = 51, N = 43;
    std::mt19937 rng(11);
    std::uniform_real_distribution<double> dist(-1.0, 1.0);

    Value A = Value::matrix(M, K, ValueType::DOUBLE, mr);
    Value B = Value::matrix(K, N, ValueType::DOUBLE, mr);
    for (size_t i = 0; i < M * K; ++i) A.doubleDataMut()[i] = dist(rng);
    for (size_t i = 0; i < K * N; ++i) B.doubleDataMut()[i] = dist(rng);

    Value C = numkit::builtin::mtimes(A, B, mr);
    ASSERT_EQ(C.dims().rows(), M);
    ASSERT_EQ(C.dims().cols(), N);

    for (size_t j = 0; j < N; ++j) {
        for (size_t i = 0; i < M; ++i) {
            double ref = 0.0;
            for (size_t k = 0; k < K; ++k)
                ref += A.doubleData()[k * M + i] * B.doubleData()[j * K + k];
            EXPECT_NEAR(C.doubleData()[j * M + i], ref, 1e-12)
                << "at (" << i << "," << j << ")";
        }
    }
}

// ════════════════════════════════════════════════════════════════════════
// Large-N elementwise parity — exercises the parallel_for path in
// the NUMKIT_WITH_THREADS build above the per-kernel parallel
// threshold (kElementwiseThreshold = 16k, kTranscendentalThreshold = 4k).
// Each kernel must produce bit-identical results to a scalar reference
// regardless of how the work is split across worker threads, because
// the supported ops (+ − .* ./ abs) and Highway's transcendental
// approximations are defined per-element with no cross-element
// dependency. On builds without threads this just runs sequentially
// — same scalar reference, same bit-identical assertion.
// ════════════════════════════════════════════════════════════════════════

namespace {

std::vector<double> makeReals(size_t n, uint32_t seed, double lo, double hi)
{
    std::mt19937                            rng(seed);
    std::uniform_real_distribution<double>  dist(lo, hi);
    std::vector<double>                     out(n);
    for (auto &v : out) v = dist(rng);
    return out;
}

} // namespace

TEST(SimdParity_ParallelLarge, AbsBitIdenticalAcrossSplits)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    constexpr size_t N = 100'003;  // prime > kElementwiseThreshold
    auto src = makeReals(N, 7, -1e6, 1e6);

    Value x = makeDoubleVector(mr, src);
    Value y = numkit::builtin::abs(x, mr);
    ASSERT_EQ(y.numel(), N);
    for (size_t i = 0; i < N; ++i)
        EXPECT_TRUE(bitEquals(y.doubleData()[i], std::fabs(src[i]))) << "i=" << i;
}

TEST(SimdParity_ParallelLarge, PlusBitIdenticalAcrossSplits)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    constexpr size_t N = 100'003;
    auto a = makeReals(N, 11, -1e3, 1e3);
    auto b = makeReals(N, 13, -1e3, 1e3);

    Value x = makeDoubleVector(mr, a);
    Value y = makeDoubleVector(mr, b);
    Value z = numkit::builtin::plus(x, y, mr);
    ASSERT_EQ(z.numel(), N);
    for (size_t i = 0; i < N; ++i)
        EXPECT_TRUE(bitEquals(z.doubleData()[i], a[i] + b[i])) << "i=" << i;
}

TEST(SimdParity_ParallelLarge, MinusBitIdenticalAcrossSplits)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    constexpr size_t N = 100'003;
    auto a = makeReals(N, 17, -1e3, 1e3);
    auto b = makeReals(N, 19, -1e3, 1e3);

    Value x = makeDoubleVector(mr, a);
    Value y = makeDoubleVector(mr, b);
    Value z = numkit::builtin::minus(x, y, mr);
    ASSERT_EQ(z.numel(), N);
    for (size_t i = 0; i < N; ++i)
        EXPECT_TRUE(bitEquals(z.doubleData()[i], a[i] - b[i])) << "i=" << i;
}

TEST(SimdParity_ParallelLarge, TimesBitIdenticalAcrossSplits)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    constexpr size_t N = 100'003;
    auto a = makeReals(N, 23, -1e3, 1e3);
    auto b = makeReals(N, 29, -1e3, 1e3);

    Value x = makeDoubleVector(mr, a);
    Value y = makeDoubleVector(mr, b);
    Value z = numkit::builtin::times(x, y, mr);
    ASSERT_EQ(z.numel(), N);
    for (size_t i = 0; i < N; ++i)
        EXPECT_TRUE(bitEquals(z.doubleData()[i], a[i] * b[i])) << "i=" << i;
}

TEST(SimdParity_ParallelLarge, RdivideBitIdenticalAcrossSplits)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    constexpr size_t N = 100'003;
    auto a = makeReals(N, 31, -1e3, 1e3);
    auto b = makeReals(N, 37, 0.5, 1e3);   // strictly positive denom

    Value x = makeDoubleVector(mr, a);
    Value y = makeDoubleVector(mr, b);
    Value z = numkit::builtin::rdivide(x, y, mr);
    ASSERT_EQ(z.numel(), N);
    for (size_t i = 0; i < N; ++i)
        EXPECT_TRUE(bitEquals(z.doubleData()[i], a[i] / b[i])) << "i=" << i;
}

// Transcendentals: SIMD math approximations have non-zero ULP error
// vs std::sin/cos/exp/log, but the per-element output must still be
// independent of how the array is split — assert bit-identical across
// any potential split by repeating the call several times. (Each call
// dispatches the same chunking; a thread-induced races would surface
// as flaky bit changes between calls.)
TEST(SimdParity_ParallelLarge, SinDeterministicAcrossCalls)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    constexpr size_t N = 100'003;     // also above kTranscendentalThreshold
    auto src = makeReals(N, 41, -10.0, 10.0);

    Value x  = makeDoubleVector(mr, src);
    Value y1 = numkit::builtin::sin(x, mr);
    Value y2 = numkit::builtin::sin(x, mr);
    Value y3 = numkit::builtin::sin(x, mr);
    ASSERT_EQ(y1.numel(), N);
    for (size_t i = 0; i < N; ++i) {
        EXPECT_TRUE(bitEquals(y1.doubleData()[i], y2.doubleData()[i])) << "i=" << i;
        EXPECT_TRUE(bitEquals(y2.doubleData()[i], y3.doubleData()[i])) << "i=" << i;
    }
}

TEST(SimdParity_ParallelLarge, ExpDeterministicAcrossCalls)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    constexpr size_t N = 100'003;
    auto src = makeReals(N, 43, -5.0, 5.0);

    Value x  = makeDoubleVector(mr, src);
    Value y1 = numkit::builtin::exp(x, mr);
    Value y2 = numkit::builtin::exp(x, mr);
    ASSERT_EQ(y1.numel(), N);
    for (size_t i = 0; i < N; ++i)
        EXPECT_TRUE(bitEquals(y1.doubleData()[i], y2.doubleData()[i])) << "i=" << i;
}

// Just-at-the-threshold and just-above to exercise the boundary.
TEST(SimdParity_ParallelLarge, ExactBoundaryCases)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    for (size_t N : {size_t(16383), size_t(16384), size_t(16385), size_t(65536)}) {
        auto a = makeReals(N, 53 + static_cast<uint32_t>(N), -1.0, 1.0);
        auto b = makeReals(N, 59 + static_cast<uint32_t>(N), -1.0, 1.0);
        Value x = makeDoubleVector(mr, a);
        Value y = makeDoubleVector(mr, b);
        Value z = numkit::builtin::plus(x, y, mr);
        ASSERT_EQ(z.numel(), N) << "N=" << N;
        for (size_t i = 0; i < N; ++i)
            EXPECT_TRUE(bitEquals(z.doubleData()[i], a[i] + b[i]))
                << "N=" << N << " i=" << i;
    }
}

// ════════════════════════════════════════════════════════════════════════
// BLAS SIMD Microkernel Parity (gemm, gemv, ger, trsm)
// ════════════════════════════════════════════════════════════════════════

#include <numkit/ops/blas.hpp>

TEST(SimdParity_Blas, GemmRealOddTails)
{
    std::mt19937 rng(42);
    std::uniform_real_distribution<double> dist(-2.0, 2.0);

    for (size_t n : {size_t(1), size_t(3), size_t(17), size_t(65), size_t(257)}) {
        std::vector<double> A(n * n), B(n * n), C_simd(n * n, 0.0), C_ref(n * n, 0.0);
        for (size_t i = 0; i < n * n; ++i) {
            A[i] = dist(rng);
            B[i] = dist(rng);
        }

        numkit::ops::gemm(numkit::ops::MatrixTranspose::NoTrans, numkit::ops::MatrixTranspose::NoTrans, n, n, n, 1.0, A.data(), n, B.data(), n, 0.0, C_simd.data(), n);

        for (size_t j = 0; j < n; ++j) {
            for (size_t k = 0; k < n; ++k) {
                const double bkj = B[k + j * n];
                for (size_t i = 0; i < n; ++i) {
                    C_ref[i + j * n] += A[i + k * n] * bkj;
                }
            }
        }

        for (size_t i = 0; i < n * n; ++i) {
            EXPECT_NEAR(C_simd[i], C_ref[i], 1e-10) << "n=" << n << " at index " << i;
        }
    }
}

TEST(SimdParity_Blas, GemmComplexOddTails)
{
    using Complex = std::complex<double>;
    std::mt19937 rng(1337);
    std::uniform_real_distribution<double> dist(-2.0, 2.0);

    for (size_t n : {size_t(1), size_t(3), size_t(17), size_t(65)}) {
        std::vector<Complex> A(n * n), B(n * n), C_simd(n * n, Complex(0,0)), C_ref(n * n, Complex(0,0));
        for (size_t i = 0; i < n * n; ++i) {
            A[i] = Complex(dist(rng), dist(rng));
            B[i] = Complex(dist(rng), dist(rng));
        }

        numkit::ops::gemm(numkit::ops::MatrixTranspose::NoTrans, numkit::ops::MatrixTranspose::NoTrans, n, n, n, Complex(1.0, 0.0), A.data(), n, B.data(), n, Complex(0.0, 0.0), C_simd.data(), n);

        for (size_t j = 0; j < n; ++j) {
            for (size_t k = 0; k < n; ++k) {
                const Complex bkj = B[k + j * n];
                for (size_t i = 0; i < n; ++i) {
                    C_ref[i + j * n] += A[i + k * n] * bkj;
                }
            }
        }

        for (size_t i = 0; i < n * n; ++i) {
            EXPECT_NEAR(C_simd[i].real(), C_ref[i].real(), 1e-10) << "n=" << n << " at index " << i;
            EXPECT_NEAR(C_simd[i].imag(), C_ref[i].imag(), 1e-10) << "n=" << n << " at index " << i;
        }
    }
}

TEST(SimdParity_Blas, GemvRealOddTails)
{
    std::mt19937 rng(7);
    std::uniform_real_distribution<double> dist(-2.0, 2.0);

    for (size_t n : {size_t(1), size_t(7), size_t(33), size_t(129)}) {
        std::vector<double> A(n * n), x(n), y_simd(n, 0.0), y_ref(n, 0.0);
        for (size_t i = 0; i < n * n; ++i) A[i] = dist(rng);
        for (size_t i = 0; i < n; ++i) x[i] = dist(rng);

        numkit::ops::gemv(n, n, 1.0, A.data(), n, x.data(), 1, 0.0, y_simd.data(), 1);

        for (size_t j = 0; j < n; ++j) {
            for (size_t i = 0; i < n; ++i) {
                y_ref[i] += A[i + j * n] * x[j];
            }
        }

        for (size_t i = 0; i < n; ++i) {
            EXPECT_NEAR(y_simd[i], y_ref[i], 1e-10) << "n=" << n << " at index " << i;
        }
    }
}

TEST(SimdParity_Blas, GerRealOddTails)
{
    std::mt19937 rng(99);
    std::uniform_real_distribution<double> dist(-2.0, 2.0);

    for (size_t n : {size_t(1), size_t(7), size_t(33), size_t(129)}) {
        std::vector<double> x(n), y(n), A_simd(n * n, 0.0), A_ref(n * n, 0.0);
        for (size_t i = 0; i < n; ++i) { x[i] = dist(rng); y[i] = dist(rng); }

        numkit::ops::ger(n, n, 1.0, x.data(), 1, y.data(), 1, A_simd.data(), n);

        for (size_t j = 0; j < n; ++j) {
            for (size_t i = 0; i < n; ++i) {
                A_ref[i + j * n] += x[i] * y[j];
            }
        }

        for (size_t i = 0; i < n * n; ++i) {
            EXPECT_NEAR(A_simd[i], A_ref[i], 1e-10) << "n=" << n << " at index " << i;
        }
    }
}

TEST(SimdParity_Blas, TrsmRealLowerSolve)
{
    std::mt19937 rng(101);
    std::uniform_real_distribution<double> dist(0.5, 2.0);

    for (size_t n : {size_t(1), size_t(7), size_t(33)}) {
        std::vector<double> L(n * n, 0.0), B(n * n), B_copy(n * n);
        for (size_t i = 0; i < n; ++i) {
            for (size_t j = 0; j <= i; ++j) {
                L[i + j * n] = dist(rng);
            }
        }
        for (size_t i = 0; i < n * n; ++i) B[i] = B_copy[i] = dist(rng);

        numkit::ops::trsm(numkit::ops::MatrixSide::Left, numkit::ops::MatrixUplo::Lower,
                          numkit::ops::MatrixTranspose::NoTrans, numkit::ops::MatrixDiag::NonUnit,
                          n, n, 1.0, L.data(), n, B.data(), n);

        // Verify L * X == B_copy
        std::vector<double> B_check(n * n, 0.0);
        for (size_t j = 0; j < n; ++j) {
            for (size_t k = 0; k < n; ++k) {
                for (size_t i = 0; i < n; ++i) {
                    B_check[i + j * n] += L[i + k * n] * B[k + j * n];
                }
            }
        }
        for (size_t i = 0; i < n * n; ++i) {
            EXPECT_NEAR(B_check[i], B_copy[i], 1e-10) << "n=" << n << " at index " << i;
        }
    }
}

TEST(SimdParity_Blas, GemmP1_RandomSizes)
{
    std::mt19937 rng(404);
    std::uniform_real_distribution<double> dist(-2.0, 2.0);

    for (size_t n : {size_t(63), size_t(64), size_t(65), size_t(127), size_t(129), size_t(255), size_t(257), size_t(513)}) {
        std::vector<double> A(n * n), B(n * n), C_simd(n * n, 1.5), C_ref(n * n, 1.5);
        for (size_t i = 0; i < n * n; ++i) {
            A[i] = dist(rng);
            B[i] = dist(rng);
        }

        const double alpha = 1.25;
        const double beta = 0.75;

        numkit::ops::gemm(numkit::ops::MatrixTranspose::NoTrans, numkit::ops::MatrixTranspose::NoTrans, n, n, n, alpha, A.data(), n, B.data(), n, beta, C_simd.data(), n);

        for (size_t j = 0; j < n; ++j) {
            for (size_t k = 0; k < n; ++k) {
                const double bkj = alpha * B[k + j * n];
                for (size_t i = 0; i < n; ++i) {
                    if (k == 0) C_ref[i + j * n] *= beta;
                    C_ref[i + j * n] += A[i + k * n] * bkj;
                }
            }
        }

        for (size_t i = 0; i < n * n; ++i) {
            EXPECT_NEAR(C_simd[i], C_ref[i], 1e-9) << "n=" << n << " at index " << i;
        }
    }
}

TEST(SimdParity_Blas, GemmP1_NanInfPropagation)
{
    std::vector<double> A = {1.0, 2.0, 3.0, 4.0};
    std::vector<double> B = {0.0, std::numeric_limits<double>::quiet_NaN(), 0.0, 1.0};
    std::vector<double> C(4, 0.0);

    numkit::ops::gemm(numkit::ops::MatrixTranspose::NoTrans, numkit::ops::MatrixTranspose::NoTrans, 2, 2, 2, 1.0, A.data(), 2, B.data(), 2, 0.0, C.data(), 2);
    EXPECT_TRUE(std::isnan(C[0]) || std::isnan(C[1]) || std::isnan(C[2]) || std::isnan(C[3]));
}
TEST(SimdParity_Blas, GemmP2_BitwiseDeterminism)
{
    std::mt19937 rng(808);
    std::uniform_real_distribution<double> dist(-2.0, 2.0);
    const size_t n = 256;
    std::vector<double> A(n * n), B(n * n), C1(n * n, 0.0), C2(n * n, 0.0);
    for (size_t i = 0; i < n * n; ++i) {
        A[i] = dist(rng);
        B[i] = dist(rng);
    }

    numkit::ops::gemm(numkit::ops::MatrixTranspose::NoTrans, numkit::ops::MatrixTranspose::NoTrans, n, n, n, 1.25, A.data(), n, B.data(), n, 0.0, C1.data(), n);
    EXPECT_GT(numkit::ops::get_last_gemm_threads_used(), 1u);

    numkit::ops::gemm(numkit::ops::MatrixTranspose::NoTrans, numkit::ops::MatrixTranspose::NoTrans, n, n, n, 1.25, A.data(), n, B.data(), n, 0.0, C2.data(), n);
    EXPECT_GT(numkit::ops::get_last_gemm_threads_used(), 1u);

    for (size_t i = 0; i < n * n; ++i) {
        EXPECT_EQ(C1[i], C2[i]) << "Bitwise discrepancy at index " << i;
    }
}

TEST(SimdParity_Blas, TrsmP4_AllCombosRealAndComplex)
{
    std::mt19937 rng(505);
    std::uniform_real_distribution<double> dist(0.5, 2.0);

    const size_t m = 7, n = 7;
    for (auto side : {numkit::ops::MatrixSide::Left, numkit::ops::MatrixSide::Right}) {
        for (auto uplo : {numkit::ops::MatrixUplo::Lower, numkit::ops::MatrixUplo::Upper}) {
            for (auto trans : {numkit::ops::MatrixTranspose::NoTrans, numkit::ops::MatrixTranspose::Trans, numkit::ops::MatrixTranspose::ConjTrans}) {
                for (auto diag : {numkit::ops::MatrixDiag::NonUnit, numkit::ops::MatrixDiag::Unit}) {
                    // Real double test
                    {
                        std::vector<double> A(m * m, 0.0), B(m * n), B_copy(m * n);
                        for (size_t j = 0; j < m; ++j) {
                            for (size_t i = 0; i < m; ++i) {
                                if (uplo == numkit::ops::MatrixUplo::Lower && i >= j) A[i + j * m] = dist(rng);
                                else if (uplo == numkit::ops::MatrixUplo::Upper && i <= j) A[i + j * m] = dist(rng);
                                if (i == j && diag == numkit::ops::MatrixDiag::Unit) A[i + j * m] = 1.0;
                            }
                        }
                        for (size_t i = 0; i < m * n; ++i) B[i] = B_copy[i] = dist(rng);

                        numkit::ops::trsm(side, uplo, trans, diag, m, n, 1.25, A.data(), m, B.data(), m);

                        double max_err = 0.0;
                        for (size_t j = 0; j < n; ++j) {
                            for (size_t i = 0; i < m; ++i) {
                                double lhs = 0.0;
                                if (side == numkit::ops::MatrixSide::Left) {
                                    for (size_t k = 0; k < m; ++k) {
                                        double a_val = (trans == numkit::ops::MatrixTranspose::NoTrans) ? A[i + k * m] : A[k + i * m];
                                        lhs += a_val * B[k + j * m];
                                    }
                                } else {
                                    for (size_t k = 0; k < n; ++k) {
                                        double a_val = (trans == numkit::ops::MatrixTranspose::NoTrans) ? A[k + j * n] : A[j + k * n];
                                        lhs += B[i + k * m] * a_val;
                                    }
                                }
                                max_err = std::max(max_err, std::abs(lhs - 1.25 * B_copy[i + j * m]));
                            }
                        }
                        EXPECT_LT(max_err, 1e-9);
                    }

                    // Complex double test
                    {
                        using Complex = std::complex<double>;
                        std::vector<Complex> A(m * m, Complex(0.0, 0.0)), B(m * n), B_copy(m * n);
                        for (size_t j = 0; j < m; ++j) {
                            for (size_t i = 0; i < m; ++i) {
                                if (uplo == numkit::ops::MatrixUplo::Lower && i >= j) A[i + j * m] = Complex(dist(rng), dist(rng));
                                else if (uplo == numkit::ops::MatrixUplo::Upper && i <= j) A[i + j * m] = Complex(dist(rng), dist(rng));
                                if (i == j && diag == numkit::ops::MatrixDiag::Unit) A[i + j * m] = Complex(1.0, 0.0);
                            }
                        }
                        for (size_t i = 0; i < m * n; ++i) B[i] = B_copy[i] = Complex(dist(rng), dist(rng));

                        Complex alpha(1.25, -0.5);
                        numkit::ops::trsm(side, uplo, trans, diag, m, n, alpha, A.data(), m, B.data(), m);

                        double max_err = 0.0;
                        for (size_t j = 0; j < n; ++j) {
                            for (size_t i = 0; i < m; ++i) {
                                Complex lhs(0.0, 0.0);
                                if (side == numkit::ops::MatrixSide::Left) {
                                    for (size_t k = 0; k < m; ++k) {
                                        Complex a_val = (trans == numkit::ops::MatrixTranspose::NoTrans) ? A[i + k * m] : A[k + i * m];
                                        if (trans == numkit::ops::MatrixTranspose::ConjTrans) a_val = std::conj(a_val);
                                        lhs += a_val * B[k + j * m];
                                    }
                                } else {
                                    for (size_t k = 0; k < n; ++k) {
                                        Complex a_val = (trans == numkit::ops::MatrixTranspose::NoTrans) ? A[k + j * n] : A[j + k * n];
                                        if (trans == numkit::ops::MatrixTranspose::ConjTrans) a_val = std::conj(a_val);
                                        lhs += B[i + k * m] * a_val;
                                    }
                                }
                                max_err = std::max(max_err, std::abs(lhs - alpha * B_copy[i + j * m]));
                            }
                        }
                        EXPECT_LT(max_err, 1e-9);
                    }
                }
            }
        }
    }
}

TEST(SimdParity_Blas, SyrkP4_ParityTest)
{
    using Complex = std::complex<double>;
    std::mt19937 rng(606);
    std::uniform_real_distribution<double> dist(-2.0, 2.0);

    const size_t n = 7, k = 5;

    // 1. Real Syrk test (NoTrans & Trans)
    for (auto trans : {numkit::ops::MatrixTranspose::NoTrans, numkit::ops::MatrixTranspose::Trans}) {
        size_t rows_a = (trans == numkit::ops::MatrixTranspose::NoTrans) ? n : k;
        size_t cols_a = (trans == numkit::ops::MatrixTranspose::NoTrans) ? k : n;
        std::vector<double> A(rows_a * cols_a), C_simd(n * n, 1.0), C_ref(n * n, 1.0);
        for (size_t i = 0; i < rows_a * cols_a; ++i) A[i] = dist(rng);

        numkit::ops::syrk(numkit::ops::MatrixUplo::Lower, trans, n, k, 1.5, A.data(), rows_a, 0.5, C_simd.data(), n);

        for (size_t j = 0; j < n; ++j) {
            for (size_t i = j; i < n; ++i) {
                double acc = 0.0;
                for (size_t l = 0; l < k; ++l) {
                    double a_ik = (trans == numkit::ops::MatrixTranspose::NoTrans) ? A[i + l * rows_a] : A[l + i * rows_a];
                    double a_jk = (trans == numkit::ops::MatrixTranspose::NoTrans) ? A[j + l * rows_a] : A[l + j * rows_a];
                    acc += a_ik * a_jk;
                }
                C_ref[i + j * n] = 0.5 * C_ref[i + j * n] + 1.5 * acc;
            }
        }

        for (size_t j = 0; j < n; ++j) {
            for (size_t i = j; i < n; ++i) {
                EXPECT_NEAR(C_simd[i + j * n], C_ref[i + j * n], 1e-10);
            }
        }
    }

    // 2. Complex Herk test (NoTrans & ConjTrans) (P4-b critical fix test)
    for (auto trans : {numkit::ops::MatrixTranspose::NoTrans, numkit::ops::MatrixTranspose::ConjTrans}) {
        size_t rows_a = (trans == numkit::ops::MatrixTranspose::NoTrans) ? n : k;
        size_t cols_a = (trans == numkit::ops::MatrixTranspose::NoTrans) ? k : n;
        std::vector<Complex> A(rows_a * cols_a), C_simd(n * n, Complex(1.0, 0.5)), C_ref(n * n, Complex(1.0, 0.5));
        for (size_t i = 0; i < rows_a * cols_a; ++i) A[i] = Complex(dist(rng), dist(rng));

        Complex alpha(1.5, -0.25);
        Complex beta(0.5, 0.1);
        numkit::ops::syrk(numkit::ops::MatrixUplo::Lower, trans, n, k, alpha, A.data(), rows_a, beta, C_simd.data(), n);

        for (size_t j = 0; j < n; ++j) {
            for (size_t i = j; i < n; ++i) {
                Complex acc(0.0, 0.0);
                for (size_t l = 0; l < k; ++l) {
                    if (trans == numkit::ops::MatrixTranspose::NoTrans) {
                        // C = alpha * A * A^T + beta * C
                        acc += A[i + l * rows_a] * A[j + l * rows_a];
                    } else {
                        // C = alpha * A^H * A + beta * C
                        acc += std::conj(A[l + i * rows_a]) * A[l + j * rows_a];
                    }
                }
                C_ref[i + j * n] = beta * C_ref[i + j * n] + alpha * acc;
            }
        }

        double max_err = 0.0;
        for (size_t j = 0; j < n; ++j) {
            for (size_t i = j; i < n; ++i) {
                max_err = std::max(max_err, std::abs(C_simd[i + j * n] - C_ref[i + j * n]));
            }
        }
        EXPECT_LT(max_err, 1e-12);
    }
}




