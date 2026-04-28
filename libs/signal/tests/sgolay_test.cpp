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
    Engine engine;
    void SetUp() override { BuiltinLibrary::install(engine); }
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

TEST_F(SgolayTest, SgolayfiltMatrixThrows)
{
    EXPECT_THROW(eval("y = sgolayfilt(ones(5, 5), 2, 5);"), std::exception);
}
