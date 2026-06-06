// libs/builtin/tests/sinpi_cospi_test.cpp
//
// Offline regression guard for the accurate sinpi / cospi kernel
// (exact int64 octant reduction + single-double SLEEF sinpik/cospik
// polynomial). Expected values pinned from MATLAB R2025b. One TEST_F
// per documented branch. The pre-fix naive sin(pi*x) failed several of
// these (exact integer zeros, sign-of-zero, accuracy at 1/6, and the
// ~1e-10 drift by x=1e7).

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>

#include <gtest/gtest.h>

#include <cmath>

class SinpiCospiTest : public ::testing::Test {
public:
    numkit::StdEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    numkit::Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// ── sinpi: integer arguments are an exact zero ───────────────────────
TEST_F(SinpiCospiTest, SinpiIntegerExactZero)
{
    EXPECT_EQ(evalScalar("sinpi(0)"), 0.0);
    EXPECT_EQ(evalScalar("sinpi(1)"), 0.0);
    EXPECT_EQ(evalScalar("sinpi(7)"), 0.0);
    EXPECT_EQ(evalScalar("sinpi(1e6)"), 0.0);
    EXPECT_EQ(evalScalar("sinpi(1e7)"), 0.0); // naive sin(pi*x) gave ~5.6e-10
}

// ── sinpi: MATLAB takes the zero's sign from the input ───────────────
TEST_F(SinpiCospiTest, SinpiSignOfZero)
{
    EXPECT_EQ(evalScalar("sinpi(1)"), 0.0);
    EXPECT_FALSE(std::signbit(evalScalar("sinpi(1)")));  // +0
    EXPECT_FALSE(std::signbit(evalScalar("sinpi(3)")));  // +0 (odd, positive)
    EXPECT_FALSE(std::signbit(evalScalar("sinpi(0)")));  // +0
    EXPECT_TRUE(std::signbit(evalScalar("sinpi(-1)")));  // -0
    EXPECT_TRUE(std::signbit(evalScalar("sinpi(-2)")));  // -0
}

// ── sinpi: half-integers are exact +/-1 ──────────────────────────────
TEST_F(SinpiCospiTest, SinpiHalfIntegers)
{
    EXPECT_EQ(evalScalar("sinpi(0.5)"),  1.0);
    EXPECT_EQ(evalScalar("sinpi(1.5)"), -1.0);
    EXPECT_EQ(evalScalar("sinpi(2.5)"),  1.0);
    EXPECT_EQ(evalScalar("sinpi(-0.5)"), -1.0);
    EXPECT_EQ(evalScalar("sinpi(10.5)"), 1.0);
}

// ── sinpi: accurate values (MATLAB-pinned) ───────────────────────────
TEST_F(SinpiCospiTest, SinpiAccurateValues)
{
    EXPECT_NEAR(evalScalar("sinpi(1/6)"), 0.5, 1e-15);            // naive: 0.49999999999999994
    EXPECT_NEAR(evalScalar("sinpi(1/3)"), 0.8660254037844386, 1e-13);
    EXPECT_NEAR(evalScalar("sinpi(0.25)"), 0.70710678118654757, 1e-13);
    EXPECT_NEAR(evalScalar("sinpi(0.1)"), 0.30901699437494745, 1e-13);
    EXPECT_NEAR(evalScalar("sinpi(1/7)"), 0.43388373911755812, 1e-13);
    EXPECT_NEAR(evalScalar("sinpi(-1/6)"), -0.5, 1e-15);
}

// ── sinpi: large arguments stay correct (no 32-bit-lane flush) ───────
TEST_F(SinpiCospiTest, SinpiLargeArgs)
{
    EXPECT_NEAR(evalScalar("sinpi(123456.25)"), 0.70710678118654757, 1e-13);
    EXPECT_NEAR(evalScalar("sinpi(1e10+0.5)"), 1.0, 1e-13); // even integer + 0.5
    EXPECT_EQ(evalScalar("sinpi(1e10)"), 0.0);
}

// ── sinpi: vector exercises SIMD body + scalar tail consistently ─────
TEST_F(SinpiCospiTest, SinpiVectorSimdAndTail)
{
    eval("v = sinpi((0:9)/10);"); // 10 elems: SIMD body (8) + tail (2)
    EXPECT_EQ(evalScalar("v(1)"), 0.0);
    EXPECT_NEAR(evalScalar("v(2)"), 0.30901699437494745, 1e-13); // 0.1, SIMD lane
    EXPECT_NEAR(evalScalar("v(6)"), 1.0, 1e-13);                 // 0.5
    EXPECT_NEAR(evalScalar("v(9)"), 0.58778525229247303, 1e-12); // 0.8, tail
    EXPECT_NEAR(evalScalar("v(10)"), 0.3090169943749474, 1e-12); // 0.9, tail
}

// ── cospi: half-integers are an exact (positive) zero ────────────────
TEST_F(SinpiCospiTest, CospiHalfIntegerExactZero)
{
    EXPECT_EQ(evalScalar("cospi(0.5)"), 0.0);
    EXPECT_EQ(evalScalar("cospi(1.5)"), 0.0);
    EXPECT_EQ(evalScalar("cospi(-0.5)"), 0.0);
    EXPECT_EQ(evalScalar("cospi(1e7+0.5)"), 0.0);
    EXPECT_FALSE(std::signbit(evalScalar("cospi(0.5)")));  // +0 always
    EXPECT_FALSE(std::signbit(evalScalar("cospi(-0.5)"))); // +0 always
    EXPECT_FALSE(std::signbit(evalScalar("cospi(2.5)")));  // +0 always
}

// ── cospi: integers are exact +/-1 ───────────────────────────────────
TEST_F(SinpiCospiTest, CospiIntegers)
{
    EXPECT_EQ(evalScalar("cospi(0)"),  1.0);
    EXPECT_EQ(evalScalar("cospi(1)"), -1.0);
    EXPECT_EQ(evalScalar("cospi(2)"),  1.0);
    EXPECT_EQ(evalScalar("cospi(-3)"), -1.0);
    EXPECT_EQ(evalScalar("cospi(1e7)"), 1.0);
}

// ── cospi: accurate values (MATLAB-pinned) ───────────────────────────
TEST_F(SinpiCospiTest, CospiAccurateValues)
{
    EXPECT_NEAR(evalScalar("cospi(1/3)"), 0.5, 1e-15);
    EXPECT_NEAR(evalScalar("cospi(0.25)"), 0.70710678118654757, 1e-13);
    EXPECT_NEAR(evalScalar("cospi(1/6)"), 0.8660254037844386, 1e-13);
    EXPECT_NEAR(evalScalar("cospi(0.1)"), 0.95105651629515353, 1e-13);
    EXPECT_NEAR(evalScalar("cospi(1/7)"), 0.90096886790241915, 1e-13);
}

// ── specials: Inf -> NaN, NaN -> NaN ─────────────────────────────────
TEST_F(SinpiCospiTest, Specials)
{
    EXPECT_TRUE(std::isnan(evalScalar("sinpi(Inf)")));
    EXPECT_TRUE(std::isnan(evalScalar("sinpi(-Inf)")));
    EXPECT_TRUE(std::isnan(evalScalar("cospi(Inf)")));
    EXPECT_TRUE(std::isnan(evalScalar("sinpi(NaN)")));
    EXPECT_TRUE(std::isnan(evalScalar("cospi(NaN)")));
}
