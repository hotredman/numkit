// libs/builtin/tests/poly_test.cpp
//
// Polynomial-domain builtins. roots now; polyder/polyint/polyval to
// land alongside.

#include "dual_engine_fixture.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

using namespace m_test;

class PolyTest : public DualEngineTest
{};

// ── roots ──────────────────────────────────────────────────────

TEST_P(PolyTest, RootsLinearReturnsConstant)
{
    // 2x - 4 = 0 → x = 2.
    eval("r = roots([2 -4]);");
    auto *r = getVarPtr("r");
    EXPECT_EQ(r->numel(), 1u);
    EXPECT_NEAR(r->elemAsDouble(0), 2.0, 1e-12);
}

TEST_P(PolyTest, RootsRealQuadratic)
{
    // (x - 3)(x - 5) = x² - 8x + 15.
    eval("r = sort(roots([1 -8 15]));");
    auto *r = getVarPtr("r");
    EXPECT_EQ(r->numel(), 2u);
    EXPECT_NEAR(r->elemAsDouble(0), 3.0, 1e-10);
    EXPECT_NEAR(r->elemAsDouble(1), 5.0, 1e-10);
}

TEST_P(PolyTest, RootsComplexConjugatePair)
{
    // x² + 1 → roots ±i.
    eval("r = roots([1 0 1]);");
    auto *r = getVarPtr("r");
    EXPECT_EQ(r->type(), MType::COMPLEX);
    EXPECT_EQ(r->numel(), 2u);
    // Order isn't specified — sort by imaginary part for predictability.
    std::vector<double> imags{ r->complexData()[0].imag(), r->complexData()[1].imag() };
    std::sort(imags.begin(), imags.end());
    EXPECT_NEAR(imags[0], -1.0, 1e-10);
    EXPECT_NEAR(imags[1],  1.0, 1e-10);
    // Real parts both ~0.
    EXPECT_NEAR(r->complexData()[0].real(), 0.0, 1e-10);
    EXPECT_NEAR(r->complexData()[1].real(), 0.0, 1e-10);
}

TEST_P(PolyTest, RootsCubicMixedRoots)
{
    // (x - 1)(x² + 4) = x³ - x² + 4x - 4 → roots 1, ±2i.
    eval("r = roots([1 -1 4 -4]);");
    auto *r = getVarPtr("r");
    EXPECT_EQ(r->type(), MType::COMPLEX);
    EXPECT_EQ(r->numel(), 3u);

    int realCount = 0, complexCount = 0;
    double realRoot = 0;
    std::vector<double> imags;
    for (size_t i = 0; i < 3; ++i) {
        const auto c = r->complexData()[i];
        if (std::abs(c.imag()) < 1e-9) {
            ++realCount;
            realRoot = c.real();
        } else {
            ++complexCount;
            imags.push_back(c.imag());
        }
    }
    EXPECT_EQ(realCount, 1);
    EXPECT_EQ(complexCount, 2);
    EXPECT_NEAR(realRoot, 1.0, 1e-9);
    std::sort(imags.begin(), imags.end());
    EXPECT_NEAR(imags[0], -2.0, 1e-9);
    EXPECT_NEAR(imags[1],  2.0, 1e-9);
}

TEST_P(PolyTest, RootsTrailingZerosBecomeOriginRoots)
{
    // x³ - 2x² = x²·(x - 2). Roots: 2, 0, 0.
    eval("r = sort(roots([1 -2 0 0]));");
    auto *r = getVarPtr("r");
    EXPECT_EQ(r->numel(), 3u);
    EXPECT_NEAR(r->elemAsDouble(0), 0.0, 1e-12);
    EXPECT_NEAR(r->elemAsDouble(1), 0.0, 1e-12);
    EXPECT_NEAR(r->elemAsDouble(2), 2.0, 1e-9);
}

TEST_P(PolyTest, RootsLeadingZerosStripped)
{
    // [0 0 1 -3 2] should be treated as [1 -3 2] → roots 1, 2.
    eval("r = sort(roots([0 0 1 -3 2]));");
    auto *r = getVarPtr("r");
    EXPECT_EQ(r->numel(), 2u);
    EXPECT_NEAR(r->elemAsDouble(0), 1.0, 1e-10);
    EXPECT_NEAR(r->elemAsDouble(1), 2.0, 1e-10);
}

TEST_P(PolyTest, RootsConstantPolyEmpty)
{
    // p = [5] (constant 5) has no roots.
    eval("r = roots([5]);");
    auto *r = getVarPtr("r");
    EXPECT_EQ(r->numel(), 0u);
}

TEST_P(PolyTest, RootsEmptyInputEmpty)
{
    eval("r = roots([]);");
    auto *r = getVarPtr("r");
    EXPECT_EQ(r->numel(), 0u);
}

TEST_P(PolyTest, RootsHigherOrderApproximate)
{
    // (x - 1)(x - 2)(x - 3)(x - 4) = x⁴ - 10x³ + 35x² - 50x + 24.
    eval("r = sort(roots([1 -10 35 -50 24]));");
    auto *r = getVarPtr("r");
    EXPECT_EQ(r->numel(), 4u);
    EXPECT_NEAR(r->elemAsDouble(0), 1.0, 1e-8);
    EXPECT_NEAR(r->elemAsDouble(1), 2.0, 1e-8);
    EXPECT_NEAR(r->elemAsDouble(2), 3.0, 1e-8);
    EXPECT_NEAR(r->elemAsDouble(3), 4.0, 1e-8);
}

