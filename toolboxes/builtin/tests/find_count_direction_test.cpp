// toolboxes/builtin/tests/find_count_direction_test.cpp
//
// Regression guard for bugs/builtin/find-count-direction.md (FIXED):
// find(X, K[, 'first'/'last']) now applies the count + direction in BOTH the
// single-output (linear index) and the [r,c] / [r,c,v] subscript forms.
// MATLAB R2025b reference values.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

class FindCountDirectionTest : public ::testing::Test
{
public:
    numkit::StandardEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    numkit::Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// find(X, K): first K nonzero indices.
TEST_F(FindCountDirectionTest, FirstK)
{
    eval("a = find([0 1 0 1 1], 2);");                 // MATLAB: [2 4]
    EXPECT_EQ(static_cast<int>(evalScalar("numel(a)")), 2);
    EXPECT_DOUBLE_EQ(evalScalar("a(1)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("a(2)"), 4.0);
}

// find(X, 1, 'last'): the very last nonzero index (the common idiom).
TEST_F(FindCountDirectionTest, LastOne)
{
    eval("b = find([0 1 0 1 1], 1, 'last');");         // MATLAB: 5
    EXPECT_EQ(static_cast<int>(evalScalar("numel(b)")), 1);
    EXPECT_DOUBLE_EQ(evalScalar("b"), 5.0);
}

// find(X, K, 'last'): last K nonzeros, returned in ascending index order.
TEST_F(FindCountDirectionTest, LastK)
{
    eval("c = find([0 1 0 1 1], 2, 'last');");         // MATLAB: [4 5]
    EXPECT_EQ(static_cast<int>(evalScalar("numel(c)")), 2);
    EXPECT_DOUBLE_EQ(evalScalar("c(1)"), 4.0);
    EXPECT_DOUBLE_EQ(evalScalar("c(2)"), 5.0);
}

// Explicit 'first' equals the default.
TEST_F(FindCountDirectionTest, FirstExplicit)
{
    eval("c = find([0 1 0 1 1], 2, 'first');");        // MATLAB: [2 4]
    EXPECT_DOUBLE_EQ(evalScalar("c(1)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("c(2)"), 4.0);
}

// K exceeding the nonzero count returns all of them.
TEST_F(FindCountDirectionTest, KExceedsCountReturnsAll)
{
    eval("d = find([0 1 0 1 1], 10);");                // MATLAB: [2 4 5]
    EXPECT_EQ(static_cast<int>(evalScalar("numel(d)")), 3);
    EXPECT_DOUBLE_EQ(evalScalar("d(3)"), 5.0);
}

// No count → all nonzero indices (unchanged behaviour).
TEST_F(FindCountDirectionTest, NoCountReturnsAll)
{
    eval("d = find([0 1 0 1 1]);");
    EXPECT_EQ(static_cast<int>(evalScalar("numel(d)")), 3);
}

// [r, c] = find(A, K): subscript form honours the count.
TEST_F(FindCountDirectionTest, MultiOutputFirst)
{
    eval("A = [0 3; 4 0]; [r, c] = find(A, 1);");      // MATLAB: r=2, c=1
    EXPECT_EQ(static_cast<int>(evalScalar("numel(r)")), 1);
    EXPECT_DOUBLE_EQ(evalScalar("r"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("c"), 1.0);
}

// [r, c] = find(A, K, 'last'): subscript form honours count + direction.
TEST_F(FindCountDirectionTest, MultiOutputLast)
{
    eval("A = [0 3; 4 0]; [r, c] = find(A, 1, 'last');");  // MATLAB: r=1, c=2
    EXPECT_EQ(static_cast<int>(evalScalar("numel(r)")), 1);
    EXPECT_DOUBLE_EQ(evalScalar("r"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("c"), 2.0);
}

// [r, c, v] = find(A, K, 'last'): the values output is windowed too.
TEST_F(FindCountDirectionTest, MultiOutputValues)
{
    eval("A = [0 3; 4 0]; [r, c, v] = find(A, 1, 'last');");
    EXPECT_DOUBLE_EQ(evalScalar("v"), 3.0);            // A(1,2) == 3
}

// K must be a positive scalar integer (MATLAB errors on 0 / negative).
TEST_F(FindCountDirectionTest, PositiveIntegerRequired)
{
    EXPECT_ANY_THROW(eval("find([1 1 1], 0);"));
    EXPECT_ANY_THROW(eval("find([1 1 1], -1);"));
}
