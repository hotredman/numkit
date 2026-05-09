// libs/stats/tests/corr_detrend_test.cpp
//
// Regression guard for corr (Pearson alias) and detrend.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class CorrDetrendTest : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// ── corr ──────────────────────────────────────────────────

TEST_F(CorrDetrendTest, CorrPerfectLinearColumns)
{
    // X has perfectly correlated columns -> corr is all-ones.
    eval("C = corr([1 2; 2 4; 3 6; 4 8]);");
    EXPECT_DOUBLE_EQ(evalScalar("C(1,1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("C(1,2)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("C(2,2)"), 1.0);
}

TEST_F(CorrDetrendTest, CorrNegativeCorrelation)
{
    // Anti-correlated columns.
    eval("C = corr([1 4; 2 3; 3 2; 4 1]);");
    EXPECT_NEAR(evalScalar("C(1,2)"), -1.0, 1e-12);
}

TEST_F(CorrDetrendTest, CorrThreeColumns)
{
    eval("C = corr([1 2 3; 4 5 6; 7 8 9; 10 11 12]);");
    // All linearly related -> all 1s.
    EXPECT_DOUBLE_EQ(evalScalar("C(1,3)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("C(2,3)"), 1.0);
}

TEST_F(CorrDetrendTest, CorrTwoArgRejected)
{
    // Two-arg form deferred.
    EXPECT_THROW(eval("corr([1 2; 3 4], [5; 6]);"), std::exception);
}

// ── detrend ───────────────────────────────────────────────

TEST_F(CorrDetrendTest, DetrendLinearRemovesPerfectly)
{
    // y = 2x + 5: linear trend, detrend should give zeros.
    eval("y = (1:10)' * 2 + 5; yd = detrend(y);");
    EXPECT_NEAR(evalScalar("max(abs(yd))"), 0.0, 1e-12);
}

TEST_F(CorrDetrendTest, DetrendQuadraticRemovesPerfectly)
{
    // y = x^2: order-2 detrend should give zeros.
    eval("y = ((1:10)').^2; yd = detrend(y, 2);");
    EXPECT_NEAR(evalScalar("max(abs(yd))"), 0.0, 1e-10);
}

TEST_F(CorrDetrendTest, DetrendConstantOnly)
{
    // detrend(x, 0) = subtract mean.
    eval("y = [1 2 3 4 5]; yd = detrend(y, 0);");
    EXPECT_NEAR(evalScalar("yd(1)"), -2.0, 1e-12);
    EXPECT_NEAR(evalScalar("yd(3)"),  0.0, 1e-12);
    EXPECT_NEAR(evalScalar("yd(5)"),  2.0, 1e-12);
}

TEST_F(CorrDetrendTest, DetrendStringMode)
{
    eval("y = [1 2 3 4 5]; yd1 = detrend(y, 'constant'); yd2 = detrend(y, 'linear');");
    EXPECT_NEAR(evalScalar("yd1(1)"), -2.0, 1e-12);
    EXPECT_NEAR(evalScalar("max(abs(yd2))"), 0.0, 1e-12);
}
