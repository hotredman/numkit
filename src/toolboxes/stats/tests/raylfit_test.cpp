// toolboxes/stats/tests/raylfit_test.cpp
// raylfit.

#include <numkit/core/engine.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class RaylfitTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override {}
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(RaylfitTest, BasicMLE)
{
    eval("x = [1.5 2.3 0.8 3.1 1.7 2.0 1.4 2.8 1.9 2.2]';");
    eval("[s, sc] = raylfit(x);");
    EXPECT_NEAR(evalScalar("s"),     1.4650938536, 1e-9);
    EXPECT_NEAR(evalScalar("sc(1)"), 1.1208834401, 1e-9);
    EXPECT_NEAR(evalScalar("sc(2)"), 2.1156973340, 1e-9);
}

TEST_F(RaylfitTest, NonDefaultAlpha)
{
    eval("x = [1.5 2.3 0.8 3.1 1.7 2.0 1.4 2.8 1.9 2.2]';");
    eval("[s, sc] = raylfit(x, 0.01);");
    EXPECT_NEAR(evalScalar("s"),     1.4650938536, 1e-9);
    EXPECT_NEAR(evalScalar("sc(1)"), 1.0360186408, 1e-9);
    EXPECT_NEAR(evalScalar("sc(2)"), 2.4031103559, 1e-9);
}

TEST_F(RaylfitTest, SingleElement)
{
    eval("[s, sc] = raylfit([2.5]');");
    EXPECT_NEAR(evalScalar("s"),     1.7677669530, 1e-9);
    EXPECT_NEAR(evalScalar("sc(1)"), 0.9204024777, 1e-9);
    EXPECT_NEAR(evalScalar("sc(2)"), 11.1099463046, 1e-9);
}

TEST_F(RaylfitTest, EmptyInput)
{
    eval("[s, sc] = raylfit([]);");
    EXPECT_TRUE(std::isnan(evalScalar("s")));
    EXPECT_TRUE(std::isnan(evalScalar("sc(1)")));
    EXPECT_TRUE(std::isnan(evalScalar("sc(2)")));
}
