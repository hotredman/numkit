// libs/builtin/tests/predicates_linalg_test.cpp
//
// Regression guard for the linalg predicates batch:
//   issymmetric, ishermitian, isbanded, isdiag, istril, istriu,
//   bandwidth, vecnorm.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

#include <cmath>

using namespace numkit;

class PredicatesLinalgTest : public ::testing::Test
{
public:
    StdEngine engine;
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// ── issymmetric ───────────────────────────────────────────────────────
TEST_F(PredicatesLinalgTest, IssymmetricRealTrue)
{
    eval("r = issymmetric([1 2; 2 1]);");
    EXPECT_DOUBLE_EQ(evalScalar("double(r)"), 1.0);
}

TEST_F(PredicatesLinalgTest, IssymmetricRealFalse)
{
    eval("r = issymmetric([1 2; 3 1]);");
    EXPECT_DOUBLE_EQ(evalScalar("double(r)"), 0.0);
}

TEST_F(PredicatesLinalgTest, IssymmetricComplexTransposeNoConj)
{
    // [1+1i 2; 2 1-1i] is symmetric in MATLAB sense (A == A.', no conj).
    eval("r = issymmetric([1+1i 2; 2 1-1i]);");
    EXPECT_DOUBLE_EQ(evalScalar("double(r)"), 1.0);
}

TEST_F(PredicatesLinalgTest, IssymmetricSkewOpt)
{
    eval("r = issymmetric([0 -2; 2 0], 'skew');");
    EXPECT_DOUBLE_EQ(evalScalar("double(r)"), 1.0);
    eval("r2 = issymmetric([1 2; 2 1], 'skew');");
    EXPECT_DOUBLE_EQ(evalScalar("double(r2)"), 0.0);
}

// ── ishermitian ───────────────────────────────────────────────────────
TEST_F(PredicatesLinalgTest, IshermitianTrue)
{
    eval("r = ishermitian([1 1i; -1i 1]);");
    EXPECT_DOUBLE_EQ(evalScalar("double(r)"), 1.0);
}

TEST_F(PredicatesLinalgTest, IshermitianFalse)
{
    eval("r = ishermitian([1 1i; 1i 1]);");
    EXPECT_DOUBLE_EQ(evalScalar("double(r)"), 0.0);
}

TEST_F(PredicatesLinalgTest, IshermitianSkewOpt)
{
    eval("r = ishermitian([0 1i; 1i 0], 'skew');");
    EXPECT_DOUBLE_EQ(evalScalar("double(r)"), 1.0);
}

// ── isbanded / isdiag / istril / istriu ───────────────────────────────
TEST_F(PredicatesLinalgTest, IsbandedExactZeroOutside)
{
    eval("D = diag([1 2 3]);"
         "T = [1 2 0; 3 4 5; 0 6 7];");
    EXPECT_DOUBLE_EQ(evalScalar("double(isbanded(D, 0, 0))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(isbanded(T, 1, 1))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(isbanded(T, 0, 1))"), 0.0);  // (3,1)=3
}

TEST_F(PredicatesLinalgTest, IsdiagPureDiagonal)
{
    EXPECT_DOUBLE_EQ(evalScalar("double(isdiag(diag([1 2 3])))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(isdiag(eye(3)))"), 1.0);
    // Any single off-diag breaks it.
    EXPECT_DOUBLE_EQ(evalScalar("double(isdiag([1 0; 0 1] + [0 1e-10; 0 0]))"),
                     0.0);
}

TEST_F(PredicatesLinalgTest, IstrilIstriu)
{
    EXPECT_DOUBLE_EQ(evalScalar("double(istril([1 0 0; 2 3 0; 4 5 6]))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(istril([1 1; 0 1]))"),            0.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(istriu([1 1; 0 1]))"),            1.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(istriu([1 0; 1 1]))"),            0.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(istril(eye(3)))"),                1.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(istriu(eye(3)))"),                1.0);
}

