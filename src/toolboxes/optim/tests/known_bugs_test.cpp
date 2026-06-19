// toolboxes/optim/tests/known_bugs_test.cpp
//
// One DISABLED_ test per OPEN bug in bugs/optim/*.md. Disabled until fixed;
// remove `DISABLED_` to turn into a live regression guard. MATLAB R2025b.

#include <numkit/core/engine.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class OptimKnownBug : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// bugs/optim/nonlinear-lsq.md — lsqcurvefit recovers a*exp(b*x) params (FIXED).
TEST_F(OptimKnownBug, Lsqcurvefit)
{
    eval("p = lsqcurvefit(@(p,xd) p(1)*exp(p(2)*xd), [2 -0.5], "
         "[0 1 2 3], [2 1.2 0.7 0.45]);");
    EXPECT_NEAR(evalScalar("p(1)"),  2.0,  0.05);
    EXPECT_NEAR(evalScalar("p(2)"), -0.5,  0.05);
}

// bugs/optim/nonlinear-lsq.md — lsqnonlin minimizes a residual to its root (FIXED).
TEST_F(OptimKnownBug, Lsqnonlin)
{
    eval("p = lsqnonlin(@(p) [p(1)-1; p(2)-2], [0 0]);");
    EXPECT_NEAR(evalScalar("p(1)"), 1.0, 1e-4);
    EXPECT_NEAR(evalScalar("p(2)"), 2.0, 1e-4);
}

// bugs/optim/constrained-solvers.md — fminunc on a parabola.
TEST_F(OptimKnownBug, DISABLED_Fminunc)
{
    eval("x = fminunc(@(x) (x-3)^2, 0);");
    EXPECT_NEAR(evalScalar("x"), 3.0, 1e-4);
}

// bugs/optim/constrained-solvers.md — quadprog: min 0.5 x'x - [1 1]x -> [1 1].
TEST_F(OptimKnownBug, DISABLED_Quadprog)
{
    eval("x = quadprog(eye(2), [-1 -1]);");
    EXPECT_NEAR(evalScalar("x(1)"), 1.0, 1e-4);
    EXPECT_NEAR(evalScalar("x(2)"), 1.0, 1e-4);
}

// bugs/optim/constrained-solvers.md — linprog with lower-bound constraints.
TEST_F(OptimKnownBug, DISABLED_Linprog)
{
    eval("x = linprog([1 1], [-1 0; 0 -1], [-1; -1]);");  // x1>=1, x2>=1
    EXPECT_NEAR(evalScalar("x(1)"), 1.0, 1e-4);
    EXPECT_NEAR(evalScalar("x(2)"), 1.0, 1e-4);
}

// bugs/optim/constrained-solvers.md — fmincon on a bounded parabola -> [0 0].
TEST_F(OptimKnownBug, DISABLED_Fmincon)
{
    eval("x = fmincon(@(x) x(1)^2+x(2)^2, [1 1], [],[],[],[], [0 0],[2 2]);");
    EXPECT_NEAR(evalScalar("x(1)"), 0.0, 1e-3);
    EXPECT_NEAR(evalScalar("x(2)"), 0.0, 1e-3);
}

// bugs/optim/fsolve.md — nonlinear system solver (FIXED, promoted live).
TEST_F(OptimKnownBug, Fsolve)
{
    EXPECT_NEAR(evalScalar("fsolve(@(x) x^2 - 2, 1)"), 1.41421356237469, 1e-6);
    eval("xv = fsolve(@(v) [v(1)^2+v(2)^2-1; v(1)-v(2)], [0.5 0.5]);");
    EXPECT_NEAR(evalScalar("xv(1)"), 0.70710678118, 1e-5);
    EXPECT_NEAR(evalScalar("xv(2)"), 0.70710678118, 1e-5);
    // 3-variable system (multiple roots; converges to [1 2 3] like MATLAB).
    eval("x3 = fsolve(@(x)[x(1)+x(2)+x(3)-6; x(1)^2+x(2)^2+x(3)^2-14; x(1)*x(2)*x(3)-6], [1 0 4]);");
    EXPECT_NEAR(evalScalar("x3(1)"), 1.0, 1e-5);
    EXPECT_NEAR(evalScalar("x3(2)"), 2.0, 1e-5);
    EXPECT_NEAR(evalScalar("x3(3)"), 3.0, 1e-5);
}
