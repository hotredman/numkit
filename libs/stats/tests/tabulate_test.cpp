// libs/stats/tests/tabulate_test.cpp
//
// Regression guard for tabulate() — frequency table.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class TabulateTest : public ::testing::Test
{
public:
    Engine engine;
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
