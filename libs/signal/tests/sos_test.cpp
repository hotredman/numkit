// libs/dsp/tests/sos_test.cpp
//
// SOS family — sosfilt, zp2sos, tf2sos. Tests cover:
//   * sosfilt cascade equivalence to filter() on direct-form (b, a)
//     for a known biquad and a 4-section cascade.
//   * sosfilt 2D matrix input (column-wise filtering).
//   * zp2sos for the simplest case (one real pole pair, no zeros).
//   * tf2sos round-trip — filter signal x with (b, a) directly and
//     also via tf2sos → sosfilt; outputs should agree to ~1e-10.
//   * Validation throws (bad shapes, missing conjugates, etc.).

#include "dual_engine_fixture.hpp"

#include <cmath>

using namespace m_test;

class SosTest : public DualEngineTest
{};

// ─────────────────────────────────────────────────────────────────
// sosfilt
// ─────────────────────────────────────────────────────────────────

TEST_P(SosTest, SosfiltSingleBiquadMatchesFilter)
{
    // 2nd-order Butterworth-like biquad. Compare sosfilt with filter.
    eval("b = [0.0675, 0.1349, 0.0675];"
         "a = [1.0000, -1.1430, 0.4128];"
         "x = sin(2 * pi * (0:99)' / 50);"
         "y_ref = filter(b, a, x);"
         "sos = [b a];"
         "y_sos = sosfilt(sos, x);"
         "diff = max(abs(y_sos - y_ref));");
    EXPECT_LT(evalScalar("diff;"), 1e-12);
}

TEST_P(SosTest, SosfiltCascadeMatchesFilter)
{
    // Two cascaded biquads = filter with the convolved (b, a).
    eval("b1 = [0.5, 0.3, 0.1]; a1 = [1.0, -0.4, 0.2];"
         "b2 = [0.2, 0.0, 0.1]; a2 = [1.0,  0.1, 0.05];"
         "b = conv(b1, b2);"
         "a = conv(a1, a2);"
         "sos = [b1 a1; b2 a2];"
         "x = (1:50)' / 50.0;"
         "y_filt = filter(b, a, x);"
         "y_sos  = sosfilt(sos, x);"
         "diff = max(abs(y_sos - y_filt));");
    EXPECT_LT(evalScalar("diff;"), 1e-12);
}

