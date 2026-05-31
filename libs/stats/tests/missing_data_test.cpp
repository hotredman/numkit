// libs/stats/tests/missing_data_test.cpp
//
// Regression guard for isoutlier / rmoutliers / fillmissing /
// rmmissing / standardizeMissing.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class MissingDataTest : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// ── isoutlier ─────────────────────────────────────────────

TEST_F(MissingDataTest, IsoutlierFlagsExtreme)
{
    eval("m = isoutlier([1 2 3 4 5 6 7 100]);");
    EXPECT_DOUBLE_EQ(evalScalar("double(m(1))"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(m(8))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("sum(double(m))"), 1.0);
}

TEST_F(MissingDataTest, IsoutlierUniformNoOutliers)
{
    eval("m = isoutlier([5 5 5 5 5]);");
    EXPECT_DOUBLE_EQ(evalScalar("sum(double(m))"), 0.0);
}

// isoutlier(A, method [,'ThresholdFactor',tf]): the method arg was
// parsed-and-ignored (always median/MAD). 'mean' flags >3 std from the
// FULL-sample mean; 'quartiles' uses Q1-1.5IQR..Q3+1.5IQR; matrices are
// per-column. vs MATLAB R2025b. DEEP-PROBE 2026-05-31.
TEST_F(MissingDataTest, IsoutlierMethods)
{
    // 'mean': 100 is within 3*std (~118.9) of mean(19.17) -> NO outlier.
    eval("me = isoutlier([1 2 3 100 4 5], 'mean');");
    EXPECT_DOUBLE_EQ(evalScalar("sum(double(me))"), 0.0);
    // 'mean' with ThresholdFactor 1 -> flags 100 only.
    eval("me1 = isoutlier([1 2 3 100 4 5], 'mean', 'ThresholdFactor', 1);");
    EXPECT_DOUBLE_EQ(evalScalar("sum(double(me1))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(me1(4))"), 1.0);
    // 'median' (default rule) flags 100; tf=1 also flags x(1)=1.
    eval("md = isoutlier([1 2 3 100 4 5]);");
    EXPECT_DOUBLE_EQ(evalScalar("sum(double(md))"), 1.0);
    eval("mt = isoutlier([1 2 3 100 4 5], 'median', 'ThresholdFactor', 1);");
    EXPECT_DOUBLE_EQ(evalScalar("double(mt(1))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("sum(double(mt))"), 2.0);
    // 'quartiles' flags 100.
    eval("q = isoutlier([1 2 3 100 4 5], 'quartiles');");
    EXPECT_DOUBLE_EQ(evalScalar("sum(double(q))"), 1.0);
    // matrix: detection is per-column; only (3,2)=100 is an outlier.
    eval("M = isoutlier([1 2;3 4;5 100;7 8;9 10]);");
    EXPECT_DOUBLE_EQ(evalScalar("double(M(3,2))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("sum(double(M(:)))"), 1.0);
    // gesd still deferred (throws).
    EXPECT_THROW(eval("isoutlier([1 2 3 100 4 5], 'gesd');"), std::exception);
}

// DEEP-PROBE c172: isoutlier 'grubbs' — iterative Grubbs's test,
// ThresholdFactor = significance alpha (default 0.05). vs MATLAB R2025b.
TEST_F(MissingDataTest, IsoutlierGrubbs)
{
    // A lone large outlier is flagged.
    eval("g = isoutlier([1 2 3 4 5 6 7 8 9 50], 'grubbs');");
    EXPECT_DOUBLE_EQ(evalScalar("sum(double(g))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(g(10))"), 1.0);
    // Two opposite extremes mask each other -> none flagged (Grubbs masking).
    eval("g2 = isoutlier([10 12 11 13 12 11 50 12 11 -30 12], 'grubbs');");
    EXPECT_DOUBLE_EQ(evalScalar("sum(double(g2))"), 0.0);
    // A stricter alpha still flags the lone 50.
    eval("ga = isoutlier([1 2 3 4 5 6 7 8 9 50], 'grubbs', 'ThresholdFactor', 0.01);");
    EXPECT_DOUBLE_EQ(evalScalar("sum(double(ga))"), 1.0);
    // Per-column on a matrix.
    eval("gm = isoutlier([1 2; 2 3; 3 4; 4 5; 100 6; 5 50], 'grubbs');");
    EXPECT_DOUBLE_EQ(evalScalar("sum(double(gm(:,1)))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("sum(double(gm(:,2)))"), 1.0);
}

// ── rmoutliers ────────────────────────────────────────────

TEST_F(MissingDataTest, RmoutliersDropsExtreme)
{
    eval("y = rmoutliers([1 2 3 4 5 6 7 100]);");
    EXPECT_EQ(static_cast<int>(evalScalar("numel(y)")), 7);
    EXPECT_DOUBLE_EQ(evalScalar("max(y)"), 7.0);
}

// rmoutliers(A, method[, percentiles vec][,'ThresholdFactor',tf]): the
// method/percentiles/ThresholdFactor args were parsed-and-ignored (always
// the default median detector), and matrices were flattened instead of
// having outlier ROWS removed. vs MATLAB R2025b. DEEP-PROBE 2026-05-31.
TEST_F(MissingDataTest, RmoutliersMethodsAndMatrix)
{
    // 'mean': 100 within 3*std -> removes nothing.
    eval("me = rmoutliers([1 2 3 100 4 5], 'mean');");
    EXPECT_EQ(static_cast<int>(evalScalar("numel(me)")), 6);
    EXPECT_DOUBLE_EQ(evalScalar("me(4)"), 100.0);
    // 'percentiles' [10 90] removes the 10th-pct low (1) and 90th-pct high (100).
    eval("pc = rmoutliers([1 2 3 100 4 5], 'percentiles', [10 90]);");
    EXPECT_EQ(static_cast<int>(evalScalar("numel(pc)")), 4);
    EXPECT_DOUBLE_EQ(evalScalar("sum(pc)"), 14.0);   // 2+3+4+5
    EXPECT_DOUBLE_EQ(evalScalar("pc(1)"), 2.0);
    // 'median' with ThresholdFactor 1 removes 1 and 100.
    eval("mt = rmoutliers([1 2 3 100 4 5], 'median', 'ThresholdFactor', 1);");
    EXPECT_EQ(static_cast<int>(evalScalar("numel(mt)")), 4);
    EXPECT_DOUBLE_EQ(evalScalar("sum(mt)"), 14.0);
    // matrix: per-column detection removes the ROW containing 100.
    eval("R = rmoutliers([1 2; 3 4; 5 100; 7 8; 9 10]);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(R,1)")), 4);
    EXPECT_EQ(static_cast<int>(evalScalar("size(R,2)")), 2);
    EXPECT_DOUBLE_EQ(evalScalar("R(3,1)"), 7.0);     // row [7 8] survived
    EXPECT_DOUBLE_EQ(evalScalar("sum(R(:))"), 44.0); // 1+3+7+9 + 2+4+8+10
    // 2nd output: vector removed-mask (same orientation).
    eval("[yv, iv] = rmoutliers([1 2 3 100 4 5]);");
    EXPECT_DOUBLE_EQ(evalScalar("double(iv(4))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("sum(double(iv))"), 1.0);
    // 2nd output: matrix removed-rows logical (r x 1).
    eval("[yM, iM] = rmoutliers([1 2; 3 4; 5 100; 7 8; 9 10]);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(iM,1)")), 5);
    EXPECT_EQ(static_cast<int>(evalScalar("size(iM,2)")), 1);
    EXPECT_DOUBLE_EQ(evalScalar("double(iM(3))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("sum(double(iM))"), 1.0);
    // unsupported method throws (deferred).
    EXPECT_THROW(eval("rmoutliers([1 2 3 100 4 5], 'gesd');"), std::exception);
}

// ── fillmissing ───────────────────────────────────────────

TEST_F(MissingDataTest, FillmissingPrevious)
{
    eval("fp = fillmissing([1 2 NaN 4 5 NaN 7], 'previous');");
    EXPECT_DOUBLE_EQ(evalScalar("fp(3)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("fp(6)"), 5.0);
}

TEST_F(MissingDataTest, FillmissingNext)
{
    eval("fn = fillmissing([1 2 NaN 4 5 NaN 7], 'next');");
    EXPECT_DOUBLE_EQ(evalScalar("fn(3)"), 4.0);
    EXPECT_DOUBLE_EQ(evalScalar("fn(6)"), 7.0);
}

TEST_F(MissingDataTest, FillmissingConstant)
{
    eval("fc = fillmissing([1 NaN 3 NaN 5], 'constant', 99);");
    EXPECT_DOUBLE_EQ(evalScalar("fc(2)"), 99.0);
    EXPECT_DOUBLE_EQ(evalScalar("fc(4)"), 99.0);
    EXPECT_DOUBLE_EQ(evalScalar("fc(1)"), 1.0);  // unchanged
}

TEST_F(MissingDataTest, FillmissingBadMethodThrows)
{
    EXPECT_THROW(eval("fillmissing([1 NaN 3], 'unknown_method');"), std::exception);
}

// fillmissing(..., 'EndValues', ev): the option governs only the endpoint
// missing entries (before first / after last original good value); interior
// runs are always filled by the method. numkit previously threw on the NV
// pair. vs MATLAB R2025b on a = [NaN NaN 3 5 NaN 9 NaN]. DEEP-PROBE 2026-05-31.
TEST_F(MissingDataTest, FillmissingEndValues)
{
    // linear default extrapolates endpoints (unchanged).
    eval("ex = fillmissing([NaN NaN 3 5 NaN 9 NaN], 'linear');");
    EXPECT_DOUBLE_EQ(evalScalar("ex(1)"), -1.0);
    EXPECT_DOUBLE_EQ(evalScalar("ex(4)"), 5.0);
    EXPECT_DOUBLE_EQ(evalScalar("ex(7)"), 11.0);
    // 'none' leaves endpoints NaN, fills interior (en(5) interpolates to 7;
    // en(4) is the unchanged good value 5).
    eval("en = fillmissing([NaN NaN 3 5 NaN 9 NaN], 'linear', 'EndValues', 'none');");
    EXPECT_DOUBLE_EQ(evalScalar("double(isnan(en(1)))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("en(4)"), 5.0);
    EXPECT_DOUBLE_EQ(evalScalar("en(5)"), 7.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(isnan(en(7)))"), 1.0);
    // numeric constant for endpoints.
    eval("ec = fillmissing([NaN NaN 3 5 NaN 9 NaN], 'linear', 'EndValues', 0);");
    EXPECT_DOUBLE_EQ(evalScalar("ec(1)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("ec(7)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("ec(5)"), 7.0);
    // 'nearest' endpoints: leading=first good, trailing=last good.
    eval("enr = fillmissing([NaN NaN 3 5 NaN 9 NaN], 'linear', 'EndValues', 'nearest');");
    EXPECT_DOUBLE_EQ(evalScalar("enr(1)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("enr(7)"), 9.0);
    // EndValues with a directional method: interior filled by method,
    // endpoints (incl. trailing 'previous' fill) overridden by EndValues.
    eval("ep = fillmissing([NaN NaN 3 5 NaN 9 NaN], 'previous', 'EndValues', -7);");
    EXPECT_DOUBLE_EQ(evalScalar("ep(1)"), -7.0);
    EXPECT_DOUBLE_EQ(evalScalar("ep(5)"), 5.0);
    EXPECT_DOUBLE_EQ(evalScalar("ep(7)"), -7.0);
    // matrix per-column with 'none'.
    eval("Men = fillmissing([NaN 10; 2 NaN; NaN 30], 'linear', 'EndValues', 'none');");
    EXPECT_DOUBLE_EQ(evalScalar("double(isnan(Men(1,1)))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("Men(2,2)"), 20.0);
    // 'EndValues' rejected for 'constant'; 'previous'/'next' EndValues deferred.
    EXPECT_THROW(eval("fillmissing([1 NaN 3], 'constant', 5, 'EndValues', 0);"), std::exception);
    EXPECT_THROW(eval("fillmissing([1 NaN 3], 'linear', 'EndValues', 'previous');"), std::exception);
}

// ── rmmissing ─────────────────────────────────────────────

TEST_F(MissingDataTest, RmmissingDropsNaN)
{
    eval("z = rmmissing([1 2 NaN 4 5 NaN 7]);");
    EXPECT_EQ(static_cast<int>(evalScalar("numel(z)")), 5);
    EXPECT_DOUBLE_EQ(evalScalar("sum(z)"), 19.0);  // 1+2+4+5+7
}

// ── standardizeMissing ────────────────────────────────────

TEST_F(MissingDataTest, StandardizeMissingReplacesSentinel)
{
    eval("s = standardizeMissing([1 2 -999 4 5 -999 7], -999);");
    EXPECT_EQ(static_cast<int>(evalScalar("sum(isnan(s))")), 2);
    EXPECT_DOUBLE_EQ(evalScalar("nansum(s)"), 19.0);
}
