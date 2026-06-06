// libs/image/tests/illum_test.cpp
//
// Regression guard for illumwhite + illumgray (white-balance
// illuminant estimation). Pinned against MATLAB R2025b on a
// deterministic 10×10×3 RGB image whose values are exactly
// representable; we asserted MATLAB outputs via direct probing and
// the inspected MATLAB toolbox source.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

#include <cmath>

using namespace numkit;

class IllumTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override {
        engine.eval("import compat.*;"
                    "A = zeros(10, 10, 3);"
                    "for i = 1:10;"
                    "  for j = 1:10;"
                    "    A(i,j,1) = 0.01 * (10*(i-1) + (j-1));"
                    "    A(i,j,2) = 0.01 * (10*(j-1) + (i-1));"
                    "    A(i,j,3) = 1.0 - A(i,j,1);"
                    "  end;"
                    "end;");
    }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// ── illumwhite ──────────────────────────────────────────────────────

TEST_F(IllumTest, IllumwhiteP0PerChannelMax)
{
    eval("v = illumwhite(A, 0);");
    EXPECT_DOUBLE_EQ(evalScalar("v(1)"), 0.99);
    EXPECT_DOUBLE_EQ(evalScalar("v(2)"), 0.99);
    EXPECT_DOUBLE_EQ(evalScalar("v(3)"), 1.00);
}

TEST_F(IllumTest, IllumwhiteDefaultP1)
{
    eval("v = illumwhite(A);");
    // P = 1 ⇒ K = 1 ⇒ 2nd-largest per channel.
    EXPECT_DOUBLE_EQ(evalScalar("v(1)"), 0.98);
    EXPECT_DOUBLE_EQ(evalScalar("v(2)"), 0.98);
    EXPECT_DOUBLE_EQ(evalScalar("v(3)"), 0.99);
}

TEST_F(IllumTest, IllumwhiteP5)
{
    eval("v = illumwhite(A, 5);");
    EXPECT_DOUBLE_EQ(evalScalar("v(1)"), 0.94);
    EXPECT_DOUBLE_EQ(evalScalar("v(2)"), 0.94);
    EXPECT_DOUBLE_EQ(evalScalar("v(3)"), 0.95);
}

TEST_F(IllumTest, IllumwhiteP50)
{
    eval("v = illumwhite(A, 50);");
    EXPECT_DOUBLE_EQ(evalScalar("v(1)"), 0.49);
    EXPECT_DOUBLE_EQ(evalScalar("v(2)"), 0.49);
    EXPECT_DOUBLE_EQ(evalScalar("v(3)"), 0.50);
}

TEST_F(IllumTest, IllumwhiteShape1x3)
{
    eval("v = illumwhite(A);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(v,1)")), 1);
    EXPECT_EQ(static_cast<int>(evalScalar("size(v,2)")), 3);
}

TEST_F(IllumTest, IllumwhitePOutOfRangeThrows)
{
    EXPECT_THROW(eval("illumwhite(A, -1);"), std::exception);
    EXPECT_THROW(eval("illumwhite(A, 100);"), std::exception);
    EXPECT_THROW(eval("illumwhite(A, 110);"), std::exception);
}

TEST_F(IllumTest, IllumwhiteBadShapeThrows)
{
    EXPECT_THROW(eval("illumwhite(zeros(5,5));"),    std::exception);
    EXPECT_THROW(eval("illumwhite(zeros(5,5,4));"),  std::exception);
}

TEST_F(IllumTest, IllumwhiteMask)
{
    // Mask out top-half rows ⇒ channel max should be the bottom-row max.
    // R = 0.01*(10*(i-1)+(j-1)); with i in [6..10], max R = 0.99 still
    // (last row, last col). G similarly. B = 1 - R: min in row 10
    // ⇒ max B from row 6 col 0 → B = 1 - 0.50 = 0.50. Actually B per
    // formula: B = 1 - R, so when i=6,j=1: R=0.50, B=0.50; i=6,j=10:
    // R=0.59, B=0.41. Max B over rows 6..10 is at i=6,j=1 → 0.50.
    eval("M = true(10, 10); M(1:5, :) = false;"
         "v = illumwhite(A, 0, 'Mask', M);");
    EXPECT_DOUBLE_EQ(evalScalar("v(1)"), 0.99);
    EXPECT_DOUBLE_EQ(evalScalar("v(2)"), 0.99);
    EXPECT_DOUBLE_EQ(evalScalar("v(3)"), 0.50);
}

// ── illumgray ───────────────────────────────────────────────────────

TEST_F(IllumTest, IllumgrayDefault)
{
    // Image mean per channel: R = G = 0.495, B = 0.505.
    eval("v = illumgray(A);");
    EXPECT_NEAR(evalScalar("v(1)"), 0.495, 1e-10);
    EXPECT_NEAR(evalScalar("v(2)"), 0.495, 1e-10);
    EXPECT_NEAR(evalScalar("v(3)"), 0.505, 1e-10);
}

TEST_F(IllumTest, IllumgrayVectorPercentile)
{
    // Trimming a symmetric distribution about the mean does not move
    // the mean — same result as default.
    eval("v = illumgray(A, [10 10]);");
    EXPECT_NEAR(evalScalar("v(1)"), 0.495, 1e-10);
    EXPECT_NEAR(evalScalar("v(2)"), 0.495, 1e-10);
    EXPECT_NEAR(evalScalar("v(3)"), 0.505, 1e-10);
}

TEST_F(IllumTest, IllumgrayScalarPercentile)
{
    eval("v = illumgray(A, 25);");
    EXPECT_NEAR(evalScalar("v(1)"), 0.495, 1e-10);
    EXPECT_NEAR(evalScalar("v(2)"), 0.495, 1e-10);
    EXPECT_NEAR(evalScalar("v(3)"), 0.505, 1e-10);
}

TEST_F(IllumTest, IllumgrayNormOption)
{
    // Norm = 2 ⇒ sqrt(sum(x^2)) / N (NOT sqrt(mean(x^2))).
    eval("v = illumgray(A, 1, 'Norm', 2);");
    // For the (sorted, trimmed-by-1%) sequence we get a specific
    // value; just compare against MATLAB R2025b reference.
    EXPECT_NEAR(evalScalar("v(1)"), 5.759198498657498e-02, 1e-12);
    EXPECT_NEAR(evalScalar("v(2)"), 5.759198498657498e-02, 1e-12);
    EXPECT_NEAR(evalScalar("v(3)"), 5.847116854502232e-02, 1e-12);
}

TEST_F(IllumTest, IllumgrayPercentileTooLargeThrows)
{
    EXPECT_THROW(eval("illumgray(A, [60 60]);"), std::exception);
    EXPECT_THROW(eval("illumgray(A, -1);"),       std::exception);
    EXPECT_THROW(eval("illumgray(A, 100);"),      std::exception);
}

TEST_F(IllumTest, IllumgrayBadShapeThrows)
{
    EXPECT_THROW(eval("illumgray(zeros(5,5));"),   std::exception);
    EXPECT_THROW(eval("illumgray(zeros(5,5,4));"), std::exception);
}

TEST_F(IllumTest, IllumgrayMaskShapeMismatchThrows)
{
    EXPECT_THROW(eval("illumgray(A, 0, 'Mask', true(8,10));"),
                 std::exception);
}

TEST_F(IllumTest, IllumgrayUnknownOptionThrows)
{
    EXPECT_THROW(eval("illumgray(A, 0, 'NotAnOption', 1);"),
                 std::exception);
}
