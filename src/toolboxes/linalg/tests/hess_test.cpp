// toolboxes/builtin/tests/hess_test.cpp
//
// Regression guard for builtin::hess (Phase 2c foundation).

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class HessTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override {}
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(HessTest, HessReconstructIdentity)
{
    eval("A = [4 1 2; 1 3 7; 2 8 5]; [P, H] = hess(A);");
    EXPECT_NEAR(evalScalar("max(max(abs(P*H*P' - A)))"), 0.0, 1e-12);
    EXPECT_NEAR(evalScalar("max(max(abs(P'*P - eye(3))))"), 0.0, 1e-12);
}

TEST_F(HessTest, HessIsUpperHessenberg)
{
    // For 4×4: entries below sub-diagonal must be zero.
    eval("A = [1 2 3 4; 5 6 7 8; 9 10 11 13; 1 0 1 2]; [P, H] = hess(A);");
    EXPECT_DOUBLE_EQ(evalScalar("H(3,1)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("H(4,1)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("H(4,2)"), 0.0);
}

TEST_F(HessTest, HessSmallMatricesPassthrough)
{
    // n <= 2: any matrix is already Hessenberg.
    eval("[P1, H1] = hess([5]);");
    EXPECT_DOUBLE_EQ(evalScalar("H1(1,1)"), 5.0);
    EXPECT_DOUBLE_EQ(evalScalar("P1(1,1)"), 1.0);

    eval("[P2, H2] = hess([1 2; 3 4]);");
    EXPECT_DOUBLE_EQ(evalScalar("max(max(abs(H2 - [1 2; 3 4])))"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("max(max(abs(P2 - eye(2))))"), 0.0);
}

TEST_F(HessTest, HessSingleOutputReturnsHOnly)
{
    eval("H = hess([4 1 2; 1 3 7; 2 8 5]);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(H,1)")), 3);
    EXPECT_EQ(static_cast<int>(evalScalar("size(H,2)")), 3);
    EXPECT_NEAR(evalScalar("abs(H(3,1))"), 0.0, 1e-12);
}

TEST_F(HessTest, HessNonSquareRejected)
{
    EXPECT_THROW(eval("hess([1 2 3; 4 5 6]);"), std::exception);
}
