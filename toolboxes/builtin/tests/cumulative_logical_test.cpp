// toolboxes/builtin/tests/cumulative_logical_test.cpp
//
// Regression guard for bugs/builtin/cumulative-logical.md: cumsum / cumprod /
// cummax / cummin used to throw "Not a double array" on logical input.
// MATLAB R2025b accepts logical for all four, with a SPLIT class rule:
//   cumsum / cumprod  PROMOTE logical -> double
//   cummax / cummin   PRESERVE the logical class
// Values + classes below are bit-exact MATLAB R2025b.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class CumulativeLogicalTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// cumsum(logical) -> double [1 1 2 3].
TEST_F(CumulativeLogicalTest, CumsumVectorPromotesToDouble)
{
    eval("y = cumsum(logical([1 0 1 1]));");
    EXPECT_DOUBLE_EQ(evalScalar("y(1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(2)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(3)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(4)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("islogical(y)"), 0.0);   // promoted to double
    EXPECT_DOUBLE_EQ(evalScalar("isnumeric(y)"), 1.0);
}

// cumprod(logical) -> double [1 1 0 0].
TEST_F(CumulativeLogicalTest, CumprodVectorPromotesToDouble)
{
    eval("y = cumprod(logical([1 1 0 1]));");
    EXPECT_DOUBLE_EQ(evalScalar("y(1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(2)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(3)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(4)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("islogical(y)"), 0.0);
}

// cummax(logical) -> logical [0 1 1 1] (CLASS PRESERVED).
TEST_F(CumulativeLogicalTest, CummaxVectorPreservesLogical)
{
    eval("y = cummax(logical([0 1 0 1]));");
    EXPECT_DOUBLE_EQ(evalScalar("y(1)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(2)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(3)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(4)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("islogical(y)"), 1.0);   // class preserved
}

// cummin(logical) -> logical [1 1 0 0] (CLASS PRESERVED).
TEST_F(CumulativeLogicalTest, CumminVectorPreservesLogical)
{
    eval("y = cummin(logical([1 1 0 1]));");
    EXPECT_DOUBLE_EQ(evalScalar("y(1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(2)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(3)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(4)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("islogical(y)"), 1.0);
}

// 2-D logical: default (column) scan and explicit dim 2.
TEST_F(CumulativeLogicalTest, Cumsum2DColumnAndDim2)
{
    eval("A = logical([1 0; 1 1]);");
    eval("C = cumsum(A);");        // column-wise: [1 0; 2 1]
    EXPECT_DOUBLE_EQ(evalScalar("C(1,1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("C(2,1)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("C(1,2)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("C(2,2)"), 1.0);
    eval("R = cumsum(A,2);");      // row-wise: [1 1; 1 2]
    EXPECT_DOUBLE_EQ(evalScalar("R(1,2)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("R(2,2)"), 2.0);
}

// Scalar logical + 'reverse' flag.
TEST_F(CumulativeLogicalTest, ScalarAndReverse)
{
    EXPECT_DOUBLE_EQ(evalScalar("cumsum(true)"), 1.0);
    eval("y = cumsum(logical([1 0 1 1]),'reverse');");   // [3 2 2 1]
    EXPECT_DOUBLE_EQ(evalScalar("y(1)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(2)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(3)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(4)"), 1.0);
}
