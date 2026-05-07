// libs/stats/tests/lognstat_test.cpp
// Audit ТЗ closure for lognstat. Closes audit/findings/stats/lognstat.md.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class LognstatTest : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(LognstatTest, ScalarMomentsLN01)
{
    // LN(0,1): m = exp(0.5), v = (e-1)·e
    eval("[m, v] = lognstat(0, 1);");
    EXPECT_NEAR(evalScalar("m"), 1.6487212707001282, 1e-9);
    EXPECT_NEAR(evalScalar("v"), 4.6707742704716046, 1e-9);
}

TEST_F(LognstatTest, VectorBroadcasting)
{
    eval("[m, v] = lognstat([0 1 2], [1 0.5 1]);");
    EXPECT_NEAR(evalScalar("m(1)"), 1.6487212707001282, 1e-9);
    EXPECT_NEAR(evalScalar("m(2)"), 3.0802168489614706, 1e-9);
    EXPECT_NEAR(evalScalar("m(3)"), 12.1824939607034728, 1e-9);
}

TEST_F(LognstatTest, ScalarMuVectorSigma)
{
    eval("[m, v] = lognstat(0, [0.5 1 2]);");
    EXPECT_NEAR(evalScalar("m(1)"), 1.1331484530668263, 1e-9);
    EXPECT_NEAR(evalScalar("m(3)"), 7.3890560989306504, 1e-9);
}

TEST_F(LognstatTest, InvalidSigmaReturnsNaN)
{
    EXPECT_TRUE(std::isnan(evalScalar("lognstat(0,  0)")));
    EXPECT_TRUE(std::isnan(evalScalar("lognstat(0, -1)")));
}
