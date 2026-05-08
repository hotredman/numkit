// libs/stats/tests/evstat_test.cpp
// Audit ТЗ closure for evstat. Closes audit/findings/stats/evstat.md.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class EvstatTest : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(EvstatTest, ScalarMomentsEV01)
{
    // EV(0, 1): m = -γ_E ≈ -0.5772, v = π²/6 ≈ 1.6449.
    eval("[m, v] = evstat(0, 1);");
    EXPECT_NEAR(evalScalar("m"), -0.5772156649015329, 1e-9);
    EXPECT_NEAR(evalScalar("v"),  1.6449340668482264, 1e-9);
}

TEST_F(EvstatTest, VectorBroadcasting)
{
    eval("[m, v] = evstat([0 1 -2], [1 2 0.5]);");
    EXPECT_NEAR(evalScalar("m(1)"), -0.5772156649015329, 1e-9);
    EXPECT_NEAR(evalScalar("v(2)"),  6.5797362673929058, 1e-9);
}

TEST_F(EvstatTest, InvalidSigmaReturnsNaN)
{
    EXPECT_TRUE(std::isnan(evalScalar("evstat(0,  0)")));
    EXPECT_TRUE(std::isnan(evalScalar("evstat(0, -1)")));
}
