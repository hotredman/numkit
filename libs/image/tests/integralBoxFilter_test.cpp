// libs/image/tests/integralBoxFilter_test.cpp
//
// Regression guard for image/integralBoxFilter.
// Fingerprints from MATLAB R2025b on magic(8) integral image.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

class IntegralBoxFilterTest : public ::testing::Test
{
public:
    numkit::Engine engine;
    void SetUp() override
    {
        engine.eval("import compat.*;");
        engine.eval("A = magic(8);");
        engine.eval("I = integralImage(A);");
    }
    double evalScalar(const std::string &c) { return engine.eval(c).toScalar(); }
};

// Default 3x3 box filter on integralImage(magic(8)) → 6x6 mean.
TEST_F(IntegralBoxFilterTest, DefaultThreeByThree)
{
    engine.eval("B = integralBoxFilter(I);");
    EXPECT_DOUBLE_EQ(evalScalar("size(B,1)"), 6.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(B,2)"), 6.0);
    EXPECT_NEAR(evalScalar("B(1,1)"), 33.0,             1e-12);
    EXPECT_NEAR(evalScalar("B(1,2)"), 33.3333333333333, 1e-10);
    EXPECT_NEAR(evalScalar("B(6,6)"), 32.0,             1e-12);
}

// Explicit 5x5 box filter — output is 4x4.
TEST_F(IntegralBoxFilterTest, FiveByFiveBox)
{
    engine.eval("B = integralBoxFilter(I, 5);");
    EXPECT_DOUBLE_EQ(evalScalar("size(B,1)"), 4.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(B,2)"), 4.0);
}

// Asymmetric [3 5] — output 6×4.
TEST_F(IntegralBoxFilterTest, AsymmetricThreeByFive)
{
    engine.eval("B = integralBoxFilter(I, [3 5]);");
    EXPECT_DOUBLE_EQ(evalScalar("size(B,1)"), 6.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(B,2)"), 4.0);
    EXPECT_NEAR(evalScalar("B(1,1)"), 32.2666666666667, 1e-10);
}

// NormalizationFactor=1 returns raw sum.
TEST_F(IntegralBoxFilterTest, NormalizationFactorOne)
{
    engine.eval("B = integralBoxFilter(I, 3, 'NormalizationFactor', 1);");
    // sum(A(1:3, 1:3)) for magic(8) is 297.
    EXPECT_DOUBLE_EQ(evalScalar("B(1,1)"), 297.0);
}

// 3-D color: per-channel processing.
TEST_F(IntegralBoxFilterTest, ThreeDColorPerChannel)
{
    engine.eval("A3 = reshape(1:192, 8, 8, 3);");
    engine.eval("I3 = zeros(9, 9, 3); "
                "for c = 1:3; I3(:,:,c) = integralImage(A3(:,:,c)); end");
    engine.eval("B3 = integralBoxFilter(I3, 3);");
    EXPECT_DOUBLE_EQ(evalScalar("size(B3,1)"), 6.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(B3,2)"), 6.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(B3,3)"), 3.0);
    // First channel: A3(1:3, 1:3, 1) = [1 9 17; 2 10 18; 3 11 19], sum = 90, /9 = 10.
    EXPECT_NEAR(evalScalar("B3(1,1,1)"), 10.0, 1e-12);
}

// Even filterSize throws.
TEST_F(IntegralBoxFilterTest, EvenFilterSizeThrows)
{
    EXPECT_THROW(engine.eval("integralBoxFilter(I, 4);"), std::exception);
    EXPECT_THROW(engine.eval("integralBoxFilter(I, [3 4]);"), std::exception);
}

// Filter larger than underlying image throws.
TEST_F(IntegralBoxFilterTest, FilterTooLargeThrows)
{
    EXPECT_THROW(engine.eval("integralBoxFilter(I, 9);"), std::exception);
}

// Unknown NV-pair throws.
TEST_F(IntegralBoxFilterTest, UnknownNVKeyThrows)
{
    EXPECT_THROW(engine.eval("integralBoxFilter(I, 3, 'Bogus', 1);"),
                 std::exception);
}
