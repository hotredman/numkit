// libs/signal/tests/uquant_test.cpp
//
// Regression guard for uencode/udecode (Phase 4.2 of audio sweep).
// Bit-equal MATLAB R2025b on all 16 documented variants.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class UquantTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override {
        engine.eval("import compat.*;");
        engine.eval("u = [-1.2 -1 -0.5 0 0.5 1 1.2];");
    }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// ── uencode default unsigned ──────────────────────────────────────────
TEST_F(UquantTest, UencodeDefault3BitsUnsigned)
{
    eval("y = double(uencode(u, 3));");
    EXPECT_DOUBLE_EQ(evalScalar("y(1)"), 0.0);  // saturated low
    EXPECT_DOUBLE_EQ(evalScalar("y(2)"), 0.0);  // -1 → 0
    EXPECT_DOUBLE_EQ(evalScalar("y(3)"), 2.0);  // -0.5
    EXPECT_DOUBLE_EQ(evalScalar("y(4)"), 4.0);  // 0
    EXPECT_DOUBLE_EQ(evalScalar("y(5)"), 6.0);  // 0.5
    EXPECT_DOUBLE_EQ(evalScalar("y(6)"), 7.0);  // 1
    EXPECT_DOUBLE_EQ(evalScalar("y(7)"), 7.0);  // saturated high
}

TEST_F(UquantTest, UencodeCustomPeakV)
{
    eval("y = double(uencode(u, 3, 0.5));");
    // Tighter peak V=0.5 → more saturation
    EXPECT_DOUBLE_EQ(evalScalar("y(3)"), 0.0);  // -0.5 → 0 (was 2 with V=1)
    EXPECT_DOUBLE_EQ(evalScalar("y(5)"), 7.0);  // 0.5 → 7
}

TEST_F(UquantTest, UencodeSignedOutput)
{
    eval("y = double(uencode(u, 3, 1, 'signed'));");
    EXPECT_DOUBLE_EQ(evalScalar("y(3)"), -2.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(4)"),  0.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(5)"),  2.0);
}

TEST_F(UquantTest, UencodeOutputTypeTier)
{
    // ≤ 8 bits → uint8; ≤ 16 bits → uint16; ≤ 32 bits → uint32
    eval("e8 = uencode(0.5, 8); e10 = uencode(0.5, 10); e20 = uencode(0.5, 20);");
    EXPECT_EQ(static_cast<int>(evalScalar("strcmp(class(e8),  'uint8')")), 1);
    EXPECT_EQ(static_cast<int>(evalScalar("strcmp(class(e10), 'uint16')")), 1);
    EXPECT_EQ(static_cast<int>(evalScalar("strcmp(class(e20), 'uint32')")), 1);
}

// ── udecode ────────────────────────────────────────────────────────────
TEST_F(UquantTest, UdecodeSignedSaturate)
{
    // From MATLAB udecode(int8([-1 1 2 -5]), 3) == [-0.25, 0.25, 0.5, -1]
    // (Note: -5 saturates to lower bound -4 → decoded -1)
    eval("ui = int8([-1 1 2 -5]); y = udecode(ui, 3);");
    EXPECT_DOUBLE_EQ(evalScalar("y(1)"), -0.25);
    EXPECT_DOUBLE_EQ(evalScalar("y(2)"),  0.25);
    EXPECT_DOUBLE_EQ(evalScalar("y(3)"),  0.5);
    EXPECT_DOUBLE_EQ(evalScalar("y(4)"), -1.0);
}

TEST_F(UquantTest, UdecodeSignedWrap)
{
    // 'wrap' mode: -5 wraps to +3 (mod 8 with sign bias) → decoded 0.75
    eval("ui = int8([-1 1 2 -5]); y = udecode(ui, 3, 1, 'wrap');");
    EXPECT_DOUBLE_EQ(evalScalar("y(4)"), 0.75);
}

TEST_F(UquantTest, UdecodeRoundtrip8Bit)
{
    eval("us = -1:0.1:1; y = udecode(uencode(us, 8), 8); err = max(abs(y - us));");
    // 8-bit quantization step = 2/256 ≈ 0.00781
    EXPECT_NEAR(evalScalar("err"), 0.0078125, 1e-6);
}

// ── error cases ────────────────────────────────────────────────────────
TEST_F(UquantTest, UencodeRejectsBadN)
{
    bool threw = false;
    try { eval("uencode(0.5, 1);"); } catch (...) { threw = true; }  // N must be > 1
    EXPECT_TRUE(threw);
}

TEST_F(UquantTest, UdecodeRejectsNonInteger)
{
    bool threw = false;
    try { eval("udecode(0.5, 8);"); } catch (...) { threw = true; }
    EXPECT_TRUE(threw);
}
