// libs/linalg/tests/condest_test.cpp
//
// Regression guard for condest — 1-norm condition number estimate.
// Hardcoded expected values from MATLAB R2025b on the well-
// conditioned cases. Near-singular matrices (where MATLAB's
// Higham 1988 power-iteration estimator drifts from the exact
// value) are accepted with a wide relative tolerance — see
// tools/parity/specs/condest.json for the documented gap.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class CondestTest : public ::testing::Test
{
public:
    StdEngine engine;
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// Identity has condition number 1 exactly.
TEST_F(CondestTest, IdentityIsOne)
{
    EXPECT_DOUBLE_EQ(evalScalar("condest(eye(3))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("condest(eye(7))"), 1.0);
}

// Diagonal matrix: cond = max|d| / min|d|.
TEST_F(CondestTest, DiagonalMatrixHasExactCond)
{
    // diag([1, 1e-3, 1e-6]) — 1-norm cond = 1e6.
    EXPECT_NEAR(evalScalar("condest(diag([1 1e-3 1e-6]))"), 1e6, 1e-9);
}

// Small upper-triangular: matches MATLAB exact.
TEST_F(CondestTest, SmallUpperTriangular)
{
    // norm([1 2 3; 0 4 5; 0 0 6], 1) = max col sum = 14 (3+5+6).
    // inv = [1 -1/2 -1/12; 0 1/4 -5/24; 0 0 1/6].
    // norm(inv, 1) = max col sum = 1 + 1/2 + 1/12 + ... computed below.
    eval("A = [1 2 3; 0 4 5; 0 0 6]; c = condest(A);");
    // From MATLAB R2025b: condest([1 2 3; 0 4 5; 0 0 6]) == 14.
    EXPECT_NEAR(evalScalar("c"), 14.0, 1e-12);
}

// condest(singular) == Inf.
TEST_F(CondestTest, SingularReturnsInfinity)
{
    eval("A = [1 1; 1 1]; c = condest(A);");
    EXPECT_TRUE(std::isinf(evalScalar("c")));
}

// 1x1 trivial: condest of scalar [x] = |x|/|x| = 1 (or Inf for zero).
TEST_F(CondestTest, ScalarMatrixIsOne)
{
    EXPECT_DOUBLE_EQ(evalScalar("condest([5])"), 1.0);
}
