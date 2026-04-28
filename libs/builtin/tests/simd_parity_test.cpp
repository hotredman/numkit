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

#include <numkit/builtin/lang/operators/binary_ops.hpp>
#include <numkit/builtin/math/elementary/exponents.hpp>
#include <numkit/builtin/math/elementary/rounding.hpp>
#include <numkit/builtin/math/elementary/trigonometry.hpp>

#include <numkit/core/allocator.hpp>
#include <numkit/core/types.hpp>
#include <numkit/core/value.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <cstring>
#include <limits>
#include <random>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using numkit::Allocator;
using numkit::ValueType;
using numkit::Value;

namespace {

Value makeDoubleVector(Allocator &alloc, const std::vector<double> &vals)
{
    Value v = Value::matrix(vals.size(), 1, ValueType::DOUBLE, &alloc);
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

    Value x = makeDoubleVector(alloc, src);
    Value y = numkit::builtin::abs(alloc, x);

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

    Value x = makeDoubleVector(alloc, src);
    Value y = numkit::builtin::abs(alloc, x);

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
    Value x = Value::scalar(-3.5, &alloc);
    Value y = numkit::builtin::abs(alloc, x);
    EXPECT_TRUE(bitEquals(y.toScalar(), 3.5));
}

TEST(SimdParity_Abs, ComplexFallsBackToScalarImpl)
{
    Allocator alloc = Allocator::defaultAllocator();

    // The SIMD backend shouldn't touch the complex path — it delegates
    // to std::abs(Complex). A quick check that this path still works
    // under the SIMD build.
    auto v = Value::complexMatrix(1, 3, &alloc);
    v.complexDataMut()[0] = {3.0, 4.0};
    v.complexDataMut()[1] = {-5.0, 12.0};
    v.complexDataMut()[2] = {0.0, 0.0};

    Value y = numkit::builtin::abs(alloc, v);
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
    Allocator alloc = Allocator::defaultAllocator();

    constexpr size_t N = 1021;
    std::mt19937 rng(271828);
    std::uniform_real_distribution<double> dist(lo, hi);

    std::vector<double> src(N);
    for (auto &v : src) v = dist(rng);

    Value x = makeDoubleVector(alloc, src);
    Value y = simdFn(alloc, x);

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
        [](Allocator &a, const Value &x) { return numkit::builtin::sin(a, x); },
        [](double x) { return std::sin(x); },
        -10.0, 10.0, "sin");
}

TEST(SimdParity_Cos, WithinUlpBudget)
{
    checkTranscendentalParity(
        [](Allocator &a, const Value &x) { return numkit::builtin::cos(a, x); },
        [](double x) { return std::cos(x); },
        -10.0, 10.0, "cos");
}

TEST(SimdParity_Exp, WithinUlpBudget)
{
    // Clamp to a range where exp() doesn't overflow — past ~709 it
    // becomes Inf and ULP distance is undefined / infinite.
    checkTranscendentalParity(
        [](Allocator &a, const Value &x) { return numkit::builtin::exp(a, x); },
        [](double x) { return std::exp(x); },
        -5.0, 5.0, "exp");
}

TEST(SimdParity_Log, WithinUlpBudget)
{
    // Strictly positive inputs — negatives produce NaN, whose ULP
    // distance doesn't compare meaningfully.
    checkTranscendentalParity(
        [](Allocator &a, const Value &x) { return numkit::builtin::log(a, x); },
        [](double x) { return std::log(x); },
        0.01, 100.0, "log");
}

TEST(SimdParity_Transcendental, NegativeLogScalarStillComplex)
{
    Allocator alloc = Allocator::defaultAllocator();
    // MATLAB contract (preserved in both backends): scalar log(-1) → i·π.
    Value y = numkit::builtin::log(alloc, Value::scalar(-1.0, &alloc));
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
    Allocator alloc = Allocator::defaultAllocator();

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

    Value A = makeDoubleVector(alloc, av);
    Value B = makeDoubleVector(alloc, bv);
    Value Y = simdFn(alloc, A, B);

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
        [](Allocator &a, const Value &x, const Value &y) { return numkit::builtin::plus(a, x, y); },
        [](double x, double y) { return x + y; }, "plus");
}

TEST(SimdParity_Minus,   MatchesScalar)
{
    checkBinaryParity(
        [](Allocator &a, const Value &x, const Value &y) { return numkit::builtin::minus(a, x, y); },
        [](double x, double y) { return x - y; }, "minus");
}

