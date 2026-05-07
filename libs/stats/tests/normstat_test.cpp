// libs/stats/tests/normstat_test.cpp
//
// Audit ТЗ closure for normstat. Reference values from MATLAB R2025b.
// Closes audit/findings/stats/normstat.md.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class NormstatTest : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(NormstatTest, ScalarMomentsAreMuAndSigmaSquared)
{
    eval("[m, v] = normstat(0, 1);");
    EXPECT_DOUBLE_EQ(evalScalar("m"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("v"), 1.0);
}

TEST_F(NormstatTest, VectorBroadcasting)
{
    eval("[m, v] = normstat([0 1 -2], [1 2 0.5]);");
    EXPECT_DOUBLE_EQ(evalScalar("m(1)"),  0.0);
    EXPECT_DOUBLE_EQ(evalScalar("m(2)"),  1.0);
    EXPECT_DOUBLE_EQ(evalScalar("m(3)"), -2.0);
    EXPECT_DOUBLE_EQ(evalScalar("v(1)"),  1.0);
    EXPECT_DOUBLE_EQ(evalScalar("v(2)"),  4.0);
    EXPECT_DOUBLE_EQ(evalScalar("v(3)"),  0.25);
}

TEST_F(NormstatTest, ScalarMuVectorSigma)
{
    eval("[m, v] = normstat(0, [1 2 5]);");
    EXPECT_DOUBLE_EQ(evalScalar("m(1)"),  0.0);
    EXPECT_DOUBLE_EQ(evalScalar("m(3)"),  0.0);
    EXPECT_DOUBLE_EQ(evalScalar("v(1)"),  1.0);
    EXPECT_DOUBLE_EQ(evalScalar("v(2)"),  4.0);
    EXPECT_DOUBLE_EQ(evalScalar("v(3)"), 25.0);
}

TEST_F(NormstatTest, InvalidSigmaReturnsNaN)
{
    EXPECT_TRUE(std::isnan(evalScalar("normstat(0,  0)")));
    EXPECT_TRUE(std::isnan(evalScalar("normstat(0, -1)")));
}
