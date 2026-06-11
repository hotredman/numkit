// toolboxes/stats/tests/mvncdf_test.cpp
//
// Regression guard for mvncdf — multivariate normal CDF.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using namespace numkit;

class MvncdfTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// d=1: forwards to normcdf.
TEST_F(MvncdfTest, UnivariateMatchesNormcdf)
{
    eval("p1 = mvncdf(0, [], []); p2 = mvncdf(1.5, [], []);"
         "ref1 = normcdf(0); ref2 = normcdf(1.5);");
    EXPECT_NEAR(evalScalar("p1"), evalScalar("ref1"), 1e-12);
    EXPECT_NEAR(evalScalar("p2"), evalScalar("ref2"), 1e-12);
}

// d=2 independent: P(X1≤0, X2≤0) = 0.5² = 0.25 for standard normal.
TEST_F(MvncdfTest, BivariateIndependentOriginIsOneQuarter)
{
    eval("p = mvncdf([0 0], [], eye(2));");
    EXPECT_NEAR(evalScalar("p"), 0.25, 1e-9);
}

// d=2 correlated: ρ = 0.5 → P(X1≤0, X2≤0) = 1/4 + arcsin(ρ)/(2π) = 1/3.
TEST_F(MvncdfTest, BivariateRho05OriginIsOneThird)
{
    eval("S = [1 0.5; 0.5 1];"
         "p = mvncdf([0 0], [0 0], S);");
    EXPECT_NEAR(evalScalar("p"), 1.0 / 3.0, 1e-6);
}

// d=2 perfect correlation: ρ = +1 → P = min marginal = Φ(0) = 0.5.
TEST_F(MvncdfTest, BivariatePerfectCorrelation)
{
    eval("S = [1 0.999; 0.999 1];"
         "p = mvncdf([0 0], [0 0], S);");
    EXPECT_NEAR(evalScalar("p"), 0.5, 0.01);
}

// d=2 with mu: shift the query by mu.
TEST_F(MvncdfTest, BivariateWithMu)
{
    eval("p1 = mvncdf([1 2], [1 2], eye(2));"   // q == mu → 0.25
         "p2 = mvncdf([0 0], [],    eye(2));");
    EXPECT_NEAR(evalScalar("p1"), evalScalar("p2"), 1e-9);
}

// d=3 independent: P = 0.5³ = 0.125 (Monte Carlo).
TEST_F(MvncdfTest, TrivariateIndependentOrigin)
{
    eval("p = mvncdf([0 0 0], [], eye(3));");
    EXPECT_NEAR(evalScalar("p"), 0.125, 0.02);   // MC tolerance
}

// Multiple queries.
TEST_F(MvncdfTest, MultipleQueries)
{
    eval("X = [0 0; 1 1; -1 -1]; P = mvncdf(X, [0 0], eye(2));");
    EXPECT_EQ(static_cast<int>(evalScalar("numel(P)")), 3);
    EXPECT_NEAR(evalScalar("P(1)"), 0.25, 1e-9);
    EXPECT_GT(evalScalar("P(2)"), evalScalar("P(1)"));
    EXPECT_LT(evalScalar("P(3)"), evalScalar("P(1)"));
}

// Mismatch throws.
TEST_F(MvncdfTest, ShapeMismatchThrows)
{
    EXPECT_THROW(eval("mvncdf([0 0], [0], eye(2));"), std::exception);
}
