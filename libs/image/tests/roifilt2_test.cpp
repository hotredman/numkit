// libs/image/tests/roifilt2_test.cpp
//
// Regression guard for image/roifilt2 (filter a region of interest).
// Fingerprints from MATLAB R2025b.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class Roifilt2Test : public ::testing::Test
{
public:
    StdEngine engine;
    void SetUp() override
    {
        engine.eval("import compat.*;");
        engine.eval("I = magic(6);");
        engine.eval("BW = false(6,6); BW(2:4,2:4) = true;");
        engine.eval("h = [0 1 0; 1 -4 1; 0 1 0];");
    }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// Form 1: filter form, only the masked pixels change.
TEST_F(Roifilt2Test, FilterFormReplacesOnlyMaskedPixels)
{
    eval("J = roifilt2(h, I, BW);");
    EXPECT_TRUE(eval("J").type() == ValueType::DOUBLE);
    EXPECT_EQ(static_cast<int>(evalScalar("size(J,1)")), 6);
    // Outside the mask: equals I.
    EXPECT_DOUBLE_EQ(evalScalar("J(1,1)"), evalScalar("I(1,1)"));
    EXPECT_DOUBLE_EQ(evalScalar("J(6,6)"), evalScalar("I(6,6)"));
    // Inside the mask: Laplacian-filtered values (from MATLAB).
    EXPECT_DOUBLE_EQ(evalScalar("J(3,3)"), 63.0);
    EXPECT_DOUBLE_EQ(evalScalar("J(2,2)"), -108.0);
    EXPECT_DOUBLE_EQ(evalScalar("J(4,4)"), 9.0);
}

// Filter form equals full-image imfilter then mask.
TEST_F(Roifilt2Test, FilterFormEqualsFullImfilterMasked)
{
    eval("J = roifilt2(h, I, BW);");
    eval("F = imfilter(I, h); Jref = I; Jref(BW) = F(BW);");
    EXPECT_DOUBLE_EQ(evalScalar("isequal(J, Jref)"), 1.0);
}

// Form 2: function-handle form, output class follows the handle.
TEST_F(Roifilt2Test, FunctionFormUint8)
{
    eval("I8 = uint8(magic(6)*5);");
    eval("J = roifilt2(I8, BW, @(x) x*2);");
    EXPECT_TRUE(eval("J").type() == ValueType::UINT8);
    EXPECT_DOUBLE_EQ(evalScalar("double(J(1,1))"), evalScalar("double(I8(1,1))"));  // outside
    EXPECT_DOUBLE_EQ(evalScalar("double(J(3,3))"), 20.0);                            // 2*10 inside
}

TEST_F(Roifilt2Test, FunctionFormClassChange)
{
    eval("I8 = uint8(magic(6)*5);");
    eval("J = roifilt2(I8, BW, @(x) double(x)+0.5);");
    EXPECT_TRUE(eval("J").type() == ValueType::DOUBLE);
    EXPECT_DOUBLE_EQ(evalScalar("J(1,1)"), 175.0);   // I cast to double, outside mask
    EXPECT_DOUBLE_EQ(evalScalar("J(3,3)"), 10.5);    // double(10)+0.5 inside
}

TEST_F(Roifilt2Test, MaskSizeMismatchThrows)
{
    bool threw = false;
    try { eval("roifilt2(h, I, false(3,3));"); } catch (...) { threw = true; }
    EXPECT_TRUE(threw);
}

TEST_F(Roifilt2Test, NumericMaskAcceptedAsLogical)
{
    eval("BWn = double(BW);");           // numeric mask, treated as BWn != 0
    eval("J = roifilt2(h, I, BWn);");
    EXPECT_DOUBLE_EQ(evalScalar("J(3,3)"), 63.0);
}
