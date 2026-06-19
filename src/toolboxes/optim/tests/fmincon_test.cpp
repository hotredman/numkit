// fmincon_test.cpp — constrained minimization (embedded-.m SQP over quadprog).
//
// min f(x) s.t. A x <= b, Aeq x = beq, lb <= x <= ub. Parity with MATLAB is
// on the solution (the minimiser). Nonlinear constraints (nonlcon) are
// rejected (blocked by the VM multi-output-handle limitation). Verified vs
// MATLAB R2025b. Fixes bugs/optim/fmincon.md.

#include <numkit/bundle/standard_engine.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

class FminconTest : public ::testing::Test {
public:
    numkit::StandardEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    numkit::Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// Bounds only: min x1^2+x2^2 s.t. 0<=x<=2 -> [0 0] (the documented repro).
TEST_F(FminconTest, BoundsOnly) {
    eval("[x, fval, ef] = fmincon(@(x) x(1)^2+x(2)^2, [1 1], [],[],[],[], [0 0],[2 2]);");
    EXPECT_NEAR(evalScalar("x(1)"), 0.0, 1e-5);
    EXPECT_NEAR(evalScalar("x(2)"), 0.0, 1e-5);
    EXPECT_NEAR(evalScalar("fval"), 0.0, 1e-8);
    EXPECT_EQ(static_cast<int>(evalScalar("ef")), 1);
}

// Linear inequality x1+x2<=2 pulls the far objective center to [1 1].
TEST_F(FminconTest, LinearInequality) {
    eval("x = fmincon(@(x) (x(1)-2)^2+(x(2)-2)^2, [0 0], [1 1], 2);");
    EXPECT_NEAR(evalScalar("x(1)"), 1.0, 1e-5);
    EXPECT_NEAR(evalScalar("x(2)"), 1.0, 1e-5);
}

// Equality x1+x2=2 -> [1 1].
TEST_F(FminconTest, Equality) {
    eval("x = fmincon(@(x) x(1)^2+x(2)^2, [2 0], [],[], [1 1], 2);");
    EXPECT_NEAR(evalScalar("x(1)"), 1.0, 1e-5);
    EXPECT_NEAR(evalScalar("x(2)"), 1.0, 1e-5);
}

// Objective minimum outside the box clamps to the box corner -> [2 0].
TEST_F(FminconTest, BoundedCorner) {
    eval("x = fmincon(@(x) (x(1)-3)^2 + (x(2)+1)^2, [0 0], [],[],[],[], [0 0],[2 2]);");
    EXPECT_NEAR(evalScalar("x(1)"), 2.0, 1e-5);
    EXPECT_NEAR(evalScalar("x(2)"), 0.0, 1e-5);
}

// Output orientation mirrors x0 (column x0 -> column minimiser).
TEST_F(FminconTest, ColumnOrientation) {
    eval("x = fmincon(@(x) x(1)^2+x(2)^2, [1;1], [],[],[],[], [0;0],[2;2]);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(x,1)")), 2);
    EXPECT_EQ(static_cast<int>(evalScalar("size(x,2)")), 1);
}

// Nonlinear constraints are rejected (VM multi-output-handle limitation).
TEST_F(FminconTest, NonlconRejected) {
    EXPECT_THROW(
        eval("fmincon(@(x) x(1)+x(2), [0.5 0.5], [],[],[],[],[],[], @(x) deal(x(1)^2-1, []));"),
        std::exception);
}
