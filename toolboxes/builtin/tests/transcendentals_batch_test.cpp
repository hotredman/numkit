// toolboxes/builtin/tests/transcendentals_batch_test.cpp
// transcendental + rounding family:
//   atan2 / atan2d
//   exp / expm1
//   log / log2 / log10 / log1p
//   sqrt / hypot
//   floor / ceil / round / fix
// All 14  — libm-backed, bit-identical
// MATLAB R2025b on probed inputs (parity tol=1e-12).

#include <numkit/core/engine.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class TranscendentalsBatchTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(TranscendentalsBatchTest, Atan2QuadrantsRadians)
{
    EXPECT_NEAR(evalScalar("atan2( 1,  1)"),  0.785398163397448, 1e-12);
    EXPECT_NEAR(evalScalar("atan2( 1, -1)"),  2.356194490192345, 1e-12);
    EXPECT_NEAR(evalScalar("atan2(-1,  1)"), -0.785398163397448, 1e-12);
    EXPECT_NEAR(evalScalar("atan2(-1, -1)"), -2.356194490192345, 1e-12);
}

TEST_F(TranscendentalsBatchTest, Atan2dDegrees)
{
    EXPECT_NEAR(evalScalar("atan2d(1, 1)"),  45.0, 1e-12);
    EXPECT_NEAR(evalScalar("atan2d(1, -1)"), 135.0, 1e-12);
}

TEST_F(TranscendentalsBatchTest, ExpExpm1)
{
    EXPECT_NEAR(evalScalar("exp(0)"),    1.0,               1e-12);
    EXPECT_NEAR(evalScalar("exp(1)"),    2.718281828459045, 1e-12);
    EXPECT_NEAR(evalScalar("expm1(0)"),  0.0,               1e-12);
    // expm1 vs naive: exp(1e-10)-1 should be ~1e-10 with full precision
    EXPECT_NEAR(evalScalar("expm1(1e-10)"), 1.0000000000500000e-10, 1e-22);
}

TEST_F(TranscendentalsBatchTest, LogFamily)
{
    EXPECT_NEAR(evalScalar("log(1)"),      0.0, 1e-12);
    EXPECT_NEAR(evalScalar("log(exp(1))"), 1.0, 1e-12);
    EXPECT_NEAR(evalScalar("log2(8)"),     3.0, 1e-12);
    EXPECT_NEAR(evalScalar("log10(1000)"), 3.0, 1e-12);
    EXPECT_NEAR(evalScalar("log1p(0)"),    0.0, 1e-12);
    // log1p precision win at small x: log(1 + 1e-10) ≈ 1e-10
    EXPECT_NEAR(evalScalar("log1p(1e-10)"), 9.999999999500000e-11, 1e-22);
}

// log10 moved to the Highway SIMD path (hn::Log10). The vector body must
// still return exact integers at powers of 10, like MATLAB R2025b.
TEST_F(TranscendentalsBatchTest, Log10VectorExactPowersOfTen)
{
    eval("p = log10([1 10 100 1000 1e4 1e5 1e6 1e7 1e8 1e9]);"); // SIMD body + tail
    EXPECT_EQ(evalScalar("p(1)"),  0.0);
    EXPECT_EQ(evalScalar("p(3)"),  2.0);   // SIMD lane
    EXPECT_EQ(evalScalar("p(7)"),  6.0);   // SIMD lane
    EXPECT_EQ(evalScalar("p(9)"),  8.0);   // tail
    EXPECT_EQ(evalScalar("p(10)"), 9.0);   // tail
    EXPECT_NEAR(evalScalar("log10(2)"),     0.3010299956639812, 1e-13);
    EXPECT_NEAR(evalScalar("log10(0.001)"), -3.0,               1e-13);
}

