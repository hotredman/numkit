// toolboxes/signal/tests/fftn_test.cpp
//
// gtest unit coverage for fftn / ifftn — N-D forward and inverse FFT.
// Implementation iterates the per-axis fft / ifft over every dimension
// of the input, so the test surface mostly exercises shape handling
// (2-D delegating to fft2-equivalent, 3-D pages, sz override, round-trip
// identity) rather than re-testing the FFT kernel itself.

#include <numkit/core/engine.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

class FftnTest : public ::testing::Test
{
public:
    numkit::StandardEngine engine;
    void   SetUp() override {  }
    double eval_scalar(const std::string &c) { return engine.eval(c).toScalar(); }
};

// 2-D delegate path — fftn on a matrix must produce the same answer as
// fft2 (within fp noise). The constant-input case has all FFT energy
// at the DC bin only.
TEST_F(FftnTest, TwoDimMatchesFft2)
{
    engine.eval("X = [1 2 3; 4 5 6; 7 8 9];");
    engine.eval("Y = fftn(X);");
    EXPECT_NEAR(eval_scalar("real(Y(1,1))"), 45.0, 1e-9);
    // Off-DC bins should be zero magnitude up to fp noise.
    EXPECT_LT(eval_scalar("abs(Y(2,2))"), 1e-9);
    EXPECT_LT(eval_scalar("abs(Y(3,3))"), 1e-9);
}

// 3-D path — fingerprints from MATLAB R2025b on reshape(1:24, 2,3,4).
TEST_F(FftnTest, ThreeDimMatchesMatlab)
{
    engine.eval("X = reshape(1:24, 2, 3, 4);");
    engine.eval("Y = fftn(X);");
    EXPECT_DOUBLE_EQ(eval_scalar("size(Y, 1)"), 2.0);
    EXPECT_DOUBLE_EQ(eval_scalar("size(Y, 2)"), 3.0);
    EXPECT_DOUBLE_EQ(eval_scalar("size(Y, 3)"), 4.0);
    EXPECT_NEAR(eval_scalar("real(Y(1,1,1))"),  300.0, 1e-9);  // DC of all
    EXPECT_NEAR(eval_scalar("real(Y(1,1,2))"),  -72.0, 1e-9);
    EXPECT_NEAR(eval_scalar("imag(Y(1,1,2))"),   72.0, 1e-9);
    EXPECT_NEAR(eval_scalar("real(Y(1,1,4))"),  -72.0, 1e-9);
    EXPECT_NEAR(eval_scalar("imag(Y(1,1,4))"),  -72.0, 1e-9);
}

// sz override — zero-pad each dimension to a target length before its
// FFT. The DC bin becomes the input sum (not size-scaled).
TEST_F(FftnTest, SizeOverridePads)
{
    engine.eval("Y = fftn([1 2; 3 4], [4 4]);");
    EXPECT_DOUBLE_EQ(eval_scalar("size(Y, 1)"), 4.0);
    EXPECT_DOUBLE_EQ(eval_scalar("size(Y, 2)"), 4.0);
    EXPECT_NEAR(eval_scalar("real(Y(1,1))"), 10.0, 1e-9);  // 1+2+3+4
}

// ifftn round-trip — must recover the original real input within
// ulp-level numerical noise.
TEST_F(FftnTest, IfftnRoundTrip)
{
    engine.eval("X = reshape(1:24, 2, 3, 4);");
    engine.eval("Xr = ifftn(fftn(X));");
    engine.eval("err = max(abs(X(:) - Xr(:)));");
    EXPECT_LT(eval_scalar("err"), 1e-12);
}

// ifftn on the FFT output bin pattern — recover input scalars at the
// corners.
TEST_F(FftnTest, IfftnRecoversCorners)
{
    engine.eval("X = reshape(1:24, 2, 3, 4);");
    engine.eval("Yi = ifftn(fftn(X));");
    EXPECT_NEAR(eval_scalar("real(Yi(1,1,1))"), 1.0,  1e-12);
    EXPECT_NEAR(eval_scalar("real(Yi(2,3,4))"), 24.0, 1e-12);
}

// sz vector longer than ndims(X) — must throw (matches the documented
// "size vector length exceeds ndims(X)" guard).
TEST_F(FftnTest, SizeArgTooLongThrows)
{
    EXPECT_THROW(engine.eval("fftn([1 2; 3 4], [4 4 4 4]);"), std::exception);
}
