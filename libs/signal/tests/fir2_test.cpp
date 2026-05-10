// libs/signal/tests/fir2_test.cpp
//
// Regression guard for fir2 (Phase 4.9). Bit-equal MATLAB R2025b.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class Fir2Test : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(Fir2Test, LowpassBitEqual)
{
    eval("b = fir2(20, [0 0.4 0.5 1], [1 1 0 0]);");
    EXPECT_EQ(static_cast<int>(evalScalar("numel(b)")), 21);
    // Peak at center (impulse-response shifted to (N-1)/2 = 10 → 1-based 11).
    EXPECT_NEAR(evalScalar("b(11)"), 0.449219, 1e-5);
    EXPECT_NEAR(evalScalar("b(10)"), 0.305990, 1e-5);
    EXPECT_NEAR(evalScalar("b(12)"), 0.305990, 1e-5);
    // Symmetric (linear-phase FIR).
    EXPECT_NEAR(evalScalar("b(1)"), evalScalar("b(end)"), 1e-12);
    EXPECT_NEAR(evalScalar("b(1)"), 0.001659, 1e-5);
}

TEST_F(Fir2Test, BandpassMultiband)
{
    eval("b = fir2(30, [0 0.2 0.3 0.6 0.7 1], [0 0 1 1 0 0]);");
    EXPECT_EQ(static_cast<int>(evalScalar("numel(b)")), 31);
    EXPECT_NEAR(evalScalar("b(16)"), 0.401367, 1e-5);
    // First and last symmetric.
    EXPECT_NEAR(evalScalar("b(1)"), evalScalar("b(end)"), 1e-12);
}

TEST_F(Fir2Test, HighpassBitEqual)
{
    eval("b = fir2(20, [0 0.5 0.6 1], [0 0 1 1]);");
    EXPECT_EQ(static_cast<int>(evalScalar("numel(b)")), 21);
    EXPECT_NEAR(evalScalar("b(11)"), 0.451172, 1e-5);
    EXPECT_NEAR(evalScalar("b(1)"),  0.001658, 1e-5);
}

TEST_F(Fir2Test, RejectsBadFEdges)
{
    bool threw = false;
    try { eval("fir2(10, [0.1 0.5 1], [1 1 0]);"); } catch (...) { threw = true; }
    EXPECT_TRUE(threw);
}
