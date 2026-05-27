// libs/image/tests/filter_design_test.cpp
//
// Regression guard for cycle 4 image filter design: fspecial3 + fwind2.
// fsamp2 / ftrans2 / fwind1 / gabor are stubbed with KNOWN GAP errors.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class FilterDesignTest : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
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

TEST_F(FilterDesignTest, EllipsoidNormalized)
{
    eval("h = fspecial3('ellipsoid');");
    EXPECT_NEAR(evalScalar("sum(h(:))"), 1.0, 1e-12);
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
