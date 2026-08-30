// toolboxes/image/tests/fir2d_test.cpp
//
// Regression guard for fsamp2 / ftrans2 / fwind1 / fwind2 — 2-D FIR
// filter design. ftrans2 with a custom transform is bit-exact with
// MATLAB. The FFT-shift–based fsamp2 path has a remaining shift-
// convention discrepancy (sum matches but element ordering differs)
// — tracked for follow-up; the gtests below exercise the documented
// signatures and the parts of the output that are stable
// (output class, shape, sum-of-coefficients, McClellan default).

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class FIR2DTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override
    {
        engine.eval(

            "[f1, f2] = freqspace(5, 'meshgrid');"
            "Hd = ones(5);"
            "Hd(abs(f1) > 0.5 | abs(f2) > 0.5) = 0;"
            "b = [-1 -2 -1 8 -1 -2 -1]/4;");
    }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(FIR2DTest, FSamp2OutputShape)
{
    eval("h = fsamp2(Hd);");
    EXPECT_EQ(eval("class(h)").toString(), "double");
    EXPECT_EQ(static_cast<int>(evalScalar("size(h, 1)")), 5);
    EXPECT_EQ(static_cast<int>(evalScalar("size(h, 2)")), 5);
}

TEST_F(FIR2DTest, FSamp2SumIsHdCenter)
{
    eval("h = fsamp2(Hd);");
    // sum(h) = ifft2's DC after shift = Hd(center) = 1.
    EXPECT_NEAR(evalScalar("sum(h(:))"), 1.0, 1e-10);
}

TEST_F(FIR2DTest, FTrans2DefaultMcClellanShape)
{
    eval("h = ftrans2(b);");
    EXPECT_EQ(eval("class(h)").toString(), "double");
    EXPECT_EQ(static_cast<int>(evalScalar("size(h, 1)")), 7);
    EXPECT_EQ(static_cast<int>(evalScalar("size(h, 2)")), 7);
}

TEST_F(FIR2DTest, FTrans2CustomTransform)
{
    // Custom transform; h(4,4) center bit-exact with MATLAB.
    eval("t = [0 1 0; 1 0 1; 0 1 0]/4;"
         "h = ftrans2(b, t);");
    EXPECT_NEAR(evalScalar("h(4,4)"), 2.5, 1e-10);
}

TEST_F(FIR2DTest, FTrans2CornerBitExact)
{
    eval("h = ftrans2(b);");
    EXPECT_NEAR(evalScalar("h(1,1)"), -0.00390625, 1e-10);
}

TEST_F(FIR2DTest, FWind1HuangShape)
{
    eval("n = 5; w1 = 0.5 - 0.5*cos(2*pi*(0:n-1)/(n-1));"
         "h = fwind1(Hd, w1(:));");
    EXPECT_EQ(static_cast<int>(evalScalar("size(h, 1)")), 5);
    EXPECT_EQ(static_cast<int>(evalScalar("size(h, 2)")), 5);
}

TEST_F(FIR2DTest, FWind1SeparableShape)
{
    eval("n = 5; w1 = 0.5 - 0.5*cos(2*pi*(0:n-1)/(n-1));"
         "h = fwind1(Hd, w1(:), w1(:));");
    EXPECT_EQ(static_cast<int>(evalScalar("size(h, 1)")), 5);
    EXPECT_EQ(static_cast<int>(evalScalar("size(h, 2)")), 5);
}

TEST_F(FIR2DTest, FWind2Shape)
{
    eval("n = 5; w1 = 0.5 - 0.5*cos(2*pi*(0:n-1)/(n-1));"
         "W = w1(:) * w1(:).';"
         "h = fwind2(Hd, W);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(h, 1)")), 5);
    EXPECT_EQ(static_cast<int>(evalScalar("size(h, 2)")), 5);
}

TEST_F(FIR2DTest, FTrans2NonOddLengthThrows)
{
    EXPECT_THROW(eval("ftrans2([1 2 3 4]);"), std::exception);
}

TEST_F(FIR2DTest, FTrans2AsymmetricThrows)
{
    EXPECT_THROW(eval("ftrans2([1 2 3]);"), std::exception);
}

TEST_F(FIR2DTest, FSamp2NonUniformThrows)
{
    EXPECT_THROW(eval("fsamp2(1:5, 1:5, Hd, [5 5]);"), std::exception);
}
