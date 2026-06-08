// toolboxes/stats/tests/wishrnd_iwishrnd_test.cpp
//
// Regression guard for wishrnd + iwishrnd.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

#include <cmath>

using namespace numkit;

class WishrndIwishrndTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override { engine.eval("import compat.*; rng(0);"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(WishrndIwishrndTest, WishrndShape)
{
    eval("W = wishrnd(eye(3), 10);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(W, 1)")), 3);
    EXPECT_EQ(static_cast<int>(evalScalar("size(W, 2)")), 3);
}

TEST_F(WishrndIwishrndTest, WishrndIsSymmetric)
{
    // W must be exactly symmetric: max |W - W'| == 0 by construction.
    eval("W = wishrnd([2 0.5; 0.5 1], 6); err = max(max(abs(W - W')));");
    EXPECT_LT(evalScalar("err"), 1e-12);
}

TEST_F(WishrndIwishrndTest, WishrndIsPositiveDefinite)
{
    // Diagonal must be positive (necessary for PD).
    eval("W = wishrnd(eye(2), 5);");
    EXPECT_GT(evalScalar("W(1, 1)"), 0.0);
    EXPECT_GT(evalScalar("W(2, 2)"), 0.0);
    EXPECT_GT(evalScalar("W(1, 1) * W(2, 2) - W(1, 2)^2"), 0.0);  // det(W) > 0
}

TEST_F(WishrndIwishrndTest, WishrndMeanMatches)
{
    // E[W] = df · Sigma. Average over many draws.
    eval(R"(
        Sigma = [2 0.3; 0.3 1];
        df = 8;
        acc = zeros(2);
        for i = 1:600
            acc = acc + wishrnd(Sigma, df);
        end
        M = acc / (600 * df);
    )");
    EXPECT_NEAR(evalScalar("M(1, 1)"), 2.0, 0.3);
    EXPECT_NEAR(evalScalar("M(2, 2)"), 1.0, 0.2);
    EXPECT_NEAR(evalScalar("M(1, 2)"), 0.3, 0.2);
}

TEST_F(WishrndIwishrndTest, WishrndBadDfThrows)
{
    // df must exceed p - 1.
    EXPECT_THROW(eval("wishrnd(eye(3), 1.5);"), std::exception);
}

TEST_F(WishrndIwishrndTest, WishrndNonSquareThrows)
{
    EXPECT_THROW(eval("wishrnd(ones(2, 3), 5);"), std::exception);
}

// ── iwishrnd ────────────────────────────────────────────────────────

TEST_F(WishrndIwishrndTest, IwishrndShape)
{
    eval("W = iwishrnd(eye(3), 10);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(W, 1)")), 3);
    EXPECT_EQ(static_cast<int>(evalScalar("size(W, 2)")), 3);
}

TEST_F(WishrndIwishrndTest, IwishrndIsSymmetric)
{
    eval("W = iwishrnd([2 0.5; 0.5 1], 6); err = max(max(abs(W - W')));");
    EXPECT_LT(evalScalar("err"), 1e-10);
}

TEST_F(WishrndIwishrndTest, IwishrndMeanMatches)
{
    // E[W] = Tau / (df - p - 1) for df > p + 1.
    eval(R"(
        Tau = [2 0.3; 0.3 1];
        df = 6;
        acc = zeros(2);
        for i = 1:600
            acc = acc + iwishrnd(Tau, df);
        end
        M = acc / 600 * (df - 3);
    )");
    EXPECT_NEAR(evalScalar("M(1, 1)"), 2.0, 0.4);
    EXPECT_NEAR(evalScalar("M(2, 2)"), 1.0, 0.3);
    EXPECT_NEAR(evalScalar("M(1, 2)"), 0.3, 0.2);
}

TEST_F(WishrndIwishrndTest, IwishrndBadDfThrows)
{
    EXPECT_THROW(eval("iwishrnd(eye(3), 1.5);"), std::exception);
}

// ── 3-arg form: pre-computed Cholesky factor ────────────────────────

TEST_F(WishrndIwishrndTest, WishrndDArgRoundTrip)
{
    // [W, D] = wishrnd(Sigma, df); W2 = wishrnd(Sigma, df, D) with same
    // RNG should yield W == W2 (deterministic).
    eval(R"(
        Sigma = [2 0.3; 0.3 1]; df = 8;
        rng(0); [W1, D] = wishrnd(Sigma, df);
        rng(0); W2 = wishrnd(Sigma, df, D);
        err = max(max(abs(W1 - W2)));
    )");
    EXPECT_LT(evalScalar("err"), 1e-12);
}

TEST_F(WishrndIwishrndTest, WishrndDIsUpperChol)
{
    // D = chol(Sigma, 'upper'); verify D' * D == Sigma.
    eval(R"(
        Sigma = [2 0.3; 0.3 1];
        rng(0); [~, D] = wishrnd(Sigma, 8);
        rec = D' * D;
        err = max(max(abs(rec - Sigma)));
    )");
    EXPECT_LT(evalScalar("err"), 1e-12);
}

TEST_F(WishrndIwishrndTest, IwishrndDIIsInverseChol)
{
    // DI = chol(inv(Tau), 'lower'); verify DI * DI' == inv(Tau).
    eval(R"(
        Tau = [2 0.5; 0.5 1];
        rng(0); [~, DI] = iwishrnd(Tau, 6);
        rec = DI * DI';
        err = max(max(abs(rec - inv(Tau))));
    )");
    EXPECT_LT(evalScalar("err"), 1e-12);
}

TEST_F(WishrndIwishrndTest, IwishrndDIArgRoundTrip)
{
    // Round-trip: pass DI back as 3rd arg, get same W with same RNG.
    eval(R"(
        Tau = [2 0.5; 0.5 1]; df = 7;
        rng(0); [W1, DI] = iwishrnd(Tau, df);
        rng(0); W2 = iwishrnd(Tau, df, DI);
        err = max(max(abs(W1 - W2)));
    )");
    EXPECT_LT(evalScalar("err"), 1e-12);
}

TEST_F(WishrndIwishrndTest, WishrndBadDShapeThrows)
{
    EXPECT_THROW(eval("wishrnd([2 0.3; 0.3 1], 5, [1 0; 0 1; 0 0]);"),
                 std::exception);
}
