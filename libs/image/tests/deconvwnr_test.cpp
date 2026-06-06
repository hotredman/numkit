// libs/image/tests/deconvwnr_test.cpp
//
// Regression guard for deconvwnr — Wiener deconvolution. Reference
// values from MATLAB R2025b verified at 1e-7 (the small numerical
// noise comes from psf2otf's complex residual on real-symmetric
// PSFs).

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class DeconvwnrTest : public ::testing::Test
{
public:
    StdEngine engine;
    void SetUp() override {
        engine.eval("import compat.*;"
                    "I = double([0.1 0.2 0.3; 0.4 0.5 0.6; 0.7 0.8 0.9]);"
                    "PSF = ones(3) / 9;");
    }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(DeconvwnrTest, NSRZeroIdealInverse)
{
    eval("J = deconvwnr(I, PSF, 0);");
    // Averaging-kernel inverse of a linear gradient → constant mean = 0.5.
    EXPECT_NEAR(evalScalar("J(1,1)"), 0.5, 1e-7);
    EXPECT_NEAR(evalScalar("J(2,2)"), 0.5, 1e-7);
    EXPECT_NEAR(evalScalar("J(3,3)"), 0.5, 1e-7);
}

TEST_F(DeconvwnrTest, NSRRegularised)
{
    eval("J = deconvwnr(I, PSF, 0.01);");
    // 1/(1+NSR) = 1/1.01 ≈ 0.9901; result = 0.5 * 0.9901 ≈ 0.495049.
    EXPECT_NEAR(evalScalar("J(1,1)"), 0.4950495049504950, 1e-12);
    EXPECT_NEAR(evalScalar("J(2,2)"), 0.4950495049504950, 1e-12);
}

TEST_F(DeconvwnrTest, NSRStronglyRegularised)
{
    eval("J = deconvwnr(I, PSF, 0.1);");
    // 1/1.1 ≈ 0.9091; 0.5 * 0.9091 ≈ 0.454545.
    EXPECT_NEAR(evalScalar("J(1,1)"), 0.4545454545454545, 1e-12);
}

TEST_F(DeconvwnrTest, FourArgScalarEquivalenceWithNSR)
{
    // (NCORR=0.01, ICORR=1) must equal NSR=0.01.
    eval("Ja = deconvwnr(I, PSF, 0.01);"
         "Jb = deconvwnr(I, PSF, 0.01, 1);"
         "d  = max(max(abs(Ja - Jb)));");
    EXPECT_LT(evalScalar("d"), 1e-12);
}

TEST_F(DeconvwnrTest, Uint8ClassPreserved)
{
    eval("Iu = uint8([10 20 30; 40 50 60; 70 80 90]);"
         "Ju = deconvwnr(Iu, PSF, 0.01);");
    EXPECT_EQ(eval("Ju").type(), ValueType::UINT8);
    EXPECT_EQ(static_cast<int>(evalScalar("Ju(1,1)")), 50);
    EXPECT_EQ(static_cast<int>(evalScalar("Ju(2,2)")), 50);
}

TEST_F(DeconvwnrTest, RoundTripWithSmallPSF)
{
    eval("PSF3 = fspecial('gaussian', 7, 1.5);"
         "Iorig = double(reshape(1:100, 10, 10));"
         "H = psf2otf(PSF3, [10 10]);"
         "Iblur = real(ifft2(fft2(Iorig) .* H));"
         "J = deconvwnr(Iblur, PSF3, 0);"
         "d = max(max(abs(J - Iorig)));");
    EXPECT_LT(evalScalar("d"), 1e-6);
}

TEST_F(DeconvwnrTest, DoubleOutputForDoubleInput)
{
    EXPECT_EQ(eval("deconvwnr(I, PSF, 0)").type(), ValueType::DOUBLE);
}

TEST_F(DeconvwnrTest, ThreeDimVolumePerPagePassThrough)
{
    eval("V = repmat(I, [1 1 2]);"
         "J = deconvwnr(V, PSF, 0);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(J,3)")), 2);
    // Each page processed independently with the same OTF.
    EXPECT_NEAR(evalScalar("J(1,1,1)"), 0.5, 1e-7);
    EXPECT_NEAR(evalScalar("J(1,1,2)"), 0.5, 1e-7);
}

TEST_F(DeconvwnrTest, BadNarginThrows)
{
    EXPECT_THROW(eval("deconvwnr(I);"), std::exception);
}
