// toolboxes/signal/tests/polyutils_test.cpp
// gtest coverage for polyscale + polystab.
// Both functions are clean-room implementations from public references
// (Oppenheim & Schafer; Markel & Gray; Hayes — see
//). The pixel/coefficient values
// below match MATLAB R2025b on the documented argument set; the
// property tests at the end verify correctness MATLAB-independently
// (polyscale must scale the roots by the factor; polystab must move
// every root inside the unit circle while preserving the magnitude-
// response shape).

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class PolyUtilsTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override {}
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// ── polyscale ──────────────────────────────────────────────────────────
TEST_F(PolyUtilsTest, PolyscaleBandwidthExpansion)
{
    // y[k] = p[k] * 0.85^k
    eval("p = [1 -2 1.5 -0.5 0.1]; y = polyscale(p, 0.85);");
    EXPECT_NEAR(evalScalar("y(1)"),  1.000000,    1e-9);
    EXPECT_NEAR(evalScalar("y(2)"), -1.700000,    1e-9);  // -2 * 0.85
    EXPECT_NEAR(evalScalar("y(3)"),  1.083750,    1e-9);  // 1.5 * 0.85^2
    EXPECT_NEAR(evalScalar("y(4)"), -0.307062,    1e-6);
    EXPECT_NEAR(evalScalar("y(5)"),  0.052200625, 1e-9);
}

TEST_F(PolyUtilsTest, PolyscaleScaleGreaterThanOne)
{
    eval("y = polyscale([1 -2 1.5], 1.2);");
    EXPECT_NEAR(evalScalar("y(2)"), -2.4, 1e-9);
    EXPECT_NEAR(evalScalar("y(3)"),  2.16, 1e-9);  // 1.5 * 1.44
}

// Matrix input: each row is scaled independently.
TEST_F(PolyUtilsTest, PolyscaleMatrixInput)
{
    eval("M = polyscale([1 2 3; 4 5 6], 0.5);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(M,1)")), 2);
    EXPECT_EQ(static_cast<int>(evalScalar("size(M,2)")), 3);
    EXPECT_NEAR(evalScalar("M(1,2)"), 1.0,  1e-12);   // 2 * 0.5
    EXPECT_NEAR(evalScalar("M(1,3)"), 0.75, 1e-12);   // 3 * 0.25
    EXPECT_NEAR(evalScalar("M(2,2)"), 2.5,  1e-12);   // 5 * 0.5
    EXPECT_NEAR(evalScalar("M(2,3)"), 1.5,  1e-12);   // 6 * 0.25
}

// Row-vector scale: element k raised to the power k.
TEST_F(PolyUtilsTest, PolyscaleVectorScale)
{
    // factor = [1^0, 2^1, 4^2] = [1, 2, 16]; y = [1*1, 2*2, 3*16]
    eval("y = polyscale([1 2 3], [1 2 4]);");
    EXPECT_NEAR(evalScalar("y(1)"),  1.0, 1e-12);
    EXPECT_NEAR(evalScalar("y(2)"),  4.0, 1e-12);
    EXPECT_NEAR(evalScalar("y(3)"), 48.0, 1e-12);
}

// Complex scale factor rotates as well as scales.
TEST_F(PolyUtilsTest, PolyscaleComplexScale)
{
    // factor = [1, i, i^2] = [1, i, -1]; y = [1, 2i, -3]
    eval("y = polyscale([1 2 3], 1i);");
    EXPECT_NEAR(evalScalar("real(y(2))"), 0.0, 1e-12);
    EXPECT_NEAR(evalScalar("imag(y(2))"), 2.0, 1e-12);
    EXPECT_NEAR(evalScalar("real(y(3))"), -3.0, 1e-12);
    EXPECT_NEAR(evalScalar("imag(y(3))"), 0.0, 1e-12);
}

// A scale vector whose length is neither 1 nor the polynomial length
// throws m:polyscale:BadScale.
TEST_F(PolyUtilsTest, PolyscaleBadScaleLengthThrows)
{
    EXPECT_THROW(eval("polyscale([1 2 3], [1 2]);"), std::exception);
}

