// toolboxes/control/tests/covar_test.cpp
//
// covar(sys, W) — steady-state output (P) + state (Q) covariance under
// white noise of intensity W. bugs/control/covar.md. Reference values from
// MATLAB R2025b.

#include <numkit/bundle/standard_engine.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class CovarTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// 1st-order: closed form P = B^2 W /(2|a|) C^2 = 1/(2·1) = 0.5.
TEST_F(CovarTest, FirstOrderClosedForm)
{
    EXPECT_NEAR(evalScalar("covar(ss(-1, 1, 1, 0), 1)"), 0.5, 1e-10);
}

// Noise intensity scales the output covariance linearly.
TEST_F(CovarTest, IntensityScalesLinearly)
{
    EXPECT_NEAR(evalScalar("covar(ss(-1, 1, 1, 0), 4)"), 2.0, 1e-10);
}

// Two-state: P = C·Q·Cᵀ, Q the gramian Lyapunov solution.
TEST_F(CovarTest, TwoStatePandQ)
{
    eval("[P, Q] = covar(ss([-1 0; 0 -2], [1; 1], [1 1], 0), 1);");
    EXPECT_NEAR(evalScalar("P"),      1.41666666666667, 1e-9);
    EXPECT_NEAR(evalScalar("Q(1,1)"), 0.5,              1e-10);
    EXPECT_NEAR(evalScalar("Q(1,2)"), 1.0 / 3.0,        1e-10);
    EXPECT_NEAR(evalScalar("Q(2,2)"), 0.25,             1e-10);
}

// Discrete: A·Q·Aᵀ − Q + B·W·Bᵀ = 0; P = C·Q·Cᵀ.
TEST_F(CovarTest, Discrete)
{
    EXPECT_NEAR(evalScalar("covar(ss([0.5 0; 0 0.3], [1; 1], [1 1], 0, 0.1), 2)"),
                9.57035111, 1e-6);
}

// Continuous with a direct feedthrough D≠0 → infinite output covariance.
TEST_F(CovarTest, ContinuousFeedthroughIsInf)
{
    EXPECT_TRUE(std::isinf(evalScalar("covar(ss(-1, 1, 1, 0.5), 1)")));
}
