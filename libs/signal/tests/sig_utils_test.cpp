// libs/signal/tests/sig_utils_test.cpp
//
// Regression guard for cycle 5 signal utilities: seqperiod /
// zerocrossrate / cusum.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class SigUtilsTest : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// ── seqperiod ─────────────────────────────────────────────────────────
TEST_F(SigUtilsTest, SeqperiodPerfectRepeat)
{
    eval("[p, n] = seqperiod([1 2 3 1 2 3 1 2 3]);");
    EXPECT_DOUBLE_EQ(evalScalar("p"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("n"), 3.0);
}

TEST_F(SigUtilsTest, SeqperiodNoPeriod)
{
    eval("[p, n] = seqperiod([1 2 3 4]);");
    EXPECT_DOUBLE_EQ(evalScalar("p"), 4.0);
    EXPECT_DOUBLE_EQ(evalScalar("n"), 1.0);
}

TEST_F(SigUtilsTest, SeqperiodConstant)
{
    eval("[p, n] = seqperiod([7 7 7 7 7]);");
    EXPECT_DOUBLE_EQ(evalScalar("p"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("n"), 5.0);
}

TEST_F(SigUtilsTest, SeqperiodTolerance)
{
    // With tol=0.1, [1 2 1.05 2.05] has period 2 (close to [1 2]).
    eval("[p, n] = seqperiod([1 2 1.05 2.05], 0.1);");
    EXPECT_DOUBLE_EQ(evalScalar("p"), 2.0);
}

// ── zerocrossrate ─────────────────────────────────────────────────────
TEST_F(SigUtilsTest, ZeroCrossRateAlternating)
{
    EXPECT_NEAR(evalScalar("zerocrossrate([1 -1 1 -1])"), 0.875, 1e-12);
}

TEST_F(SigUtilsTest, ZeroCrossRateNoChange)
{
    // No crossings → only boundary credit 0.5; 0.5 / 2 = 0.25.
    EXPECT_NEAR(evalScalar("zerocrossrate([1 1])"), 0.25, 1e-12);
}

TEST_F(SigUtilsTest, ZeroCrossRateLong)
{
    EXPECT_NEAR(evalScalar("zerocrossrate([1 -1 2 -2 3 -3 4 -4])"),
                0.9375, 1e-12);
}

TEST_F(SigUtilsTest, ZeroCrossRateCountTwoOut)
{
    eval("[r, c] = zerocrossrate([1 -1 1 -1]);");
    EXPECT_NEAR(evalScalar("r"), 0.875, 1e-12);
    EXPECT_NEAR(evalScalar("c"), 3.5, 1e-12);  // 3 crossings + 0.5 credit
}

TEST_F(SigUtilsTest, ZeroCrossRateWithLevel)
{
    // With level=2: signal [1 3 1 3] crosses 2 three times; count=3.5; rate=3.5/4=0.875.
    eval("r = zerocrossrate([1 3 1 3], 2);");
    EXPECT_NEAR(evalScalar("r"), 0.875, 1e-12);
}

// ── cusum ─────────────────────────────────────────────────────────────
TEST_F(SigUtilsTest, CusumDetectsMeanShift)
{
    // Step input: 20 zeros then 20 threes; with explicit (climit=5,
    // mshift=1, tmean=0, tdev=1) → upper sum should breach near idx 22-23.
    eval("x = [zeros(20,1); 3*ones(20,1)];"
         "[iup, ilo] = cusum(x, 5, 1, 0, 1);");
    EXPECT_GE(evalScalar("iup"), 22.0);
    EXPECT_LE(evalScalar("iup"), 25.0);
    EXPECT_DOUBLE_EQ(evalScalar("numel(ilo)"), 0.0);  // no lower breach
}

TEST_F(SigUtilsTest, CusumNoChange)
{
    // Constant zero input → no breach.
    eval("[iup, ilo] = cusum(zeros(50, 1), 5, 1, 0, 1);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(iup)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("numel(ilo)"), 0.0);
}

TEST_F(SigUtilsTest, CusumFourOutputForm)
{
    eval("[iup, ilo, us, ls] = cusum([zeros(20,1); 3*ones(20,1)], 5, 1, 0, 1);");
    EXPECT_EQ(static_cast<int>(evalScalar("numel(us)")), 40);
    EXPECT_DOUBLE_EQ(evalScalar("us(1)"), 0.0);  // first sample = 0 - 0.5 floored at 0
    EXPECT_GT(evalScalar("us(40)"), evalScalar("us(20)"));  // monotone after shift
}