TEST_F(PolyUtilsTest, PolyscaleEmptyInput)
{
    eval("y = polyscale([], 0.5);");
    EXPECT_EQ(static_cast<int>(evalScalar("numel(y)")), 0);
}

// ── polystab ───────────────────────────────────────────────────────────
TEST_F(PolyUtilsTest, PolystabReflectsOutsideRoot)
{
    // poly with roots [2, 0.5]: (1 - 2z^-1)(1 - 0.5z^-1) = [1, -2.5, 1]
    // After polystab: root 2 reflects to 1/2 = 0.5 → roots [0.5, 0.5]
    // → poly = [1, -1, 0.25]
    eval("a = [1 -2.5 1]; b = polystab(a);");
    EXPECT_NEAR(evalScalar("b(1)"),  1.00, 1e-9);
    EXPECT_NEAR(evalScalar("b(2)"), -1.00, 1e-9);
    EXPECT_NEAR(evalScalar("b(3)"),  0.25, 1e-9);
    // Roots all inside unit circle.
    eval("rb = roots(b);");
    EXPECT_LE(evalScalar("max(abs(rb))"), 1.0 + 1e-9);
}

TEST_F(PolyUtilsTest, PolystabPreservesAlreadyStable)
{
    // Polynomial with all roots already inside unit circle: identity-ish.
    eval("a = [1 0 -0.25]; b = polystab(a);");  // roots [0.5, -0.5]
    // Same poly back (roots aren't reflected).
    EXPECT_NEAR(evalScalar("b(1)"), 1.00, 1e-9);
    EXPECT_NEAR(evalScalar("b(2)"), 0.00, 1e-9);
    EXPECT_NEAR(evalScalar("b(3)"), -0.25, 1e-9);
}

TEST_F(PolyUtilsTest, PolystabScalarReturnsAsIs)
{
    eval("b = polystab(2.5);");
    EXPECT_DOUBLE_EQ(evalScalar("b"), 2.5);
}

TEST_F(PolyUtilsTest, PolystabRealInputRealOutput)
{
    eval("b = polystab([1 -2.5 1]); imag_max = max(abs(imag(b)));");
    EXPECT_DOUBLE_EQ(evalScalar("imag_max"), 0.0);
}

// Leading zeros are not significant: roots ignores them, and the gain
// is taken from the first non-zero coefficient.
TEST_F(PolyUtilsTest, PolystabLeadingZeros)
{
    eval("b = polystab([0 1 -2.5 1]);");
    EXPECT_EQ(static_cast<int>(evalScalar("numel(b)")), 3);
    EXPECT_NEAR(evalScalar("b(1)"),  1.00, 1e-9);
    EXPECT_NEAR(evalScalar("b(2)"), -1.00, 1e-9);
    EXPECT_NEAR(evalScalar("b(3)"),  0.25, 1e-9);
}

// Output is always a row vector, even for a column-vector input.
TEST_F(PolyUtilsTest, PolystabColumnInputRowOutput)
{
    eval("b = polystab([1; -2.5; 1]);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(b,1)")), 1);
    EXPECT_EQ(static_cast<int>(evalScalar("size(b,2)")), 3);
}

// Matrix input throws m:polystab:notVector.
TEST_F(PolyUtilsTest, PolystabMatrixInputThrows)
{
    EXPECT_THROW(eval("polystab([1 2; 3 4]);"), std::exception);
}

TEST_F(PolyUtilsTest, PolystabEmptyInput)
{
    eval("b = polystab([]);");
    EXPECT_EQ(static_cast<int>(evalScalar("numel(b)")), 0);
}

