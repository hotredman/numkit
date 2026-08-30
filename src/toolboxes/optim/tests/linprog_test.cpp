// linprog_test.cpp — linear program (embedded-.m, proximal regularization
// over quadprog).
//
// min f'x s.t. A x <= b, Aeq x = beq, lb <= x <= ub. Exact vertex for a
// unique optimum, verified vs MATLAB R2025b. Fixes bugs/optim/linprog.md.

#include <numkit/bundle/standard_engine.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

class LinprogTest : public ::testing::Test {
public:
    numkit::StandardEngine engine;
    void SetUp() override {}
    numkit::Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// Lower-bound LP: x1>=1, x2>=1, min x1+x2 -> [1 1] (free vars, no lb passed).
TEST_F(LinprogTest, LowerBound) {
    eval("[x, fval, ef] = linprog([1 1], [-1 0; 0 -1], [-1; -1]);");
    EXPECT_NEAR(evalScalar("x(1)"), 1.0, 1e-6);
    EXPECT_NEAR(evalScalar("x(2)"), 1.0, 1e-6);
    EXPECT_NEAR(evalScalar("fval"), 2.0, 1e-6);
    EXPECT_EQ(static_cast<int>(evalScalar("ef")), 1);
    EXPECT_EQ(static_cast<int>(evalScalar("size(x,1)")), 2);   // column
    EXPECT_EQ(static_cast<int>(evalScalar("size(x,2)")), 1);
}

// Classic LP: max 3x+2y s.t. x+y<=4, x+3y<=6, x>=0 (= min -3x-2y) -> [4 0].
TEST_F(LinprogTest, ClassicMax) {
    eval("[x, fval] = linprog([-3 -2], [1 1; 1 3], [4; 6], [], [], [0 0], []);");
    EXPECT_NEAR(evalScalar("x(1)"), 4.0, 1e-6);
    EXPECT_NEAR(evalScalar("x(2)"), 0.0, 1e-6);
    EXPECT_NEAR(evalScalar("fval"), -12.0, 1e-6);
}

// min -x1-2x2 s.t. x1+x2<=4, x1<=3, x>=0 -> [0 4].
TEST_F(LinprogTest, BoundedVertex) {
    eval("x = linprog([-1 -2], [1 1; 1 0], [4; 3], [], [], [0 0], []);");
    EXPECT_NEAR(evalScalar("x(1)"), 0.0, 1e-6);
    EXPECT_NEAR(evalScalar("x(2)"), 4.0, 1e-6);
}

// Upper/lower bounds only: min -x1-x2 s.t. 0<=x<=[2 3] -> [2 3].
TEST_F(LinprogTest, BoxBounds) {
    eval("x = linprog([-1 -1], [], [], [], [], [0 0], [2 3]);");
    EXPECT_NEAR(evalScalar("x(1)"), 2.0, 1e-6);
    EXPECT_NEAR(evalScalar("x(2)"), 3.0, 1e-6);
}
