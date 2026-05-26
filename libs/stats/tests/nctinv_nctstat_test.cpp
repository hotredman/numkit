// libs/stats/tests/nctinv_nctstat_test.cpp
//
// Regression guard for nctinv + nctstat.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

#include <cmath>

using namespace numkit;

class NctInvStatTest : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// ── nctstat ─────────────────────────────────────────────────────────

TEST_F(NctInvStatTest, NctstatMatchesMatlab)
{
    eval("[m, v] = nctstat(10, 2);");
    EXPECT_NEAR(evalScalar("m"), 2.1674446159, 1e-9);
    EXPECT_NEAR(evalScalar("v"), 1.5521838371, 1e-9);
}

TEST_F(NctInvStatTest, NctstatAtNuFive)
{
    eval("[m, v] = nctstat(5, 1.5);");
    EXPECT_NEAR(evalScalar("m"), 1.7841241162, 1e-9);
    EXPECT_NEAR(evalScalar("v"), 2.2335678048, 1e-9);
}

TEST_F(NctInvStatTest, NctstatNuEqualsTwoVarianceIsNaN)
{
    eval("[m, v] = nctstat(2, 1.0);");
    // For ν = 2 the variance is undefined.
    EXPECT_TRUE(std::isnan(evalScalar("v")));
}

TEST_F(NctInvStatTest, NctstatNuLessThanOneIsNaN)
{
    eval("[m, v] = nctstat(0.5, 1.0);");
    EXPECT_TRUE(std::isnan(evalScalar("m")));
    EXPECT_TRUE(std::isnan(evalScalar("v")));
}

// ── nctinv ──────────────────────────────────────────────────────────

TEST_F(NctInvStatTest, NctinvMatchesMatlab)
{
    EXPECT_NEAR(evalScalar("nctinv(0.3, 10, 2)"), 1.4856759815, 1e-6);
}

TEST_F(NctInvStatTest, NctinvVectorMatchesMatlab)
{
    eval("x = nctinv([0.1 0.5 0.9], 10, 2);");
    EXPECT_NEAR(evalScalar("x(1)"), 0.7200, 1e-3);
    EXPECT_NEAR(evalScalar("x(2)"), 2.0537, 1e-3);
    EXPECT_NEAR(evalScalar("x(3)"), 3.7466, 1e-3);
}

TEST_F(NctInvStatTest, NctinvRoundTrip)
{
    eval("p = 0.42; x = nctinv(p, 8, -1.5); rt = nctcdf(x, 8, -1.5);");
    EXPECT_NEAR(evalScalar("rt"), 0.42, 1e-8);
}

TEST_F(NctInvStatTest, NctinvBoundaryP0)
{
    EXPECT_TRUE(std::isinf(evalScalar("nctinv(0, 10, 2)")));
    EXPECT_LT(evalScalar("nctinv(0, 10, 2)"), 0.0);
}

TEST_F(NctInvStatTest, NctinvBoundaryP1)
{
    EXPECT_TRUE(std::isinf(evalScalar("nctinv(1, 10, 2)")));
    EXPECT_GT(evalScalar("nctinv(1, 10, 2)"), 0.0);
}

TEST_F(NctInvStatTest, NctinvDeltaZeroEqualsCentral)
{
    // δ = 0 → central tinv.
    EXPECT_NEAR(evalScalar("nctinv(0.7, 10, 0)"),
                evalScalar("tinv(0.7, 10)"), 1e-10);
}
