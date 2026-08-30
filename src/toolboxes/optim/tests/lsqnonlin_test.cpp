// lsqnonlin_test.cpp — nonlinear least squares (lsqnonlin / lsqcurvefit),
// embedded-.m Levenberg-Marquardt.
//
// lsqnonlin(fun, p0) minimises ‖F(p)‖²; lsqcurvefit(fun, p0, x, y) =
// lsqnonlin(@(p) fun(p,x)-y, p0). Parity with MATLAB is on the solution
// (well-determined params + the resnorm). Verified vs MATLAB R2025b.
// Fixes bugs/optim/nonlinear-lsq.md.

#include <numkit/bundle/standard_engine.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

class LsqnonlinTest : public ::testing::Test {
public:
    numkit::StandardEngine engine;
    void SetUp() override {}
    numkit::Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// Exact linear residual: F = [p1-1; p2-2] -> [1 2], resnorm 0.
TEST_F(LsqnonlinTest, LinearResidual) {
    eval("[p, resnorm, residual, ef] = lsqnonlin(@(p) [p(1)-1; p(2)-2], [0 0]);");
    EXPECT_NEAR(evalScalar("p(1)"), 1.0, 1e-6);
    EXPECT_NEAR(evalScalar("p(2)"), 2.0, 1e-6);
    EXPECT_LT(evalScalar("resnorm"), 1e-12);
    EXPECT_EQ(static_cast<int>(evalScalar("ef")), 1);
    EXPECT_EQ(static_cast<int>(evalScalar("size(residual,1)")), 2);  // column
    EXPECT_EQ(static_cast<int>(evalScalar("size(residual,2)")), 1);
}

// Rosenbrock residual -> [1 1].
TEST_F(LsqnonlinTest, RosenbrockResidual) {
    eval("p = lsqnonlin(@(x) [10*(x(2)-x(1)^2); 1-x(1)], [-1.2 1]);");
    EXPECT_NEAR(evalScalar("p(1)"), 1.0, 1e-6);
    EXPECT_NEAR(evalScalar("p(2)"), 1.0, 1e-6);
}

// lsqcurvefit on a*exp(b*x): the minimum is in a flat valley, so the resnorm
// is the tightly-determined quantity (params loosely so). MATLAB resnorm =
// 0.001248164767.
TEST_F(LsqnonlinTest, CurvefitExpResnorm) {
    eval("[p, resnorm] = lsqcurvefit(@(p,xd) p(1)*exp(p(2)*xd), [1 -1], "
         "[0 1 2], [1 0.5 0.2]);");
    EXPECT_NEAR(evalScalar("resnorm"), 0.001248164767, 1e-8);
    EXPECT_NEAR(evalScalar("p(1)"),  1.0057, 1e-2);
    EXPECT_NEAR(evalScalar("p(2)"), -0.7480, 1e-2);
}

// lsqcurvefit recovers exact params from noise-free data: 2*sin(1.5*x).
TEST_F(LsqnonlinTest, CurvefitSinExact) {
    eval("xd = [0 0.5 1 1.5 2]; yd = 2*sin(1.5*xd);");
    eval("p = lsqcurvefit(@(p,x) p(1)*sin(p(2)*x), [1 1], xd, yd);");
    EXPECT_NEAR(evalScalar("p(1)"), 2.0, 1e-5);
    EXPECT_NEAR(evalScalar("p(2)"), 1.5, 1e-5);
}

// lsqcurvefit(fun,p0,x,y) == lsqnonlin(@(p) fun(p,x)-y, p0).
TEST_F(LsqnonlinTest, CurvefitEqualsNonlin) {
    eval("xd = [0 1 2 3]; yd = [2 1.2 0.7 0.45];");
    eval("a = lsqcurvefit(@(p,x) p(1)*exp(p(2)*x), [2 -0.5], xd, yd);");
    eval("b = lsqnonlin(@(p) p(1)*exp(p(2)*xd) - yd, [2 -0.5]);");
    EXPECT_LT(evalScalar("max(abs(a - b))"), 1e-10);
}

// Output orientation mirrors p0 (column p0 -> column p).
TEST_F(LsqnonlinTest, ColumnOrientation) {
    eval("p = lsqnonlin(@(p) [p(1)-1; p(2)-2], [0; 0]);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(p,1)")), 2);
    EXPECT_EQ(static_cast<int>(evalScalar("size(p,2)")), 1);
}

// Bound constraints are deferred -> a clear error.
TEST_F(LsqnonlinTest, BoundsRejected) {
    EXPECT_THROW(eval("lsqnonlin(@(p) p, [1 1], [0 0], [2 2]);"), std::exception);
}