// ── bandwidth ─────────────────────────────────────────────────────────
TEST_F(PredicatesLinalgTest, BandwidthTwoOut)
{
    // [lower, upper] = bandwidth(A) for a tridiagonal matrix → [1, 1]
    eval("[lo, up] = bandwidth([1 2 0; 3 4 5; 0 6 7]);");
    EXPECT_DOUBLE_EQ(evalScalar("lo"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("up"), 1.0);
}

TEST_F(PredicatesLinalgTest, BandwidthOneOutReturnsLower)
{
    // 1-out form returns the lower bandwidth (MATLAB convention).
    EXPECT_DOUBLE_EQ(evalScalar("bandwidth([1 2 3; 0 4 5; 0 0 6])"), 0.0); // upper-tri
    EXPECT_DOUBLE_EQ(evalScalar("bandwidth([1 0 0; 2 3 0; 4 5 6])"), 2.0); // lower-tri
    EXPECT_DOUBLE_EQ(evalScalar("bandwidth(eye(4))"),                0.0);
}

TEST_F(PredicatesLinalgTest, BandwidthNamedOpt)
{
    EXPECT_DOUBLE_EQ(evalScalar("bandwidth([1 2 0; 3 4 5; 0 6 7], 'lower')"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("bandwidth([1 2 0; 3 4 5; 0 6 7], 'upper')"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("bandwidth([1 2 3; 0 4 5; 0 0 6], 'upper')"), 2.0);
}

// ── vecnorm ───────────────────────────────────────────────────────────
TEST_F(PredicatesLinalgTest, VecnormDefaults)
{
    EXPECT_DOUBLE_EQ(evalScalar("vecnorm([3 4])"),  5.0);
    EXPECT_DOUBLE_EQ(evalScalar("vecnorm([3; 4])"), 5.0);
    eval("v = vecnorm([3 4; 6 8]);");  // (1×2): [sqrt(45), sqrt(80)]
    EXPECT_NEAR(evalScalar("v(1)"), std::sqrt(45.0), 1e-12);
    EXPECT_NEAR(evalScalar("v(2)"), std::sqrt(80.0), 1e-12);
}

TEST_F(PredicatesLinalgTest, VecnormWithP)
{
    EXPECT_DOUBLE_EQ(evalScalar("vecnorm([1 2 3 4], 1)"),    10.0);
    EXPECT_DOUBLE_EQ(evalScalar("vecnorm([1 2 3 4], Inf)"),   4.0);
    EXPECT_DOUBLE_EQ(evalScalar("vecnorm([1 2 3 4], -Inf)"),  1.0);
}

TEST_F(PredicatesLinalgTest, VecnormWithDim)
{
    eval("v = vecnorm([1 2; 3 4], 2, 2);");  // row-wise, result (2×1)
    EXPECT_NEAR(evalScalar("v(1)"), std::sqrt(5.0),  1e-12);
    EXPECT_NEAR(evalScalar("v(2)"), std::sqrt(25.0), 1e-12);
}

TEST_F(PredicatesLinalgTest, VecnormEmptyToScalarZero)
{
    // MATLAB convention: vecnorm([]) → scalar 0, not 0×0 empty.
    EXPECT_DOUBLE_EQ(evalScalar("vecnorm([])"), 0.0);
}

TEST_F(PredicatesLinalgTest, VecnormPropagatesNaN)
{
    EXPECT_TRUE(std::isnan(evalScalar("vecnorm([1 NaN 2])")));
}

// ── error / shape edges ────────────────────────────────────────────────
TEST_F(PredicatesLinalgTest, IssymmetricRectangularFalse)
{
    // Non-square matrices are not symmetric.
    EXPECT_DOUBLE_EQ(evalScalar("double(issymmetric([1 2 3; 2 1 3]))"), 0.0);
}

TEST_F(PredicatesLinalgTest, IsbandedRectangular)
{
    // 2×4 with diagonal-only nonzeros → diag-banded.
    EXPECT_DOUBLE_EQ(evalScalar("double(isbanded([1 0 0 0; 0 2 0 0], 0, 0))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(isbanded([1 1 0 0; 0 2 0 0], 0, 0))"), 0.0);
}
