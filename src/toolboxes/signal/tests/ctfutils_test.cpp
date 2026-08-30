// toolboxes/signal/tests/ctfutils_test.cpp
//
// Regression guard for ctf2zp + scaleFilterSections (Phase 4.11).
// Bit-equal MATLAB R2025b.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class CtfUtilsTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override {}
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// ── ctf2zp ────────────────────────────────────────────────────────────
TEST_F(CtfUtilsTest, SingleSectionVectorInput)
{
    eval("[z, p, k] = ctf2zp([1 -1 0.5], [1 -0.6 0.2]);");
    EXPECT_EQ(static_cast<int>(evalScalar("numel(z)")), 2);
    EXPECT_EQ(static_cast<int>(evalScalar("numel(p)")), 2);
    EXPECT_NEAR(evalScalar("real(k)"), 1.0, 1e-9);
}

TEST_F(CtfUtilsTest, MultiSectionGainProduct)
{
    eval("NUM = [1 -1 0.5; 1 0 -1]; DEN = [1 -0.6 0.2; 1 0 -0.25];"
         "[z, p, k] = ctf2zp(NUM, DEN);");
    EXPECT_NEAR(evalScalar("real(k)"), 1.0, 1e-9);
}

TEST_F(CtfUtilsTest, SVScalingAccumulates)
{
    eval("NUM = [1 -1 0.5; 1 0 -1]; DEN = [1 -0.6 0.2; 1 0 -0.25];"
         "[~, ~, k] = ctf2zp(NUM, DEN, [2 3 5]);");
    EXPECT_NEAR(evalScalar("real(k)"), 30.0, 1e-9);  // 2*3*5
}

// ── scaleFilterSections ───────────────────────────────────────────────
TEST_F(CtfUtilsTest, VectorScaleDistributedAcrossSections)
{
    eval("ctf = [1 2 1; 1 -1 0.5]; sv = scaleFilterSections(ctf, [2 3 5]);");
    // Expected per MATLAB (manually computed):
    // sv[1,:] = sqrt(5) * 2 * [1 2 1] = [4.4721, 8.9443, 4.4721]
    // sv[2,:] = sign(5) * sqrt(5) * 3 * [1 -1 0.5] = [6.7082, -6.7082, 3.3541]
    EXPECT_NEAR(evalScalar("sv(1, 1)"),  4.47214, 1e-5);
    EXPECT_NEAR(evalScalar("sv(1, 2)"),  8.94427, 1e-5);
    EXPECT_NEAR(evalScalar("sv(2, 1)"),  6.70820, 1e-5);
    EXPECT_NEAR(evalScalar("sv(2, 2)"), -6.70820, 1e-5);
    EXPECT_NEAR(evalScalar("sv(2, 3)"),  3.35410, 1e-5);
}

TEST_F(CtfUtilsTest, ScalarScaleUniformDistribution)
{
    // Scalar SV → |sv|^(1/K) on each row, sign on last.
    eval("ctf = [1 2 1; 1 -1 0.5]; sv = scaleFilterSections(ctf, 4);");
    // |4|^(1/2) = 2; row 1 = 2*[1 2 1]; row 2 = sign(4)*2*[1 -1 0.5]
    EXPECT_NEAR(evalScalar("sv(1, 1)"),  2.0, 1e-9);
    EXPECT_NEAR(evalScalar("sv(2, 2)"), -2.0, 1e-9);
}

TEST_F(CtfUtilsTest, AllOnesSVReturnsOriginal)
{
    eval("ctf = [1 2 1; 1 -1 0.5]; sv = scaleFilterSections(ctf, [1 1 1]);");
    EXPECT_NEAR(evalScalar("sv(1, 1)"),  1.0, 1e-12);
    EXPECT_NEAR(evalScalar("sv(2, 2)"), -1.0, 1e-12);
}

// Single section (K = 1, row-vector input): |g|^(1/1) = |g| scales the
// whole row. MATLAB R2025b reference values.
TEST_F(CtfUtilsTest, SingleSectionScalarScale)
{
    eval("b = scaleFilterSections([1 0.5 0.2], 8);");
    EXPECT_NEAR(evalScalar("b(1)"), 8.0, 1e-12);
    EXPECT_NEAR(evalScalar("b(2)"), 4.0, 1e-12);
    EXPECT_NEAR(evalScalar("b(3)"), 1.6, 1e-12);
}

// Single section with a length-(K+1)=2 scale vector.
TEST_F(CtfUtilsTest, SingleSectionVectorScale)
{
    eval("b = scaleFilterSections([1 0.5 0.2], [3 8]);");  // 8 * 3 * row
    EXPECT_NEAR(evalScalar("b(1)"), 24.0, 1e-12);
    EXPECT_NEAR(evalScalar("b(2)"), 12.0, 1e-12);
    EXPECT_NEAR(evalScalar("b(3)"),  4.8, 1e-12);
}

// Complex numerator coefficients are supported. MATLAB R2025b values.
TEST_F(CtfUtilsTest, ComplexCoefficients)
{
    eval("b = scaleFilterSections([1 0.5+0.2i 0.1; 1 -0.3 0.4i], 8);");
    // |8|^(1/2) = 2.8284271247...
    EXPECT_NEAR(evalScalar("real(b(1,2))"), 1.4142135624, 1e-9);
    EXPECT_NEAR(evalScalar("imag(b(1,2))"), 0.5656854249, 1e-9);
    EXPECT_NEAR(evalScalar("imag(b(2,3))"), 1.1313708499, 1e-9);
}

// A scale vector whose length is neither 1 nor K+1 is rejected.
TEST_F(CtfUtilsTest, BadScaleLengthThrows)
{
    bool threw = false;
    try { eval("scaleFilterSections([1 0.5; 1 0.3], [1 2 3 4]);"); }
    catch (...) { threw = true; }
    EXPECT_TRUE(threw);
}

// ── MATLAB-independent correctness test ───────────────────────────────
// The defining property: the cascade product (the convolution of all
// section numerators) of the scaled filter equals the original cascade
// product multiplied by exactly the overall gain g.
TEST_F(CtfUtilsTest, CascadeProductScaledByG)
{
    eval("B = [1 0.6 0.1; 1 -0.4 0.25; 1 0.2 -0.3]; g = 12;\n"
         "Bg = scaleFilterSections(B, g);\n"
         "cB  = conv(conv(B(1,:),  B(2,:)),  B(3,:));\n"
         "cBg = conv(conv(Bg(1,:), Bg(2,:)), Bg(3,:));\n"
         "err = max(abs(cBg - g * cB));");
    EXPECT_LT(evalScalar("err"), 1e-12);

    // Vector g: overall gain = g(end) * prod(g(1:K)).
    eval("gv = [2 3 0.5 12];\n"
         "Bgv = scaleFilterSections(B, gv);\n"
         "cBgv = conv(conv(Bgv(1,:), Bgv(2,:)), Bgv(3,:));\n"
         "overall = gv(4) * prod(gv(1:3));\n"
         "errv = max(abs(cBgv - overall * cB));");
    EXPECT_LT(evalScalar("errv"), 1e-12);
}
