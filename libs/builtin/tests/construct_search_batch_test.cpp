// libs/builtin/tests/construct_search_batch_test.cpp
// matrix-construction + search/sort/mod:
//   construction: zeros / ones / eye / linspace / logspace / repmat
//   search/sort:  sort / find / unique
//   mod/rem:      mod / rem
//   booleans:     true / false
// All  — bit-identical MATLAB R2025b
// on probed inputs.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class ConstructSearchBatchTest : public ::testing::Test
{
public:
    StandardEngine engine;
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(ConstructSearchBatchTest, ZerosOnesEye)
{
    EXPECT_DOUBLE_EQ(evalScalar("sum(zeros(2,3)(:))"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("sum(ones(3,2)(:))"),  6.0);
    EXPECT_DOUBLE_EQ(evalScalar("sum(eye(4)(:))"),     4.0);   // trace = 4
    EXPECT_DOUBLE_EQ(evalScalar("eye(4)(2,2)"),        1.0);
    EXPECT_DOUBLE_EQ(evalScalar("eye(4)(1,2)"),        0.0);
}

TEST_F(ConstructSearchBatchTest, LinspaceLogspace)
{
    eval("v = linspace(0, 1, 5);");
    EXPECT_DOUBLE_EQ(evalScalar("v(1)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("v(3)"), 0.5);
    EXPECT_DOUBLE_EQ(evalScalar("v(5)"), 1.0);
    eval("L = logspace(0, 2, 3);");  // 1, 10, 100
    EXPECT_NEAR(evalScalar("L(1)"),   1.0, 1e-12);
    EXPECT_NEAR(evalScalar("L(2)"),  10.0, 1e-12);
    EXPECT_NEAR(evalScalar("L(3)"), 100.0, 1e-12);
}

TEST_F(ConstructSearchBatchTest, Repmat)
{
    eval("R = repmat([1 2], 2, 3);");
    EXPECT_DOUBLE_EQ(evalScalar("size(R,1)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(R,2)"), 6.0);
    EXPECT_DOUBLE_EQ(evalScalar("R(1,1)"),    1.0);
    EXPECT_DOUBLE_EQ(evalScalar("R(2,4)"),    2.0);  // tile [1 2] along cols
    EXPECT_DOUBLE_EQ(evalScalar("R(2,5)"),    1.0);
}

TEST_F(ConstructSearchBatchTest, Sort)
{
    eval("v = sort([3, 1, 4, 1, 5, 9]);");
    EXPECT_DOUBLE_EQ(evalScalar("v(1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("v(2)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("v(6)"), 9.0);
}

TEST_F(ConstructSearchBatchTest, Find)
{
    eval("ix = find([0, 1, 0, 1, 1]);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(ix)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("ix(1)"),     2.0);
    EXPECT_DOUBLE_EQ(evalScalar("ix(2)"),     4.0);
    EXPECT_DOUBLE_EQ(evalScalar("ix(3)"),     5.0);
}

TEST_F(ConstructSearchBatchTest, Unique)
{
    eval("u = unique([1, 2, 1, 3, 2, 4]);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(u)"), 4.0);
    EXPECT_DOUBLE_EQ(evalScalar("u(1)"),     1.0);
    EXPECT_DOUBLE_EQ(evalScalar("u(4)"),     4.0);
}

TEST_F(ConstructSearchBatchTest, ModRem)
{
    // mod follows divisor sign; rem follows dividend sign.
    EXPECT_DOUBLE_EQ(evalScalar("mod(10,  3)"),  1.0);
    EXPECT_DOUBLE_EQ(evalScalar("mod(-10, 3)"),  2.0);   // mod toward +inf
    EXPECT_DOUBLE_EQ(evalScalar("mod(10, -3)"), -2.0);
    EXPECT_DOUBLE_EQ(evalScalar("rem(10,  3)"),  1.0);
    EXPECT_DOUBLE_EQ(evalScalar("rem(-10, 3)"), -1.0);   // rem toward zero
    EXPECT_DOUBLE_EQ(evalScalar("rem(10, -3)"),  1.0);
}

TEST_F(ConstructSearchBatchTest, TrueFalse)
{
    EXPECT_DOUBLE_EQ(evalScalar("double(true)"),  1.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(false)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("sum(true(3)(:))"),  9.0);   // 3×3 trues
    EXPECT_DOUBLE_EQ(evalScalar("sum(false(3)(:))"), 0.0);
}

// ── find: [r, c] and [r, c, v] subscript / value outputs ────────────────
// Bug: find only returned linear indices; [r,c]=find(X) errored.
TEST_F(ConstructSearchBatchTest, FindRowColMatrix)
{
    eval("[r, c] = find([0 1; 1 0]);");
    EXPECT_DOUBLE_EQ(evalScalar("r(1)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("r(2)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("c(1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("c(2)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(r,1)"), 2.0); // column vectors
    EXPECT_DOUBLE_EQ(evalScalar("size(r,2)"), 1.0);
}

TEST_F(ConstructSearchBatchTest, FindRowColValue)
{
    eval("[r, c, v] = find([0 5; 7 0]);");
    EXPECT_DOUBLE_EQ(evalScalar("r(1)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("c(1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("v(1)"), 7.0); // column-major order: 7 first
    EXPECT_DOUBLE_EQ(evalScalar("v(2)"), 5.0);
}

TEST_F(ConstructSearchBatchTest, FindRowVectorOrientation)
{
    // Row-vector input → row-vector subscript outputs.
    eval("[r, c] = find([0 1 0 1]);");
    EXPECT_DOUBLE_EQ(evalScalar("size(r,1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(r,2)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("r(1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("r(2)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("c(1)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("c(2)"), 4.0);
}

TEST_F(ConstructSearchBatchTest, FindSingleOutputUnchanged)
{
    eval("k = find([0; 3; 4]);");
    EXPECT_DOUBLE_EQ(evalScalar("k(1)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("k(2)"), 3.0);
}