TEST(SimdParity_Times,   MatchesScalar)
{
    checkBinaryParity(
        [](Allocator &a, const Value &x, const Value &y) { return numkit::builtin::times(a, x, y); },
        [](double x, double y) { return x * y; }, "times");
}

TEST(SimdParity_Rdivide, MatchesScalar)
{
    checkBinaryParity(
        [](Allocator &a, const Value &x, const Value &y) { return numkit::builtin::rdivide(a, x, y); },
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
    Allocator alloc = Allocator::defaultAllocator();

    // 128 is large enough to accumulate FMA/non-FMA divergence but
    // small enough to compute a naive reference loop in the test.
    constexpr size_t N = 128;
    std::mt19937 rng(97);
    std::uniform_real_distribution<double> dist(-1.0, 1.0);

    Value A = Value::matrix(N, N, ValueType::DOUBLE, &alloc);
    Value B = Value::matrix(N, N, ValueType::DOUBLE, &alloc);
    for (size_t i = 0; i < N * N; ++i) {
        A.doubleDataMut()[i] = dist(rng);
        B.doubleDataMut()[i] = dist(rng);
    }

    Value C = numkit::builtin::mtimes(alloc, A, B);
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
    Allocator alloc = Allocator::defaultAllocator();

    constexpr size_t M = 16;
    constexpr size_t K = 300;
    constexpr size_t N = 12;

    std::mt19937 rng(123);
    std::uniform_real_distribution<double> dist(-1.0, 1.0);

    Value A = Value::matrix(M, K, ValueType::DOUBLE, &alloc);
    Value B = Value::matrix(K, N, ValueType::DOUBLE, &alloc);
    for (size_t i = 0; i < M * K; ++i) A.doubleDataMut()[i] = dist(rng);
    for (size_t i = 0; i < K * N; ++i) B.doubleDataMut()[i] = dist(rng);

    Value C = numkit::builtin::mtimes(alloc, A, B);
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
    Allocator alloc = Allocator::defaultAllocator();

    constexpr size_t M = 16;
    constexpr size_t K = 768;   // = 3 * 256, exact multiple of KC
    constexpr size_t N = 8;

    std::mt19937 rng(456);
    std::uniform_real_distribution<double> dist(-1.0, 1.0);

    Value A = Value::matrix(M, K, ValueType::DOUBLE, &alloc);
    Value B = Value::matrix(K, N, ValueType::DOUBLE, &alloc);
    for (size_t i = 0; i < M * K; ++i) A.doubleDataMut()[i] = dist(rng);
    for (size_t i = 0; i < K * N; ++i) B.doubleDataMut()[i] = dist(rng);

    Value C = numkit::builtin::mtimes(alloc, A, B);

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
    Allocator alloc = Allocator::defaultAllocator();
    auto x = Value::matrix(1, 256, ValueType::DOUBLE, &alloc);
    for (size_t i = 0; i < 256; ++i) x.doubleDataMut()[i] = -double(i);
    auto y = numkit::builtin::abs(alloc, x);
    EXPECT_EQ(y.dims().rows(), 1u);
    EXPECT_EQ(y.dims().cols(), 256u);
    for (size_t i = 0; i < 256; ++i)
        EXPECT_TRUE(bitEquals(y.doubleData()[i], double(i)));
}

TEST(SimdParity_Dim, AbsOn1DColumn)
{
    Allocator alloc = Allocator::defaultAllocator();
    auto x = Value::matrix(256, 1, ValueType::DOUBLE, &alloc);
    for (size_t i = 0; i < 256; ++i) x.doubleDataMut()[i] = -double(i);
    auto y = numkit::builtin::abs(alloc, x);
    EXPECT_EQ(y.dims().rows(), 256u);
    EXPECT_EQ(y.dims().cols(), 1u);
    for (size_t i = 0; i < 256; ++i)
        EXPECT_TRUE(bitEquals(y.doubleData()[i], double(i)));
}

TEST(SimdParity_Dim, AbsOn3D)
{
    Allocator alloc = Allocator::defaultAllocator();
    auto x = Value::matrix3d(3, 4, 5, ValueType::DOUBLE, &alloc);
    for (size_t i = 0; i < x.numel(); ++i) x.doubleDataMut()[i] = -double(i);
    auto y = numkit::builtin::abs(alloc, x);
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
    Allocator alloc = Allocator::defaultAllocator();
    auto x = Value::matrix3d(2, 3, 4, ValueType::DOUBLE, &alloc);
    for (size_t i = 0; i < x.numel(); ++i) x.doubleDataMut()[i] = 0.1 * double(i);
    auto y = numkit::builtin::sin(alloc, x);
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
    Allocator alloc = Allocator::defaultAllocator();
    auto a = Value::matrix3d(3, 5, 7, ValueType::DOUBLE, &alloc);  // 105 elems, all odd
    auto b = Value::matrix3d(3, 5, 7, ValueType::DOUBLE, &alloc);

    std::mt19937 rng(65537);
    std::uniform_real_distribution<double> dist(-100.0, 100.0);
    for (size_t i = 0; i < a.numel(); ++i) {
        a.doubleDataMut()[i] = dist(rng);
        double bv = dist(rng);
        if (std::fabs(bv) < 1.0) bv += std::copysign(1.0, bv);  // avoid /0 for rdivide
        b.doubleDataMut()[i] = bv;
    }

    auto c = fn(alloc, a, b);
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
        [](Allocator &a, const Value &x, const Value &y) { return numkit::builtin::plus(a, x, y); },
        [](double x, double y) { return x + y; }, "plus");
}

