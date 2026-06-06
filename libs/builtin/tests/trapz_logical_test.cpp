// libs/builtin/tests/trapz_logical_test.cpp
//
// Regression guard for bugs/builtin/trapz-logical.md: trapz used to throw
// "Not a double array" on logical input. MATLAB R2025b promotes a logical
// X and/or Y to double (the logical class is NOT preserved — integration
// returns double). Values below are bit-exact MATLAB R2025b.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class TrapzLogicalTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// trapz(logical(Y)) unit spacing -> double 2.
TEST_F(TrapzLogicalTest, UnitSpacingVector)
{
    EXPECT_DOUBLE_EQ(evalScalar("trapz(logical([1 0 1 1]))"), 2.0);
    eval("y = trapz(logical([1 0 1 1]));");
    EXPECT_DOUBLE_EQ(evalScalar("islogical(y)"), 0.0);   // promoted to double
}

// trapz(X, logical(Y)) — non-uniform X spacing.
TEST_F(TrapzLogicalTest, NonUniformXLogicalY)
{
    EXPECT_DOUBLE_EQ(evalScalar("trapz([1 3 4 7], logical([1 0 1 1]))"), 4.5);
}

// trapz(logical(X), Y) — logical X is promoted for spacing too.
TEST_F(TrapzLogicalTest, LogicalXDoubleY)
{
    EXPECT_DOUBLE_EQ(evalScalar("trapz(logical([0 1 1 1]), [1 2 3 4])"), 1.5);
}

// 2-D logical: column-wise (default) and along dim 2.
TEST_F(TrapzLogicalTest, MatrixColumnAndDim2)
{
    eval("c = trapz(logical([1 0; 1 1]));");    // [1 0.5]
    EXPECT_DOUBLE_EQ(evalScalar("c(1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("c(2)"), 0.5);
    eval("r = trapz(logical([1 0; 1 1]), 2);"); // [0.5; 1]
    EXPECT_DOUBLE_EQ(evalScalar("r(1)"), 0.5);
    EXPECT_DOUBLE_EQ(evalScalar("r(2)"), 1.0);
}

// Scalar and empty logical -> 0 (MATLAB).
TEST_F(TrapzLogicalTest, ScalarAndEmpty)
{
    EXPECT_DOUBLE_EQ(evalScalar("trapz(true)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("trapz(logical([]))"), 0.0);
}
