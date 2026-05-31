// libs/image/tests/multissim_test.cpp
//
// Regression guard for multissim — multi-scale structural similarity
// (Wang/Simoncelli/Bovik 2003). All reference values from MATLAB
// R2025b on a deterministic test pattern (no rand/randn so
// numkit's MT19937 vs MATLAB's randn divergence doesn't enter).
//
// Differences from MATLAB are ~1e-7 (single-vs-double precision in
// the imfilter accumulation; the algorithm itself is bit-equivalent).

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class MultiSSIMTest : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override
    {
        engine.eval(
            "import compat.*;"
            "[X, Y] = meshgrid(1:32, 1:32);"
            "A = uint8(min(255, X + Y));"
            "B = uint8(min(255, X + 2*Y));");
    }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// ── Default 5-scale ──────────────────────────────────────────────

TEST_F(MultiSSIMTest, DefaultScore)
{
    EXPECT_NEAR(evalScalar("multissim(A, B)"), 0.8983750939, 5e-5);
}

// ── NumScales ────────────────────────────────────────────────────

TEST_F(MultiSSIMTest, NumScales1)
{
    EXPECT_NEAR(evalScalar("multissim(A, B, 'NumScales', 1)"),
                0.8935790062, 5e-5);
}

TEST_F(MultiSSIMTest, NumScales3)
{
    EXPECT_NEAR(evalScalar("multissim(A, B, 'NumScales', 3)"),
                0.9119560719, 5e-5);
}

// ── Custom ScaleWeights ─────────────────────────────────────────

TEST_F(MultiSSIMTest, WangSimoncelliBovikWeights)
{
    EXPECT_NEAR(
        evalScalar("multissim(A, B, 'ScaleWeights', "
                   "[0.0448 0.2856 0.3001 0.2363 0.1333])"),
        0.8920533657, 5e-5);
}

// ── Sigma ───────────────────────────────────────────────────────

TEST_F(MultiSSIMTest, SigmaHalfSize)
{
    EXPECT_NEAR(evalScalar("multissim(A, B, 'Sigma', 0.5)"),
                0.9531264305, 5e-5);
}

// ── DynamicRange ─────────────────────────────────────────────────

TEST_F(MultiSSIMTest, DynamicRange128)
{
    EXPECT_NEAR(evalScalar("multissim(A, B, 'DynamicRange', 128)"),
                0.8723636866, 5e-5);
}

// ── Identical images: score = 1.0 ───────────────────────────────

TEST_F(MultiSSIMTest, IdenticalIsOne)
{
    EXPECT_NEAR(evalScalar("multissim(A, A)"), 1.0, 1e-12);
}

// ── Class preservation: double in, double out ───────────────────

TEST_F(MultiSSIMTest, DoubleInDoubleOut)
{
    eval("Ad = double(A)/255; Bd = double(B)/255;"
         "s = multissim(Ad, Bd);");
    EXPECT_EQ(eval("class(s)").toString(), "double");
    EXPECT_NEAR(evalScalar("s"), 0.8983752005, 5e-5);
}

TEST_F(MultiSSIMTest, SingleInSingleOut)
{
    eval("As = single(A)/255; Bs = single(B)/255;"
         "s = multissim(As, Bs);");
    EXPECT_EQ(eval("class(s)").toString(), "single");
}

TEST_F(MultiSSIMTest, Uint8InSingleOut)
{
    eval("s = multissim(A, B);");
    EXPECT_EQ(eval("class(s)").toString(), "single");
}

// ── qualityMap output ───────────────────────────────────────────

TEST_F(MultiSSIMTest, QualityMapHasNumScalesEntries)
{
    eval("[s, qmap] = multissim(A, B);");
    EXPECT_EQ(static_cast<int>(evalScalar("length(qmap)")), 5);
    EXPECT_EQ(static_cast<int>(evalScalar("size(qmap{1}, 1)")), 32);
    EXPECT_EQ(static_cast<int>(evalScalar("size(qmap{2}, 1)")), 16);
    EXPECT_EQ(static_cast<int>(evalScalar("size(qmap{3}, 1)")), 8);
}

TEST_F(MultiSSIMTest, QualityMapNumScales3)
{
    eval("[s, qmap] = multissim(A, B, 'NumScales', 3);");
    EXPECT_EQ(static_cast<int>(evalScalar("length(qmap)")), 3);
}

// ── Errors ──────────────────────────────────────────────────────

TEST_F(MultiSSIMTest, SizeMismatchThrows)
{
    EXPECT_THROW(eval("multissim(A, uint8(zeros(16, 16)));"),
                 std::exception);
}

TEST_F(MultiSSIMTest, ClassMismatchThrows)
{
    EXPECT_THROW(eval("multissim(A, single(B)/255);"),
                 std::exception);
}

TEST_F(MultiSSIMTest, ScaleWeightsLengthMismatchThrows)
{
    EXPECT_THROW(
        eval("multissim(A, B, 'NumScales', 3, 'ScaleWeights', [1 1]);"),
        std::exception);
}

TEST_F(MultiSSIMTest, NegativeSigmaThrows)
{
    EXPECT_THROW(eval("multissim(A, B, 'Sigma', 0);"), std::exception);
}

TEST_F(MultiSSIMTest, NumScalesTooLargeThrows)
{
    EXPECT_THROW(
        eval("multissim(A, B, 'NumScales', 20);"),
        std::exception);
}
