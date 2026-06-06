// libs/image/tests/ind2gray_test.cpp
//
// Regression guard for ind2gray after cycle-30 rewrite.
//
// The previous implementation just took column 0 of MAP (so it was
// wrong for any non-grey RGB colormap) and always returned DOUBLE
// regardless of input class. This guard pins MATLAB-bit-equal
// behaviour for:
//   * grey colormap (works either way),
//   * RGB colormap with double index (BT.601 YIQ luma applied),
//   * uint8 / uint16 X (class-preserving via intlut LUT),
//   * past-end integer index (LUT padded with last value),
//   * out-of-range double index (clamped to [1, M]).

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class Ind2GrayTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override {
        engine.eval("import compat.*;"
                    "cm_gray = [0 0 0; 0.25 0.25 0.25; 0.5 0.5 0.5; 0.75 0.75 0.75; 1 1 1];"
                    "cm_rgb  = [1 0 0; 0 1 0; 0 0 1; 0.5 0.5 0; 0.2 0.4 0.6];");
    }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(Ind2GrayTest, GrayColormapDoubleIdx)
{
    eval("v = ind2gray([1 2 3; 3 4 5], cm_gray);");
    // Tolerance, not DOUBLE_EQ — the sum of the three coefficients
    // 0.298936+0.587043+0.114021 ≈ 1 - 1ulp on IEEE 754, so the
    // [1 1 1] row produces 0.99999999999999911 rather than exactly 1.
    EXPECT_NEAR(evalScalar("v(1,1)"), 0.0,  1e-15);
    EXPECT_NEAR(evalScalar("v(1,2)"), 0.25, 1e-14);
    EXPECT_NEAR(evalScalar("v(1,3)"), 0.5,  1e-14);
    EXPECT_NEAR(evalScalar("v(2,1)"), 0.5,  1e-14);
    EXPECT_NEAR(evalScalar("v(2,2)"), 0.75, 1e-14);
    EXPECT_NEAR(evalScalar("v(2,3)"), 1.0,  1e-14);
}

TEST_F(Ind2GrayTest, RgbColormapDoubleIdx)
{
    // YIQ luma coefficients applied per-row:
    //   [1 0 0] → 0.2989360213
    //   [0 1 0] → 0.5870430745
    //   [0 0 1] → 0.1140209043
    //   [0.5 0.5 0] → 0.4429895479
    //   [0.2 0.4 0.6] → 0.3630169766
    eval("v = ind2gray([1 2 3 4 5], cm_rgb);");
    EXPECT_NEAR(evalScalar("v(1)"), 0.298936021293775, 1e-14);
    EXPECT_NEAR(evalScalar("v(2)"), 0.587043074451121, 1e-14);
    EXPECT_NEAR(evalScalar("v(3)"), 0.114020904255103, 1e-14);
    EXPECT_NEAR(evalScalar("v(4)"), 0.442989547872448, 1e-14);
    EXPECT_NEAR(evalScalar("v(5)"), 0.363016976592266, 1e-14);
}

TEST_F(Ind2GrayTest, Uint8IndexClassPreserved)
{
    eval("v = ind2gray(uint8([0 1 4; 2 3 4]), cm_rgb);");
    EXPECT_EQ(static_cast<int>(evalScalar("v(1,1)")),  76);  // graycm(1)*255
    EXPECT_EQ(static_cast<int>(evalScalar("v(1,2)")), 150);  // graycm(2)*255
    EXPECT_EQ(static_cast<int>(evalScalar("v(1,3)")),  93);  // graycm(5)*255
    EXPECT_EQ(static_cast<int>(evalScalar("v(2,1)")),  29);  // graycm(3)*255
    EXPECT_EQ(static_cast<int>(evalScalar("v(2,2)")), 113);  // graycm(4)*255
    EXPECT_EQ(static_cast<int>(evalScalar("v(2,3)")),  93);  // graycm(5)*255
}

TEST_F(Ind2GrayTest, Uint16IndexClampPastEnd)
{
    eval("v = ind2gray(uint16([0 1 1000; 4 4 4]), cm_rgb);");
    EXPECT_EQ(static_cast<int>(evalScalar("v(1,1)")), 19591);  // graycm(1)*65535
    EXPECT_EQ(static_cast<int>(evalScalar("v(1,2)")), 38472);  // graycm(2)*65535
    EXPECT_EQ(static_cast<int>(evalScalar("v(1,3)")), 23790);  // clamp to graycm(5)*65535
    EXPECT_EQ(static_cast<int>(evalScalar("v(2,1)")), 23790);
    EXPECT_EQ(static_cast<int>(evalScalar("v(2,2)")), 23790);
    EXPECT_EQ(static_cast<int>(evalScalar("v(2,3)")), 23790);
}

TEST_F(Ind2GrayTest, OutOfRangeDoubleClamp)
{
    // -1 and 0 clamp up to 1 → graycm(1).  6 and 10 clamp down to 5
    // → graycm(5).
    eval("v = ind2gray([-1 0 6 10], cm_rgb);");
    EXPECT_NEAR(evalScalar("v(1)"), 0.298936021293775, 1e-14);
    EXPECT_NEAR(evalScalar("v(2)"), 0.298936021293775, 1e-14);
    EXPECT_NEAR(evalScalar("v(3)"), 0.363016976592266, 1e-14);
    EXPECT_NEAR(evalScalar("v(4)"), 0.363016976592266, 1e-14);
}

TEST_F(Ind2GrayTest, OutputClassMatchesInputClass)
{
    EXPECT_EQ(eval("ind2gray([1 2], cm_rgb)").type(),
              ValueType::DOUBLE);
    EXPECT_EQ(eval("ind2gray(single([1 2]), cm_rgb)").type(),
              ValueType::SINGLE);
    EXPECT_EQ(eval("ind2gray(uint8([0 1]), cm_rgb)").type(),
              ValueType::UINT8);
    EXPECT_EQ(eval("ind2gray(uint16([0 1]), cm_rgb)").type(),
              ValueType::UINT16);
}

TEST_F(Ind2GrayTest, EmptyInputReturnsEmpty)
{
    eval("v = ind2gray([], cm_rgb);");
    EXPECT_EQ(static_cast<int>(evalScalar("isempty(v)")), 1);
}

TEST_F(Ind2GrayTest, BadMapShapeThrows)
{
    EXPECT_THROW(eval("ind2gray([1 2 3], [0.5 0.5]);"),
                 std::exception);
    EXPECT_THROW(eval("ind2gray([1 2 3], zeros(3,3,3));"),
                 std::exception);
}

TEST_F(Ind2GrayTest, EmptyMapThrows)
{
    EXPECT_THROW(eval("ind2gray([1 2 3], []);"), std::exception);
}
