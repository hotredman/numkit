// toolboxes/stats/tests/geornd_hygernd_pcacov_test.cpp
//
// Last three parity-only public stats functions: the geometric / hypergeometric
// random generators (geornd / hygernd) and PCA-from-covariance (pcacov). With
// this file every registered stats function has at least one gtest. pcacov is
// deterministic (eigendecomposition of the covariance), the rnd generators are
// checked by sample shape + mean against the closed-form population mean.

#include "dual_engine_fixture.hpp"

using namespace m_test;

class GeoHygePcacovTest : public DualEngineTest
{};

TEST_P(GeoHygePcacovTest, GeorndShapeMean)
{
    eval("rng(0); g = geornd(0.3, 3000, 1);");
    EXPECT_EQ(eval("g").numel(), 3000u);
    EXPECT_DOUBLE_EQ(evalScalar("all(g >= 0)"), 1.0);          // support k >= 0
    EXPECT_DOUBLE_EQ(evalScalar("all(g == round(g))"), 1.0);   // integer-valued
    EXPECT_NEAR(evalScalar("mean(g)"), 2.3333, 0.4);           // (1-p)/p
}

TEST_P(GeoHygePcacovTest, HygerndShapeMean)
{
    eval("rng(0); h = hygernd(50, 20, 10, 3000, 1);");
    EXPECT_EQ(eval("h").numel(), 3000u);
    EXPECT_NEAR(evalScalar("mean(h)"), 4.0, 0.4);              // n*K/M = 10*20/50
}

TEST_P(GeoHygePcacovTest, Pcacov)
{
    // Diagonal covariance: components are the axes, variances 4 and 1.
    eval("[coeff, latent, expl] = pcacov([4 0; 0 1]);");
    EXPECT_NEAR(evalScalar("latent(1)"), 4.0, 1e-12);
    EXPECT_NEAR(evalScalar("latent(2)"), 1.0, 1e-12);
    EXPECT_NEAR(evalScalar("expl(1)"), 80.0, 1e-12);           // 4/(4+1)*100
    EXPECT_NEAR(evalScalar("expl(2)"), 20.0, 1e-12);
    EXPECT_NEAR(evalScalar("abs(coeff(1,1))"), 1.0, 1e-12);    // identity up to sign
    // Correlated covariance: eigenvalues of [2 1; 1 2] are 3 and 1. (Single
    // output is coeff, MATLAB-style; latent is the 2nd output.)
    eval("[~, l2] = pcacov([2 1; 1 2]);");
    EXPECT_NEAR(evalScalar("l2(1)"), 3.0, 1e-12);
    EXPECT_NEAR(evalScalar("l2(2)"), 1.0, 1e-12);
}

INSTANTIATE_DUAL(GeoHygePcacovTest);