TEST(SimdParity_Dim, MinusOn3D)
{
    checkBinaryOn3D(
        [](Allocator &a, const Value &x, const Value &y) { return numkit::builtin::minus(a, x, y); },
        [](double x, double y) { return x - y; }, "minus");
}

TEST(SimdParity_Dim, TimesOn3D)
{
    checkBinaryOn3D(
        [](Allocator &a, const Value &x, const Value &y) { return numkit::builtin::times(a, x, y); },
        [](double x, double y) { return x * y; }, "times");
}

TEST(SimdParity_Dim, RdivideOn3D)
{
    checkBinaryOn3D(
        [](Allocator &a, const Value &x, const Value &y) { return numkit::builtin::rdivide(a, x, y); },
        [](double x, double y) { return x / y; }, "rdivide");
}

TEST(SimdParity_Dim, PlusOn1DRowAndColumn)
{
    Allocator alloc = Allocator::defaultAllocator();
    // Row vector: cols > 1, rows = 1 — NOT excluded by the fast path,
    // so goes through SIMD when NUMKIT_WITH_SIMD=ON.
    auto aRow = Value::matrix(1, 256, ValueType::DOUBLE, &alloc);
    auto bRow = Value::matrix(1, 256, ValueType::DOUBLE, &alloc);
    for (size_t i = 0; i < 256; ++i) {
        aRow.doubleDataMut()[i] = double(i);
        bRow.doubleDataMut()[i] = 3.0;
    }
    auto cRow = numkit::builtin::plus(alloc, aRow, bRow);
    EXPECT_EQ(cRow.dims().rows(), 1u);
    EXPECT_EQ(cRow.dims().cols(), 256u);
    for (size_t i = 0; i < 256; ++i)
        EXPECT_TRUE(bitEquals(cRow.doubleData()[i], double(i) + 3.0));

    // Column vector: rows > 1, cols = 1 — also SIMD-eligible.
    auto aCol = Value::matrix(256, 1, ValueType::DOUBLE, &alloc);
    auto bCol = Value::matrix(256, 1, ValueType::DOUBLE, &alloc);
    for (size_t i = 0; i < 256; ++i) {
        aCol.doubleDataMut()[i] = double(i);
        bCol.doubleDataMut()[i] = 3.0;
    }
    auto cCol = numkit::builtin::plus(alloc, aCol, bCol);
    EXPECT_EQ(cCol.dims().rows(), 256u);
    EXPECT_EQ(cCol.dims().cols(), 1u);
    for (size_t i = 0; i < 256; ++i)
        EXPECT_TRUE(bitEquals(cCol.doubleData()[i], double(i) + 3.0));
}

