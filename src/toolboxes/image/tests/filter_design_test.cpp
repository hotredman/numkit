// toolboxes/image/tests/filter_design_test.cpp
//
// Regression guard for cycle 4 image filter design: fspecial3 + fwind2.
// fsamp2 / ftrans2 / fwind1 / gabor are stubbed with KNOWN GAP errors.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class FilterDesignTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override {}
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// ── fspecial3 ─────────────────────────────────────────────────────────
TEST_F(FilterDesignTest, AverageDefault5x5x5)
{
    eval("h = fspecial3('average');");
    EXPECT_EQ(static_cast<int>(evalScalar("size(h, 1)")), 5);
    EXPECT_EQ(static_cast<int>(evalScalar("size(h, 3)")), 5);
    EXPECT_NEAR(evalScalar("sum(h(:))"), 1.0, 1e-12);
    EXPECT_NEAR(evalScalar("h(3, 3, 3)"), 1.0/125.0, 1e-12);
}

TEST_F(FilterDesignTest, GaussianSumsToOne)
{
    eval("h = fspecial3('gaussian', [3 3 3], 1);");
    EXPECT_NEAR(evalScalar("sum(h(:))"), 1.0, 1e-12);
    // Center value computed from analytic 3-D gaussian (matches MATLAB).
    EXPECT_NEAR(evalScalar("h(2, 2, 2)"), 0.0922613, 1e-5);
}

TEST_F(FilterDesignTest, LaplacianSumsToZero)
{
    eval("h = fspecial3('laplacian');");
    EXPECT_EQ(static_cast<int>(evalScalar("size(h, 1)")), 3);
    EXPECT_NEAR(evalScalar("sum(h(:))"), 0.0, 1e-12);
    EXPECT_DOUBLE_EQ(evalScalar("h(2, 2, 2)"), -6.0);
    // Face neighbours = 1.
    EXPECT_DOUBLE_EQ(evalScalar("h(1, 2, 2)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("h(2, 2, 3)"), 1.0);
}

TEST_F(FilterDesignTest, SobelXKernelStructure)
{
    eval("h = fspecial3('sobel', 'X');");
    EXPECT_EQ(static_cast<int>(evalScalar("size(h, 1)")), 3);
    // Page 1 (z=0) edge-page weight: [1 0 -1; 2 0 -2; 1 0 -1].
    EXPECT_DOUBLE_EQ(evalScalar("h(1, 1, 1)"),  1.0);
    EXPECT_DOUBLE_EQ(evalScalar("h(2, 1, 1)"),  2.0);
    EXPECT_DOUBLE_EQ(evalScalar("h(1, 3, 1)"), -1.0);
    // Page 2 (z=1) center page weight: [2 0 -2; 4 0 -4; 2 0 -2].
    EXPECT_DOUBLE_EQ(evalScalar("h(1, 1, 2)"),  2.0);
    EXPECT_DOUBLE_EQ(evalScalar("h(2, 1, 2)"),  4.0);
    EXPECT_DOUBLE_EQ(evalScalar("h(2, 2, 2)"),  0.0);
}

TEST_F(FilterDesignTest, SobelZDirection)
{
    eval("h = fspecial3('sobel', 'Z');");
    // h(2, 2, 1) = -derivative at top page, smoothed = 4.
    EXPECT_DOUBLE_EQ(evalScalar("h(2, 2, 1)"),  4.0);
    EXPECT_DOUBLE_EQ(evalScalar("h(2, 2, 3)"), -4.0);
    EXPECT_DOUBLE_EQ(evalScalar("h(2, 2, 2)"),  0.0);
}

TEST_F(FilterDesignTest, EllipsoidDefaultSizeAndNormalized)
{
    eval("h = fspecial3('ellipsoid');");
    // semiaxes default 5 -> size 2*ceil(5)+1 = 11 per axis.
    EXPECT_EQ(static_cast<int>(evalScalar("size(h, 1)")), 11);
    EXPECT_EQ(static_cast<int>(evalScalar("size(h, 2)")), 11);
    EXPECT_EQ(static_cast<int>(evalScalar("size(h, 3)")), 11);
    EXPECT_NEAR(evalScalar("sum(h(:))"), 1.0, 1e-12);
    // 515 voxels inside the unit ball -> each = 1/515.
    EXPECT_EQ(static_cast<int>(evalScalar("nnz(h)")), 515);
    EXPECT_NEAR(evalScalar("h(6,6,6)"), 1.0/515.0, 1e-12);
}

TEST_F(FilterDesignTest, EllipsoidAnisotropicSemiaxes)
{
    eval("h = fspecial3('ellipsoid', [2 3 4]);");
    // size = 2*[2 3 4]+1 = [5 7 9]; semiaxes element 1->rows, 2->cols, 3->pages.
    EXPECT_EQ(static_cast<int>(evalScalar("size(h, 1)")), 5);
    EXPECT_EQ(static_cast<int>(evalScalar("size(h, 2)")), 7);
    EXPECT_EQ(static_cast<int>(evalScalar("size(h, 3)")), 9);
    EXPECT_NEAR(evalScalar("sum(h(:))"), 1.0, 1e-12);
    EXPECT_EQ(static_cast<int>(evalScalar("nnz(h)")), 99);
    EXPECT_NEAR(evalScalar("h(3,4,5)"), 1.0/99.0, 1e-12);  // center
}

TEST_F(FilterDesignTest, GaussianAnisotropicAxisMapping)
{
    // sigma element 1->rows, 2->cols, 3->pages. [0.5 5 5]: tight in rows.
    eval("h = fspecial3('gaussian', [3 3 3], [0.5 5 5]);");
    EXPECT_NEAR(evalScalar("sum(h(:))"), 1.0, 1e-12);
    EXPECT_NEAR(evalScalar("h(2,2,2)"), 0.0897980730345, 1e-10);  // center
    EXPECT_NEAR(evalScalar("h(1,1,1)"), 0.0116763276760, 1e-10);  // corner
    // A row-offset element (tight sigma) is much smaller than a col-offset one.
    EXPECT_LT(evalScalar("h(1,2,2)"), evalScalar("h(2,1,2)"));
}

TEST_F(FilterDesignTest, LaplacianGammaTwoParameter)
{
    eval("h = fspecial3('laplacian', 0.2, 0.3);");
    EXPECT_NEAR(evalScalar("sum(h(:))"), 0.0, 1e-12);
    EXPECT_NEAR(evalScalar("h(2,2,2)"), -4.2, 1e-12);   // center
    EXPECT_NEAR(evalScalar("h(2,2,1)"),  0.5, 1e-12);   // face (1-g1-g2)
    EXPECT_NEAR(evalScalar("h(1,2,1)"),  0.05, 1e-12);  // edge (g1/4)
    EXPECT_NEAR(evalScalar("h(1,1,1)"),  0.075, 1e-12); // corner (g2/4)
}

TEST_F(FilterDesignTest, LaplacianGammaValidation)
{
    bool threwSum = false, threwNeg = false;
    try { eval("fspecial3('laplacian', 0.6, 0.6);"); } catch (...) { threwSum = true; }
    try { eval("fspecial3('laplacian', -0.1, 0);"); } catch (...) { threwNeg = true; }
    EXPECT_TRUE(threwSum);   // g1+g2 > 1
    EXPECT_TRUE(threwNeg);   // g1 < 0
}

TEST_F(FilterDesignTest, LogDefaultZeroMean)
{
    eval("h = fspecial3('log');");
    EXPECT_EQ(static_cast<int>(evalScalar("size(h, 1)")), 5);
    EXPECT_NEAR(evalScalar("sum(h(:))"), 0.0, 1e-12);
    EXPECT_NEAR(evalScalar("h(3,3,3)"), -0.19398098016652943, 1e-10);  // center
    EXPECT_NEAR(evalScalar("h(1,1,1)"),  0.0032725085013668954, 1e-12); // corner
}

TEST_F(FilterDesignTest, LogAnisotropicSigma)
{
    eval("h = fspecial3('log', [5 5 5], [1 1.5 2]);");
    EXPECT_NEAR(evalScalar("sum(h(:))"), 0.0, 1e-12);
    EXPECT_NEAR(evalScalar("h(1,1,1)"),  0.0064097576165746944, 1e-10);
    EXPECT_NEAR(evalScalar("h(3,3,3)"), -0.047018462294465692,  1e-10);
}

TEST_F(FilterDesignTest, AverageSizeVector)
{
    eval("h = fspecial3('average', [3 5 7]);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(h, 1)")), 3);
    EXPECT_EQ(static_cast<int>(evalScalar("size(h, 2)")), 5);
    EXPECT_EQ(static_cast<int>(evalScalar("size(h, 3)")), 7);
    EXPECT_NEAR(evalScalar("h(1,1,1)"), 1.0/105.0, 1e-12);
}

