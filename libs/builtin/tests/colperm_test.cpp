// libs/builtin/tests/colperm_test.cpp
//
// Regression guard for colperm.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class ColPermTest : public ::testing::Test
{
public:
    StandardEngine engine;
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(ColPermTest, BasicSquare)
{
    eval("p = colperm([0 1 0 1; 1 1 1 0; 0 1 0 0; 1 0 0 0]);");
    EXPECT_EQ(static_cast<int>(evalScalar("p(1)")), 3);
    EXPECT_EQ(static_cast<int>(evalScalar("p(2)")), 4);
    EXPECT_EQ(static_cast<int>(evalScalar("p(3)")), 1);
    EXPECT_EQ(static_cast<int>(evalScalar("p(4)")), 2);
}

TEST_F(ColPermTest, OutputShape)
{
    eval("p = colperm([0 1 0 1; 1 1 1 0; 0 1 0 0; 1 0 0 0]);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(p,1)")), 1);
    EXPECT_EQ(static_cast<int>(evalScalar("size(p,2)")), 4);
}

TEST_F(ColPermTest, NegativeEntriesCountAsNonzero)
{
    // nnz per col = [2, 2, 1]; col 3 first, then 1, 2 (stable).
    eval("p = colperm([-1 0 0; 2 -3 0; 0 4 5]);");
    EXPECT_EQ(static_cast<int>(evalScalar("p(1)")), 3);
    EXPECT_EQ(static_cast<int>(evalScalar("p(2)")), 1);
    EXPECT_EQ(static_cast<int>(evalScalar("p(3)")), 2);
}

TEST_F(ColPermTest, NonSquareIncludesZeroColumn)
{
    // nnz per col = [1, 1, 2, 0]; col 4 (0), col 1 (1), col 2 (1), col 3 (2).
    eval("p = colperm([1 0 1 0; 0 1 1 0]);");
    EXPECT_EQ(static_cast<int>(evalScalar("p(1)")), 4);
    EXPECT_EQ(static_cast<int>(evalScalar("p(2)")), 1);
    EXPECT_EQ(static_cast<int>(evalScalar("p(3)")), 2);
    EXPECT_EQ(static_cast<int>(evalScalar("p(4)")), 3);
}

TEST_F(ColPermTest, MultipleZeroColumns)
{
    eval("p = colperm([0 1 0; 0 1 0]);");
    EXPECT_EQ(static_cast<int>(evalScalar("p(1)")), 1);
    EXPECT_EQ(static_cast<int>(evalScalar("p(2)")), 3);
    EXPECT_EQ(static_cast<int>(evalScalar("p(3)")), 2);
}

TEST_F(ColPermTest, NoArgsThrows)
{
    EXPECT_THROW(eval("colperm();"), std::exception);
}
