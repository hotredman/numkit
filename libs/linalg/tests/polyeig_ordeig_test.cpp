// libs/linalg/tests/polyeig_ordeig_test.cpp
//
// Regression guard for polyeig + ordeig. Spec at
// tools/parity/specs/{polyeig,ordeig}.json — both OK vs MATLAB R2025b.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

#include <cmath>

using namespace numkit;

class PolyeigOrdeigTest : public ::testing::Test
{
public:
    StdEngine engine;
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// polyeig — linear case (A0 + λI)x = 0 → eigvals(-A0).
TEST_F(PolyeigOrdeigTest, PolyeigLinearGivesEigvalsOfMinusA0)
{
    eval("A0 = [2 0; 0 3]; A1 = eye(2);"
         "e = polyeig(A0, A1);"
         "ere = sort(real(e));"
         "ei  = max(abs(imag(e)));");
    EXPECT_NEAR(evalScalar("ere(1)"), -3.0, 1e-9);
    EXPECT_NEAR(evalScalar("ere(2)"), -2.0, 1e-9);
    EXPECT_LT(evalScalar("ei"), 1e-9);
}

// polyeig — quadratic real (λ² - 5λ + 6)·I → 4 eigvals = {2,2,3,3}.
TEST_F(PolyeigOrdeigTest, PolyeigQuadraticRepeatedRoots)
{
    eval("A0 = 6*eye(2); A1 = -5*eye(2); A2 = eye(2);"
         "e = polyeig(A0, A1, A2);"
         "near2 = sum(abs(real(e) - 2) < 1e-3);"
         "near3 = sum(abs(real(e) - 3) < 1e-3);"
         "ei = max(abs(imag(e)));");
    EXPECT_EQ(static_cast<int>(evalScalar("near2")), 2);
    EXPECT_EQ(static_cast<int>(evalScalar("near3")), 2);
    EXPECT_LT(evalScalar("ei"), 1e-5);
}

// ordeig on diagonal T — order preserved (NO sort).
TEST_F(PolyeigOrdeigTest, OrdeigDiagonalPreservesOrder)
{
    eval("T = diag([3 1 2]);"
         "e = ordeig(T);");
    EXPECT_DOUBLE_EQ(evalScalar("e(1)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("e(2)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("e(3)"), 2.0);
}

// ordeig on real Schur with 2×2 block: emits the complex pair from
// the block formula.
TEST_F(PolyeigOrdeigTest, OrdeigRealSchurBlockGivesComplexPair)
{
    eval("T = [5 0 0; 0 0.5 -1.5; 0 1.5 0.5];"
         "e = ordeig(T);"
         "r1 = real(e(1)); i1 = imag(e(1));"
         "r2 = real(e(2)); i2 = imag(e(2));"
         "r3 = real(e(3)); i3 = imag(e(3));");
    EXPECT_DOUBLE_EQ(evalScalar("r1"), 5.0);
    EXPECT_DOUBLE_EQ(evalScalar("i1"), 0.0);
    EXPECT_NEAR(evalScalar("r2"),  0.5, 1e-12);
    EXPECT_NEAR(std::abs(evalScalar("i2")), 1.5, 1e-12);
    EXPECT_NEAR(evalScalar("r3"),  0.5, 1e-12);
    EXPECT_NEAR(std::abs(evalScalar("i3")), 1.5, 1e-12);
    EXPECT_NEAR(evalScalar("i2") + evalScalar("i3"), 0.0, 1e-12);
}
