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

INSTANTIATE_DUAL(PolyTest);
