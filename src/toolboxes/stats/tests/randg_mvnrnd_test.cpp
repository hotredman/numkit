// toolboxes/stats/tests/randg_mvnrnd_test.cpp
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
    StandardEngine engine;
    void SetUp() override { engine.eval("rng(0);"); }
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

// --- bugs/closed/stats/randn-legacy-seed-syntax.md (FIXED) ---
// Legacy state syntax seeds the engine stream (rand/randn, 'seed'/'state'/
// 'twister'). Sequence VALUES differ from MATLAB's legacy generators — one
// MT19937 stream, rng(n) semantics — documented divergence; the contract
// was: the call must control the stream, not error.
TEST_F(RandgMvnrndTest, RandnLegacySeedSyntax)
{
    // ('seed', S): MATLAB v4 generator, bit-identical — Park-Miller
    // x <- 16807*x mod (2^31-1), u = x/m, S -> S*2^16 (S==0 ->
    // 1144108930); randn = Marsaglia polar, FIRST of each accepted pair
    // emitted. Values probed from R2025b at 17 digits.
    eval("randn('seed', 1);");
    EXPECT_DOUBLE_EQ(eval("z1 = randn();").toScalar(), 0.97944887909532974);
    EXPECT_DOUBLE_EQ(eval("z2 = randn();").toScalar(), -0.26561126812383568);
    eval("randn('seed', 0); a = randn(1, 2);");
    EXPECT_DOUBLE_EQ(eval("a(1);").toScalar(), 1.1649535105006568);
    EXPECT_DOUBLE_EQ(eval("a(2);").toScalar(), 0.62683908263243138);
    eval("rand('seed', 5); r = rand(1, 4);");
    EXPECT_DOUBLE_EQ(eval("r(1);").toScalar(), 0.56454467892858418);
    EXPECT_DOUBLE_EQ(eval("r(4);").toScalar(), 0.47522867586241507);
    eval("rand('seed', 0);");
    EXPECT_DOUBLE_EQ(eval("r0 = rand();").toScalar(), 0.21895918632809036);
    eval("randn('seed', 7); x = randn(); randn('seed', 7); y = randn();");
    EXPECT_DOUBLE_EQ(eval("x;").toScalar(), eval("y;").toScalar());
    // ('state'/'twister'): modern-stream seeding (documented divergence);
    // must still control the stream, not error.
    eval("rand('state', 3);");
    EXPECT_TRUE(eval("c = rand();").toScalar() > 0.0);
    // rng(seed) exits legacy mode back to the modern default stream.
    eval("rand('seed', 5); rng(0); r = rand();");
    EXPECT_DOUBLE_EQ(eval("r;").toScalar(), 0.8147236863931789);
    // Legacy QUERY form: rand/randn('seed') -> the last seed (double).
    eval("randn('seed', 5); s = randn('seed');");
    EXPECT_DOUBLE_EQ(eval("s;").toScalar(), 5.0);
    eval("rand('seed', 7); s2 = rand('seed');");
    EXPECT_DOUBLE_EQ(eval("s2;").toScalar(), 7.0);
}
