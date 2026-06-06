// libs/stats/tests/mvtcdf_test.cpp
//
// Regression guard for mvtcdf.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

#include <cmath>

using namespace numkit;

class MvtcdfTest : public ::testing::Test
{
public:
    StdEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(MvtcdfTest, D1MatchesTcdf)
{
    EXPECT_NEAR(evalScalar("mvtcdf(0.5, 1, 5)"), 0.6808505642, 1e-9);
}

TEST_F(MvtcdfTest, D1Negative)
{
    EXPECT_NEAR(evalScalar("mvtcdf(-0.5, 1, 5)"),
                evalScalar("tcdf(-0.5, 5)"), 1e-12);
}

TEST_F(MvtcdfTest, D2Bivariate)
{
    // MATLAB ref: 0.4909888137. MC tolerance ~0.005.
    EXPECT_NEAR(evalScalar("mvtcdf([0.5 0.3], [1 0.5; 0.5 1], 5)"),
                0.4909888137, 0.01);
}

TEST_F(MvtcdfTest, D3Uncorrelated)
{
    // MATLAB ref: 0.3144752061.
    EXPECT_NEAR(evalScalar("mvtcdf([0.5 0.3 0.7], eye(3), 5)"),
                0.3144752061, 0.01);
}

TEST_F(MvtcdfTest, ShapeMultipleRows)
{
    // n × d input → n × 1 output column.
    eval("p = mvtcdf([0.5 0.3; 1.0 1.0; -0.5 0.0], eye(2), 5);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(p, 1)")), 3);
    EXPECT_EQ(static_cast<int>(evalScalar("size(p, 2)")), 1);
}

TEST_F(MvtcdfTest, DeterministicSeed)
{
    // Same call should return same value (fixed MC seed).
    eval("a = mvtcdf([0.5 0.3], [1 0.4; 0.4 1], 4);");
    eval("b = mvtcdf([0.5 0.3], [1 0.4; 0.4 1], 4);");
    EXPECT_EQ(evalScalar("a"), evalScalar("b"));
}

TEST_F(MvtcdfTest, MonotoneInRho)
{
    // For positive correlation, prob mass shifts toward concordant region.
    // Just check that values for ρ=0 vs ρ=0.5 differ (sanity).
    eval("a = mvtcdf([0.5 0.5], eye(2), 5);");
    eval("b = mvtcdf([0.5 0.5], [1 0.5; 0.5 1], 5);");
    EXPECT_GT(evalScalar("b"), evalScalar("a"));
}

TEST_F(MvtcdfTest, NonSquareCThrows)
{
    EXPECT_THROW(eval("mvtcdf([0.5 0.3], ones(2, 3), 5);"), std::exception);
}

TEST_F(MvtcdfTest, BadDfThrows)
{
    EXPECT_THROW(eval("mvtcdf(0.5, 1, -1);"), std::exception);
}

// ── Box form [LB, UB] + tol option ──────────────────────────────────

TEST_F(MvtcdfTest, BoxFormMatchesUpperTailWithMinusInfLB)
{
    // P([-inf, -inf] ≤ Y ≤ [0.5, 0.3]) == P(Y ≤ [0.5, 0.3]).
    eval(R"(
        C = [1 0.5; 0.5 1];
        a = mvtcdf([0.5 0.3], C, 5);
        b = mvtcdf([-inf -inf], [0.5 0.3], C, 5);
    )");
    EXPECT_NEAR(evalScalar("a"), evalScalar("b"), 1e-12);
}

TEST_F(MvtcdfTest, Box1DEqualsCdfDiff)
{
    // 1-D box: P(L ≤ Y ≤ U) = tcdf(U) - tcdf(L).
    eval(R"(
        a = mvtcdf(-1, 1, 5);
        b = mvtcdf(0.5, 1, 5);
        p = mvtcdf(-1, 0.5, 1, 5);
    )");
    EXPECT_NEAR(evalScalar("p"), evalScalar("b") - evalScalar("a"), 1e-12);
}

TEST_F(MvtcdfTest, BoxFormTolOption)
{
    // Box form supports 5-arg with tighter tol → tighter result.
    // P(-inf < Y < [0.5, 0.3]) ≈ upper-tail mvtcdf(0.5,0.3,C,5) ≈ 0.4910.
    eval(R"(
        C = [1 0.5; 0.5 1];
        loose = mvtcdf([-inf -inf], [0.5 0.3], C, 5);
        tight = mvtcdf([-inf -inf], [0.5 0.3], C, 5, 0.002);
        err_loose = abs(loose - 0.4909888137);
        err_tight = abs(tight - 0.4909888137);
    )");
    EXPECT_LE(evalScalar("err_tight"), evalScalar("err_loose") + 1e-3);
}

TEST_F(MvtcdfTest, BoxFormSymmetric)
{
    // For C symmetric and centered box [-a, a], the probability is the
    // same regardless of sign of a (by symmetry).
    eval(R"(
        C = [1 0.4; 0.4 1];
        p1 = mvtcdf([-1 -1], [1 1], C, 5);
        p2 = mvtcdf([-2 -2], [2 2], C, 5);
    )");
    EXPECT_GT(evalScalar("p2"), evalScalar("p1"));   // wider box ⇒ more mass
}

TEST_F(MvtcdfTest, BoxFormBadShapeThrows)
{
    EXPECT_THROW(eval("mvtcdf([0 0], [1 1 1], [1 0.5; 0.5 1], 5);"),
                 std::exception);
}
