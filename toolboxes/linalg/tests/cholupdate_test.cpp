// toolboxes/linalg/tests/cholupdate_test.cpp
//
// Regression guard for cholupdate — rank-1 update/downdate of
// Cholesky factor. Pinned against MATLAB R2025b
// (tools/parity/specs/cholupdate.json).

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class CholupdateTest : public ::testing::Test
{
public:
    StandardEngine engine;
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// Identity: R1'*R1 == A + x*x' to ulp.
TEST_F(CholupdateTest, UpdateSatisfiesAlgebraicIdentity)
{
    eval("A = [4 2 1; 2 5 3; 1 3 6];"
         "R = chol(A);"
         "x = [1; 2; 3];"
         "R1 = cholupdate(R, x);"
         "res = max(max(abs(R1'*R1 - (A + x*x'))));");
    EXPECT_LT(evalScalar("res"), 1e-13);
}

TEST_F(CholupdateTest, DowndateSatisfiesAlgebraicIdentity)
{
    eval("A = [4 2 1; 2 5 3; 1 3 6];"
         "R = chol(A);"
         "y = [0.1; 0.1; 0.1];"
         "R2 = cholupdate(R, y, '-');"
         "res = max(max(abs(R2'*R2 - (A - y*y'))));");
    EXPECT_LT(evalScalar("res"), 1e-13);
}

// Bad downdate (would break positive-definiteness) → throws.
TEST_F(CholupdateTest, FailingDowndateThrows)
{
    eval("A = [4 2 1; 2 5 3; 1 3 6];"
         "R = chol(A);");
    EXPECT_THROW(eval("R3 = cholupdate(R, [10; 10; 10], '-');"),
                 std::exception);
}

// '+' is the default sign.
TEST_F(CholupdateTest, PlusIsDefault)
{
    eval("A = eye(3) * 2;"
         "R = chol(A);"
         "x = [1; 0; 0];"
         "R_default = cholupdate(R, x);"
         "R_plus    = cholupdate(R, x, '+');"
         "d = max(max(abs(R_default - R_plus)));");
    EXPECT_DOUBLE_EQ(evalScalar("d"), 0.0);
}

// 1x1 trivial: R1 = sqrt(R^2 + x^2).
TEST_F(CholupdateTest, ScalarUpdate)
{
    eval("R = chol(9);"             // R = [3]
         "R1 = cholupdate(R, 4);"); // R1 = sqrt(9 + 16) = 5
    EXPECT_DOUBLE_EQ(evalScalar("R1"), 5.0);
}
