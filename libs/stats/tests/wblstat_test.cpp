// libs/stats/tests/wblstat_test.cpp
// wblstat.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class WblstatTest : public ::testing::Test
{
public:
    StdEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(WblstatTest, ScalarMomentsWbl12)
{
    eval("[m, v] = wblstat(1, 2);");
    EXPECT_NEAR(evalScalar("m"), 0.8862269254527580, 1e-9);  // Γ(1.5)
    EXPECT_NEAR(evalScalar("v"), 0.2146018366025515, 1e-9);
}

TEST_F(WblstatTest, VectorBroadcasting)
{
    eval("[m, v] = wblstat([1 2 3], [1 2 3]);");
    EXPECT_DOUBLE_EQ(evalScalar("m(1)"), 1.0);                // Γ(2) = 1
    EXPECT_NEAR(evalScalar("m(2)"), 1.7724538509055159, 1e-9);
    EXPECT_NEAR(evalScalar("m(3)"), 2.6789385347077479, 1e-9);
}

TEST_F(WblstatTest, InvalidParamsReturnNaN)
{
    EXPECT_TRUE(std::isnan(evalScalar("wblstat(0, 1)")));
    EXPECT_TRUE(std::isnan(evalScalar("wblstat(1, 0)")));
    EXPECT_TRUE(std::isnan(evalScalar("wblstat(-1, 1)")));
}
