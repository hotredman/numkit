// libs/builtin/tests/numeric_test.cpp
// Phase 7: hypot/nthroot/expm1/log1p/gcd/lcm/bit*

#include "dual_engine_fixture.hpp"
#include <cmath>

using namespace m_test;

class NumericTest : public DualEngineTest
{};

// ── hypot ───────────────────────────────────────────────────

TEST_P(NumericTest, HypotScalar)
{
    EXPECT_DOUBLE_EQ(evalScalar("hypot(3, 4);"), 5.0);
    EXPECT_DOUBLE_EQ(evalScalar("hypot(0, 0);"), 0.0);
}

TEST_P(NumericTest, HypotVectorBroadcast)
{
    eval("v = hypot([3 5 8], [4 12 15]);");
    auto *v = getVarPtr("v");
    EXPECT_DOUBLE_EQ(v->doubleData()[0], 5.0);
    EXPECT_DOUBLE_EQ(v->doubleData()[1], 13.0);
    EXPECT_DOUBLE_EQ(v->doubleData()[2], 17.0);
}

TEST_P(NumericTest, HypotOverflowSafe)
{
    // hypot avoids intermediate overflow; sqrt(x^2+y^2) would overflow.
    EXPECT_NEAR(evalScalar("hypot(1e200, 1e200);"),
                std::sqrt(2.0) * 1e200, 1e185);
}

// ── nthroot ─────────────────────────────────────────────────

TEST_P(NumericTest, NthrootBasic)
{
    EXPECT_NEAR(evalScalar("nthroot(8, 3);"), 2.0, 1e-12);
    EXPECT_NEAR(evalScalar("nthroot(81, 4);"), 3.0, 1e-12);
}

TEST_P(NumericTest, NthrootNegativeOddRoot)
{
    // -8 cube root = -2 (real)
    EXPECT_NEAR(evalScalar("nthroot(-8, 3);"), -2.0, 1e-12);
    EXPECT_NEAR(evalScalar("nthroot(-32, 5);"), -2.0, 1e-12);
}

TEST_P(NumericTest, NthrootNegativeEvenRootIsNaN)
{
    EXPECT_TRUE(std::isnan(evalScalar("nthroot(-4, 2);")));
}

// ── expm1 / log1p ──────────────────────────────────────────

TEST_P(NumericTest, Expm1NearZero)
{
    // exp(x) - 1 for small x — accurate via expm1
    EXPECT_NEAR(evalScalar("expm1(1e-15);"), 1e-15, 1e-25);
    EXPECT_NEAR(evalScalar("expm1(0);"), 0.0, 1e-12);
    EXPECT_NEAR(evalScalar("expm1(1);"), std::exp(1.0) - 1.0, 1e-12);
}

TEST_P(NumericTest, Log1pNearZero)
{
    EXPECT_NEAR(evalScalar("log1p(1e-15);"), 1e-15, 1e-25);
    EXPECT_NEAR(evalScalar("log1p(0);"), 0.0, 1e-12);
    EXPECT_NEAR(evalScalar("log1p(1);"), std::log(2.0), 1e-12);
}

TEST_P(NumericTest, Expm1Vector)
{
    eval("v = expm1([0 1 -1]);");
    auto *v = getVarPtr("v");
    EXPECT_NEAR(v->doubleData()[0], 0.0, 1e-12);
    EXPECT_NEAR(v->doubleData()[1], std::exp(1.0) - 1.0, 1e-12);
    EXPECT_NEAR(v->doubleData()[2], std::exp(-1.0) - 1.0, 1e-12);
}

// ── gcd / lcm ──────────────────────────────────────────────

