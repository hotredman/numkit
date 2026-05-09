// libs/stats/tests/crosstab_test.cpp
//
// Regression guard for crosstab() — contingency table + chi-square
// independence test.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class CrosstabTest : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(CrosstabTest, KnownTwoArgTable)
{
    // x = [1 1 2 2 3 3 1 2 3], y = [10 20 10 20 10 20 20 10 20]
    // -> T = [1 2; 2 1; 1 2], chi2 = 0.9, p = 0.637628...
    eval("x = [1 1 2 2 3 3 1 2 3]; y = [10 20 10 20 10 20 20 10 20];"
         "[T, c, p] = crosstab(x, y);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(T, 1)")), 3);
    EXPECT_EQ(static_cast<int>(evalScalar("size(T, 2)")), 2);
    EXPECT_DOUBLE_EQ(evalScalar("T(1, 1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("T(1, 2)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("T(2, 1)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("T(3, 2)"), 2.0);
    EXPECT_NEAR(evalScalar("c"), 0.9, 1e-12);
    EXPECT_NEAR(evalScalar("p"), 0.637628, 1e-5);
}

TEST_F(CrosstabTest, SingleArgFrequencyVector)
{
    eval("T = crosstab([1 2 1 3 2 1]);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(T, 1)")), 3);
    EXPECT_EQ(static_cast<int>(evalScalar("size(T, 2)")), 1);
    EXPECT_DOUBLE_EQ(evalScalar("T(1)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("T(2)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("T(3)"), 1.0);
}

TEST_F(CrosstabTest, IndependentVariablesGiveZeroChi2)
{
    // Perfectly balanced -> chi2 should be 0 and p = 1.
    eval("[T, c, p] = crosstab([1 1 2 2 1 1 2 2], [1 2 1 2 1 2 1 2]);");
    EXPECT_NEAR(evalScalar("c"), 0.0, 1e-12);
    EXPECT_NEAR(evalScalar("p"), 1.0, 1e-12);
}

TEST_F(CrosstabTest, NaNExcluded)
{
    eval("[T, c, ~] = crosstab([1 1 NaN 2], [1 2 1 2]);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(T, 1)")), 2);
    EXPECT_EQ(static_cast<int>(evalScalar("size(T, 2)")), 2);
    // 3 valid pairs: (1,1), (1,2), (2,2)
    EXPECT_DOUBLE_EQ(evalScalar("sum(sum(T))"), 3.0);
}

TEST_F(CrosstabTest, ChiSquareFromContingency)
{
    // x = [1 1 1 1 2 1 2 2 2 2], y = [1 1 1 2 1 2 2 2 2 2]
    // pairs: (1,1)x3 (1,2)x2 (2,1)x1 (2,2)x4 -> T = [3 2; 1 4]
    // N=10, row=[5 5], col=[4 6]
    // E = [2 3; 2 3]
    // chi2 = 1/2 + 1/3 + 1/2 + 1/3 = 5/3 ≈ 1.6667
    eval("x = [1 1 1 1 2 1 2 2 2 2]; y = [1 1 1 2 1 2 2 2 2 2];"
         "[T, c, p] = crosstab(x, y);");
    EXPECT_DOUBLE_EQ(evalScalar("T(1,1)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("T(1,2)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("T(2,1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("T(2,2)"), 4.0);
    EXPECT_NEAR(evalScalar("c"), 5.0 / 3.0, 1e-12);
}

TEST_F(CrosstabTest, RejectsLengthMismatch)
{
    bool threw = false;
    try { eval("crosstab([1 2 3], [1 2]);"); } catch (...) { threw = true; }
    EXPECT_TRUE(threw);
}

TEST_F(CrosstabTest, ColumnSumsAndRowSumsMatch)
{
    // For a contingency table, sum(rows) == row_total and sum(cols) == col_total.
    eval("x = [1 2 3 1 2 3 1]; y = [1 1 2 2 1 2 1];"
         "T = crosstab(x, y);"
         "row_sum = sum(T, 2); col_sum = sum(T, 1);");
    EXPECT_DOUBLE_EQ(evalScalar("sum(row_sum)"), 7.0);
    EXPECT_DOUBLE_EQ(evalScalar("sum(col_sum)"), 7.0);
}
