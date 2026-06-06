// libs/image/tests/integralBoxFilter_test.cpp
//
// Regression guard for image/integralBoxFilter.
// Fingerprints from MATLAB R2025b on magic(8) integral image.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

class IntegralBoxFilterTest : public ::testing::Test
{
public:
    numkit::StdEngine engine;
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

// NormalizationFactor is a MULTIPLIER (MATLAB semantics): box-sum * normFactor.
// raw 3x3 box sum at (1,1) is 297, so 0.5 -> 148.5 and 2 -> 594.
TEST_F(IntegralBoxFilterTest, NormalizationFactorMultiplierSemantics)
{
    engine.eval("Bh = integralBoxFilter(I, 3, 'NormalizationFactor', 0.5);");
    EXPECT_DOUBLE_EQ(evalScalar("Bh(1,1)"), 148.5);   // 297 * 0.5
    engine.eval("Bx2 = integralBoxFilter(I, 3, 'NormalizationFactor', 2);");
    EXPECT_DOUBLE_EQ(evalScalar("Bx2(1,1)"), 594.0);  // 297 * 2
    // Default (no NV) equals the mean = raw / 9 = 33.
    engine.eval("Bd = integralBoxFilter(I);");
    EXPECT_NEAR(evalScalar("Bd(1,1)"), 33.0, 1e-12);
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

// ── integralBoxFilter3 — 3-D box filter on a summed-volume table ──────
// Fingerprints from MATLAB R2025b on integralImage3(reshape(1:125,5,5,5)).
class IntegralBoxFilter3Test : public ::testing::Test
{
public:
    numkit::StdEngine engine;
    void SetUp() override
    {
        engine.eval("import compat.*;");
        engine.eval("V = reshape(1:125, 5, 5, 5);");
        engine.eval("J = integralImage3(V);");
    }
    double evalScalar(const std::string &c) { return engine.eval(c).toScalar(); }
};

// Default 3x3x3 box → 3x3x3 mean.
TEST_F(IntegralBoxFilter3Test, DefaultThreeCubed)
{
    engine.eval("B = integralBoxFilter3(J);");
    EXPECT_DOUBLE_EQ(evalScalar("size(B,1)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(B,2)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(B,3)"), 3.0);
    EXPECT_NEAR(evalScalar("B(1,1,1)"), 32.0, 1e-12);
    EXPECT_NEAR(evalScalar("B(3,3,1)"), 44.0, 1e-12);
    EXPECT_NEAR(evalScalar("B(2,2,2)"), 63.0, 1e-12);
    EXPECT_NEAR(evalScalar("B(3,3,3)"), 94.0, 1e-12);
    EXPECT_NEAR(evalScalar("B(1,1,3)"), 82.0, 1e-12);
}

// Scalar filterSize is equivalent to the [n n n] vector.
TEST_F(IntegralBoxFilter3Test, ScalarEqualsVector)
{
    engine.eval("Bs = integralBoxFilter3(J, 3);");
    EXPECT_NEAR(evalScalar("Bs(1,1,1)"), 32.0, 1e-12);
    EXPECT_NEAR(evalScalar("Bs(3,3,3)"), 94.0, 1e-12);
}

// Anisotropic [1 3 5] box → output [5 3 1].
TEST_F(IntegralBoxFilter3Test, AnisotropicFilterSize)
{
    engine.eval("Bv = integralBoxFilter3(J, [1 3 5]);");
    EXPECT_DOUBLE_EQ(evalScalar("size(Bv,1)"), 5.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(Bv,2)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(Bv,3)"), 1.0);
    EXPECT_NEAR(evalScalar("Bv(1,1,1)"), 56.0, 1e-12);
    EXPECT_NEAR(evalScalar("Bv(5,3,1)"), 70.0, 1e-12);
}

// NormalizationFactor=1 → raw box sum.
TEST_F(IntegralBoxFilter3Test, NormalizationFactorOneRawSum)
{
    engine.eval("Bn1 = integralBoxFilter3(J, 3, 'NormalizationFactor', 1);");
    EXPECT_DOUBLE_EQ(evalScalar("Bn1(1,1,1)"), 864.0);
    EXPECT_DOUBLE_EQ(evalScalar("Bn1(3,3,3)"), 2538.0);
}

// NormalizationFactor=0.5 multiplies the box sum by 0.5 (MATLAB semantics).
TEST_F(IntegralBoxFilter3Test, NormalizationFactorHalfMultiplier)
{
    engine.eval("Bh = integralBoxFilter3(J, 3, 'NormalizationFactor', 0.5);");
    EXPECT_DOUBLE_EQ(evalScalar("Bh(1,1,1)"), 432.0);  // 864 * 0.5
}

// Even filterSize throws.
TEST_F(IntegralBoxFilter3Test, EvenFilterSizeThrows)
{
    EXPECT_THROW(engine.eval("integralBoxFilter3(J, 2);"), std::exception);
    EXPECT_THROW(engine.eval("integralBoxFilter3(J, [3 3 2]);"), std::exception);
}

// Filter larger than the underlying volume throws.
TEST_F(IntegralBoxFilter3Test, FilterTooLargeThrows)
{
    EXPECT_THROW(engine.eval("integralBoxFilter3(J, 7);"), std::exception);
}

// Unknown NV-pair throws.
TEST_F(IntegralBoxFilter3Test, UnknownNVKeyThrows)
{
    EXPECT_THROW(engine.eval("integralBoxFilter3(J, 3, 'Bogus', 1);"),
                 std::exception);
}
