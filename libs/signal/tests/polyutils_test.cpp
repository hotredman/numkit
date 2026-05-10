// libs/signal/tests/polyutils_test.cpp
//
// Regression guard for polyscale + polystab (Phase 4.3 of audio sweep).
// Bit-equal MATLAB R2025b on documented inputs.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class PolyUtilsTest : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
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
