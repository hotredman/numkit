// libs/builtin/tests/transcendentals_batch_test.cpp
//
// Audit ТЗ batch closure for the transcendental + rounding family:
//   atan2 / atan2d
//   exp / expm1
//   log / log2 / log10 / log1p
//   sqrt / hypot
//   floor / ceil / round / fix
//
// All 14 flagged "no major gap detected" — libm-backed, bit-identical
// MATLAB R2025b on probed inputs (parity tol=1e-12).

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class TranscendentalsBatchTest : public ::testing::Test
{
public:
    Engine engine;
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

TEST_F(TranscendentalsBatchTest, Sqrt)
{
    EXPECT_NEAR(evalScalar("sqrt(0)"),   0.0, 1e-12);
    EXPECT_NEAR(evalScalar("sqrt(4)"),   2.0, 1e-12);
    EXPECT_NEAR(evalScalar("sqrt(100)"), 10.0, 1e-12);
    EXPECT_NEAR(evalScalar("sqrt(2)"),   1.414213562373095, 1e-12);
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