TEST_F(FilterDesignTest, PrewittAllDirections)
{
    eval("hx = fspecial3('prewitt', 'X');");
    EXPECT_DOUBLE_EQ(evalScalar("hx(1,1,1)"),  1.0);
    EXPECT_DOUBLE_EQ(evalScalar("hx(1,3,1)"), -1.0);
    EXPECT_DOUBLE_EQ(evalScalar("hx(2,2,2)"),  0.0);
    eval("hy = fspecial3('prewitt', 'Y');");
    EXPECT_DOUBLE_EQ(evalScalar("hy(1,1,1)"),  1.0);
    EXPECT_DOUBLE_EQ(evalScalar("hy(3,1,1)"), -1.0);
    eval("hz = fspecial3('prewitt', 'Z');");
    EXPECT_DOUBLE_EQ(evalScalar("hz(1,1,1)"),  1.0);
    EXPECT_DOUBLE_EQ(evalScalar("hz(1,1,3)"), -1.0);
}

TEST_F(FilterDesignTest, FspecialBadTypeThrows)
{
    bool threw = false;
    try { eval("fspecial3('bogus');"); } catch (...) { threw = true; }
    EXPECT_TRUE(threw);
}

// ── fwind2 ────────────────────────────────────────────────────────────
TEST_F(FilterDesignTest, Fwind2BasicShape)
{
    eval("h = fwind2(ones(7), ones(7));");
    EXPECT_EQ(static_cast<int>(evalScalar("size(h, 1)")), 7);
    EXPECT_EQ(static_cast<int>(evalScalar("size(h, 2)")), 7);
    EXPECT_NEAR(evalScalar("sum(h(:))"), 1.0, 1e-9);
}

// Fwind2 with a shape mismatch was previously a hard-error; we now
// interpolate Hd to match W's size automatically (matches MATLAB).
// Test removed in cycle 65 when fsamp2/ftrans2/fwind1/fwind2 stubs
// were replaced with full implementations in fir2d.cpp.
//
// Likewise, the Fsamp2KnownGap / Ftrans2KnownGap / Fwind1KnownGap
// tests are removed — those functions are now implemented (see
// fir2d_test.cpp for their regression coverage).

TEST_F(FilterDesignTest, GaborKnownGap)
{
    bool threw = false;
    try { eval("gabor(4, 0);"); } catch (...) { threw = true; }
    EXPECT_TRUE(threw);
}
