// libs/signal/tests/envelope_test.cpp
// Audit ТЗ closure for envelope (partial — spline-peak deferred).
// Closes audit/findings/signal/envelope.md.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class EnvelopeTest : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override {
        engine.eval("import compat.*;");
        engine.eval("sig = sin(2*pi*0.1*(0:31)') .* exp(-0.05*(0:31)');");
    }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// 2026-05-08 — gap closure: 2-output form populates ylower (was empty).
TEST_F(EnvelopeTest, TwoOutputFormPopulatesYLower)
{
    eval("[up, lo] = envelope(sig);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(up)"), 32.0);
    EXPECT_DOUBLE_EQ(evalScalar("numel(lo)"), 32.0);
}

TEST_F(EnvelopeTest, LowerIsNegativeOfUpper)
{
    // numkit uses FFT analytic-signal envelope: ylower = -yupper.
    // (MATLAB's default uses spline-peak — DEFERRED.)
    eval("[up, lo] = envelope(sig);");
    EXPECT_LT(evalScalar("max(abs(lo - (-up)))"), 1e-12);
}

TEST_F(EnvelopeTest, OneOutputFormStillWorks)
{
    eval("env = envelope(sig);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(env)"), 32.0);
    // First-element envelope value at sample 0 of the analytic signal.
    EXPECT_NEAR(evalScalar("env(1)"), 0.4365, 1e-3);
}

// gap closure: extra args (filter length / mode) throw clear error
// instead of silently returning the 1-output result.
TEST_F(EnvelopeTest, FilterLengthArgRejected)
{
    bool threw = false;
    try { eval("envelope(sig, 8);"); }
    catch (const std::exception &) { threw = true; }
    EXPECT_TRUE(threw);
}

TEST_F(EnvelopeTest, ModeArgRejected)
{
    bool threw = false;
    try { eval("envelope(sig, 32, 'analytic');"); }
    catch (const std::exception &) { threw = true; }
    EXPECT_TRUE(threw);
}
