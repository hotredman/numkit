// toolboxes/builtin/tests/diff_order_test.cpp
//
// Regression guard for bugs/builtin/diff-zero-order.md (FIXED): diff's
// difference order N must be a positive integer scalar — a zero, negative,
// fractional or non-scalar N is an error (numkit previously treated N=0 as an
// identity copy). Valid orders are unchanged. MATLAB R2025b: the rejected
// forms all error "Difference order N must be a positive integer scalar".

#include <numkit/core/engine.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

class DiffOrderTest : public ::testing::Test
{
public:
    numkit::StandardEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    numkit::Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// N = 0 is rejected (was an identity copy).
TEST_F(DiffOrderTest, ZeroOrderThrows)
{
    EXPECT_ANY_THROW(eval("diff([1 2 3], 0);"));
    EXPECT_ANY_THROW(eval("diff([2+3i 7+1i], 0);"));   // complex too
    EXPECT_ANY_THROW(eval("diff(int8([10 5 20]), 0);")); // integer too
}

// Negative, fractional and non-scalar orders are rejected.
TEST_F(DiffOrderTest, InvalidOrdersThrow)
{
    EXPECT_ANY_THROW(eval("diff([1 2 3], -1);"));
    EXPECT_ANY_THROW(eval("diff([1 2 3], 1.5);"));
    EXPECT_ANY_THROW(eval("diff([1 2 3], [1 2]);"));   // non-scalar order
    EXPECT_ANY_THROW(eval("diff([1 2 3], Inf);"));
    EXPECT_ANY_THROW(eval("diff([1 2 3], NaN);"));
}

// Valid positive integer orders are unchanged.
TEST_F(DiffOrderTest, ValidOrdersWork)
{
    eval("a = diff([1 3 6 10 15]);");         // default N=1
    EXPECT_NEAR(evalScalar("a(1)"), 2.0, 1e-12);
    EXPECT_NEAR(evalScalar("a(4)"), 5.0, 1e-12);
    eval("b = diff([1 3 6 10 15], 2);");
    EXPECT_NEAR(evalScalar("b(1)"), 1.0, 1e-12);
    EXPECT_EQ(static_cast<int>(evalScalar("numel(b)")), 3);
    eval("c = diff([1 3 6 10 15], 3);");
    EXPECT_NEAR(evalScalar("c(1)"), 0.0, 1e-12);
    EXPECT_EQ(static_cast<int>(evalScalar("numel(c)")), 2);
    // N exceeding the length collapses to empty (still valid, not an error).
    EXPECT_EQ(static_cast<int>(evalScalar("numel(diff([1 3 6 10 15], 10))")), 0);
    // N with an explicit dim still works.
    eval("m = diff([1 2 4 7; 10 11 13 16], 1, 2);");
    EXPECT_NEAR(evalScalar("m(1,1)"), 1.0, 1e-12);
}
