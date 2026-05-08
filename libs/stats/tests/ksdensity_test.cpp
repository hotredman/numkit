// libs/stats/tests/ksdensity_test.cpp
// Audit ТЗ closure for ksdensity. Closes audit/findings/empirical/ksdensity.md.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class KsdensityTest : public ::testing::Test
{
public:
    Engine engine;
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

// gap closure: Censoring rejected with clear error.
TEST_F(KsdensityTest, CensoringRejected)
{
    bool threw = false;
    try { eval("ksdensity(x, pts, 'Censoring', zeros(10, 1));"); }
    catch (const std::exception &) { threw = true; }
    EXPECT_TRUE(threw);
}