TEST_P(PolyTest, RootsComplexInputThrows)
{
    EXPECT_THROW(eval("r = roots([1+2i, 3, 4]);"), std::exception);
}

TEST_P(PolyTest, RootsMatrixInputThrows)
{
    EXPECT_THROW(eval("r = roots([1 2; 3 4]);"), std::exception);
}

// ── polyder ────────────────────────────────────────────────────

TEST_P(PolyTest, PolyderQuadratic)
{
    // p = x² + 2x + 3 → p' = 2x + 2.
    eval("d = polyder([1 2 3]);");
    auto *d = getVarPtr("d");
    EXPECT_EQ(d->numel(), 2u);
    EXPECT_DOUBLE_EQ(d->doubleData()[0], 2.0);
    EXPECT_DOUBLE_EQ(d->doubleData()[1], 2.0);
}

TEST_P(PolyTest, PolyderCubic)
{
    // p = 3x³ - x² + 5 → p' = 9x² - 2x.
    eval("d = polyder([3 -1 0 5]);");
    auto *d = getVarPtr("d");
    EXPECT_EQ(d->numel(), 3u);
    EXPECT_DOUBLE_EQ(d->doubleData()[0],  9.0);
    EXPECT_DOUBLE_EQ(d->doubleData()[1], -2.0);
    EXPECT_DOUBLE_EQ(d->doubleData()[2],  0.0);
}

TEST_P(PolyTest, PolyderConstantIsZero)
{
    eval("d = polyder([7]);");
    auto *d = getVarPtr("d");
    EXPECT_EQ(d->numel(), 1u);
    EXPECT_DOUBLE_EQ(d->doubleData()[0], 0.0);
}

TEST_P(PolyTest, PolyderQuotientForm)
{
    // d/dx ((x + 1) / (x - 1)) = -2 / (x - 1)²
    // num: a·b' - b·a' = (x-1)·1 - (x+1)·1 = -2.
    // den: (x - 1)² = x² - 2x + 1.
    eval("[num, den] = polyder([1 1], [1 -1]);");
    auto *num = getVarPtr("num");
    auto *den = getVarPtr("den");
    EXPECT_EQ(num->numel(), 1u);
    EXPECT_DOUBLE_EQ(num->doubleData()[0], -2.0);
    EXPECT_EQ(den->numel(), 3u);
    EXPECT_DOUBLE_EQ(den->doubleData()[0], 1.0);
    EXPECT_DOUBLE_EQ(den->doubleData()[1], -2.0);
    EXPECT_DOUBLE_EQ(den->doubleData()[2], 1.0);
}

TEST_P(PolyTest, PolyderComplexThrows)
{
    EXPECT_THROW(eval("d = polyder([1+2i, 3]);"), std::exception);
}

// ── polyint ────────────────────────────────────────────────────

TEST_P(PolyTest, PolyintQuadratic)
{
    // ∫ (x² + 2x + 3) dx = (1/3)x³ + x² + 3x + 0.
    eval("P = polyint([1 2 3]);");
    auto *P = getVarPtr("P");
    EXPECT_EQ(P->numel(), 4u);
    EXPECT_NEAR(P->doubleData()[0], 1.0/3.0, 1e-15);
    EXPECT_DOUBLE_EQ(P->doubleData()[1], 1.0);
    EXPECT_DOUBLE_EQ(P->doubleData()[2], 3.0);
    EXPECT_DOUBLE_EQ(P->doubleData()[3], 0.0);
}

TEST_P(PolyTest, PolyintWithConstant)
{
    eval("P = polyint([1 2], 5);");
    auto *P = getVarPtr("P");
    EXPECT_EQ(P->numel(), 3u);
    EXPECT_DOUBLE_EQ(P->doubleData()[0], 0.5);  // x²/2
    EXPECT_DOUBLE_EQ(P->doubleData()[1], 2.0);  // 2x
    EXPECT_DOUBLE_EQ(P->doubleData()[2], 5.0);  // constant
}

TEST_P(PolyTest, PolyintInverseOfPolyder)
{
    // polyder(polyint(p)) should equal p (modulo trailing-constant differences).
    eval("p = [1 -3 2 4];"
         "P = polyint(p);"
         "p2 = polyder(P);"
         "delta = max(abs(p - p2));");
    EXPECT_LT(evalScalar("delta;"), 1e-12);
}

TEST_P(PolyTest, PolyintEmptyReturnsConstant)
{
    eval("P = polyint([], 7);");
    auto *P = getVarPtr("P");
    EXPECT_EQ(P->numel(), 1u);
    EXPECT_DOUBLE_EQ(P->doubleData()[0], 7.0);
}

TEST_P(PolyTest, PolyintComplexThrows)
{
    EXPECT_THROW(eval("P = polyint([1+2i, 3]);"), std::exception);
}

INSTANTIATE_DUAL(PolyTest);
