// libs/stats/tests/gevstat_test.cpp
// Audit ТЗ closure for gevstat. Closes audit/findings/stats/gevstat.md.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class GevstatTest : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(GevstatTest, ScalarMomentsKpos)
{
    eval("[m, v] = gevstat(0.3, 1, 0);");
    EXPECT_NEAR(evalScalar("m"), 0.9935175193907920, 1e-6);  // tgamma precision
    EXPECT_NEAR(evalScalar("v"), 5.9245806632858700, 1e-5);
}

TEST_F(GevstatTest, K0IsGumbelLimit)
{
    // k=0 limit: m = mu + sigma·γ_E, v = sigma²·π²/6.
    eval("[m, v] = gevstat(0, 1, 0);");
    EXPECT_NEAR(evalScalar("m"), 0.5772156649015329, 1e-9);
    EXPECT_NEAR(evalScalar("v"), 1.6449340668482264, 1e-9);
}

TEST_F(GevstatTest, K05_VarianceIsInf)
{
    // k=0.5 → variance Γ(1-2k) → Γ(0) = Inf.
    eval("[m, v] = gevstat(0.5, 1, 0);");
    EXPECT_NEAR(evalScalar("m"), 1.5449077018110318, 1e-6);  // tgamma precision
    EXPECT_TRUE(std::isinf(evalScalar("v")));
}

TEST_F(GevstatTest, K1_MeanIsInf)
{
    // k>=1 → mean diverges (Γ(1-k) → Inf at k=1).
    eval("[m, v] = gevstat(1, 1, 0);");
    EXPECT_TRUE(std::isinf(evalScalar("m")));
}

TEST_F(GevstatTest, VectorBroadcasting)
{
    eval("[m, v] = gevstat([0.3 0 -0.3], 1, 0);");
    EXPECT_NEAR(evalScalar("m(1)"), 0.9935175193907920, 1e-6);  // tgamma precision
    EXPECT_NEAR(evalScalar("m(2)"), 0.5772156649015329, 1e-9);
}

TEST_F(GevstatTest, InvalidSigmaReturnsNaN)
{
    EXPECT_TRUE(std::isnan(evalScalar("gevstat(0.3,  0, 0)")));
    EXPECT_TRUE(std::isnan(evalScalar("gevstat(0.3, -1, 0)")));
}
