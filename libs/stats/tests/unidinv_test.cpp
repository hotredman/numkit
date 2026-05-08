// libs/stats/tests/unidinv_test.cpp
// Audit ТЗ closure for unidinv. Closes audit/findings/stats/unidinv.md.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class UnidinvTest : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(UnidinvTest, Median)
{
    EXPECT_DOUBLE_EQ(evalScalar("unidinv(0.5, 6)"), 3.0);
}

TEST_F(UnidinvTest, LowerAndUpperTail)
{
    EXPECT_DOUBLE_EQ(evalScalar("unidinv(0.1,  6)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("unidinv(0.99, 6)"), 6.0);
}

TEST_F(UnidinvTest, VectorQuantile)
{
    eval("v = unidinv([0.05 0.5 0.95], 10);");
    EXPECT_DOUBLE_EQ(evalScalar("v(1)"),  1.0);
    EXPECT_DOUBLE_EQ(evalScalar("v(2)"),  5.0);
    EXPECT_DOUBLE_EQ(evalScalar("v(3)"), 10.0);
}

TEST_F(UnidinvTest, BoundaryProb)
{
    // MATLAB: p=0 -> NaN (no integer pre-image), p=1 -> N.
    EXPECT_TRUE(std::isnan(evalScalar("unidinv(0, 6)")));
    EXPECT_DOUBLE_EQ(evalScalar("unidinv(1, 6)"), 6.0);
}

TEST_F(UnidinvTest, EdgeCases)
{
    EXPECT_TRUE(std::isnan(evalScalar("unidinv(-0.1, 6)")));
    EXPECT_TRUE(std::isnan(evalScalar("unidinv( 1.5, 6)")));
    EXPECT_TRUE(std::isnan(evalScalar("unidinv( 0.5, 0)")));
    EXPECT_TRUE(std::isnan(evalScalar("unidinv( 0.5, -1)")));
    EXPECT_TRUE(std::isnan(evalScalar("unidinv( 0.5, 6.5)")));  // non-integer N
    EXPECT_TRUE(std::isnan(evalScalar("unidinv( NaN, 6)")));
    EXPECT_TRUE(std::isnan(evalScalar("unidinv( 0.5, NaN)")));
}
