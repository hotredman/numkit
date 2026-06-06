// libs/stats/tests/tabulate_test.cpp
//
// Regression guard for tabulate() — frequency table.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class TabulateTest : public ::testing::Test
{
public:
    StdEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(TabulateTest, PositiveIntegerDenseLayout)
{
    // [1 2 3 2 1 4 1 5 5 5] -> 5x3 dense:
    //   1: count=3 pct=30
    //   2: count=2 pct=20
    //   3: count=1 pct=10
    //   4: count=1 pct=10
    //   5: count=3 pct=30
    eval("T = tabulate([1 2 3 2 1 4 1 5 5 5]);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(T, 1)")), 5);
    EXPECT_EQ(static_cast<int>(evalScalar("size(T, 2)")), 3);
    EXPECT_DOUBLE_EQ(evalScalar("T(1, 1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("T(1, 2)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("T(1, 3)"), 30.0);
    EXPECT_DOUBLE_EQ(evalScalar("T(5, 2)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("T(5, 3)"), 30.0);
}

TEST_F(TabulateTest, ZeroRowsForMissingIntegers)
{
    // [3 5 3 1 5 5] is positive ints with max=5 -> dense rows 1..5,
    // values 2 and 4 missing -> zero count + percent rows.
    eval("T = tabulate([3 5 3 1 5 5]);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(T, 1)")), 5);
    EXPECT_DOUBLE_EQ(evalScalar("T(2, 2)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("T(2, 3)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("T(4, 2)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("T(3, 2)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("T(5, 2)"), 3.0);
    EXPECT_NEAR(evalScalar("T(1, 3)"), 100.0/6.0, 1e-12);
}

TEST_F(TabulateTest, NonIntegerSparseLayout)
{
    // [0.5 1.5 0.5 2.0] -> sparse: rows for 0.5, 1.5, 2.0
    eval("T = tabulate([0.5 1.5 0.5 2.0]);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(T, 1)")), 3);
    EXPECT_DOUBLE_EQ(evalScalar("T(1, 1)"), 0.5);
    EXPECT_DOUBLE_EQ(evalScalar("T(1, 2)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("T(1, 3)"), 50.0);
    EXPECT_DOUBLE_EQ(evalScalar("T(2, 1)"), 1.5);
    EXPECT_DOUBLE_EQ(evalScalar("T(3, 1)"), 2.0);
}

TEST_F(TabulateTest, NaNExcluded)
{
    // [1 2 NaN 2 1] -> 2 unique non-NaN values, percentage from N=4
    eval("T = tabulate([1 2 NaN 2 1]);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(T, 1)")), 2);
    EXPECT_DOUBLE_EQ(evalScalar("T(1, 2)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("T(1, 3)"), 50.0);
    EXPECT_DOUBLE_EQ(evalScalar("T(2, 3)"), 50.0);
}

TEST_F(TabulateTest, EmptyInput)
{
    eval("T = tabulate([]);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(T, 1)")), 0);
}

TEST_F(TabulateTest, AllNaNInput)
{
    eval("T = tabulate([NaN NaN NaN]);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(T, 1)")), 0);
}

TEST_F(TabulateTest, NegativeIntegersUseSparseLayout)
{
    // [-1 -2 -1 0] has non-positive ints -> sparse layout
    eval("T = tabulate([-1 -2 -1 0]);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(T, 1)")), 3);
    EXPECT_DOUBLE_EQ(evalScalar("T(1, 1)"), -2.0);
    EXPECT_DOUBLE_EQ(evalScalar("T(2, 1)"), -1.0);
    EXPECT_DOUBLE_EQ(evalScalar("T(3, 1)"),  0.0);
}

TEST_F(TabulateTest, ColumnSumsEqual100Percent)
{
    eval("T = tabulate([1 2 3 4 5 1 2 3 4]); s = sum(T(:, 3));");
    EXPECT_NEAR(evalScalar("s"), 100.0, 1e-12);
}

TEST_F(TabulateTest, CountSumEqualsN)
{
    eval("T = tabulate([1 2 3 4 5 1 2 3 4]); n = sum(T(:, 2));");
    EXPECT_DOUBLE_EQ(evalScalar("n"), 9.0);
}

// grp2idx: turn a grouping variable into a 1-based index vector. vs MATLAB
// R2025b. Implemented 2026-05-30 (was an undefined function).
TEST_F(TabulateTest, Grp2idxCellstrFirstAppearance)
{
    // cellstr groups in first-appearance order: b,a,c -> [1 2 1 3].
    eval("[g, gn, gl] = grp2idx({'b','a','b','c'});");
    EXPECT_DOUBLE_EQ(evalScalar("iscolumn(g)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("g(1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("g(2)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("g(4)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("numel(gn)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("strcmp(gn{1},'b')"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("strcmp(gn{2},'a')"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("strcmp(gn{3},'c')"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("strcmp(gl{1},'b')"), 1.0);  // gl == gn
}

TEST_F(TabulateTest, Grp2idxNumericSorted)
{
    // numeric groups sorted ascending; index is the rank.
    eval("[g, gn] = grp2idx([3 1 3 2 1]);");
    EXPECT_DOUBLE_EQ(evalScalar("g(1)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("g(2)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("g(4)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("strcmp(gn{1},'1')"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("strcmp(gn{3},'3')"), 1.0);
    // negative + fractional names via num2str
    eval("[g2, gn2] = grp2idx([10 -5 10 3]);");
    EXPECT_DOUBLE_EQ(evalScalar("strcmp(gn2{1},'-5')"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("g2(2)"), 1.0);
}

TEST_F(TabulateTest, Grp2idxNaNAndLogical)
{
    eval("[g, gn] = grp2idx([3 1 NaN 2]);");
    EXPECT_DOUBLE_EQ(evalScalar("double(isnan(g(3)))"), 1.0); // NaN -> NaN index
    EXPECT_DOUBLE_EQ(evalScalar("numel(gn)"), 3.0);           // NaN excluded
    eval("[gl, gnl] = grp2idx(logical([1 0 1 0]));");
    EXPECT_DOUBLE_EQ(evalScalar("gl(1)"), 2.0);               // true -> group 2
    EXPECT_DOUBLE_EQ(evalScalar("gl(2)"), 1.0);               // false -> group 1
    EXPECT_DOUBLE_EQ(evalScalar("strcmp(gnl{1},'0')"), 1.0);
}
