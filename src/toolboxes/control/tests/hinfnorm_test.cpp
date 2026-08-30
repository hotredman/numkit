// toolboxes/control/tests/hinfnorm_test.cpp
//
// H-infinity norm (Bruinsma–Steinbuch Hamiltonian bisection).
// bugs/control/hinfnorm.md. Reference values from MATLAB R2025b.
// One TEST_F per documented branch/edge.

#include <numkit/bundle/standard_engine.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

#include <cmath>

using namespace numkit;

class HinfnormTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override {}
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// Peak at ω=0: G(s)=1/(s+1), |G(0)|=1.
TEST_F(HinfnormTest, FirstOrderDcPeak)
{
    EXPECT_NEAR(evalScalar("hinfnorm(ss(-1, 1, 1, 0))"), 1.0, 1e-6);
}

// Lightly-damped resonance 1/(s^2+0.1s+1): peak ≈ 10 near ω=1.
// This is the case a frequency grid would miss between samples.
TEST_F(HinfnormTest, Resonance)
{
    EXPECT_NEAR(evalScalar("hinfnorm(ss([0 1; -1 -0.1], [0; 1], [1 0], 0))"),
                10.012523, 1e-4);
}

// Static peak (no resonance): 1/(s+2)+1/(s+3) at ω=0 = 1/2 + 1/3.
TEST_F(HinfnormTest, StaticPeak)
{
    EXPECT_NEAR(evalScalar("hinfnorm(ss([-2 0; 0 -3], [1; 1], [1 1], 0))"),
                0.83333333, 1e-6);
}

// Non-zero feedthrough D: 0.5 + 1/(s+1), peak 1.5 at ω=0.
TEST_F(HinfnormTest, WithFeedthrough)
{
    EXPECT_NEAR(evalScalar("hinfnorm(ss(-1, 1, 1, 0.5))"), 1.5, 1e-6);
}

// Marginally stable / jω-axis poles (±i) → Inf.
TEST_F(HinfnormTest, MarginalIsInf)
{
    EXPECT_TRUE(std::isinf(evalScalar(
        "hinfnorm(ss([0 1; -1 0], [0; 1], [1 0], 0))")));
}

// Unstable pole (Re > 0) → Inf.
TEST_F(HinfnormTest, UnstableIsInf)
{
    EXPECT_TRUE(std::isinf(evalScalar("hinfnorm(ss(1, 1, 1, 0))")));
}

// tf input path: 1/(s+1)^2, peak 1 at ω=0.
TEST_F(HinfnormTest, TfInput)
{
    EXPECT_NEAR(evalScalar("hinfnorm(tf(1, [1 2 1]))"), 1.0, 1e-6);
}

// Discrete-time is a documented gap — clear error, not a wrong number.
TEST_F(HinfnormTest, DiscreteThrows)
{
    EXPECT_THROW(eval("hinfnorm(ss(0.5, 1, 1, 0, 0.1));"), std::exception);
}
