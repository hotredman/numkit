// quadprog_test.cpp — quadratic program (embedded-.m primal active-set).
//
// min 0.5 x'Hx + f'x s.t. A x <= b, Aeq x = beq, lb <= x <= ub. Strictly-
// convex (PD H) -> unique solution, verified vs MATLAB R2025b across all
// constraint types. Fixes bugs/optim/quadprog.md.

#include <numkit/bundle/standard_engine.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

class QuadprogTest : public ::testing::Test {
public:
    numkit::StandardEngine engine;
    void SetUp() override {}
    numkit::Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// Unconstrained: min 0.5(x1^2+x2^2) - x1 - x2 -> [1 1].
TEST_F(QuadprogTest, Unconstrained) {
    eval("x = quadprog(eye(2), [-1 -1]);");
    EXPECT_NEAR(evalScalar("x(1)"), 1.0, 1e-9);
    EXPECT_NEAR(evalScalar("x(2)"), 1.0, 1e-9);
    EXPECT_EQ(static_cast<int>(evalScalar("size(x,1)")), 2);   // column
    EXPECT_EQ(static_cast<int>(evalScalar("size(x,2)")), 1);
}

// Inequality x1+x2 <= 1 -> [0.5 0.5], fval -0.75.
TEST_F(QuadprogTest, Inequality) {
    eval("[x, fval, ef] = quadprog(eye(2), [-1 -1], [1 1], 1);");
    EXPECT_NEAR(evalScalar("x(1)"), 0.5, 1e-7);
    EXPECT_NEAR(evalScalar("x(2)"), 0.5, 1e-7);
    EXPECT_NEAR(evalScalar("fval"), -0.75, 1e-7);
    EXPECT_EQ(static_cast<int>(evalScalar("ef")), 1);
}

// Equality x1+x2 = 3 -> [1.5 1.5].
TEST_F(QuadprogTest, Equality) {
    eval("x = quadprog(eye(2), [-1 -1], [], [], [1 1], 3);");
    EXPECT_NEAR(evalScalar("x(1)"), 1.5, 1e-9);
    EXPECT_NEAR(evalScalar("x(2)"), 1.5, 1e-9);
}

// Bounds 0 <= x <= 0.3 -> [0.3 0.3] (the upper bound binds).
TEST_F(QuadprogTest, Bounds) {
    eval("x = quadprog(eye(2), [-1 -1], [], [], [], [], [0 0], [0.3 0.3]);");
    EXPECT_NEAR(evalScalar("x(1)"), 0.3, 1e-7);
    EXPECT_NEAR(evalScalar("x(2)"), 0.3, 1e-7);
}

// Non-identity H with an active inequality: H=diag(2,4), f=[-2;-8], x1+x2<=1.
TEST_F(QuadprogTest, MixedHessian) {
    eval("x = quadprog([2 0; 0 4], [-2; -8], [1 1], 1);");
    EXPECT_NEAR(evalScalar("x(1)"), -1.0/3.0, 1e-7);
    EXPECT_NEAR(evalScalar("x(2)"),  4.0/3.0, 1e-7);
}

// Two inequality constraints, both relevant.
TEST_F(QuadprogTest, TwoInequalities) {
    // min 0.5||x||^2 - [2 2]x  s.t. x1+x2<=1, x1<=0.3  -> [0.3 0.7].
    eval("x = quadprog(eye(2), [-2 -2], [1 1; 1 0], [1; 0.3]);");
    EXPECT_NEAR(evalScalar("x(1)"), 0.3, 1e-7);
    EXPECT_NEAR(evalScalar("x(2)"), 0.7, 1e-7);
}
