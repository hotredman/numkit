// libs/stats/tests/ksdensity_test.cpp
// ksdensity.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>
#include <cmath>

using namespace numkit;

class KsdensityTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override {
        engine.eval("import compat.*;");
        engine.eval("x = [1 2 2.5 3 3.5 4 5 6 7 9]';");
        engine.eval("pts = (0:0.5:10)';");
    }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// 2026-05-08 — gap closure: bandwidth uses MAD/0.6745 with IQR
// fallback (was just IQR), matching MATLAB R2025b exactly.
TEST_F(KsdensityTest, DefaultBandwidthMatchesMATLAB)
{
    eval("[f, xi, bw] = ksdensity(x, pts);");
    EXPECT_NEAR(evalScalar("bw"), 1.4862684537, 1e-6);
}

TEST_F(KsdensityTest, NormalKernelPdf)
{
    eval("f = ksdensity(x, pts);");
    EXPECT_NEAR(evalScalar("f(1)"), 0.044779, 1e-5);
    EXPECT_NEAR(evalScalar("f(5)"), 0.126313, 1e-5);
}

// gap closure: 4 kernel types with σ²=1 normalization.
TEST_F(KsdensityTest, BoxKernel)
{
    eval("f = ksdensity(x, pts, 'Kernel', 'box');");
    EXPECT_NEAR(evalScalar("f(1)"), 0.058268, 1e-5);
    EXPECT_NEAR(evalScalar("f(5)"), 0.116537, 1e-5);
}

TEST_F(KsdensityTest, TriangleKernel)
{
    eval("f = ksdensity(x, pts, 'Kernel', 'triangle');");
    EXPECT_NEAR(evalScalar("f(1)"), 0.046801, 1e-5);
    EXPECT_NEAR(evalScalar("f(5)"), 0.124372, 1e-5);
}

TEST_F(KsdensityTest, EpanechnikovKernel)
{
    eval("f = ksdensity(x, pts, 'Kernel', 'epanechnikov');");
    EXPECT_NEAR(evalScalar("f(1)"), 0.048894, 1e-5);
    EXPECT_NEAR(evalScalar("f(5)"), 0.122215, 1e-5);
}

// gap closure: Function modes.
TEST_F(KsdensityTest, FunctionCdf)
{
    eval("f = ksdensity(x, pts, 'Function', 'cdf');");
    EXPECT_NEAR(evalScalar("f(1)"), 0.042102, 1e-5);
    EXPECT_NEAR(evalScalar("f(5)"), 0.213963, 1e-5);
    EXPECT_NEAR(evalScalar("f(numel(f))"), 0.972376, 1e-4);
}

TEST_F(KsdensityTest, FunctionSurvivor)
{
    eval("f = ksdensity(x, pts, 'Function', 'survivor');");
    // s = 1 - cdf
    EXPECT_NEAR(evalScalar("f(1)"), 0.957898, 1e-5);
}

// gap closure: Weights.
TEST_F(KsdensityTest, Weights)
{
    eval("w = ones(10, 1); w(1:5) = 2;");
    eval("f = ksdensity(x, pts, 'Weights', w);");
    EXPECT_NEAR(evalScalar("f(1)"), 0.059159, 1e-5);
    EXPECT_NEAR(evalScalar("f(5)"), 0.158307, 1e-5);
}

// DEEP-PROBE c170: 'Function','icdf' (inverse CDF). The 2nd arg are
// PROBABILITIES; the result is the inverse of the smoothed CDF, solved via
// Newton's method (matching MATLAB R2025b to ~1e-10). numkit previously
// threw "icdf is not yet supported".
TEST_F(KsdensityTest, FunctionIcdf)
{
    eval("ic = ksdensity(x, [0.1 0.25 0.5 0.75 0.9], 'Function', 'icdf');");
    EXPECT_NEAR(evalScalar("ic(1)"), 0.9191050959, 1e-6);
    EXPECT_NEAR(evalScalar("ic(2)"), 2.2760353116, 1e-6);
    EXPECT_NEAR(evalScalar("ic(3)"), 4.0173996405, 1e-6);
    EXPECT_NEAR(evalScalar("ic(4)"), 6.1567629614, 1e-6);
    EXPECT_NEAR(evalScalar("ic(5)"), 8.2055341632, 1e-6);
}

// icdf is the inverse of cdf: icdf(cdf(x0)) == x0.
TEST_F(KsdensityTest, IcdfRoundTrip)
{
    eval("c = ksdensity(x, 4.0, 'Function', 'cdf');");
    eval("rt = ksdensity(x, c, 'Function', 'icdf');");
    EXPECT_NEAR(evalScalar("rt"), 4.0, 1e-6);
}

// icdf boundary / out-of-range probabilities.
TEST_F(KsdensityTest, IcdfBoundaries)
{
    eval("e = ksdensity(x, [0 1 -0.1 1.1], 'Function', 'icdf');");
    EXPECT_TRUE(std::isinf(evalScalar("e(1)")) && evalScalar("e(1)") < 0.0); // p=0  -> -Inf
    EXPECT_TRUE(std::isinf(evalScalar("e(2)")) && evalScalar("e(2)") > 0.0); // p=1  -> +Inf
    EXPECT_TRUE(std::isnan(evalScalar("e(3)")));                             // p<0  -> NaN
    EXPECT_TRUE(std::isnan(evalScalar("e(4)")));                             // p>1  -> NaN
}

// gap closure: Censoring rejected with clear error.
TEST_F(KsdensityTest, CensoringRejected)
{
    bool threw = false;
    try { eval("ksdensity(x, pts, 'Censoring', zeros(10, 1));"); }
    catch (const std::exception &) { threw = true; }
    EXPECT_TRUE(threw);
}