TEST(SimdParity_Mtimes, ThrowsOn3DInput)
{
    Allocator alloc = Allocator::defaultAllocator();

    // 3D * 3D — matrix multiply is undefined here; must throw, not
    // silently strip pages and produce garbage. Pre-Phase 8 this
    // quietly used (rows, cols) and ignored pages.
    {
        auto A = Value::matrix3d(3, 4, 2, ValueType::DOUBLE, &alloc);
        auto B = Value::matrix3d(4, 3, 2, ValueType::DOUBLE, &alloc);
        for (size_t i = 0; i < A.numel(); ++i) A.doubleDataMut()[i] = 1.0;
        for (size_t i = 0; i < B.numel(); ++i) B.doubleDataMut()[i] = 1.0;
        EXPECT_THROW({ (void)numkit::builtin::mtimes(alloc, A, B); },
                     numkit::Error);
    }

    // 3D * 2D — also undefined.
    {
        auto A = Value::matrix3d(3, 4, 2, ValueType::DOUBLE, &alloc);
        auto B = Value::matrix(4, 3, ValueType::DOUBLE, &alloc);
        for (size_t i = 0; i < A.numel(); ++i) A.doubleDataMut()[i] = 1.0;
        for (size_t i = 0; i < B.numel(); ++i) B.doubleDataMut()[i] = 1.0;
        EXPECT_THROW({ (void)numkit::builtin::mtimes(alloc, A, B); },
                     numkit::Error);
    }
}

TEST(SimdParity_Mtimes, ScalarTimes3DArrayStillWorks)
{
    Allocator alloc = Allocator::defaultAllocator();

    // scalar * 3D — MATLAB degenerates this to elementwise scaling;
    // our code routes it through elementwiseDouble() and the result
    // preserves the 3D shape with every element scaled.
    auto A = Value::matrix3d(2, 3, 4, ValueType::DOUBLE, &alloc);
    for (size_t i = 0; i < A.numel(); ++i) A.doubleDataMut()[i] = double(i);
    auto s = Value::scalar(2.5, &alloc);

    auto C = numkit::builtin::mtimes(alloc, s, A);
    ASSERT_TRUE(C.dims().is3D());
    EXPECT_EQ(C.dims().pages(), 4u);
    EXPECT_EQ(C.numel(), 24u);
    for (size_t i = 0; i < C.numel(); ++i)
        EXPECT_DOUBLE_EQ(C.doubleData()[i], 2.5 * double(i));
}

TEST(SimdParity_Mtimes, HandlesNonSquareDimensions)
{
    Allocator alloc = Allocator::defaultAllocator();

    // M=37, K=51, N=43 — all odd, none a multiple of typical SIMD
    // widths (2/4/8). Exercises the scalar tail on every loop.
    constexpr size_t M = 37, K = 51, N = 43;
    std::mt19937 rng(11);
    std::uniform_real_distribution<double> dist(-1.0, 1.0);

    Value A = Value::matrix(M, K, ValueType::DOUBLE, &alloc);
    Value B = Value::matrix(K, N, ValueType::DOUBLE, &alloc);
    for (size_t i = 0; i < M * K; ++i) A.doubleDataMut()[i] = dist(rng);
    for (size_t i = 0; i < K * N; ++i) B.doubleDataMut()[i] = dist(rng);

    Value C = numkit::builtin::mtimes(alloc, A, B);
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
    Allocator alloc = Allocator::defaultAllocator();
    constexpr size_t N = 100'003;  // prime > kElementwiseThreshold
    auto src = makeReals(N, 7, -1e6, 1e6);

    Value x = makeDoubleVector(alloc, src);
    Value y = numkit::builtin::abs(alloc, x);
    ASSERT_EQ(y.numel(), N);
    for (size_t i = 0; i < N; ++i)
        EXPECT_TRUE(bitEquals(y.doubleData()[i], std::fabs(src[i]))) << "i=" << i;
}

TEST(SimdParity_ParallelLarge, PlusBitIdenticalAcrossSplits)
{
    Allocator alloc = Allocator::defaultAllocator();
    constexpr size_t N = 100'003;
    auto a = makeReals(N, 11, -1e3, 1e3);
    auto b = makeReals(N, 13, -1e3, 1e3);

    Value x = makeDoubleVector(alloc, a);
    Value y = makeDoubleVector(alloc, b);
    Value z = numkit::builtin::plus(alloc, x, y);
    ASSERT_EQ(z.numel(), N);
    for (size_t i = 0; i < N; ++i)
        EXPECT_TRUE(bitEquals(z.doubleData()[i], a[i] + b[i])) << "i=" << i;
}

