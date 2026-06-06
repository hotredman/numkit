// libs/wavelet/tests/dyaddown_test.cpp
//
// Backfill gtest for libs/wavelet/src/dwt/dyad.cpp::dyaddown.
// Reference values captured from MATLAB R2025b probe.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class DyaddownTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// Default ODD = 0 → keep even-indexed positions (1-based 2:2:end).

TEST_F(DyaddownTest, DefaultEvenPositions)
{
    eval("y = dyaddown([10 20 30 40 50 60 70]);");
    EXPECT_EQ(static_cast<size_t>(evalScalar("numel(y)")), 3u);
    EXPECT_DOUBLE_EQ(evalScalar("y(1)"), 20);
    EXPECT_DOUBLE_EQ(evalScalar("y(2)"), 40);
    EXPECT_DOUBLE_EQ(evalScalar("y(3)"), 60);
}

TEST_F(DyaddownTest, ExplicitOddZero)
{
    eval("y = dyaddown([10 20 30 40 50 60 70], 0);");
    EXPECT_EQ(static_cast<size_t>(evalScalar("numel(y)")), 3u);
    EXPECT_DOUBLE_EQ(evalScalar("y(1)"), 20);
    EXPECT_DOUBLE_EQ(evalScalar("y(3)"), 60);
}

TEST_F(DyaddownTest, OddOneKeepsOddPositions)
{
    // 1-based odd positions: 1, 3, 5, 7 → values 10, 30, 50, 70
    eval("y = dyaddown([10 20 30 40 50 60 70], 1);");
    EXPECT_EQ(static_cast<size_t>(evalScalar("numel(y)")), 4u);
    EXPECT_DOUBLE_EQ(evalScalar("y(1)"), 10);
    EXPECT_DOUBLE_EQ(evalScalar("y(2)"), 30);
    EXPECT_DOUBLE_EQ(evalScalar("y(3)"), 50);
    EXPECT_DOUBLE_EQ(evalScalar("y(4)"), 70);
}

TEST_F(DyaddownTest, ColumnPreservesShape)
{
    eval("y = dyaddown([1; 2; 3; 4]);");
    EXPECT_EQ(static_cast<size_t>(evalScalar("size(y, 1)")), 2u);
    EXPECT_EQ(static_cast<size_t>(evalScalar("size(y, 2)")), 1u);
    EXPECT_DOUBLE_EQ(evalScalar("y(1)"), 2);
    EXPECT_DOUBLE_EQ(evalScalar("y(2)"), 4);
}

TEST_F(DyaddownTest, FloatNegatives)
{
    eval("y = dyaddown([1.5 -2 3.5 -4]);");
    EXPECT_DOUBLE_EQ(evalScalar("y(1)"), -2);
    EXPECT_DOUBLE_EQ(evalScalar("y(2)"), -4);
}

TEST_F(DyaddownTest, EvenLength)
{
    // Length 4: ODD=0 → [2, 4]; ODD=1 → [1, 3]
    eval("y0 = dyaddown([1 2 3 4], 0); y1 = dyaddown([1 2 3 4], 1);");
    EXPECT_DOUBLE_EQ(evalScalar("y0(1)"), 2);
    EXPECT_DOUBLE_EQ(evalScalar("y0(2)"), 4);
    EXPECT_DOUBLE_EQ(evalScalar("y1(1)"), 1);
    EXPECT_DOUBLE_EQ(evalScalar("y1(2)"), 3);
}

// Bug fix 2026-05-08 — matrix path was a flat 1-D operation that
// ignored the `type` arg. Now matches MATLAB dyaddown(M, evenodd, type).

TEST_F(DyaddownTest, MatrixDefaultIsColumnDownsample)
{
    eval("M = [1 4; 2 5; 3 6; 4 7]; y = dyaddown(M);");
    // type='c' default, ODD=0 → keep col 2 = [4;5;6;7].
    EXPECT_EQ(static_cast<size_t>(evalScalar("size(y, 1)")), 4u);
    EXPECT_EQ(static_cast<size_t>(evalScalar("size(y, 2)")), 1u);
    EXPECT_DOUBLE_EQ(evalScalar("y(1)"), 4);
    EXPECT_DOUBLE_EQ(evalScalar("y(4)"), 7);
}

TEST_F(DyaddownTest, MatrixTypeR)
{
    eval("M = [1 4; 2 5; 3 6; 4 7]; y = dyaddown(M, 0, 'r');");
    // ODD=0, type='r' → keep rows 2, 4 → [2 5; 4 7].
    EXPECT_EQ(static_cast<size_t>(evalScalar("size(y, 1)")), 2u);
    EXPECT_EQ(static_cast<size_t>(evalScalar("size(y, 2)")), 2u);
    EXPECT_DOUBLE_EQ(evalScalar("y(1,1)"), 2);
    EXPECT_DOUBLE_EQ(evalScalar("y(1,2)"), 5);
    EXPECT_DOUBLE_EQ(evalScalar("y(2,1)"), 4);
    EXPECT_DOUBLE_EQ(evalScalar("y(2,2)"), 7);
}

TEST_F(DyaddownTest, MatrixTypeM)
{
    eval("M = [1 4; 2 5; 3 6; 4 7]; y = dyaddown(M, 0, 'm');");
    // 'r' then 'c' → [5; 7] (col 2 of rows 2, 4).
    EXPECT_EQ(static_cast<size_t>(evalScalar("size(y, 1)")), 2u);
    EXPECT_EQ(static_cast<size_t>(evalScalar("size(y, 2)")), 1u);
    EXPECT_DOUBLE_EQ(evalScalar("y(1)"), 5);
    EXPECT_DOUBLE_EQ(evalScalar("y(2)"), 7);
}

TEST_F(DyaddownTest, MatrixOddTypeC)
{
    eval("M = [1 4; 2 5; 3 6; 4 7]; y = dyaddown(M, 1, 'c');");
    // ODD=1, type='c' → keep col 1 = [1;2;3;4].
    EXPECT_DOUBLE_EQ(evalScalar("y(1)"), 1);
    EXPECT_DOUBLE_EQ(evalScalar("y(4)"), 4);
}
