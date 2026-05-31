// libs/control/tests/response_xtraj_test.cpp
//
// step / impulse / lsim third output (state trajectory x).
// State values are realization-dependent, so tests use explicit ss()
// systems (fixed realization) and check against MATLAB R2025b.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class ResponseXTrajTest : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(ResponseXTrajTest, StepStateTrajectory)
{
    eval("function [a,b,c] = stp(sys, t)\n"
         "  [a,b,c] = step(sys, t);\n"
         "end");
    eval("sys = ss([-2 -1; 1 0], [1; 0], [0 1], 0); t = (0:0.5:2)';"
         "[y, tt, x] = stp(sys, t);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(x,1)")), 5);
    EXPECT_EQ(static_cast<int>(evalScalar("size(x,2)")), 2);
    // MATLAB R2025b reference values.
    EXPECT_NEAR(evalScalar("x(2,1)"), 0.30326532985631671, 1e-9);
    EXPECT_NEAR(evalScalar("x(3,1)"), 0.36787944117144233, 1e-9);
    EXPECT_NEAR(evalScalar("x(5,2)"), 0.59399415029016178, 1e-9);
    // Output y equals C*x (here C = [0 1] → y = state 2).
    EXPECT_NEAR(evalScalar("max(abs(y(:) - x(:,2)))"), 0.0, 1e-12);
}

TEST_F(ResponseXTrajTest, LsimStateTrajectory)
{
    eval("function [a,b,c] = lsm(sys, u, t)\n"
         "  [a,b,c] = lsim(sys, u, t);\n"
         "end");
    eval("sys = ss([-2 -1; 1 0], [1; 0], [0 1], 0); t = (0:0.5:2)'; u = ones(size(t));"
         "[y, tt, x] = lsm(sys, u, t);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(x,1)")), 5);
    EXPECT_EQ(static_cast<int>(evalScalar("size(x,2)")), 2);
    // Unit-input lsim equals the step response.
    EXPECT_NEAR(evalScalar("max(abs(y(:) - x(:,2)))"), 0.0, 1e-12);
    EXPECT_NEAR(evalScalar("x(2,1)"), 0.30326532985631671, 1e-9);
    EXPECT_EQ(static_cast<int>(evalScalar("numel(tt)")), 5);
}

TEST_F(ResponseXTrajTest, ImpulseStateTrajectoryShape)
{
    eval("function [a,b,c] = imp(sys, t)\n"
         "  [a,b,c] = impulse(sys, t);\n"
         "end");
    eval("sys = ss([-2 -1; 1 0], [1; 0], [0 1], 0); t = (0:0.5:2)';"
         "[y, tt, x] = imp(sys, t);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(x,1)")), 5);
    EXPECT_EQ(static_cast<int>(evalScalar("size(x,2)")), 2);
}
