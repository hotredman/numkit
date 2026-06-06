// libs/comm/tests/fmmod_test.cpp
//
// Regression guard for fmmod() — frequency modulator.
//   int_x = cumsum(x) / Fs
//   y = cos(2π·Fc·t + 2π·freqdev·int_x + ini_phase)

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class FmmodTest : public ::testing::Test
{
public:
    StdEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(FmmodTest, KnownReferenceValues)
{
    // MATLAB ref (full precision): fmmod(sin(2*pi*5*((0:9)'/8000)), 100, 8000, 50)
    eval("fs = 8000; fc = 100; t = (0:9)'/fs; x = sin(2*pi*5*t);"
         "y = fmmod(x, fc, fs, 50);");
    EXPECT_DOUBLE_EQ(evalScalar("y(1)"), 1.0);
    // Highway sin/cos contributes a few ULP -> compose -> ~1e-9.
    EXPECT_NEAR(evalScalar("y(2)"),  0.9969052225315366, 1e-9);
    EXPECT_NEAR(evalScalar("y(10)"), 0.7558813303256287, 1e-9);
}

TEST_F(FmmodTest, IniPhaseShifts)
{
    // y(1) = cos(0 + 0 + pi/3) = 0.5
    eval("y = fmmod(sin(2*pi*5*((0:9)'/8000)), 100, 8000, 50, pi/3);");
    EXPECT_NEAR(evalScalar("y(1)"), 0.5, 1e-12);
}

TEST_F(FmmodTest, RowOrientationPreserved)
{
    eval("y = fmmod(sin(2*pi*5*((0:9)/8000)), 100, 8000, 50);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(y, 1)")), 1);
    EXPECT_EQ(static_cast<int>(evalScalar("size(y, 2)")), 10);
}

TEST_F(FmmodTest, ColumnOrientationPreserved)
{
    eval("y = fmmod(sin(2*pi*5*((0:9)'/8000)), 100, 8000, 50);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(y, 1)")), 10);
    EXPECT_EQ(static_cast<int>(evalScalar("size(y, 2)")), 1);
}

TEST_F(FmmodTest, OutputBoundedByOne)
{
    // cos output is bounded in [-1, 1]
    eval("y = fmmod(sin(2*pi*5*((0:99)'/8000)), 100, 8000, 50);"
         "ymax = max(abs(y));");
    EXPECT_LE(evalScalar("ymax"), 1.0 + 1e-14);
}

TEST_F(FmmodTest, RejectsFsBelowNyquist)
{
    bool threw = false;
    try { eval("fmmod(sin((0:9)'/100), 100, 100, 50);"); }
    catch (...) { threw = true; }
    EXPECT_TRUE(threw);
}

TEST_F(FmmodTest, RejectsNonPositiveFreqdev)
{
    bool threw = false;
    try { eval("fmmod(sin((0:9)'/8000), 100, 8000, 0);"); }
    catch (...) { threw = true; }
    EXPECT_TRUE(threw);
}

TEST_F(FmmodTest, ZeroInputGivesPureCarrier)
{
    // x = 0 everywhere -> int_x = 0 -> y = cos(2*pi*Fc*t + ini_phase)
    eval("N = 50; fc = 100; fs = 8000; t = (0:N-1)'/fs;"
         "y = fmmod(zeros(N, 1), fc, fs, 50);"
         "yref = cos(2*pi*fc*t);"
         "err = max(abs(y - yref));");
    EXPECT_LT(evalScalar("err"), 1e-12);
}
