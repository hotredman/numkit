// toolboxes/stats/tests/gpstat_test.cpp
// gpstat.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class GpstatTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(GpstatTest, ScalarMoments)
{
    eval("[m, v] = gpstat(0.3, 1, 0);");
    EXPECT_NEAR(evalScalar("m"), 1.0/0.7,         1e-12);  // 1/(1-0.3)
    EXPECT_NEAR(evalScalar("v"), 1.0/(0.49*0.4),  1e-12);
}

TEST_F(GpstatTest, K05_VarianceIsInf)
{
    eval("[m, v] = gpstat(0.5, 1, 0);");
    EXPECT_DOUBLE_EQ(evalScalar("m"), 2.0);
    EXPECT_TRUE(std::isinf(evalScalar("v")));
}

TEST_F(GpstatTest, K1_MeanIsInf)
{
    eval("[m, v] = gpstat(1, 1, 0);");
    EXPECT_TRUE(std::isinf(evalScalar("m")));
}

TEST_F(GpstatTest, VectorBroadcastingK)
{
    eval("[m, v] = gpstat([0.3 0 -0.3], 1, 0);");
    EXPECT_NEAR(evalScalar("m(1)"), 1.0/0.7, 1e-12);
    EXPECT_DOUBLE_EQ(evalScalar("m(2)"), 1.0);              // k=0 → m=σ
    EXPECT_NEAR(evalScalar("m(3)"), 1.0/1.3, 1e-12);
}

TEST_F(GpstatTest, InvalidSigmaReturnsNaN)
{
    EXPECT_TRUE(std::isnan(evalScalar("gpstat(0.3,  0, 0)")));
    EXPECT_TRUE(std::isnan(evalScalar("gpstat(0.3, -1, 0)")));
}
