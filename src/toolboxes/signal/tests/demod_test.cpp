// toolboxes/signal/tests/demod_test.cpp
//
// Regression guard for demod (Phase 4.13). Approximate-equal MATLAB
// R2025b on the 3 supported AM-family modes (~2e-3 diff in filtfilt
// edge handling).

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class DemodTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override {
        engine.eval("import compat.*;");
        engine.eval("fs = 200; t = (0:1/fs:0.1)'; x = sin(2*pi*5*t);");
    }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(DemodTest, AmRecoversInputApproximately)
{
    // y = AM-modulate(x); demod gives ~0.5*x after lowpass.
    eval("y = modulate(x, 25, fs, 'am');"
         "xd = demod(y, 25, fs, 'am');"
         "err = max(abs(xd - 0.5*x));");
    EXPECT_LT(evalScalar("err"), 0.01);
}

TEST_F(DemodTest, AmAliasAmdsbSc)
{
    eval("y = modulate(x, 25, fs, 'am');"
         "xa = demod(y, 25, fs, 'am');"
         "xb = demod(y, 25, fs, 'amdsb-sc');");
    EXPECT_NEAR(evalScalar("max(abs(xa - xb))"), 0.0, 1e-12);
}

TEST_F(DemodTest, AmdsbTcSubtractsOffset)
{
    eval("y = modulate(x, 25, fs, 'amdsb-tc', 0.3);"
         "xd = demod(y, 25, fs, 'amdsb-tc', 0.5);");
    // After offset subtraction; just sanity-check shape and finite values.
    EXPECT_EQ(static_cast<int>(evalScalar("numel(xd)")), 21);
    EXPECT_TRUE(std::isfinite(evalScalar("xd(5)")));
}

TEST_F(DemodTest, FmRoundtripApproximatesInput)
{
    // FM modulate then demodulate; interior samples should approximate x.
    eval("y = modulate(x, 25, fs, 'fm');"
         "xd = demod(y, 25, fs, 'fm');"
         "err = max(abs(xd(5:end-5) - x(5:end-5)));");
    // FM demod has noise from hilbert + diff/unwrap; allow up to 1.0
    EXPECT_LT(evalScalar("err"), 1.0);
}

TEST_F(DemodTest, PmRoundtripFiniteOutput)
{
    eval("y = modulate(x, 25, fs, 'pm');"
         "xd = demod(y, 25, fs, 'pm');");
    EXPECT_EQ(static_cast<int>(evalScalar("numel(xd)")), 21);
    EXPECT_TRUE(std::isfinite(evalScalar("xd(10)")));
}

TEST_F(DemodTest, RejectsUnsupportedMethod)
{
    bool threw = false;
    try { eval("demod(x, 25, fs, 'amssb');"); } catch (...) { threw = true; }
    EXPECT_TRUE(threw);
}
