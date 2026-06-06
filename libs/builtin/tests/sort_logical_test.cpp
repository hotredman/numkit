// libs/builtin/tests/sort_logical_test.cpp
//
// Regression guard for bugs/builtin/sort-logical.md: sort used to throw
// "Not a double array" on logical input. MATLAB R2025b sorts logical (as
// 0/1) PRESERVING the logical class on the values; the 2nd-output index
// stays double. Values + classes below are bit-exact MATLAB R2025b.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class SortLogicalTest : public ::testing::Test
{
public:
    StdEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// sort(logical) -> logical [0 0 1 1] (CLASS PRESERVED).
TEST_F(SortLogicalTest, VectorPreservesLogical)
{
    eval("y = sort(logical([0 1 0 1]));");
    EXPECT_DOUBLE_EQ(evalScalar("y(1)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(2)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(3)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(4)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("islogical(y)"), 1.0);
}

// [S,I] = sort(logical(...)) — S logical, I double, stable index [1 3 2 4].
TEST_F(SortLogicalTest, IndexOutputIsDouble)
{
    eval("[S, I] = sort(logical([0 1 0 1]));");
    EXPECT_DOUBLE_EQ(evalScalar("islogical(S)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("islogical(I)"), 0.0);   // index is double
    EXPECT_DOUBLE_EQ(evalScalar("I(1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("I(2)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("I(3)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("I(4)"), 4.0);
}

// 'descend' on logical -> [1 1 0 0], stable index [1 3 2 4 5].
TEST_F(SortLogicalTest, Descend)
{
    eval("[S, I] = sort(logical([1 0 1 0 0]), 'descend');");
    EXPECT_DOUBLE_EQ(evalScalar("S(1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("S(2)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("S(3)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("islogical(S)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("I(1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("I(2)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("I(5)"), 5.0);
}

// 2-D logical: column-wise (default) and along dim 2.
TEST_F(SortLogicalTest, MatrixColumnAndDim2)
{
    eval("C = sort(logical([1 0; 0 1]));");     // [0 0; 1 1]
    EXPECT_DOUBLE_EQ(evalScalar("C(1,1)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("C(2,1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("C(1,2)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("C(2,2)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("islogical(C)"), 1.0);
    eval("R = sort(logical([1 0; 0 1]), 2);");  // [0 1; 0 1]
    EXPECT_DOUBLE_EQ(evalScalar("R(1,1)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("R(1,2)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("R(2,1)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("R(2,2)"), 1.0);
}

// Scalar logical -> logical 1.
TEST_F(SortLogicalTest, Scalar)
{
    eval("y = sort(true);");
    EXPECT_DOUBLE_EQ(evalScalar("y"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("islogical(y)"), 1.0);
}

// NOTE: sort of a CHAR array (MATLAB sorts by code point, stays char) is
// guarded separately in sort_char_test.cpp (fixed 2026-06-05,
// bugs/builtin/sort-char.md). This file guards the logical fix only.
