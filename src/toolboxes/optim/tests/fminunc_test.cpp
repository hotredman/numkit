// fminunc_test.cpp — unconstrained gradient minimization (embedded-.m BFGS).
//
// fminunc(fun, x0) minimises a smooth objective via BFGS quasi-Newton with a
// central-difference gradient. Parity with MATLAB is on the SOLUTION (the
// minimiser). Verified vs MATLAB R2025b. Fixes bugs/optim/fminunc.md.

#include <numkit/bundle/standard_engine.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

class FminuncTest : public ::testing::Test {
public:
    numkit::StandardEngine engine;
    void SetUp() override {}
    numkit::Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// Scalar parabola (x-3)^2 -> 3.
TEST_F(FminuncTest, Parabola) {
    EXPECT_NEAR(evalScalar("fminunc(@(x) (x-3)^2, 0)"), 3.0, 1e-5);
}

// 2-D quadratic bowl -> [1 -2].
TEST_F(FminuncTest, QuadraticBowl) {
    eval("[x, fval, ef] = fminunc(@(x) (x(1)-1)^2 + 2*(x(2)+2)^2 + 3, [0 0]);");
    EXPECT_NEAR(evalScalar("x(1)"),  1.0, 1e-5);
    EXPECT_NEAR(evalScalar("x(2)"), -2.0, 1e-5);
    EXPECT_NEAR(evalScalar("fval"),  3.0, 1e-7);   // the bowl's floor
    EXPECT_EQ(static_cast<int>(evalScalar("ef")), 1);
}

// Rosenbrock -> [1 1].
TEST_F(FminuncTest, Rosenbrock) {
    eval("x = fminunc(@(x) 100*(x(2)-x(1)^2)^2 + (1-x(1))^2, [-1.2 1]);");
    EXPECT_NEAR(evalScalar("x(1)"), 1.0, 1e-4);
    EXPECT_NEAR(evalScalar("x(2)"), 1.0, 1e-4);
}

// Output orientation mirrors x0 (column x0 -> column minimiser).
TEST_F(FminuncTest, ColumnOrientation) {
    eval("x = fminunc(@(x) (x(1)-1)^2 + 2*(x(2)+2)^2, [0; 0]);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(x,1)")), 2);
    EXPECT_EQ(static_cast<int>(evalScalar("size(x,2)")), 1);
    EXPECT_NEAR(evalScalar("x(1)"),  1.0, 1e-5);
    EXPECT_NEAR(evalScalar("x(2)"), -2.0, 1e-5);
}

// A non-handle first argument is rejected.
TEST_F(FminuncTest, RejectsNonHandle) {
    EXPECT_THROW(eval("fminunc(5, 0);"), std::exception);
}
