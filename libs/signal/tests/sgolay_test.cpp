// libs/signal/tests/sgolay_test.cpp
//
// Savitzky-Golay smoothing filter — sgolay() projection matrix and
// sgolayfilt() applied to vectors.

#include <numkit/core/engine.hpp>
#include <numkit/builtin/library.hpp>

#include <gtest/gtest.h>

#include <cmath>

using namespace numkit;

class SgolayTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &code) { return engine.eval(code); }
    double evalScalar(const std::string &code) { return eval(code).toScalar(); }
};

// ── sgolay (projection matrix) ─────────────────────────────────

TEST_F(SgolayTest, SgolayShape)
{
    eval("B = sgolay(2, 5);");
    EXPECT_DOUBLE_EQ(evalScalar("size(B, 1);"), 5.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(B, 2);"), 5.0);
}

// [B,G] = sgolay(order,framelen): the 2nd output G is the framelen×(order+1)
// differentiation-filter matrix G = V*(V'V)^-1. Previously unimplemented
// ([b,g]=sgolay(...) errored 'Undefined variable g'). vs MATLAB R2025b.
// DEEP-PROBE 2026-05-31.
TEST_F(SgolayTest, SgolayDiffMatrix)
{
    eval("[B, G] = sgolay(3, 5);");
    EXPECT_DOUBLE_EQ(evalScalar("size(G, 1);"), 5.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(G, 2);"), 4.0);   // order+1
    // G(:,1) = smoothing filter [-3 12 17 12 -3]/35 (= central row of B).
    EXPECT_NEAR(evalScalar("G(1,1);"), -3.0 / 35.0, 1e-12);
    EXPECT_NEAR(evalScalar("G(3,1);"), 17.0 / 35.0, 1e-12);
    EXPECT_NEAR(evalScalar("G(3,1) - B(3,3);"), 0.0, 1e-12);  // G(:,1)==B center
    // G(:,2) = first-derivative filter [1 -8 0 8 -1]/12.
    EXPECT_NEAR(evalScalar("G(1,2);"),  1.0 / 12.0, 1e-12);
    EXPECT_NEAR(evalScalar("G(2,2);"), -8.0 / 12.0, 1e-12);
    EXPECT_NEAR(evalScalar("G(3,2);"),  0.0,        1e-12);
    EXPECT_NEAR(evalScalar("G(4,2);"),  8.0 / 12.0, 1e-12);
    // G(:,3) = [2 -1 -2 -1 2]/14.
    EXPECT_NEAR(evalScalar("G(1,3);"),  2.0 / 14.0, 1e-12);
    EXPECT_NEAR(evalScalar("G(3,3);"), -2.0 / 14.0, 1e-12);
}

TEST_F(SgolayTest, SgolayCenterRowSums)
{
    // For any order, the central-row coefficients sum to 1 (preserves
    // a constant signal: a constant input maps to itself).
    eval("B = sgolay(3, 7);"
         "s = sum(B(4, :));");
    EXPECT_NEAR(evalScalar("s;"), 1.0, 1e-12);
}

TEST_F(SgolayTest, SgolayOrderOneIsAverage)
{
    // sgolay(0, framelen) reduces to a moving-average filter:
    // central-row coefficients all equal 1/framelen.
    eval("B = sgolay(0, 5);");
    for (int j = 1; j <= 5; ++j) {
        const std::string code = "B(3, " + std::to_string(j) + ");";
        EXPECT_NEAR(evalScalar(code), 0.2, 1e-12);
    }
}

TEST_F(SgolayTest, SgolayEvenFramelenThrows)
{
    EXPECT_THROW(eval("B = sgolay(2, 6);"), std::exception);
}

TEST_F(SgolayTest, SgolayOrderTooHighThrows)
{
    EXPECT_THROW(eval("B = sgolay(7, 5);"), std::exception);
}

// ── sgolayfilt ─────────────────────────────────────────────────

TEST_F(SgolayTest, SgolayfiltConstantSignalUnchanged)
{
    // A constant signal must come out exactly equal (ignoring fp noise).
    eval("x = ones(1, 20) * 3.7;"
         "y = sgolayfilt(x, 2, 5);"
         "delta = max(abs(y - x));");
    EXPECT_LT(evalScalar("delta;"), 1e-12);
}

TEST_F(SgolayTest, SgolayfiltLinearSignalUnchanged)
{
    // Linear x → output equals input for any order ≥ 1 (polynomial fit
    // is exact for degree-1 input with degree ≥ 1 filter).
    eval("x = (1:30);"
         "y = sgolayfilt(x, 2, 5);"
         "delta = max(abs(y - x));");
    EXPECT_LT(evalScalar("delta;"), 1e-10);
}

TEST_F(SgolayTest, SgolayfiltQuadraticUnchangedAtOrderTwo)
{
    // x = (1:N).^2 → exactly fit by degree-2 polynomial.
    eval("x = (1:20).^2;"
         "y = sgolayfilt(x, 2, 5);"
         "delta = max(abs(y - x));");
    EXPECT_LT(evalScalar("delta;"), 1e-8);
}

TEST_F(SgolayTest, SgolayfiltSmoothsNoise)
{
    // A noisy signal should have lower variance after smoothing.
    eval("rng(0);"
         "x = sin((1:200) / 10) + 0.5 * randn(1, 200);"
         "y = sgolayfilt(x, 2, 11);"
         "vx = var(x);"
         "vy = var(y);");
    // Smoothing should reduce the noise floor — variance should drop.
    EXPECT_LT(evalScalar("vy;"), evalScalar("vx;"));
}

TEST_F(SgolayTest, SgolayfiltShapePreserved)
{
    eval("y = sgolayfilt((1:30)', 2, 5);");
    EXPECT_DOUBLE_EQ(evalScalar("size(y, 1);"), 30.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(y, 2);"), 1.0);
}

TEST_F(SgolayTest, SgolayfiltShortSignalThrows)
{
    EXPECT_THROW(eval("y = sgolayfilt([1 2 3], 2, 5);"), std::exception);
}

TEST_F(SgolayTest, SgolayfiltComplexThrows)
{
    EXPECT_THROW(eval("y = sgolayfilt([1+2i, 3, 5, 7, 9], 2, 5);"), std::exception);
}

// ── matrix (per-column / per-row) + weights + dim (DEEP-PROBE 2026-05-31) ──
// sgolayfilt previously errored on matrices and ignored the weights/dim args.

TEST_F(SgolayTest, SgolayfiltMatrixDim1Columns)
{
    // Each column filtered independently (default dim = 1).
    eval("C = sgolayfilt([2 5;1 8;3 9;4 7;6 2], 1, 3);");
    EXPECT_DOUBLE_EQ(evalScalar("size(C,1);"), 5.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(C,2);"), 2.0);
    EXPECT_NEAR(evalScalar("C(1,1);"), 1.5, 1e-9);
    EXPECT_NEAR(evalScalar("C(3,1);"), 2.6666666666666667, 1e-9);
    EXPECT_NEAR(evalScalar("C(1,2);"), 5.3333333333333333, 1e-9);
    EXPECT_NEAR(evalScalar("C(5,2);"), 2.5, 1e-9);
}

TEST_F(SgolayTest, SgolayfiltMatrixDim2Rows)
{
    // dim = 2 filters each row.
    eval("R = sgolayfilt([2 5 1 8 3;9 4 7 6 2], 1, 3, [], 2);");
    EXPECT_DOUBLE_EQ(evalScalar("size(R,1);"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(R,2);"), 5.0);
    EXPECT_NEAR(evalScalar("R(1,1);"), 3.1666666666666667, 1e-9);
    EXPECT_NEAR(evalScalar("R(2,5);"), 2.5, 1e-9);
}

TEST_F(SgolayTest, SgolayfiltWeighted)
{
    // Weighted least-squares: result differs from the unweighted fit.
    eval("x = [2 5 1 8 3 9 4 7 6];");
    eval("yw = sgolayfilt(x, 2, 5, [1 2 3 2 1]);");
    EXPECT_NEAR(evalScalar("yw(1);"), 2.5333333333333335, 1e-9);
    EXPECT_NEAR(evalScalar("yw(5);"), 6.0, 1e-9);
    EXPECT_NEAR(evalScalar("yw(9);"), 5.8666666666666671, 1e-9);
    // Confirm weighting actually changed the output vs unweighted.
    eval("yu = sgolayfilt(x, 2, 5);");
    EXPECT_GT(std::abs(evalScalar("yw(1) - yu(1);")), 1e-6);
}

TEST_F(SgolayTest, SgolayfiltColumnVectorUnchanged)
{
    // A column vector is still a single-column matrix → one filtered slice.
    eval("y = sgolayfilt((1:30)', 2, 5);");
    EXPECT_DOUBLE_EQ(evalScalar("size(y,1);"), 30.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(y,2);"), 1.0);
    EXPECT_NEAR(evalScalar("y(15);"), 15.0, 1e-9);
}

TEST_F(SgolayTest, SgolayfiltWeightsWrongLengthThrows)
{
    EXPECT_THROW(eval("y = sgolayfilt([1 2 3 4 5], 2, 5, [1 2 3]);"),
                 std::exception);
}