TEST_P(NumericTest, GcdBasic)
{
    EXPECT_DOUBLE_EQ(evalScalar("gcd(12, 18);"), 6.0);
    EXPECT_DOUBLE_EQ(evalScalar("gcd(7, 5);"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("gcd(0, 5);"), 5.0);
    EXPECT_DOUBLE_EQ(evalScalar("gcd(0, 0);"), 0.0);
}

TEST_P(NumericTest, GcdNegative)
{
    // gcd is always non-negative
    EXPECT_DOUBLE_EQ(evalScalar("gcd(-12, 18);"), 6.0);
    EXPECT_DOUBLE_EQ(evalScalar("gcd(-12, -18);"), 6.0);
}

TEST_P(NumericTest, GcdVector)
{
    eval("v = gcd([12 15 8], [18 25 12]);");
    auto *v = getVarPtr("v");
    EXPECT_DOUBLE_EQ(v->doubleData()[0], 6.0);
    EXPECT_DOUBLE_EQ(v->doubleData()[1], 5.0);
    EXPECT_DOUBLE_EQ(v->doubleData()[2], 4.0);
}

TEST_P(NumericTest, LcmBasic)
{
    EXPECT_DOUBLE_EQ(evalScalar("lcm(4, 6);"), 12.0);
    EXPECT_DOUBLE_EQ(evalScalar("lcm(7, 5);"), 35.0);
    EXPECT_DOUBLE_EQ(evalScalar("lcm(0, 5);"), 0.0);
}

TEST_P(NumericTest, LcmGcdRelationship)
{
    // gcd(a, b) * lcm(a, b) == |a*b|
    const double a = 36, b = 24;
    eval("g = gcd(36, 24); l = lcm(36, 24);");
    EXPECT_DOUBLE_EQ(getVar("g") * getVar("l"), a * b);
}

// ── bitand / bitor / bitxor ────────────────────────────────

TEST_P(NumericTest, BitandBasic)
{
    EXPECT_DOUBLE_EQ(evalScalar("bitand(12, 10);"), 8.0);  // 1100 & 1010 = 1000
    EXPECT_DOUBLE_EQ(evalScalar("bitand(255, 15);"), 15.0);
}

TEST_P(NumericTest, BitorBasic)
{
    EXPECT_DOUBLE_EQ(evalScalar("bitor(12, 10);"), 14.0); // 1100 | 1010 = 1110
    EXPECT_DOUBLE_EQ(evalScalar("bitor(0, 0);"), 0.0);
}

TEST_P(NumericTest, BitxorBasic)
{
    EXPECT_DOUBLE_EQ(evalScalar("bitxor(12, 10);"), 6.0);  // 1100 ^ 1010 = 0110
    EXPECT_DOUBLE_EQ(evalScalar("bitxor(255, 255);"), 0.0);
}

TEST_P(NumericTest, BitwiseElementwise)
{
    eval("v = bitand([12 255 1], [10 15 3]);");
    auto *v = getVarPtr("v");
    EXPECT_DOUBLE_EQ(v->doubleData()[0], 8.0);
    EXPECT_DOUBLE_EQ(v->doubleData()[1], 15.0);
    EXPECT_DOUBLE_EQ(v->doubleData()[2], 1.0);
}

// ── bitshift ────────────────────────────────────────────────

TEST_P(NumericTest, BitshiftLeft)
{
    EXPECT_DOUBLE_EQ(evalScalar("bitshift(1, 4);"), 16.0);
    EXPECT_DOUBLE_EQ(evalScalar("bitshift(3, 2);"), 12.0);
}

TEST_P(NumericTest, BitshiftRight)
{
    EXPECT_DOUBLE_EQ(evalScalar("bitshift(16, -2);"), 4.0);
    EXPECT_DOUBLE_EQ(evalScalar("bitshift(255, -4);"), 15.0);
}

TEST_P(NumericTest, BitshiftZero)
{
    EXPECT_DOUBLE_EQ(evalScalar("bitshift(42, 0);"), 42.0);
}

// ── bitcmp ──────────────────────────────────────────────────

TEST_P(NumericTest, BitcmpUint8)
{
    // ~0xAA = 0x55 in 8-bit
    EXPECT_DOUBLE_EQ(evalScalar("bitcmp(170, 'uint8');"), 85.0);
    EXPECT_DOUBLE_EQ(evalScalar("bitcmp(0, 'uint8');"), 255.0);
    EXPECT_DOUBLE_EQ(evalScalar("bitcmp(255, 'uint8');"), 0.0);
}

TEST_P(NumericTest, BitcmpUint16)
{
    EXPECT_DOUBLE_EQ(evalScalar("bitcmp(0, 'uint16');"), 65535.0);
    EXPECT_DOUBLE_EQ(evalScalar("bitcmp(1, 'uint16');"), 65534.0);
}

TEST_P(NumericTest, BitcmpUint32)
{
    EXPECT_DOUBLE_EQ(evalScalar("bitcmp(0, 'uint32');"), 4294967295.0);
}

INSTANTIATE_DUAL(NumericTest);
