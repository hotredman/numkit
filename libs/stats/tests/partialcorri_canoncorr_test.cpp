// libs/stats/tests/partialcorri_canoncorr_test.cpp
//
// Regression guards for the correlation cycle of Group A:
//   partialcorri — semi-partial correlation, control varies per X col
//   canoncorr    — canonical correlation analysis

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

#include <cmath>

using namespace numkit;

class PCCTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override { engine.eval("import compat.*; rng(0);"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// ── partialcorri ────────────────────────────────────────────────────

TEST_F(PCCTest, PartialcorriShapeIsPYByPX)
{
    eval("X = randn(50, 4); Y = randn(50, 2);"
         "R = partialcorri(Y, X);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(R, 1)")), 2);
    EXPECT_EQ(static_cast<int>(evalScalar("size(R, 2)")), 4);
}

// Y_i strongly depends on X_i (i=1,2), so diag of R should be near ±1
// and off-diag near 0.
TEST_F(PCCTest, PartialcorriRecoverDiagonalDependence)
{
    eval("n = 500; X = randn(n, 3);"
         "Y = [X(:,1) + 0.05*randn(n,1), X(:,2) + 0.05*randn(n,1)];"
         "R = partialcorri(Y, X);");
    EXPECT_GT(evalScalar("R(1, 1)"), 0.9);
    EXPECT_GT(evalScalar("R(2, 2)"), 0.9);
    EXPECT_LT(std::abs(evalScalar("R(1, 2)")), 0.2);
    EXPECT_LT(std::abs(evalScalar("R(2, 1)")), 0.2);
    EXPECT_LT(std::abs(evalScalar("R(1, 3)")), 0.2);
}

// With explicit Z controls (already-independent) the result is the same
// shape; correlations only shift if Z genuinely explains variance.
TEST_F(PCCTest, PartialcorriWithExtraControlsShapeUnchanged)
{
    eval("n = 200; X = randn(n, 2); Y = randn(n, 2); Z = randn(n, 1);"
         "R = partialcorri(Y, X, Z);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(R, 1)")), 2);
    EXPECT_EQ(static_cast<int>(evalScalar("size(R, 2)")), 2);
}

// Empty Z is the same as the 2-arg form.
TEST_F(PCCTest, PartialcorriEmptyZEqualsTwoArg)
{
    eval("rng(0); n = 200; X = randn(n, 3); Y = randn(n, 2);"
         "R0 = partialcorri(Y, X);"
         "R1 = partialcorri(Y, X, []);"
         "err = max(max(abs(R0 - R1)));");
    EXPECT_LT(evalScalar("err"), 1e-12);
}

// ── canoncorr ───────────────────────────────────────────────────────

TEST_F(PCCTest, CanoncorrSharedFactorRecoversNearOne)
{
    // rng(0) resets the global stream deterministically, so this test is
    // order-independent. Thresholds stay loose because the assertion is
    // about the planted shared factor (z): r(1) overwhelmingly large and
    // r(2) small with 1000 samples.
    eval("rng(0); n = 1000; z = randn(n, 1);"
         "X = [z + 0.05*randn(n,1), z + 0.1*randn(n,1), randn(n,1)];"
         "Y = [z + 0.07*randn(n,1), -z + 0.1*randn(n,1)];"
         "[A, B, r] = canoncorr(X, Y);");
    EXPECT_GT(evalScalar("r(1)"), 0.85);
    EXPECT_LT(evalScalar("r(2)"), 0.45);
}

TEST_F(PCCTest, CanoncorrShapes)
{
    // Seed so the test is deterministic regardless of suite ordering
    // (the global RNG stream is shared across the process).
    eval("rng(0); X = randn(100, 4); Y = randn(100, 3);"
         "[A, B, r] = canoncorr(X, Y);");
    // k = min(p, q) = 3
    EXPECT_EQ(static_cast<int>(evalScalar("size(A, 1)")), 4);
    EXPECT_EQ(static_cast<int>(evalScalar("size(A, 2)")), 3);
    EXPECT_EQ(static_cast<int>(evalScalar("size(B, 1)")), 3);
    EXPECT_EQ(static_cast<int>(evalScalar("size(B, 2)")), 3);
    EXPECT_EQ(static_cast<int>(evalScalar("numel(r)")),   3);
}

// Cross-check: corr(X·A(:, i), Y·B(:, i)) must equal r(i).
TEST_F(PCCTest, CanoncorrUVCorrelationMatchesR)
{
    eval("rng(0); n = 400; z = randn(n, 1);"
         "X = [z + 0.1*randn(n,1), randn(n,1)];"
         "Y = [z + 0.1*randn(n,1), randn(n,1)];"
         "[A, B, r] = canoncorr(X, Y);"
         "U = X * A; V = Y * B;"
         "c1 = corrcoef([U(:,1), V(:,1)]);"
         "c2 = corrcoef([U(:,2), V(:,2)]);"
         "err1 = abs(c1(1, 2) - r(1));"
         "err2 = abs(c2(1, 2) - r(2));");
    EXPECT_LT(evalScalar("err1"), 1e-6);
    EXPECT_LT(evalScalar("err2"), 1e-6);
}

// r values are in [0, 1] and non-increasing.
TEST_F(PCCTest, CanoncorrRIsClampedAndNonIncreasing)
{
    eval("rng(0); X = randn(100, 3); Y = randn(100, 3);"
         "[~, ~, r] = canoncorr(X, Y);");
    EXPECT_TRUE(evalScalar("all(r >= 0)") > 0.5);
    EXPECT_TRUE(evalScalar("all(r <= 1)") > 0.5);
    EXPECT_GE(evalScalar("r(1)"), evalScalar("r(2)"));
    EXPECT_GE(evalScalar("r(2)"), evalScalar("r(3)"));
}

TEST_F(PCCTest, CanoncorrTooFewRowsThrows)
{
    EXPECT_THROW(eval("rng(0); canoncorr(randn(3, 4), randn(3, 3));"),
                 std::exception);
}
