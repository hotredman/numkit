// libs/image/tests/rgb2lightness_test.cpp
//
// Regression guard for rgb2lightness — L* channel of CIELAB.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class RGB2LightnessTest : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override
    {
        engine.eval(
            "import compat.*;"
            "A = uint8(reshape(linspace(20, 240, 48), [4 4 3]));");
    }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(RGB2LightnessTest, OutputClassSingle)
{
    eval("L = rgb2lightness(A);");
    EXPECT_EQ(eval("class(L)").toString(), "single");
    EXPECT_EQ(static_cast<int>(evalScalar("size(L, 1)")), 4);
    EXPECT_EQ(static_cast<int>(evalScalar("size(L, 2)")), 4);
}

TEST_F(RGB2LightnessTest, DiagonalValues)
{
    eval("L = rgb2lightness(A);");
    EXPECT_NEAR(evalScalar("double(L(1,1))"), 39.97166, 0.01);
    EXPECT_NEAR(evalScalar("double(L(2,2))"), 48.65904, 0.01);
    EXPECT_NEAR(evalScalar("double(L(3,3))"), 57.60390, 0.01);
    EXPECT_NEAR(evalScalar("double(L(4,4))"), 66.03730, 0.01);
}

TEST_F(RGB2LightnessTest, AllBlackIsZero)
{
    eval("L = rgb2lightness(uint8(zeros(3, 3, 3)));");
    EXPECT_NEAR(evalScalar("double(L(2,2))"), 0.0, 0.01);
}

TEST_F(RGB2LightnessTest, AllWhiteIsHundred)
{
    eval("L = rgb2lightness(uint8(255*ones(3, 3, 3)));");
    EXPECT_NEAR(evalScalar("double(L(2,2))"), 100.0, 0.01);
}

TEST_F(RGB2LightnessTest, BadShapeThrows)
{
    EXPECT_THROW(eval("rgb2lightness(uint8(zeros(3, 3)));"), std::exception);
}
