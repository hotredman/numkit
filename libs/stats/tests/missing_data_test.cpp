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

// ── rmoutliers ────────────────────────────────────────────

TEST_F(MissingDataTest, RmoutliersDropsExtreme)
{
    eval("y = rmoutliers([1 2 3 4 5 6 7 100]);");
    EXPECT_EQ(static_cast<int>(evalScalar("numel(y)")), 7);
    EXPECT_DOUBLE_EQ(evalScalar("max(y)"), 7.0);
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
