// libs/builtin/tests/sylvester_test.cpp
//
// Regression guard for builtin::sylvester (symmetric A and B form).

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class SylvesterTest : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(SylvesterTest, SylvesterSymmetricSPD)
{
    // A*X + X*B = C, A and B symmetric SPD.
    eval("A = [4 1; 1 3]; B = [2 0.5; 0.5 5]; C = [1 2; 3 4];"
         "X = sylvester(A, B, C);");
    EXPECT_NEAR(evalScalar("max(max(abs(A*X + X*B - C)))"), 0.0, 1e-12);
}

TEST_F(SylvesterTest, SylvesterDiagonalAB)
{
    // Diagonal A and B: closed-form X(i,j) = C(i,j) / (a_i + b_j).
    eval("A = [3 0; 0 5]; B = [1 0; 0 4]; C = [10 6; 8 18];"
         "X = sylvester(A, B, C);");
    EXPECT_NEAR(evalScalar("X(1,1)"), 10.0/4.0, 1e-12);   // 10/(3+1)
    EXPECT_NEAR(evalScalar("X(1,2)"), 6.0/7.0, 1e-12);    // 6/(3+4)
    EXPECT_NEAR(evalScalar("X(2,1)"), 8.0/6.0, 1e-12);    // 8/(5+1)
    EXPECT_NEAR(evalScalar("X(2,2)"), 18.0/9.0, 1e-12);   // 18/(5+4)
}

TEST_F(SylvesterTest, SylvesterRectangularC)
{
    // n=3, m=2: A is 3x3, B is 2x2, C is 3x2.
    eval("A = eye(3) + 0.1*[0 1 0; 1 0 1; 0 1 0]; "
         "B = [2 0; 0 3]; "
         "C = ones(3, 2); "
         "X = sylvester(A, B, C);");
    EXPECT_NEAR(evalScalar("max(max(abs(A*X + X*B - C)))"), 0.0, 1e-12);
}

TEST_F(SylvesterTest, SylvesterAsymmetricRejected)
{
    EXPECT_THROW(eval("sylvester([1 2; 3 4], eye(2), eye(2));"), std::exception);
    EXPECT_THROW(eval("sylvester(eye(2), [1 2; 3 4], eye(2));"), std::exception);
}

TEST_F(SylvesterTest, SylvesterBadDimsRejected)
{
    // C dimensions must match: A(n×n), B(m×m), C(n×m).
    EXPECT_THROW(eval("sylvester(eye(2), eye(3), ones(2, 2));"), std::exception);
}
