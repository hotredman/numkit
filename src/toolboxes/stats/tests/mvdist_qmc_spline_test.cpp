// toolboxes/stats/tests/mvdist_qmc_spline_test.cpp
//
// Coverage for previously gtest-uncovered stats files (parity-spec only):
// mvdist/mvdist.cpp (mvnpdf, mvncdf), qmc/qmc.cpp (haltonset, net), and
// spline/spline.cpp (spline). Reference values from numkit's parity-validated
// output, cross-checked against closed form (mvnpdf([0 0]) = 1/(2*pi),
// mvncdf(0) = 0.5, Halton base-2/base-3 digits, spline-at-nodes = sin).

#include "dual_engine_fixture.hpp"

#include <cmath>

using namespace m_test;

class MvdistQmcSplineTest : public DualEngineTest
{};

// ── mvdist: mvnpdf / mvncdf ─────────────────────────────────

TEST_P(MvdistQmcSplineTest, MvnpdfStandard)
{
    EXPECT_NEAR(evalScalar("mvnpdf([0 0])"), 0.159154943092, 1e-9);  // 1/(2*pi)
    EXPECT_NEAR(evalScalar("mvnpdf(1, 0, 1)"), 0.241970724519, 1e-9);     // = normpdf(1)
}

TEST_P(MvdistQmcSplineTest, MvncdfStandard)
{
    EXPECT_NEAR(evalScalar("mvncdf(0, [], [])"), 0.5, 1e-9);
    EXPECT_NEAR(evalScalar("mvncdf(1.5, [], [])"), 0.933192798731, 1e-9);  // = normcdf(1.5)
}

// ── qmc: haltonset + net (low-discrepancy sequence) ─────────

TEST_P(MvdistQmcSplineTest, HaltonNet)
{
    eval("X = net(haltonset(2), 5);");
    EXPECT_EQ(eval("X").dims().rows(), 5u);
    EXPECT_EQ(eval("X").dims().cols(), 2u);
    EXPECT_NEAR(evalScalar("X(1,1)"), 0.0, 1e-12);
    EXPECT_NEAR(evalScalar("X(1,2)"), 0.0, 1e-12);
    EXPECT_NEAR(evalScalar("X(2,1)"), 0.5, 1e-12);          // base-2: 1/2
    EXPECT_NEAR(evalScalar("X(5,2)"), 0.444444444444, 1e-9);  // base-3: 4/9
}

// ── spline: cubic interpolation ─────────────────────────────

TEST_P(MvdistQmcSplineTest, SplineAtNodes)
{
    // linspace(0,10,11) includes 0, 5, 10 as nodes → exact sin values there.
    eval("xs = linspace(0,10,11); yq = spline(xs, sin(xs), [0 5 10]);");
    EXPECT_NEAR(evalScalar("yq(1)"), 0.0, 1e-9);              // sin(0)
    EXPECT_NEAR(evalScalar("yq(2)"), std::sin(5.0), 1e-9);   // sin(5)
    EXPECT_NEAR(evalScalar("yq(3)"), std::sin(10.0), 1e-9);  // sin(10)
}

TEST_P(MvdistQmcSplineTest, SplineInterpolatesBetweenNodes)
{
    // Off-node query: cubic spline of a fine sin grid is close to sin.
    eval("xs = linspace(0,10,21); yq = spline(xs, sin(xs), 2.5);");
    EXPECT_NEAR(evalScalar("yq"), std::sin(2.5), 1e-3);
}

INSTANTIATE_DUAL(MvdistQmcSplineTest);
