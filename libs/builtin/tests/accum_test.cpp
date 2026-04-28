// libs/builtin/tests/accum_test.cpp
//
// accumarray — group-by reduction. Tests cover:
//   * 1D and 2D output shapes
//   * Default reducer (sum)
//   * Built-in reducers via @sum / @max / @min / @prod / @mean / @any / @all
//   * fillVal for empty cells
//   * Explicit sz, including over-sizing
//   * Scalar vals (broadcast) and vector vals
//   * Empty inputs
//   * Validation: bad subs, custom function handle, sparse flag

#include "dual_engine_fixture.hpp"

#include <cmath>

using namespace m_test;

class AccumTest : public DualEngineTest
{};

TEST_P(AccumTest, BasicSum1D)
{
    // subs = [1; 2; 1; 3]; vals = [10; 20; 30; 40]
    // Group by subs: A(1) = 10+30 = 40, A(2) = 20, A(3) = 40
    eval("subs = [1; 2; 1; 3]; vals = [10; 20; 30; 40]; A = accumarray(subs, vals);");
    EXPECT_DOUBLE_EQ(evalScalar("size(A, 1);"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(A, 2);"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("A(1);"), 40.0);
    EXPECT_DOUBLE_EQ(evalScalar("A(2);"), 20.0);
    EXPECT_DOUBLE_EQ(evalScalar("A(3);"), 40.0);
}

TEST_P(AccumTest, MissingCellsZeroByDefault)
{
    // No subscript = 2 → A(2) stays 0.
    eval("subs = [1; 3]; vals = [5; 7]; A = accumarray(subs, vals);");
    EXPECT_DOUBLE_EQ(evalScalar("A(1);"), 5.0);
    EXPECT_DOUBLE_EQ(evalScalar("A(2);"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("A(3);"), 7.0);
}

TEST_P(AccumTest, ExplicitSizeGrowsOutput)
{
    // sz=5 forces a 5×1 output even though max(subs) = 3.
    eval("subs = [1; 3]; vals = [5; 7]; A = accumarray(subs, vals, [5, 1]);");
    EXPECT_DOUBLE_EQ(evalScalar("size(A, 1);"), 5.0);
    EXPECT_DOUBLE_EQ(evalScalar("A(5);"), 0.0);
}

TEST_P(AccumTest, ScalarValsBroadcast)
{
    // val is a scalar — every subscript contributes 1 → counts per group.
    eval("subs = [1; 2; 1; 3; 1]; A = accumarray(subs, 1);");
    EXPECT_DOUBLE_EQ(evalScalar("A(1);"), 3.0);  // three 1's at index 1
    EXPECT_DOUBLE_EQ(evalScalar("A(2);"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("A(3);"), 1.0);
}

TEST_P(AccumTest, FillValueAppliesToEmptyCells)
{
    eval("subs = [1; 4]; vals = [5; 7];"
         "A = accumarray(subs, vals, [5, 1], @sum, -1);");
    EXPECT_DOUBLE_EQ(evalScalar("A(1);"), 5.0);
    EXPECT_DOUBLE_EQ(evalScalar("A(2);"), -1.0);  // empty
    EXPECT_DOUBLE_EQ(evalScalar("A(3);"), -1.0);  // empty
    EXPECT_DOUBLE_EQ(evalScalar("A(4);"), 7.0);
    EXPECT_DOUBLE_EQ(evalScalar("A(5);"), -1.0);  // empty
}

TEST_P(AccumTest, FillValueDoesNotOverwriteContributions)
{
    eval("subs = [1; 2; 1]; vals = [0; 0; 0];"
         "A = accumarray(subs, vals, [3, 1], @sum, 99);");
    // A(1) = 0+0 = 0 (touched, NOT 99); A(2) = 0; A(3) = 99 (untouched)
    EXPECT_DOUBLE_EQ(evalScalar("A(1);"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("A(2);"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("A(3);"), 99.0);
}

TEST_P(AccumTest, MaxReducer)
{
    eval("subs = [1; 1; 2; 2]; vals = [3; 7; 5; 1];"
         "A = accumarray(subs, vals, [], @max);");
    EXPECT_DOUBLE_EQ(evalScalar("A(1);"), 7.0);
    EXPECT_DOUBLE_EQ(evalScalar("A(2);"), 5.0);
}

TEST_P(AccumTest, MinReducer)
{
    eval("subs = [1; 1; 2; 2]; vals = [3; 7; 5; 1];"
         "A = accumarray(subs, vals, [], @min);");
    EXPECT_DOUBLE_EQ(evalScalar("A(1);"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("A(2);"), 1.0);
}

TEST_P(AccumTest, ProdReducer)
{
    eval("subs = [1; 1; 1; 2]; vals = [2; 3; 4; 5];"
         "A = accumarray(subs, vals, [], @prod);");
    EXPECT_DOUBLE_EQ(evalScalar("A(1);"), 24.0);  // 2*3*4
    EXPECT_DOUBLE_EQ(evalScalar("A(2);"), 5.0);
}

TEST_P(AccumTest, MeanReducer)
{
    eval("subs = [1; 1; 1; 2; 2]; vals = [10; 20; 30; 40; 60];"
         "A = accumarray(subs, vals, [], @mean);");
    EXPECT_DOUBLE_EQ(evalScalar("A(1);"), 20.0);  // (10+20+30)/3
    EXPECT_DOUBLE_EQ(evalScalar("A(2);"), 50.0);  // (40+60)/2
}

TEST_P(AccumTest, AnyReducer)
{
    eval("subs = [1; 1; 2; 2]; vals = [0; 1; 0; 0];"
         "A = accumarray(subs, vals, [], @any);");
    EXPECT_DOUBLE_EQ(evalScalar("A(1);"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("A(2);"), 0.0);
}

TEST_P(AccumTest, AllReducer)
{
    eval("subs = [1; 1; 2; 2]; vals = [1; 1; 1; 0];"
         "A = accumarray(subs, vals, [], @all);");
    EXPECT_DOUBLE_EQ(evalScalar("A(1);"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("A(2);"), 0.0);
}

TEST_P(AccumTest, TwoDimensionalOutput)
{
    // subs is N×2 → 2D output. (1,1) gets 5+15, (1,2) gets 10, (2,2) gets 20.
    eval("subs = [1 1; 1 2; 1 1; 2 2];"
         "vals = [5; 10; 15; 20];"
         "A = accumarray(subs, vals);");
    EXPECT_DOUBLE_EQ(evalScalar("size(A, 1);"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(A, 2);"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("A(1, 1);"), 20.0);
    EXPECT_DOUBLE_EQ(evalScalar("A(1, 2);"), 10.0);
    EXPECT_DOUBLE_EQ(evalScalar("A(2, 2);"), 20.0);
    EXPECT_DOUBLE_EQ(evalScalar("A(2, 1);"), 0.0);  // unused
}

TEST_P(AccumTest, TwoDimensionalExplicitSize)
{
    eval("subs = [1 1; 2 3];"
         "vals = [10; 20];"
         "A = accumarray(subs, vals, [3, 4]);");
    EXPECT_DOUBLE_EQ(evalScalar("size(A, 1);"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(A, 2);"), 4.0);
    EXPECT_DOUBLE_EQ(evalScalar("A(1, 1);"), 10.0);
    EXPECT_DOUBLE_EQ(evalScalar("A(2, 3);"), 20.0);
    EXPECT_DOUBLE_EQ(evalScalar("A(3, 4);"), 0.0);
}

TEST_P(AccumTest, EmptyInputZeroOutput)
{
    // Empty subs + sz given → zero-shape output.
    eval("subs = zeros(0, 1); vals = zeros(0, 1); A = accumarray(subs, vals, [3, 1]);");
    EXPECT_DOUBLE_EQ(evalScalar("size(A, 1);"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("A(1);"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("A(2);"), 0.0);
}

TEST_P(AccumTest, SubscriptZeroThrows)
{
    eval("subs = [0; 1]; vals = [5; 6];");
    EXPECT_THROW(eval("A = accumarray(subs, vals);"), std::exception);
}

TEST_P(AccumTest, NegativeSubscriptThrows)
{
    eval("subs = [-1; 1]; vals = [5; 6];");
    EXPECT_THROW(eval("A = accumarray(subs, vals);"), std::exception);
}

TEST_P(AccumTest, NonIntegerSubscriptThrows)
{
    eval("subs = [1.5; 2]; vals = [5; 6];");
    EXPECT_THROW(eval("A = accumarray(subs, vals);"), std::exception);
}

TEST_P(AccumTest, MismatchedValSizeThrows)
{
    eval("subs = [1; 2; 3]; vals = [10; 20];");  // length 3 vs 2
    EXPECT_THROW(eval("A = accumarray(subs, vals);"), std::exception);
}

TEST_P(AccumTest, UnsupportedReducerThrows)
{
    eval("subs = [1; 2]; vals = [10; 20];");
    EXPECT_THROW(eval("A = accumarray(subs, vals, [], @sin);"), std::exception);
}

TEST_P(AccumTest, SparseFlagThrows)
{
    eval("subs = [1; 2]; vals = [10; 20];");
    EXPECT_THROW(eval("A = accumarray(subs, vals, [], @sum, 0, 1);"), std::exception);
}

TEST_P(AccumTest, SparseFlagZeroIsOk)
{
    // issparse = 0 (the default value) is silently allowed.
    eval("subs = [1; 2]; vals = [10; 20];"
         "A = accumarray(subs, vals, [], @sum, 0, 0);");
    EXPECT_DOUBLE_EQ(evalScalar("A(1);"), 10.0);
}

INSTANTIATE_DUAL(AccumTest);