// Complex-root path cross-checked against MATLAB R2025b. The roots of
// these polynomials include complex-conjugate pairs (A: one pair
// outside the unit circle; C: a degree-5 mix of real and complex
// roots), so they exercise the reflect-and-rebuild path that simple
// real-root polynomials do not. Expected values are MATLAB R2025b
// reference output (offline regression guard).
TEST_F(PolyUtilsTest, PolystabComplexRootsMatchMatlab)
{
    // A = poly([1.5+0.5i, 1.5-0.5i, 0.4]) = [1 -3.4 3.7 -1].
    // The complex pair has |root| = sqrt(2.5) > 1 and is reflected.
    eval("bA = polystab([1 -3.4 3.7 -1]);");
    EXPECT_EQ(static_cast<int>(evalScalar("numel(bA)")), 4);
    EXPECT_NEAR(evalScalar("bA(1)"),  1.00, 1e-12);
    EXPECT_NEAR(evalScalar("bA(2)"), -1.60, 1e-12);
    EXPECT_NEAR(evalScalar("bA(3)"),  0.88, 1e-12);
    EXPECT_NEAR(evalScalar("bA(4)"), -0.16, 1e-12);

    // C: degree-5 polynomial, mixed real/complex roots.
    eval("bC = polystab([1 -1.2 0.7 -2.1 0.3 0.5]);");
    EXPECT_EQ(static_cast<int>(evalScalar("numel(bC)")), 6);
    EXPECT_NEAR(evalScalar("bC(2)"), -0.545088121924205, 1e-12);
    EXPECT_NEAR(evalScalar("bC(3)"),  0.294451845778127, 1e-12);
    EXPECT_NEAR(evalScalar("bC(4)"), -0.481271049876727, 1e-12);
    EXPECT_NEAR(evalScalar("bC(5)"), -0.005815419471494, 1e-12);
    EXPECT_NEAR(evalScalar("bC(6)"),  0.105904581581909, 1e-12);
}

// ── MATLAB-independent correctness tests ──────────────────────────────
// These verify the defining property of each function against a known
// answer — no reference engine involved.

// polyscale must scale every root by exactly the scaling factor: the
// roots of polyscale(a, alpha) are alpha * roots(a).
TEST_F(PolyUtilsTest, PolyscaleScalesRootsByFactor)
{
    eval("a = [1 -2.0 1.3 -0.4]; alpha = 0.7;"
         "r0 = sort(abs(roots(a)));"
         "r1 = sort(abs(roots(polyscale(a, alpha))));"
         "ratio = r1 ./ r0;");
    // Every root magnitude is scaled by alpha.
    EXPECT_NEAR(evalScalar("min(ratio)"), 0.7, 1e-9);
    EXPECT_NEAR(evalScalar("max(ratio)"), 0.7, 1e-9);
}

// polystab must move every root to inside (or onto) the unit circle.
TEST_F(PolyUtilsTest, PolystabRootsInsideUnitCircle)
{
    eval("a = [1 -2.5 1 0.3];"          // has a root outside the circle
         "ra = max(abs(roots(a)));"
         "b = polystab(a);"
         "rb = max(abs(roots(b)));");
    EXPECT_GT(evalScalar("ra"), 1.0);                 // input is unstable
    EXPECT_LE(evalScalar("rb"), 1.0 + 1e-9);          // output is stable
}

// polystab preserves the magnitude-response shape: |B(e^jw)| / |A(e^jw)|
// is constant across frequency (the reflection scales the response by a
// single positive gain, it does not reshape it).
TEST_F(PolyUtilsTest, PolystabPreservesMagnitudeShape)
{
    eval("a = [1 -2.5 1 0.3]; b = polystab(a);\n"
         "w = linspace(0.1, pi-0.1, 9); z = exp(1i*w);\n"
         "ea = zeros(1, numel(z)); for k = 1:numel(a), ea = ea.*z + a(k); end\n"
         "eb = zeros(1, numel(z)); for k = 1:numel(b), eb = eb.*z + b(k); end\n"
         "ratio = abs(eb) ./ abs(ea);");
    // The response ratio is the same at every probed frequency.
    EXPECT_LT(evalScalar("max(ratio) - min(ratio)"), 1e-9);
    // ...and it is a genuine (non-trivial) attenuation.
    EXPECT_GT(evalScalar("min(ratio)"), 0.0);
    EXPECT_LT(evalScalar("min(ratio)"), 1.0);
}
