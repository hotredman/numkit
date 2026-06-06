// libs/comm/tests/pmmod_test.cpp
//
// Regression guard for pmmod() — phase modulator.
// y = cos(2π·Fc·t + phasedev·x + ini_phase)

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class PmmodTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(PmmodTest, KnownReferenceValues)
{
    // MATLAB reference (full precision):
    //   pmmod(sin(2*pi*5*((0:9)'/8000)), 100, 8000, pi/4)
    //   y(1) = 1 (cos(0))
    //   y(2) = 0.9966706056...
    //   y(10) = 0.7420916690...
    eval("fs = 8000; fc = 100; t = (0:9)'/fs; x = sin(2*pi*5*t);"
         "y = pmmod(x, fc, fs, pi/4);");
    EXPECT_DOUBLE_EQ(evalScalar("y(1)"), 1.0);
    // tol=1e-9: numkit's sin/cos go through Highway SIMD (≤ 5 ULP);
    // composing via phasedev*sin(...) into cos(...) accumulates a few
    // ULP. Real bugs would show diff >> this magnitude.
    EXPECT_NEAR(evalScalar("y(2)"),  0.9966706056297996,  1e-9);
    EXPECT_NEAR(evalScalar("y(10)"), 0.7420916689955103,  1e-9);
}

TEST_F(PmmodTest, IniPhaseShifts)
{
    // ini_phase = pi/3 -> y(1) = cos(pi/3) = 0.5
    eval("y = pmmod(sin(2*pi*5*((0:9)'/8000)), 100, 8000, pi/4, pi/3);");
    EXPECT_NEAR(evalScalar("y(1)"), 0.5, 1e-12);
}

TEST_F(PmmodTest, RowOrientationPreserved)
{
    // Input row -> output row
    eval("y = pmmod(sin(2*pi*5*((0:9)/8000)), 100, 8000, pi/4);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(y, 1)")), 1);
    EXPECT_EQ(static_cast<int>(evalScalar("size(y, 2)")), 10);
}

TEST_F(PmmodTest, ColumnOrientationPreserved)
{
    eval("y = pmmod(sin(2*pi*5*((0:9)'/8000)), 100, 8000, pi/4);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(y, 1)")), 10);
    EXPECT_EQ(static_cast<int>(evalScalar("size(y, 2)")), 1);
}

TEST_F(PmmodTest, OutputRangeBounded)
{
    // Cosine output is bounded in [-1, 1]
    eval("y = pmmod(linspace(-2, 2, 200)', 100, 8000, 1.5);"
         "ymax = max(abs(y));");
    EXPECT_LE(evalScalar("ymax"), 1.0 + 1e-14);
}

TEST_F(PmmodTest, RejectsFsBelowNyquist)
{
    // Fs < 2*Fc must throw
    bool threw = false;
    try {
        eval("pmmod(sin((0:9)'/100), 100, 100, pi/4);");
    } catch (...) { threw = true; }
    EXPECT_TRUE(threw);
}

TEST_F(PmmodTest, RejectsNonPositiveFc)
{
    bool threw = false;
    try {
        eval("pmmod(sin((0:9)'/8000), -10, 8000, pi/4);");
    } catch (...) { threw = true; }
    EXPECT_TRUE(threw);
}
