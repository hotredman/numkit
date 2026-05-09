// libs/stats/tests/tiedrank_test.cpp
//
// Regression guard for tiedrank() — ranks adjusted for ties.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class TiedrankTest : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(TiedrankTest, KnownVectorRanks)
{
    // MATLAB: tiedrank([10 20 30 20 10 40]) = [1.5 3.5 5 3.5 1.5 6]
    eval("[r, t] = tiedrank([10 20 30 20 10 40]);");
    EXPECT_DOUBLE_EQ(evalScalar("r(1)"), 1.5);
    EXPECT_DOUBLE_EQ(evalScalar("r(2)"), 3.5);
    EXPECT_DOUBLE_EQ(evalScalar("r(3)"), 5.0);
    EXPECT_DOUBLE_EQ(evalScalar("r(4)"), 3.5);
    EXPECT_DOUBLE_EQ(evalScalar("r(5)"), 1.5);
    EXPECT_DOUBLE_EQ(evalScalar("r(6)"), 6.0);
    EXPECT_DOUBLE_EQ(evalScalar("t"), 6.0);
}

TEST_F(TiedrankTest, AllEqual)
{
    // n=4 all tied: avg rank = (1+4)/2 = 2.5; tieadj = (64-4)/2 = 30
    eval("[r, t] = tiedrank([5 5 5 5]);");
    EXPECT_DOUBLE_EQ(evalScalar("r(1)"), 2.5);
    EXPECT_DOUBLE_EQ(evalScalar("r(4)"), 2.5);
    EXPECT_DOUBLE_EQ(evalScalar("t"), 30.0);
}

TEST_F(TiedrankTest, NoTies)
{
    eval("[r, t] = tiedrank([3 1 4 1.5]);");  // 1, 1.5, 3, 4 -> ranks 1 2 3 4 (no ties)
    EXPECT_DOUBLE_EQ(evalScalar("r(1)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("r(2)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("r(3)"), 4.0);
    EXPECT_DOUBLE_EQ(evalScalar("r(4)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("t"), 0.0);
}

TEST_F(TiedrankTest, NaNGetsNaNRank)
{
    eval("[r, t] = tiedrank([1 NaN 3 2]);");
    EXPECT_DOUBLE_EQ(evalScalar("r(1)"), 1.0);
    EXPECT_TRUE(std::isnan(evalScalar("r(2)")));
    EXPECT_DOUBLE_EQ(evalScalar("r(3)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("r(4)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("t"), 0.0);
}

TEST_F(TiedrankTest, MatrixColumnWise)
{
    // MATLAB: tiedrank([3 1; 5 2; 5 1; 1 4]) ranks each column.
    eval("[r, t] = tiedrank([3 1; 5 2; 5 1; 1 4]);");
    EXPECT_DOUBLE_EQ(evalScalar("r(1,1)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("r(2,1)"), 3.5);
    EXPECT_DOUBLE_EQ(evalScalar("r(3,1)"), 3.5);
    EXPECT_DOUBLE_EQ(evalScalar("r(4,1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("r(1,2)"), 1.5);
    EXPECT_DOUBLE_EQ(evalScalar("r(4,2)"), 4.0);
    EXPECT_EQ(static_cast<int>(evalScalar("size(t, 1)")), 1);
    EXPECT_EQ(static_cast<int>(evalScalar("size(t, 2)")), 2);
    EXPECT_DOUBLE_EQ(evalScalar("t(1)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("t(2)"), 3.0);
}

TEST_F(TiedrankTest, RowVectorOrientation)
{
    eval("[r, ~] = tiedrank([2 1 3]);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(r, 1)")), 1);
    EXPECT_EQ(static_cast<int>(evalScalar("size(r, 2)")), 3);
}

TEST_F(TiedrankTest, ColumnVectorOrientation)
{
    eval("[r, ~] = tiedrank([2; 1; 3]);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(r, 1)")), 3);
    EXPECT_EQ(static_cast<int>(evalScalar("size(r, 2)")), 1);
}

TEST_F(TiedrankTest, RankSumIsTriangular)
{
    // For non-NaN inputs without ties, sum(rank) = n*(n+1)/2.
    eval("r = tiedrank([5 7 1 3 9 4 8 2 6]);"
         "s = sum(r); n = 9; expected = n*(n+1)/2;");
    EXPECT_DOUBLE_EQ(evalScalar("s"), evalScalar("expected"));
}