TEST(SimdParity_ParallelLarge, MinusBitIdenticalAcrossSplits)
{
    Allocator alloc = Allocator::defaultAllocator();
    constexpr size_t N = 100'003;
    auto a = makeReals(N, 17, -1e3, 1e3);
    auto b = makeReals(N, 19, -1e3, 1e3);

    Value x = makeDoubleVector(alloc, a);
    Value y = makeDoubleVector(alloc, b);
    Value z = numkit::builtin::minus(alloc, x, y);
    ASSERT_EQ(z.numel(), N);
    for (size_t i = 0; i < N; ++i)
        EXPECT_TRUE(bitEquals(z.doubleData()[i], a[i] - b[i])) << "i=" << i;
}

TEST(SimdParity_ParallelLarge, TimesBitIdenticalAcrossSplits)
{
    Allocator alloc = Allocator::defaultAllocator();
    constexpr size_t N = 100'003;
    auto a = makeReals(N, 23, -1e3, 1e3);
    auto b = makeReals(N, 29, -1e3, 1e3);

    Value x = makeDoubleVector(alloc, a);
    Value y = makeDoubleVector(alloc, b);
    Value z = numkit::builtin::times(alloc, x, y);
    ASSERT_EQ(z.numel(), N);
    for (size_t i = 0; i < N; ++i)
        EXPECT_TRUE(bitEquals(z.doubleData()[i], a[i] * b[i])) << "i=" << i;
}

TEST(SimdParity_ParallelLarge, RdivideBitIdenticalAcrossSplits)
{
    Allocator alloc = Allocator::defaultAllocator();
    constexpr size_t N = 100'003;
    auto a = makeReals(N, 31, -1e3, 1e3);
    auto b = makeReals(N, 37, 0.5, 1e3);   // strictly positive denom

    Value x = makeDoubleVector(alloc, a);
    Value y = makeDoubleVector(alloc, b);
    Value z = numkit::builtin::rdivide(alloc, x, y);
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
    Allocator alloc = Allocator::defaultAllocator();
    constexpr size_t N = 100'003;     // also above kTranscendentalThreshold
    auto src = makeReals(N, 41, -10.0, 10.0);

    Value x  = makeDoubleVector(alloc, src);
    Value y1 = numkit::builtin::sin(alloc, x);
    Value y2 = numkit::builtin::sin(alloc, x);
    Value y3 = numkit::builtin::sin(alloc, x);
    ASSERT_EQ(y1.numel(), N);
    for (size_t i = 0; i < N; ++i) {
        EXPECT_TRUE(bitEquals(y1.doubleData()[i], y2.doubleData()[i])) << "i=" << i;
        EXPECT_TRUE(bitEquals(y2.doubleData()[i], y3.doubleData()[i])) << "i=" << i;
    }
}

TEST(SimdParity_ParallelLarge, ExpDeterministicAcrossCalls)
{
    Allocator alloc = Allocator::defaultAllocator();
    constexpr size_t N = 100'003;
    auto src = makeReals(N, 43, -5.0, 5.0);

    Value x  = makeDoubleVector(alloc, src);
    Value y1 = numkit::builtin::exp(alloc, x);
    Value y2 = numkit::builtin::exp(alloc, x);
    ASSERT_EQ(y1.numel(), N);
    for (size_t i = 0; i < N; ++i)
        EXPECT_TRUE(bitEquals(y1.doubleData()[i], y2.doubleData()[i])) << "i=" << i;
}

// Just-at-the-threshold and just-above to exercise the boundary.
TEST(SimdParity_ParallelLarge, ExactBoundaryCases)
{
    Allocator alloc = Allocator::defaultAllocator();
    for (size_t N : {size_t(16383), size_t(16384), size_t(16385), size_t(65536)}) {
        auto a = makeReals(N, 53 + static_cast<uint32_t>(N), -1.0, 1.0);
        auto b = makeReals(N, 59 + static_cast<uint32_t>(N), -1.0, 1.0);
        Value x = makeDoubleVector(alloc, a);
        Value y = makeDoubleVector(alloc, b);
        Value z = numkit::builtin::plus(alloc, x, y);
        ASSERT_EQ(z.numel(), N) << "N=" << N;
        for (size_t i = 0; i < N; ++i)
            EXPECT_TRUE(bitEquals(z.doubleData()[i], a[i] + b[i]))
                << "N=" << N << " i=" << i;
    }
}
