// toolboxes/stats/tests/sample_corr_test.cpp
//
// Regression guard for autocorr / crosscorr (Econometrics sample correlation).
// Expected values from MATLAB R2025b. bugs/stats/autocorr.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class SampleCorrTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(SampleCorrTest, AutocorrDefaultLags)
{
    // MATLAB R2025b: autocorr([1 2 3 2 1 2 3 2 1]) — biased ACF, lag-0 == 1.
    eval("[acf, lags, bounds] = autocorr([1 2 3 2 1 2 3 2 1]);");
    EXPECT_EQ(eval("acf").numel(), 9u);          // default NumLags = min(20, N-1) = 8
    EXPECT_DOUBLE_EQ(evalScalar("lags(1)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("lags(9)"), 8.0);
    EXPECT_NEAR(evalScalar("acf(1)"),  1.0,          1e-12);   // lag 0
    EXPECT_NEAR(evalScalar("acf(2)"),  0.0202020202, 1e-9);
    EXPECT_NEAR(evalScalar("acf(3)"), -0.8005050505, 1e-9);
    EXPECT_NEAR(evalScalar("acf(5)"),  0.5808080808, 1e-9);
    EXPECT_NEAR(evalScalar("acf(9)"),  0.1616161616, 1e-9);
    // bounds = ±2/√N = ±2/3 (MATLAB default NumSTD = 2).
    EXPECT_NEAR(evalScalar("bounds(1)"),  0.6666666667, 1e-9);
    EXPECT_NEAR(evalScalar("bounds(2)"), -0.6666666667, 1e-9);
}

TEST_F(SampleCorrTest, AutocorrPositionalNumLags)
{
    // Positional NumLags form: autocorr(y, 4) → lags 0..4 (5 entries).
    eval("acf = autocorr([1 2 3 2 1 2 3 2 1], 4);");
    EXPECT_EQ(eval("acf").numel(), 5u);
    EXPECT_NEAR(evalScalar("acf(1)"), 1.0,          1e-12);
    EXPECT_NEAR(evalScalar("acf(3)"), -0.8005050505, 1e-9);
}

TEST_F(SampleCorrTest, CrosscorrNumLags)
{
    // MATLAB R2025b: crosscorr([1 2 3 4],[4 3 2 1],'NumLags',2).
    eval("[xcf, lags, bounds] = crosscorr([1 2 3 4], [4 3 2 1], 'NumLags', 2);");
    EXPECT_EQ(eval("xcf").numel(), 5u);          // 2*NumLags + 1
    EXPECT_DOUBLE_EQ(evalScalar("lags(1)"), -2.0);
    EXPECT_DOUBLE_EQ(evalScalar("lags(3)"),  0.0);
    EXPECT_DOUBLE_EQ(evalScalar("lags(5)"),  2.0);
    EXPECT_NEAR(evalScalar("xcf(3)"), -1.0,  1e-12);   // zero-lag: perfect anti-correlation
    EXPECT_NEAR(evalScalar("xcf(2)"), -0.25, 1e-12);
    EXPECT_NEAR(evalScalar("xcf(1)"),  0.3,  1e-12);
    EXPECT_NEAR(evalScalar("xcf(5)"),  0.3,  1e-12);
    EXPECT_NEAR(evalScalar("bounds(1)"), 1.0, 1e-12);  // ±2/√4
}
