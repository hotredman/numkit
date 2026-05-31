// libs/stats/tests/randg_mvnrnd_test.cpp
//
// Regression guards for the random-sampler cycle of Group A:
//   randg  — raw gamma(shape, 1) RNG (thin wrapper over gamrnd)
//   mvnrnd — multivariate normal RNG via Cholesky

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

#include <cmath>

using namespace numkit;

class RandgMvnrndTest : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override { engine.eval("import compat.*; rng(0);"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// ── randg ────────────────────────────────────────────────────────────

TEST_F(RandgMvnrndTest, RandgScalarShapeProducesScalar)
{
    eval("r = randg(2.0);");
    EXPECT_TRUE(evalScalar("isfinite(r)") > 0.5);
    EXPECT_GT(evalScalar("r"), 0.0);
}

TEST_F(RandgMvnrndTest, RandgMatrixSizeArg)
{
    eval("R = randg(2.0, 10, 5);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(R, 1)")), 10);
    EXPECT_EQ(static_cast<int>(evalScalar("size(R, 2)")),  5);
    EXPECT_TRUE(evalScalar("all(R(:) >= 0)") > 0.5);
}

// Gamma(a, 1) has mean = a, variance = a.
TEST_F(RandgMvnrndTest, RandgMatchesGammaMomentsLargeN)
{
    eval("R = randg(3.0, 2000, 1); m = mean(R); v = var(R);");
    EXPECT_NEAR(evalScalar("m"), 3.0, 0.2);
    EXPECT_NEAR(evalScalar("v"), 3.0, 0.4);
}

// Per-element shape: each entry gets its own shape parameter.
TEST_F(RandgMvnrndTest, RandgPerElementShape)
{
    eval("shapes = [1; 2; 5; 10]; R = randg(shapes);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(R, 1)")), 4);
    EXPECT_EQ(static_cast<int>(evalScalar("size(R, 2)")), 1);
    EXPECT_TRUE(evalScalar("all(R >= 0)") > 0.5);
}

// ── mvnrnd ───────────────────────────────────────────────────────────

TEST_F(RandgMvnrndTest, MvnrndShapeNByD)
{
    eval("mu = [0, 0]; Sigma = eye(2); R = mvnrnd(mu, Sigma, 50);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(R, 1)")), 50);
    EXPECT_EQ(static_cast<int>(evalScalar("size(R, 2)")),  2);
}

// Sample mean of N(mu, Sigma) over many samples should approach mu.
TEST_F(RandgMvnrndTest, MvnrndMeanApproachesMu)
{
    eval("mu = [1, 2, 3]; Sigma = diag([4, 9, 16]); R = mvnrnd(mu, Sigma, 3000);"
         "m = mean(R);");
    EXPECT_NEAR(evalScalar("m(1)"), 1.0, 0.15);
    EXPECT_NEAR(evalScalar("m(2)"), 2.0, 0.20);
    EXPECT_NEAR(evalScalar("m(3)"), 3.0, 0.30);
}

// Sample covariance over many samples should approach Sigma.
TEST_F(RandgMvnrndTest, MvnrndCovarianceApproachesSigma)
{
    eval("Sigma = [4 1 0; 1 9 0; 0 0 16]; R = mvnrnd([0 0 0], Sigma, 5000);"
         "C = cov(R);");
    EXPECT_NEAR(evalScalar("C(1, 1)"),  4.0, 0.5);
    EXPECT_NEAR(evalScalar("C(2, 2)"),  9.0, 1.0);
    EXPECT_NEAR(evalScalar("C(3, 3)"), 16.0, 1.5);
    EXPECT_NEAR(evalScalar("C(1, 2)"),  1.0, 0.5);
    EXPECT_NEAR(evalScalar("C(1, 3)"),  0.0, 0.5);
}

// Single-sample form.
TEST_F(RandgMvnrndTest, MvnrndSingleSampleNoNArg)
{
    eval("r = mvnrnd([10 20 30], eye(3));");
    EXPECT_EQ(static_cast<int>(evalScalar("size(r, 1)")), 1);
    EXPECT_EQ(static_cast<int>(evalScalar("size(r, 2)")), 3);
}

// Per-row mu (matrix mu) — each row is its own location.
TEST_F(RandgMvnrndTest, MvnrndPerRowMu)
{
    eval("MU = [zeros(200, 2); 5 * ones(200, 2)];"
         "R = mvnrnd(MU, eye(2));"
         "m1 = mean(R(1:200, :)); m2 = mean(R(201:400, :));");
    EXPECT_NEAR(evalScalar("m1(1)"), 0.0, 0.3);
    EXPECT_NEAR(evalScalar("m2(1)"), 5.0, 0.3);
}

// Non-PD Sigma throws.
TEST_F(RandgMvnrndTest, MvnrndNonPDThrows)
{
    EXPECT_THROW(eval("mvnrnd([0 0], [1 2; 2 1], 1);"), std::exception);
}
