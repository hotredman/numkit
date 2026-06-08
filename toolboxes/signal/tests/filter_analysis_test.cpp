// toolboxes/signal/tests/filter_analysis_test.cpp
//
// Tests for D1 — filter_analysis responses + predicates:
//   impz / impzlength / stepz / phasedelay / zerophase
//   isfir / isstable / isminphase / ismaxphase / islinphase / isallpass

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <cmath>
#include <gtest/gtest.h>

using namespace numkit;

class FilterAnalysisTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
    bool evalBool(const std::string &c) { return eval(c).toBool(); }
};

// ── impz / impzlength / stepz ────────────────────────────────────────

TEST_F(FilterAnalysisTest, ImpzFirIsCoeffs)
{
    // For an FIR filter h is the b coefficients themselves.
    eval("h = impz([1 2 3]);");
    EXPECT_DOUBLE_EQ(evalScalar("h(1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("h(2)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("h(3)"), 3.0);
}

TEST_F(FilterAnalysisTest, ImpzExplicitN)
{
    // n=5, FIR length 3 → trailing zeros at indices 4,5.
    eval("h = impz([1 1 1], 1, 5);");
    EXPECT_EQ(eval("h").numel(), 5u);
    EXPECT_DOUBLE_EQ(evalScalar("h(4)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("h(5)"), 0.0);
}

TEST_F(FilterAnalysisTest, ImpzlengthFirEqualsCoeffCount)
{
    EXPECT_DOUBLE_EQ(evalScalar("impzlength([1 1 1 1 1])"), 5.0);
}

TEST_F(FilterAnalysisTest, ImpzlengthIirReasonable)
{
    // Single-pole IIR with rho=0.9 → about -5*ln(10)/ln(0.9) ≈ 109.
    auto v = evalScalar("impzlength(1, [1 -0.9])");
    EXPECT_GT(v, 50.0);
    EXPECT_LT(v, 200.0);
}

TEST_F(FilterAnalysisTest, StepzAccumulatesImpulseResponse)
{
    // For FIR [1 1 1], step response is the cumulative sum of h:
    // s = [1, 2, 3, 3, 3, ...].
    eval("s = stepz([1 1 1], 1, 5);");
    EXPECT_DOUBLE_EQ(evalScalar("s(1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("s(2)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("s(3)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("s(5)"), 3.0);
}

// ── phasedelay / zerophase ───────────────────────────────────────────

TEST_F(FilterAnalysisTest, PhasedelaySymmetricFirIsConstant)
{
    // Linear-phase FIR [1 2 1] has constant phase delay = (N-1)/2 = 1.
    eval("[pd, w] = phasedelay([1 2 1], 1, 64);");
    // Skip endpoint w=0 (handled specially) — middle samples should be ~1.
    for (int i = 5; i <= 60; ++i) {
        const double pdi = evalScalar("pd(" + std::to_string(i) + ")");
        EXPECT_NEAR(pdi, 1.0, 1e-6) << "i=" << i;
    }
}

TEST_F(FilterAnalysisTest, ZerophaseRealForSymmetricFir)
{
    // Symmetric FIR → Hr is real.
    eval("Hr = zerophase([1 2 1], 1, 16);");
    auto v = eval("Hr");
    EXPECT_FALSE(v.isComplex());
    // Hr(0) should equal sum of b = 4 (DC gain).
    EXPECT_NEAR(evalScalar("Hr(1)"), 4.0, 1e-9);
}

// ── isfir / isstable ──────────────────────────────────────────────────

TEST_F(FilterAnalysisTest, IsfirTrueForTrivialA)
{
    EXPECT_TRUE(evalBool("isfir([1 2 3], 1)"));
    EXPECT_TRUE(evalBool("isfir([1 2 3])"));
}

TEST_F(FilterAnalysisTest, IsfirFalseForIirA)
{
    EXPECT_FALSE(evalBool("isfir([1 2 3], [1 -0.5])"));
}

TEST_F(FilterAnalysisTest, IsstableFirAlways)
{
    EXPECT_TRUE(evalBool("isstable([1 2 3 4], 1)"));
}

TEST_F(FilterAnalysisTest, IsstableSinglePoleInside)
{
    EXPECT_TRUE(evalBool("isstable(1, [1 -0.9])"));
}

TEST_F(FilterAnalysisTest, IsstableSinglePoleOutside)
{
    EXPECT_FALSE(evalBool("isstable(1, [1 -1.1])"));
}

// ── isminphase / ismaxphase ───────────────────────────────────────────

TEST_F(FilterAnalysisTest, IsminphaseZerosInside)
{
    // b = [1 -0.5] has zero at z=0.5 (inside unit circle).
    EXPECT_TRUE(evalBool("isminphase([1 -0.5], 1)"));
}

TEST_F(FilterAnalysisTest, IsminphaseZerosOutside)
{
    // b = [1 -2] has zero at z=2 (outside).
    EXPECT_FALSE(evalBool("isminphase([1 -2], 1)"));
}

TEST_F(FilterAnalysisTest, IsmaxphaseZerosOutside)
{
    EXPECT_TRUE(evalBool("ismaxphase([1 -2], 1)"));
}

TEST_F(FilterAnalysisTest, IsmaxphaseZerosInside)
{
    EXPECT_FALSE(evalBool("ismaxphase([1 -0.5], 1)"));
}

// ── islinphase ────────────────────────────────────────────────────────

TEST_F(FilterAnalysisTest, IslinphaseSymmetricFir)
{
    EXPECT_TRUE(evalBool("islinphase([1 2 3 2 1], 1)"));
}

TEST_F(FilterAnalysisTest, IslinphaseAntisymmetricFir)
{
    EXPECT_TRUE(evalBool("islinphase([1 2 0 -2 -1], 1)"));
}

TEST_F(FilterAnalysisTest, IslinphaseAsymmetricFir)
{
    EXPECT_FALSE(evalBool("islinphase([1 2 3 4 5], 1)"));
}

TEST_F(FilterAnalysisTest, IslinphaseFalseForIir)
{
    EXPECT_FALSE(evalBool("islinphase([1 2 1], [1 -0.5])"));
}

// ── isallpass ─────────────────────────────────────────────────────────

TEST_F(FilterAnalysisTest, IsallpassReversedDenominator)
{
    // Classic 1st-order all-pass: H(z) = (z^-1 - a*) / (1 - a z^-1)
    // → b = [-0.5 1], a = [1 -0.5]. (a == flip(b) in canonical form.)
    EXPECT_TRUE(evalBool("isallpass([-0.5 1], [1 -0.5])"));
}

TEST_F(FilterAnalysisTest, IsallpassFalseForGenericIir)
{
    EXPECT_FALSE(evalBool("isallpass([1 0.5], [1 -0.5])"));
}