// pow2(y) == 2^y moved to the Highway SIMD path (ported SLEEF xexp2). The
// vector body must keep integer exponents exact and honour the Inf/0 edges.
TEST_F(TranscendentalsBatchTest, Pow2VectorSimd)
{
    eval("p = pow2([0 1 2 3 4 5 6 7 8 30]);"); // SIMD body + tail
    EXPECT_EQ(evalScalar("p(1)"),  1.0);
    EXPECT_EQ(evalScalar("p(5)"),  16.0);          // 2^4, SIMD lane
    EXPECT_EQ(evalScalar("p(9)"),  256.0);         // 2^8, SIMD lane
    EXPECT_EQ(evalScalar("p(10)"), 1073741824.0);  // 2^30, tail
    eval("q = pow2([0.5 -3 10.5 0.25]);");
    EXPECT_NEAR(evalScalar("q(1)"), 1.4142135623730951, 1e-13);
    EXPECT_EQ(evalScalar("q(2)"),  0.125);
    EXPECT_NEAR(evalScalar("q(3)"), 1448.1546878700492, 1e-9);
    // Inf / 0 edges through the SIMD body (4 elems = one vector).
    eval("e = pow2([1100 -2100 1500 -3000]);");
    EXPECT_DOUBLE_EQ(evalScalar("isinf(e(1))"), 1.0); // 2^1100 -> Inf
    EXPECT_DOUBLE_EQ(evalScalar("e(2)"),        0.0); // 2^-2100 -> 0
    EXPECT_DOUBLE_EQ(evalScalar("isinf(e(3))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("e(4)"),        0.0);
}

// reallog == log with a strict-positive domain guard (now SIMD via LogLoop).
TEST_F(TranscendentalsBatchTest, Reallog)
{
    EXPECT_NEAR(evalScalar("reallog(1)"),      0.0,               1e-12);
    EXPECT_NEAR(evalScalar("reallog(exp(1))"), 1.0,               1e-12);
    EXPECT_NEAR(evalScalar("reallog(10)"),     2.302585092994046, 1e-12);
    eval("rv = reallog([1 exp(1) 10 100]);"); // SIMD path matches log on positives
    EXPECT_NEAR(evalScalar("rv(4)"), 4.605170185988092, 1e-12);
    // negative input -> error (MATLAB tells the user to switch to log)
    EXPECT_ANY_THROW(eval("reallog(-1)"));
    EXPECT_ANY_THROW(eval("reallog([1 2 -3])"));
}

TEST_F(TranscendentalsBatchTest, Sqrt)
{
    EXPECT_NEAR(evalScalar("sqrt(0)"),   0.0, 1e-12);
    EXPECT_NEAR(evalScalar("sqrt(4)"),   2.0, 1e-12);
    EXPECT_NEAR(evalScalar("sqrt(100)"), 10.0, 1e-12);
    EXPECT_NEAR(evalScalar("sqrt(2)"),   1.414213562373095, 1e-12);
}

// sqrt now runs through the Highway SIMD path (hn::Sqrt = hardware vsqrtpd,
// correctly rounded). A vector exercises the SIMD body + the scalar tail.
TEST_F(TranscendentalsBatchTest, SqrtVectorSimd)
{
    eval("s = sqrt([0 1 4 9 16 25 36 49 64 81]);"); // -> 0..9, SIMD body + tail
    EXPECT_EQ(evalScalar("s(1)"),  0.0);
    EXPECT_EQ(evalScalar("s(5)"),  4.0);   // sqrt(16), SIMD lane
    EXPECT_EQ(evalScalar("s(10)"), 9.0);   // sqrt(81), tail
    EXPECT_NEAR(evalScalar("sqrt(2)"), 1.4142135623730951, 1e-15);
}

// realsqrt == sqrt with a strict-nonnegative guard (now SIMD via SqrtLoop).
TEST_F(TranscendentalsBatchTest, Realsqrt)
{
    EXPECT_NEAR(evalScalar("realsqrt(4)"), 2.0, 1e-12);
    eval("rv = realsqrt([1 4 9 16 25 36 49 64 81 100]);"); // -> 1..10
    EXPECT_EQ(evalScalar("rv(4)"),  4.0);    // sqrt(16)
    EXPECT_EQ(evalScalar("rv(10)"), 10.0);   // sqrt(100), tail
    EXPECT_ANY_THROW(eval("realsqrt(-1)"));
    EXPECT_ANY_THROW(eval("realsqrt([1 4 -9])"));
}

TEST_F(TranscendentalsBatchTest, Hypot)
{
    // hypot(3, 4) = 5, hypot(5, 12) = 13, hypot(7, 24) = 25 — Pythagorean triples.
    EXPECT_NEAR(evalScalar("hypot(3, 4)"),  5.0,  1e-12);
    EXPECT_NEAR(evalScalar("hypot(5, 12)"), 13.0, 1e-12);
    EXPECT_NEAR(evalScalar("hypot(7, 24)"), 25.0, 1e-12);
    // hypot avoids overflow: hypot(1e200, 1e200) = sqrt(2)*1e200 (no overflow
    // even though 1e200^2 overflows). Just sanity-check it's finite.
    EXPECT_TRUE(std::isfinite(evalScalar("hypot(1e200, 1e200)")));
}

TEST_F(TranscendentalsBatchTest, FloorCeilRoundFix)
{
    // floor: toward -Inf
    EXPECT_DOUBLE_EQ(evalScalar("floor(-1.5)"), -2.0);
    EXPECT_DOUBLE_EQ(evalScalar("floor( 1.5)"),  1.0);
    EXPECT_DOUBLE_EQ(evalScalar("floor( 2.7)"),  2.0);

    // ceil: toward +Inf
    EXPECT_DOUBLE_EQ(evalScalar("ceil(-1.5)"), -1.0);
    EXPECT_DOUBLE_EQ(evalScalar("ceil( 1.5)"),  2.0);
    EXPECT_DOUBLE_EQ(evalScalar("ceil( 2.3)"),  3.0);

    // round: half-away-from-zero (MATLAB convention)
    EXPECT_DOUBLE_EQ(evalScalar("round(-1.5)"), -2.0);
    EXPECT_DOUBLE_EQ(evalScalar("round( 0.5)"),  1.0);
    EXPECT_DOUBLE_EQ(evalScalar("round( 0.4)"),  0.0);
    EXPECT_DOUBLE_EQ(evalScalar("round( 2.7)"),  3.0);

    // fix: toward zero (truncation)
    EXPECT_DOUBLE_EQ(evalScalar("fix(-1.5)"), -1.0);
    EXPECT_DOUBLE_EQ(evalScalar("fix( 1.5)"),  1.0);
    EXPECT_DOUBLE_EQ(evalScalar("fix( 2.7)"),  2.0);
    EXPECT_DOUBLE_EQ(evalScalar("fix(-2.7)"), -2.0);
}

TEST_F(TranscendentalsBatchTest, RoundTripIdentities)
{
    EXPECT_NEAR(evalScalar("log(exp(1.7))"),     1.7, 1e-12);
    EXPECT_NEAR(evalScalar("sqrt(7)^2"),         7.0, 1e-12);
    EXPECT_NEAR(evalScalar("log10(10^4.2)"),     4.2, 1e-12);
}
