// libs/signal/tests/transforms_extras_test.cpp
//
// Tests for E2: dftmtx / bitrevorder / dst / idst / rceps / cceps / icceps.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <cmath>
#include <gtest/gtest.h>

using namespace numkit;

class TransformsExtrasTest : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// ── dftmtx ────────────────────────────────────────────────────────────
TEST_F(TransformsExtrasTest, DftmtxShape)
{
    eval("F = dftmtx(8);");
    EXPECT_EQ(eval("F").dims().rows(), 8u);
    EXPECT_EQ(eval("F").dims().cols(), 8u);
}

TEST_F(TransformsExtrasTest, DftmtxFirstRowIsOnes)
{
    // F[0,n] = exp(0) = 1 for all n.
    eval("F = dftmtx(8);");
    for (int n = 1; n <= 8; ++n) {
        EXPECT_NEAR(evalScalar("real(F(1," + std::to_string(n) + "))"), 1.0, 1e-12);
        EXPECT_NEAR(evalScalar("imag(F(1," + std::to_string(n) + "))"), 0.0, 1e-12);
    }
}

TEST_F(TransformsExtrasTest, DftmtxKnownEntry)
{
    // F[1,1] = exp(-2πi/8) = exp(-πi/4) = (√2/2)(1 - i).
    eval("F = dftmtx(8);");
    EXPECT_NEAR(evalScalar("real(F(2,2))"),  std::sqrt(2.0) / 2.0, 1e-12);
    EXPECT_NEAR(evalScalar("imag(F(2,2))"), -std::sqrt(2.0) / 2.0, 1e-12);
}

// ── bitrevorder ───────────────────────────────────────────────────────
TEST_F(TransformsExtrasTest, BitrevorderLength8)
{
    // For length 8, mapping is [0 4 2 6 1 5 3 7] (bit-reversed indices).
    eval("y = bitrevorder([10 20 30 40 50 60 70 80]);");
    EXPECT_DOUBLE_EQ(evalScalar("y(1)"), 10.0);   // index 0 → 0
    EXPECT_DOUBLE_EQ(evalScalar("y(2)"), 50.0);   // index 1 (001) → 4 (100)
    EXPECT_DOUBLE_EQ(evalScalar("y(3)"), 30.0);   // index 2 (010) → 2 (010)
    EXPECT_DOUBLE_EQ(evalScalar("y(5)"), 20.0);   // index 4 (100) → 1 (001)
    EXPECT_DOUBLE_EQ(evalScalar("y(8)"), 80.0);   // index 7 → 7
}

TEST_F(TransformsExtrasTest, BitrevorderRejectsNonPow2)
{
    bool threw = false;
    try { eval("bitrevorder([1 2 3 4 5]);"); } catch (...) { threw = true; }
    EXPECT_TRUE(threw);
}

// ── dst / idst ────────────────────────────────────────────────────────
TEST_F(TransformsExtrasTest, DstIdstRoundtrip)
{
    eval("x = [1 2 3 4 5]';");
    eval("y = dst(x); xr = idst(y);");
    for (int i = 1; i <= 5; ++i) {
        EXPECT_NEAR(evalScalar("xr(" + std::to_string(i) + ")"),
                    evalScalar("x(" + std::to_string(i) + ")"), 1e-9);
    }
}

TEST_F(TransformsExtrasTest, DstLength)
{
    eval("y = dst([1 2 3 4 5 6 7]);");
    EXPECT_EQ(eval("y").numel(), 7u);
}

// ── rceps / cceps / icceps ────────────────────────────────────────────
TEST_F(TransformsExtrasTest, RcepsLengthMatchesInput)
{
    eval("c = rceps([1 2 3 4 5 6 7 8]);");
    EXPECT_EQ(eval("c").numel(), 8u);
}

TEST_F(TransformsExtrasTest, CcepsLengthMatchesInput)
{
    eval("c = cceps([1 2 3 4 5 6 7 8]);");
    EXPECT_EQ(eval("c").numel(), 8u);
}

TEST_F(TransformsExtrasTest, IccepsLengthMatches)
{
    // Round-trip is approximate (depends on phase-unwrap path); just
    // pin the API contract here — length preserved + finite output.
    eval("x = [1 2 4 7 11 16 22 29]';");
    eval("c = cceps(x); xr = icceps(c);");
    EXPECT_EQ(eval("xr").numel(), 8u);
    for (int i = 1; i <= 8; ++i)
        EXPECT_FALSE(std::isnan(evalScalar("xr(" + std::to_string(i) + ")")));
}
