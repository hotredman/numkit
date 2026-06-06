// libs/signal/tests/vco_test.cpp
//
// Regression guard for vco (Phase 4.8). Bit-equal MATLAB R2025b.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class VcoTest : public ::testing::Test
{
public:
    StdEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(VcoTest, ZeroInputProducesPureCarrier)
{
    // x = zeros(8,1), Fc=1, Fs=4 → cos(2π·t/4·k), period = 4 samples.
    eval("y = vco(zeros(8, 1), 1, 4);");
    EXPECT_NEAR(evalScalar("y(1)"),  1.0, 1e-9);
    EXPECT_NEAR(evalScalar("y(3)"), -1.0, 1e-9);
    EXPECT_NEAR(evalScalar("y(5)"),  1.0, 1e-9);
    EXPECT_NEAR(evalScalar("y(7)"), -1.0, 1e-9);
}

TEST_F(VcoTest, ConstantOffsetSpeedsUp)
{
    // x = 0.5 * ones(8,1), Fc=1, Fs=8.
    eval("y = vco(0.5 * ones(8, 1), 1, 8);");
    EXPECT_NEAR(evalScalar("y(1)"),  0.92388,  1e-5);
    EXPECT_NEAR(evalScalar("y(5)"),  0.38268,  1e-5);
}

TEST_F(VcoTest, RangeVectorFminFmax)
{
    eval("y = vco(linspace(-1, 1, 16)', [0.1 0.4]*16, 16);");
    EXPECT_NEAR(evalScalar("y(1)"), 0.587785, 1e-5);
    EXPECT_NEAR(evalScalar("y(8)"), 0.770513, 1e-5);
}

TEST_F(VcoTest, RejectsXOutOfRange)
{
    bool threw = false;
    try { eval("vco([0; 1.5; 0], 1, 8);"); } catch (...) { threw = true; }
    EXPECT_TRUE(threw);
}
