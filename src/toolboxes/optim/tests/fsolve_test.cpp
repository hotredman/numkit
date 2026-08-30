// fsolve_test.cpp — nonlinear system solver (embedded-.m Levenberg-Marquardt).
//
// fsolve(fun, x0) solves F(x)=0. Parity with MATLAB is on the SOLUTION (the
// root), verified vs MATLAB R2025b. Fixes bugs/optim/fsolve.md.

#include <numkit/bundle/standard_engine.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

class FsolveTest : public ::testing::Test {
public:
    numkit::StandardEngine engine;
    void SetUp() override {}
    numkit::Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// Scalar root: x^2 - 2 = 0 -> sqrt(2).
TEST_F(FsolveTest, ScalarSqrt2) {
    EXPECT_NEAR(evalScalar("fsolve(@(x) x^2 - 2, 1)"), 1.4142135623730951, 1e-7);
}

// 2x2 system: x1^2+x2^2=1, x1=x2 -> [1/sqrt2, 1/sqrt2].
TEST_F(FsolveTest, System2x2) {
    eval("[xv, fval, ef] = fsolve(@(v) [v(1)^2+v(2)^2-1; v(1)-v(2)], [0.5 0.5]);");
    EXPECT_NEAR(evalScalar("xv(1)"), 0.7071067811865476, 1e-6);
    EXPECT_NEAR(evalScalar("xv(2)"), 0.7071067811865476, 1e-6);
    EXPECT_LT(evalScalar("norm(fval)"), 1e-8);
    EXPECT_EQ(static_cast<int>(evalScalar("ef")), 1);   // converged
}

// A Rosenbrock-style square system with root [1 1].
TEST_F(FsolveTest, RosenbrockSystem) {
    eval("x = fsolve(@(x)[10*(x(2)-x(1)^2); 1-x(1)], [-1.2 1]);");
    EXPECT_NEAR(evalScalar("x(1)"), 1.0, 1e-6);
    EXPECT_NEAR(evalScalar("x(2)"), 1.0, 1e-6);
}

// 3-variable system with multiple roots; from [1 0 4] it lands on [1 2 3].
TEST_F(FsolveTest, System3Var) {
    eval("F = @(x)[x(1)+x(2)+x(3)-6; x(1)^2+x(2)^2+x(3)^2-14; x(1)*x(2)*x(3)-6];");
    eval("x = fsolve(F, [1 0 4]);");
    EXPECT_NEAR(evalScalar("x(1)"), 1.0, 1e-5);
    EXPECT_NEAR(evalScalar("x(2)"), 2.0, 1e-5);
    EXPECT_NEAR(evalScalar("x(3)"), 3.0, 1e-5);
}

// Output orientation mirrors x0: a column x0 yields a column root.
TEST_F(FsolveTest, ColumnOrientation) {
    eval("x = fsolve(@(v) [v(1)^2+v(2)^2-1; v(1)-v(2)], [0.5; 0.5]);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(x,1)")), 2);
    EXPECT_EQ(static_cast<int>(evalScalar("size(x,2)")), 1);
    EXPECT_NEAR(evalScalar("x(1)"), 0.7071067811865476, 1e-6);
}

// fval is the residual at the solution (≈ 0).
TEST_F(FsolveTest, FvalIsResidual) {
    eval("[x, fval] = fsolve(@(x) x^2 - 2, 1);");
    EXPECT_LT(std::abs(evalScalar("fval")), 1e-8);
}

// A non-handle first argument is rejected.
TEST_F(FsolveTest, RejectsNonHandle) {
    EXPECT_THROW(eval("fsolve(5, 1);"), std::exception);
}
