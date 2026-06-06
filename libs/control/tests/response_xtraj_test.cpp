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
    StdEngine engine;
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

// stepinfo returns MATLAB R2025b's full 9-field struct, including the
// TransientTime field (2nd, between RiseTime and SettlingTime) that numkit
// previously omitted. TransientTime = last time |y-yfinal| exceeds 2% of the
// PEAK deviation max|y(t)-yfinal|; SettlingTime uses 2% of |yinit-yfinal|.
// For a standard step the peak deviation occurs at t=0 = |yfinal|, so
// TransientTime == SettlingTime. The absolute time is grid-resolution
// dependent (pre-existing ~0.8% gap vs MATLAB shared with SettlingTime), so
// we pin the field set, the field position, and the TransientTime==SettlingTime
// invariant rather than MATLAB's absolute seconds.
TEST_F(ResponseXTrajTest, StepinfoTransientTimeField)
{
    eval("info = stepinfo(tf(1, [1 1])); fn = fieldnames(info);");
    // 9 fields, TransientTime present and positioned 2nd.
    EXPECT_EQ(static_cast<int>(evalScalar("double(numel(fn))")), 9);
    EXPECT_DOUBLE_EQ(evalScalar("isfield(info, 'TransientTime')"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("strcmp(fn{2}, 'TransientTime')"), 1.0);
    // Invariant: for this 1st-order step TransientTime == SettlingTime > 0.
    EXPECT_GT(evalScalar("info.TransientTime"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(info.TransientTime == info.SettlingTime)"), 1.0);
}

// A 2nd-order underdamped system overshoots; TransientTime still tracks
// SettlingTime when the peak deviation occurs at t=0 (= |yfinal|).
TEST_F(ResponseXTrajTest, StepinfoTransientTimeOvershoot)
{
    eval("info = stepinfo(tf(1, [1 0.4 1]));");
    EXPECT_EQ(static_cast<int>(evalScalar("double(numel(fieldnames(info)))")), 9);
    EXPECT_GT(evalScalar("info.Overshoot"), 0.0);          // underdamped → overshoot
    EXPECT_GT(evalScalar("info.TransientTime"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(info.TransientTime == info.SettlingTime)"), 1.0);
}
