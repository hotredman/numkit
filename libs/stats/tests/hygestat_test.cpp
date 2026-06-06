// libs/stats/tests/hygestat_test.cpp
// Coverage for hygestat (vectorised in sweep 5dd32c38).

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class HygestatTest : public ::testing::Test
{
public:
    StdEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(HygestatTest, ScalarMomentsHyge50_20_10)
{
    // m = 10·20/50 = 4; v = 10·20·30·40/(50²·49) = 1.959...
    eval("[m, v] = hygestat(50, 20, 10);");
    EXPECT_DOUBLE_EQ(evalScalar("m"), 4.0);
    EXPECT_NEAR(evalScalar("v"), 1.9591836734693879, 1e-12);
}

TEST_F(HygestatTest, VectorBroadcastingM)
{
    eval("[m, v] = hygestat([50 100], 20, 10);");
    EXPECT_DOUBLE_EQ(evalScalar("m(1)"), 4.0);
    EXPECT_DOUBLE_EQ(evalScalar("m(2)"), 2.0);
}

TEST_F(HygestatTest, BoundaryCases)
{
    // K=0 → no successes possible → m=v=0.
    eval("[m, v] = hygestat(50, 0, 10);");
    EXPECT_DOUBLE_EQ(evalScalar("m"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("v"), 0.0);
    // K=M → all successes → m=N, v=0.
    eval("[m, v] = hygestat(50, 50, 10);");
    EXPECT_DOUBLE_EQ(evalScalar("m"), 10.0);
    EXPECT_DOUBLE_EQ(evalScalar("v"),  0.0);
}

TEST_F(HygestatTest, InvalidParamsReturnNaN)
{
    EXPECT_TRUE(std::isnan(evalScalar("hygestat( 0,  0, 10)")));   // M=0
    EXPECT_TRUE(std::isnan(evalScalar("hygestat(50, 60, 10)")));   // K > M
    EXPECT_TRUE(std::isnan(evalScalar("hygestat(50, 20, 60)")));   // N > M
}
