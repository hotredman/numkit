// libs/stats/tests/binostat_test.cpp
// Audit ТЗ closure for binostat. Closes audit/findings/stats/binostat.md.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class BinostatTest : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(BinostatTest, ScalarMomentsBin10p03)
{
    eval("[m, v] = binostat(10, 0.3);");
    EXPECT_DOUBLE_EQ(evalScalar("m"), 3.0);
    EXPECT_NEAR(evalScalar("v"), 2.1, 1e-12);
}

TEST_F(BinostatTest, VectorBroadcastingNVector)
{
    eval("[m, v] = binostat([5 10 20], 0.5);");
    EXPECT_DOUBLE_EQ(evalScalar("m(1)"),  2.5);
    EXPECT_DOUBLE_EQ(evalScalar("m(2)"),  5.0);
    EXPECT_DOUBLE_EQ(evalScalar("m(3)"), 10.0);
}

TEST_F(BinostatTest, BoundaryProbabilities)
{
    // p=0 → all failures, p=1 → all successes; both have variance 0.
    eval("[m, v] = binostat(10, 0);");
    EXPECT_DOUBLE_EQ(evalScalar("m"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("v"), 0.0);
    eval("[m, v] = binostat(10, 1);");
    EXPECT_DOUBLE_EQ(evalScalar("m"), 10.0);
    EXPECT_DOUBLE_EQ(evalScalar("v"),  0.0);
}

TEST_F(BinostatTest, ZeroNIsValidMomentsZero)
{
    // MATLAB: binostat(0, p) → m=0, v=0 (degenerate).
    eval("[m, v] = binostat(0, 0.5);");
    EXPECT_DOUBLE_EQ(evalScalar("m"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("v"), 0.0);
}

TEST_F(BinostatTest, InvalidParamsReturnNaN)
{
    EXPECT_TRUE(std::isnan(evalScalar("binostat(-1,  0.5)")));
    EXPECT_TRUE(std::isnan(evalScalar("binostat(10, -0.1)")));
    EXPECT_TRUE(std::isnan(evalScalar("binostat(10,  1.5)")));
    EXPECT_TRUE(std::isnan(evalScalar("binostat(2.5, 0.5)")));  // non-integer n
}