TEST_P(SosTest, SosfiltColumnVectorOutput)
{
    eval("sos = [1 0 0 1 0 0];"  // pass-through identity
         "x = (1:5)';"
         "y = sosfilt(sos, x);"
         "diff = max(abs(y - x));");
    EXPECT_DOUBLE_EQ(evalScalar("size(y, 1);"), 5.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(y, 2);"), 1.0);
    EXPECT_LT(evalScalar("diff;"), 1e-15);
}

TEST_P(SosTest, SosfiltMatrixFiltersColumns)
{
    // Identity biquad → output should equal input.
    eval("sos = [1 0 0 1 0 0];"
         "X = [1 10; 2 20; 3 30];"
         "Y = sosfilt(sos, X);"
         "diff = max(abs(reshape(Y - X, 1, [])));");
    EXPECT_LT(evalScalar("diff;"), 1e-15);
}

TEST_P(SosTest, SosfiltA0NormalizationApplied)
{
    // Same biquad written with a0 = 2 instead of 1 should give the
    // same result (sosfilt normalises internally).
    eval("b = [0.5, 0.3, 0.1]; a = [1.0, -0.4, 0.2];"
         "x = (1:30)' / 10;"
         "y_ref = filter(b, a, x);"
         "sos2 = [b a] * 2;"     // multiply all 6 coefficients by 2
         "y2 = sosfilt(sos2, x);"
         "diff = max(abs(y2 - y_ref));");
    EXPECT_LT(evalScalar("diff;"), 1e-12);
}

TEST_P(SosTest, SosfiltBadSosShapeThrows)
{
    eval("sos = [1 2 3 4 5];");  // only 5 columns, not 6
    EXPECT_THROW(eval("y = sosfilt(sos, (1:10)');"), std::exception);
}

// ─────────────────────────────────────────────────────────────────
// zp2sos
// ─────────────────────────────────────────────────────────────────

TEST_P(SosTest, Zp2sosSingleRealPolePair)
{
    // Two real poles at 0.5, 0.7; no zeros; gain 1.
    eval("z = []; p = [0.5; 0.7]; sos = zp2sos(z, p, 1);");
    EXPECT_DOUBLE_EQ(evalScalar("size(sos, 1);"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(sos, 2);"), 6.0);
    // Numerator: pure passthrough (b0=1, b1=0, b2=0) since there are
    // no zeros to pair.
    EXPECT_NEAR(evalScalar("sos(1, 1);"), 1.0, 1e-12);
    EXPECT_NEAR(evalScalar("sos(1, 2);"), 0.0, 1e-12);
    EXPECT_NEAR(evalScalar("sos(1, 3);"), 0.0, 1e-12);
    // Denominator: (z - 0.5)(z - 0.7) = z² - 1.2z + 0.35
    EXPECT_NEAR(evalScalar("sos(1, 4);"), 1.0, 1e-12);
    EXPECT_NEAR(evalScalar("sos(1, 5);"), -1.2, 1e-12);
    EXPECT_NEAR(evalScalar("sos(1, 6);"), 0.35, 1e-12);
}

TEST_P(SosTest, Zp2sosComplexConjugatePolePair)
{
    // Pole pair 0.5 ± 0.5i → quadratic z² - z + 0.5 (a = 0.5, b = 0.5).
    eval("z = []; p = [0.5+0.5i; 0.5-0.5i]; sos = zp2sos(z, p, 1);");
    EXPECT_NEAR(evalScalar("sos(1, 5);"), -1.0, 1e-12);
    EXPECT_NEAR(evalScalar("sos(1, 6);"),  0.5, 1e-12);
}

TEST_P(SosTest, Zp2sosWithGainTwoOutputForm)
{
    eval("z = []; p = [0.5; 0.7]; [sos, g] = zp2sos(z, p, 3);");
    // Gain factored out — sos should be unchanged from the gain-1 case.
    EXPECT_NEAR(evalScalar("sos(1, 1);"), 1.0, 1e-12);
    EXPECT_NEAR(evalScalar("g;"), 3.0, 1e-12);
}

TEST_P(SosTest, Zp2sosGainAppliedToFirstSection)
{
    // 1-output form distributes gain into the first section's b coefficients.
    eval("z = []; p = [0.5; 0.7]; sos = zp2sos(z, p, 4);");
    EXPECT_NEAR(evalScalar("sos(1, 1);"), 4.0, 1e-12);  // b0 *= gain
}

TEST_P(SosTest, Zp2sosNoPolesThrows)
{
    eval("z = [0.1; 0.2];");
    EXPECT_THROW(eval("sos = zp2sos(z, [], 1);"), std::exception);
}

TEST_P(SosTest, Zp2sosUnpairedComplexPoleThrows)
{
    // 0.5+0.5i without its conjugate cannot be turned into a real biquad.
    eval("z = []; p = [0.5+0.5i];");
    EXPECT_THROW(eval("sos = zp2sos(z, p, 1);"), std::exception);
}

// ─────────────────────────────────────────────────────────────────
// tf2sos
// ─────────────────────────────────────────────────────────────────

TEST_P(SosTest, Tf2sosSingleBiquadRoundTrip)
{
    // Apply (b, a) directly and via tf2sos → sosfilt; outputs match
    // to ~1e-10 (Durand-Kerner roots are approximate).
    eval("b = [0.0675, 0.1349, 0.0675];"
         "a = [1.0000, -1.1430, 0.4128];"
         "x = sin(2 * pi * (0:99)' / 50);"
         "y_ref = filter(b, a, x);"
         "sos = tf2sos(b, a);"
         "y_sos = sosfilt(sos, x);"
         "diff = max(abs(y_sos - y_ref));");
    EXPECT_LT(evalScalar("diff;"), 1e-9);
}

TEST_P(SosTest, Tf2sosCascadeRoundTrip)
{
    // Two-section cascade: build via convolved (b, a) and verify
    // tf2sos → sosfilt matches filter().
    eval("b1 = [0.5, 0.3, 0.1]; a1 = [1.0, -0.4, 0.2];"
         "b2 = [0.2, 0.0, 0.1]; a2 = [1.0,  0.1, 0.05];"
         "b = conv(b1, b2);"
         "a = conv(a1, a2);"
         "x = (1:50)' / 50.0;"
         "y_ref = filter(b, a, x);"
         "sos = tf2sos(b, a);"
         "y_sos = sosfilt(sos, x);"
         "diff = max(abs(y_sos - y_ref));");
    EXPECT_LT(evalScalar("diff;"), 1e-9);
}

TEST_P(SosTest, Tf2sosWithGainTwoOutputForm)
{
    eval("b = [2, 0, 0]; a = [1, -0.5];"
         "[sos, g] = tf2sos(b, a);");
    // gain = b(1)/a(1) = 2/1 = 2.
    EXPECT_NEAR(evalScalar("g;"), 2.0, 1e-12);
}

TEST_P(SosTest, Tf2sosZeroLeadingAThrows)
{
    eval("b = [1 2]; a = [0 1];");
    EXPECT_THROW(eval("sos = tf2sos(b, a);"), std::exception);
}

INSTANTIATE_DUAL(SosTest);
