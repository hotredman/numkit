// libs/image/tests/gradientweight_test.cpp
//
// Regression guard for gradientweight — gradient-magnitude pixel
// weights for FMM segmentation. Reference values from MATLAB R2025b.
//
// Notable: we deliberately replicate MATLAB's filtRadius(1)-norm bug
// for hy on anisotropic σ (the W5 case) per the "MATLAB wins" rule.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class GradientweightTest : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override {
        engine.eval("import compat.*;"
                    "I = double(magic(8)) / 100;");
    }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// ── default args (sigma=1.5, P=3, K=0.25) ──────────────────────────

TEST_F(GradientweightTest, DefaultArgs)
{
    eval("W1 = gradientweight(I);");
    EXPECT_NEAR(evalScalar("W1(4,4)"), 0.3265754293, 1e-8);
    EXPECT_NEAR(evalScalar("W1(1,1)"), 0.001, 1e-12);
    EXPECT_NEAR(evalScalar("W1(8,8)"), 0.001, 1e-12);
}

// ── scalar sigma override ──────────────────────────────────────────

TEST_F(GradientweightTest, ScalarSigma)
{
    eval("W2 = gradientweight(I, 2.0);");
    EXPECT_NEAR(evalScalar("W2(4,4)"), 0.9999945962, 1e-5);
}

// ── 2-element sigma (replicates MATLAB's filtRadius(1)-norm bug) ───

TEST_F(GradientweightTest, VectorSigma)
{
    eval("W5 = gradientweight(I, [1.5 2.5]);");
    EXPECT_NEAR(evalScalar("W5(4,4)"), 0.5486473465, 1e-8);
}

// ── RolloffFactor NV pair ──────────────────────────────────────────

TEST_F(GradientweightTest, RolloffFactor)
{
    eval("W3 = gradientweight(I, 1.5, 'RolloffFactor', 1.0);");
    EXPECT_NEAR(evalScalar("W3(4,4)"), 0.768629544, 1e-8);
}

// ── WeightCutoff NV pair ───────────────────────────────────────────

TEST_F(GradientweightTest, WeightCutoff)
{
    eval("W4 = gradientweight(I, 1.5, 'WeightCutoff', 0.5);");
    // W4(4,4) was 0.3266 in default; cutoff=0.5 flips it to floor 1e-3.
    EXPECT_NEAR(evalScalar("W4(4,4)"), 0.001, 1e-12);
}

// ── constant-image fast-path (returns all-ones) ────────────────────

TEST_F(GradientweightTest, ConstantImageReturnsOnes)
{
    eval("Wc = gradientweight(ones(5));");
    EXPECT_NEAR(evalScalar("Wc(1,1)"), 1.0, 1e-12);
    EXPECT_NEAR(evalScalar("Wc(3,3)"), 1.0, 1e-12);
    EXPECT_NEAR(evalScalar("Wc(5,5)"), 1.0, 1e-12);
}

// ── errors ─────────────────────────────────────────────────────────

TEST_F(GradientweightTest, NegativeSigmaThrows)
{
    EXPECT_THROW(eval("gradientweight(I, -1.0);"), std::exception);
}

TEST_F(GradientweightTest, RolloffOutOfRangeThrows)
{
    EXPECT_THROW(eval("gradientweight(I, 1.5, 'RolloffFactor', -1);"),
                 std::exception);
}

TEST_F(GradientweightTest, CutoffOutOfRangeThrows)
{
    // K must be in [1e-3, 1].
    EXPECT_THROW(eval("gradientweight(I, 1.5, 'WeightCutoff', 5);"),
                 std::exception);
}
