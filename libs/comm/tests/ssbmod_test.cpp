// libs/comm/tests/ssbmod_test.cpp
//
// Regression guard for ssbmod() — single-sideband modulator.
//   y = x · cos(2π·Fc·t + ini_phase) ± imag(hilbert(x)) · sin(...)
// (lower sideband uses +, upper uses -).

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class SsbmodTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(SsbmodTest, LowerSidebandKnownValues)
{
    // MATLAB ref: ssbmod(sin(2*pi*5*((0:31)'/8000)), 100, 8000)
    eval("fs = 8000; fc = 100; t = (0:31)'/fs; x = sin(2*pi*5*t);"
         "y = ssbmod(x, fc, fs);");
    EXPECT_DOUBLE_EQ(evalScalar("y(1)"), 0.0);
    // Hilbert is FFT-based -> a few ULP from MATLAB's reference; loosen.
    EXPECT_NEAR(evalScalar("y(2)"),  0.006744048814416892, 1e-9);
    EXPECT_NEAR(evalScalar("y(5)"),  0.018093047537312110, 1e-9);
}

TEST_F(SsbmodTest, UpperSidebandKnownValues)
{
    eval("fs = 8000; fc = 100; t = (0:31)'/fs; x = sin(2*pi*5*t);"
         "y = ssbmod(x, fc, fs, 0, 'upper');");
    EXPECT_DOUBLE_EQ(evalScalar("y(1)"), 0.0);
    EXPECT_NEAR(evalScalar("y(2)"), 0.001085701491179241, 1e-9);
    EXPECT_NEAR(evalScalar("y(5)"), 0.011784045428533350, 1e-9);
}

TEST_F(SsbmodTest, IniPhaseShifts)
{
    // y(1) = 0 * cos(pi/4) + imag(hilbert(x))(1) * sin(pi/4)
    eval("y = ssbmod(sin(2*pi*5*((0:31)'/8000)), 100, 8000, pi/4);");
    EXPECT_NEAR(evalScalar("y(1)"), 0.08175694512535277, 1e-9);
}

TEST_F(SsbmodTest, RowOrientationPreserved)
{
    eval("y = ssbmod(sin(2*pi*5*((0:31)/8000)), 100, 8000);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(y, 1)")), 1);
    EXPECT_EQ(static_cast<int>(evalScalar("size(y, 2)")), 32);
}

TEST_F(SsbmodTest, ColumnOrientationPreserved)
{
    eval("y = ssbmod(sin(2*pi*5*((0:31)'/8000)), 100, 8000);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(y, 1)")), 32);
    EXPECT_EQ(static_cast<int>(evalScalar("size(y, 2)")), 1);
}

TEST_F(SsbmodTest, RejectsFsLessOrEqualTwoFc)
{
    bool threw = false;
    try { eval("ssbmod(sin((0:31)'/200), 100, 200);"); }
    catch (...) { threw = true; }
    EXPECT_TRUE(threw);
}

TEST_F(SsbmodTest, LSBPlusUSBMatchesDoubleCarrier)
{
    // x*cos via DSB-SC = 0.5 * (LSB + USB) by hilbert identity.
    eval("fs = 8000; fc = 100; t = (0:63)'/fs; x = sin(2*pi*5*t);"
         "yL = ssbmod(x, fc, fs);"
         "yU = ssbmod(x, fc, fs, 0, 'upper');"
         "yDSB = x .* cos(2*pi*fc*t);"
         "err = max(abs(0.5*(yL + yU) - yDSB));");
    EXPECT_LT(evalScalar("err"), 1e-9);
}
