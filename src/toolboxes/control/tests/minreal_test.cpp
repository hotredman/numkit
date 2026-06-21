// toolboxes/control/tests/minreal_test.cpp
//
// Minimal realization minreal(sys) — pole/zero cancellation.
// bugs/control/minreal.md. Reference values from MATLAB R2025b.
// tfdata('v') pads num to den length (MATLAB convention).

#include <numkit/bundle/standard_engine.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class MinrealTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// (s+1)/(s+1)^2 -> 1/(s+1).
TEST_F(MinrealTest, SimpleCancellation)
{
    eval("[n, d] = tfdata(minreal(tf([1 1], [1 2 1])), 'v');");
    EXPECT_EQ(static_cast<int>(evalScalar("numel(d)")), 2);   // first order
    EXPECT_NEAR(evalScalar("n(end)"), 1.0, 1e-10);
    EXPECT_NEAR(evalScalar("d(1)"),   1.0, 1e-10);
    EXPECT_NEAR(evalScalar("d(end)"), 1.0, 1e-10);
}

// (s+1)/((s+1)(s+2)) -> 1/(s+2).
TEST_F(MinrealTest, CancelOneOfTwoPoles)
{
    eval("[n, d] = tfdata(minreal(tf([1 1], [1 3 2])), 'v');");
    EXPECT_EQ(static_cast<int>(evalScalar("numel(d)")), 2);
    EXPECT_NEAR(evalScalar("n(end)"), 1.0, 1e-10);
    EXPECT_NEAR(evalScalar("d(end)"), 2.0, 1e-10);
}

// Gain is preserved: 2(s+1)/(s+1)^2 -> 2/(s+1).
TEST_F(MinrealTest, GainPreserved)
{
    eval("[n, d] = tfdata(minreal(tf(2*[1 1], [1 2 1])), 'v');");
    EXPECT_NEAR(evalScalar("n(end)"), 2.0, 1e-10);
    EXPECT_NEAR(evalScalar("d(end)"), 1.0, 1e-10);
}

// Nothing to cancel: 1/(s+1) stays 1/(s+1).
TEST_F(MinrealTest, NoCancellation)
{
    eval("[n, d] = tfdata(minreal(tf(1, [1 1])), 'v');");
    EXPECT_EQ(static_cast<int>(evalScalar("numel(d)")), 2);
    EXPECT_NEAR(evalScalar("n(end)"), 1.0, 1e-10);
    EXPECT_NEAR(evalScalar("d(end)"), 1.0, 1e-10);
}

// Complex-conjugate pair cancels cleanly (real result):
// (s^2+1)/((s^2+1)(s+3)) -> 1/(s+3).
TEST_F(MinrealTest, ComplexPairCancellation)
{
    eval("[n, d] = tfdata(minreal(tf([1 0 1], conv([1 0 1], [1 3]))), 'v');");
    EXPECT_EQ(static_cast<int>(evalScalar("numel(d)")), 2);
    EXPECT_NEAR(evalScalar("n(end)"), 1.0, 1e-9);
    EXPECT_NEAR(evalScalar("d(end)"), 3.0, 1e-9);
    EXPECT_NEAR(evalScalar("d(1)"),   1.0, 1e-9);
}

// SISO state space with an uncontrollable mode: order 2 -> 1.
TEST_F(MinrealTest, StateSpaceOrderReduction)
{
    eval("sysr = minreal(ss([-1 0; 0 -2], [1; 0], [1 1], 0));");
    EXPECT_EQ(static_cast<int>(evalScalar("size(sysr.A, 1)")), 1);
    eval("[n, d] = tfdata(sysr, 'v');");
    EXPECT_NEAR(evalScalar("n(end)"), 1.0, 1e-9);   // 1/(s+1)
    EXPECT_NEAR(evalScalar("d(end)"), 1.0, 1e-9);
}

// MIMO state space is a documented gap — clear error.
TEST_F(MinrealTest, MimoStateSpaceThrows)
{
    EXPECT_THROW(eval("minreal(ss([-1 0; 0 -2], [1 0; 0 1], [1 0; 0 1], zeros(2)));"),
                 std::exception);
}
