// toolboxes/stats/tests/unifit_test.cpp
// unifit.

#include <numkit/core/engine.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class UnifitTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(UnifitTest, BasicMLE)
{
    eval("x = [2 5 3 7 4 6 8 1 9 5]';");
    eval("[a, b, aci, bci] = unifit(x);");
    EXPECT_DOUBLE_EQ(evalScalar("a"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("b"), 9.0);
    EXPECT_NEAR(evalScalar("aci(1)"), -1.7942627814, 1e-9);
    EXPECT_DOUBLE_EQ(evalScalar("aci(2)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("bci(1)"), 9.0);
    EXPECT_NEAR(evalScalar("bci(2)"), 11.7942627814, 1e-9);
}

TEST_F(UnifitTest, NonDefaultAlpha)
{
    eval("x = [2 5 3 7 4 6 8 1 9 5]';");
    eval("[a, b, aci, bci] = unifit(x, 0.01);");
    EXPECT_DOUBLE_EQ(evalScalar("a"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("b"), 9.0);
    EXPECT_NEAR(evalScalar("aci(1)"), -3.6791455397, 1e-9);
    EXPECT_NEAR(evalScalar("bci(2)"), 13.6791455397, 1e-9);
}

TEST_F(UnifitTest, SingleElement)
{
    // Single point: range = 0, so CI = [x, x] (zero-width).
    eval("[a, b, aci, bci] = unifit([5]);");
    EXPECT_DOUBLE_EQ(evalScalar("a"), 5.0);
    EXPECT_DOUBLE_EQ(evalScalar("b"), 5.0);
    EXPECT_DOUBLE_EQ(evalScalar("aci(1)"), 5.0);
    EXPECT_DOUBLE_EQ(evalScalar("aci(2)"), 5.0);
    EXPECT_DOUBLE_EQ(evalScalar("bci(1)"), 5.0);
    EXPECT_DOUBLE_EQ(evalScalar("bci(2)"), 5.0);
}

TEST_F(UnifitTest, EmptyInputReturnsNaN)
{
    // Convention difference vs MATLAB: numkit returns NaN; MATLAB
    // returns empty arrays. We follow numkit's *fit family convention.
    eval("[a, b, aci, bci] = unifit([]);");
    EXPECT_TRUE(std::isnan(evalScalar("a")));
    EXPECT_TRUE(std::isnan(evalScalar("b")));
    EXPECT_TRUE(std::isnan(evalScalar("aci(1)")));
    EXPECT_TRUE(std::isnan(evalScalar("bci(2)")));
}
