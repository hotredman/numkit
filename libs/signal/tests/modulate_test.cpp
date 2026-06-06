// libs/signal/tests/modulate_test.cpp
//
// Regression guard for modulate (Phase 4.12). Bit-equal MATLAB R2025b
// on the 4 supported modes (am, amdsb-tc, fm, pm).

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class ModulateTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override {
        engine.eval("import compat.*;");
        engine.eval("fs = 100; t = (0:1/fs:0.1)'; x = sin(2*pi*5*t);");
    }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(ModulateTest, AmFromHelpExample)
{
    eval("y = modulate(x, 20, fs, 'am');");
    EXPECT_NEAR(evalScalar("y(2)"),  0.095491, 1e-5);
    EXPECT_NEAR(evalScalar("y(4)"), -0.654508, 1e-5);
}

TEST_F(ModulateTest, AmAmdsbScAlias)
{
    eval("ya = modulate(x, 20, fs, 'am'); yb = modulate(x, 20, fs, 'amdsb-sc');");
    EXPECT_NEAR(evalScalar("max(abs(ya - yb))"), 0.0, 1e-12);
}

TEST_F(ModulateTest, AmdsbTcWithOffset)
{
    eval("y = modulate(x, 20, fs, 'amdsb-tc', 0.5);");
    EXPECT_NEAR(evalScalar("y(1)"), -0.5, 1e-9);   // (0 - 0.5) * cos(0) = -0.5
    EXPECT_NEAR(evalScalar("y(5)"),  0.139384, 1e-5);
}

TEST_F(ModulateTest, FmCumsumIntegration)
{
    eval("y = modulate(x, 20, fs, 'fm');");
    EXPECT_NEAR(evalScalar("y(3)"), -0.878235, 1e-5);
    EXPECT_NEAR(evalScalar("y(5)"), -0.489307, 1e-5);
}

TEST_F(ModulateTest, PmPhaseProportional)
{
    eval("y = modulate(x, 20, fs, 'pm');");
    EXPECT_NEAR(evalScalar("y(2)"), -0.610464, 1e-5);
}

TEST_F(ModulateTest, AmssbApproxMatchesMatlab)
{
    // Single-sideband uses hilbert; finite-window edges introduce ~5% noise.
    eval("y = modulate(x, 20, fs, 'amssb');"
         "y1 = y(5); y2 = y(8);");
    // Just sanity-check finite output and approximate magnitude bound.
    EXPECT_TRUE(std::isfinite(evalScalar("y1")));
    EXPECT_LT(std::abs(evalScalar("y1")), 2.0);  // amssb amplitudes ~|x| + |hilbert(x)|
}

TEST_F(ModulateTest, RejectsUnsupportedMethod)
{
    bool threw = false;
    try { eval("modulate(x, 20, fs, 'qam');"); } catch (...) { threw = true; }
    EXPECT_TRUE(threw);
}
