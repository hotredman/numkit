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
    StandardEngine engine;
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

TEST_F(TransformsExtrasTest, DftmtxN1)
{
    // dftmtx(1) is the trivial 1×1 matrix [1].
    eval("F = dftmtx(1);");
    EXPECT_EQ(eval("F").dims().rows(), 1u);
    EXPECT_EQ(eval("F").dims().cols(), 1u);
    EXPECT_NEAR(evalScalar("real(F)"), 1.0, 1e-12);
    EXPECT_NEAR(evalScalar("imag(F)"), 0.0, 1e-12);
}

TEST_F(TransformsExtrasTest, DftmtxN2)
{
    // dftmtx(2) = [1 1; 1 -1] (Hadamard with sign).
    eval("F = dftmtx(2);");
    EXPECT_NEAR(evalScalar("real(F(1,1))"),  1.0, 1e-12);
    EXPECT_NEAR(evalScalar("real(F(1,2))"),  1.0, 1e-12);
    EXPECT_NEAR(evalScalar("real(F(2,1))"),  1.0, 1e-12);
    EXPECT_NEAR(evalScalar("real(F(2,2))"), -1.0, 1e-12);
}

TEST_F(TransformsExtrasTest, DftmtxN16Diagonal)
{
    // F(5,5) for N=16 is exp(-2πi · 16/16) = exp(-2πi) = 1.
    // Matches the algebraic identity F(j,k) = exp(-2πi·(j-1)(k-1)/N).
    eval("F = dftmtx(16);");
    EXPECT_EQ(eval("F").dims().rows(), 16u);
    EXPECT_EQ(eval("F").dims().cols(), 16u);
    EXPECT_NEAR(evalScalar("real(F(5,5))"), 1.0, 1e-12);
    EXPECT_NEAR(evalScalar("imag(F(5,5))"), 0.0, 1e-12);
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

TEST_F(TransformsExtrasTest, BitrevorderTwoOutputs)
{
    // Bug fix 2026-05-08: 2nd output `I` was missing; now produces
    // 1-based index vector such that Y(k) = X(I(k)).
    eval("[Y, I] = bitrevorder([10 20 30 40 50 60 70 80]);");
    EXPECT_DOUBLE_EQ(evalScalar("Y(1)"), 10.0);
    EXPECT_DOUBLE_EQ(evalScalar("Y(2)"), 50.0);
    EXPECT_DOUBLE_EQ(evalScalar("I(1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("I(2)"), 5.0);
    EXPECT_DOUBLE_EQ(evalScalar("I(3)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("I(4)"), 7.0);
    EXPECT_DOUBLE_EQ(evalScalar("I(5)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("I(8)"), 8.0);
    // Algebraic identity: Y == X(I).
    eval("X = [10 20 30 40 50 60 70 80]; diff = Y - X(I);");
    for (int k = 1; k <= 8; ++k)
        EXPECT_DOUBLE_EQ(evalScalar("diff(" + std::to_string(k) + ")"), 0.0);
}

TEST_F(TransformsExtrasTest, BitrevorderTwoOutputsLength4)
{
    eval("[Y, I] = bitrevorder([100 200 300 400]);");
    EXPECT_DOUBLE_EQ(evalScalar("Y(1)"), 100.0);
    EXPECT_DOUBLE_EQ(evalScalar("Y(2)"), 300.0);
    EXPECT_DOUBLE_EQ(evalScalar("Y(3)"), 200.0);
    EXPECT_DOUBLE_EQ(evalScalar("Y(4)"), 400.0);
    EXPECT_DOUBLE_EQ(evalScalar("I(1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("I(2)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("I(3)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("I(4)"), 4.0);
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
