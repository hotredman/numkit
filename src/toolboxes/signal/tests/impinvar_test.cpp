// toolboxes/signal/tests/impinvar_test.cpp
//
// Regression guard for impinvar (impulse-invariance analog→digital).
// bugs/signal/impinvar-repeated-poles.md: REPEATED poles now give the correct
// numerator (the distinct path used r_k=b(p_k)/a'(p_k), which is ∞ at a
// multiple root). General fix: cluster roots (centroid + Newton-refine on
// a^{(m-1)}), partial fractions with multiplicity, impulse-invariant Eulerian
// z-kernel. Bit-equal MATLAB R2025b.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class ImpinvarTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override {}
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// Distinct simple poles (the already-working path) — regression.
TEST_F(ImpinvarTest, DistinctPoles)
{
    eval("[bz, az] = impinvar(1, [1 3 2], 10);");   // poles -1, -2
    EXPECT_NEAR(evalScalar("bz(1)"), 0.0, 1e-12);
    EXPECT_NEAR(evalScalar("bz(2)"), 0.008610666496, 1e-10);
    EXPECT_NEAR(evalScalar("az(2)"), -1.723568171, 1e-9);
    EXPECT_NEAR(evalScalar("az(3)"), 0.7408182207, 1e-9);
}

// Double pole 1/(s+1)^2 — the repro from the bug md.
TEST_F(ImpinvarTest, DoublePole)
{
    eval("[bz, az] = impinvar(1, [1 2 1], 10);");
    EXPECT_NEAR(evalScalar("bz(1)"), 0.0, 1e-12);
    EXPECT_NEAR(evalScalar("bz(2)"), 0.00904837418, 1e-10);
    EXPECT_NEAR(evalScalar("az(2)"), -1.809674836, 1e-9);
    EXPECT_NEAR(evalScalar("az(3)"), 0.8187307531, 1e-9);
}

// Triple pole 1/(s+1)^3 — exercises the Eulerian N_3 = w + w^2 kernel.
TEST_F(ImpinvarTest, TriplePole)
{
    eval("[bz, az] = impinvar(1, [1 3 3 1], 10);");
    EXPECT_NEAR(evalScalar("bz(1)"), 0.0, 1e-12);
    EXPECT_NEAR(evalScalar("bz(2)"), 0.000452418709, 1e-11);
    EXPECT_NEAR(evalScalar("bz(3)"), 0.0004093653765, 1e-11);
    EXPECT_NEAR(evalScalar("az(2)"), -2.714512254, 1e-8);
    EXPECT_NEAR(evalScalar("az(4)"), -0.7408182207, 1e-8);
}

// Quadruple pole (s+1)^4 — N_4 = w(1+4w+w^2) + refinement on a'''.
TEST_F(ImpinvarTest, QuadruplePole)
{
    eval("[bz, az] = impinvar(1, [1 4 6 4 1], 10);");
    EXPECT_NEAR(evalScalar("bz(3)"), 5.458205021e-05, 1e-12);
    EXPECT_NEAR(evalScalar("az(5)"), 0.670320046, 1e-8);
}

// Mixed multiplicity: [1 2]/((s+1)^2 (s+2)) — double pole -1 + simple pole -2.
TEST_F(ImpinvarTest, MixedMultiplicity)
{
    eval("[bz, az] = impinvar([1 2], [1 4 5 2], 10);");
    EXPECT_NEAR(evalScalar("bz(2)"),  0.00904837418, 1e-10);
    EXPECT_NEAR(evalScalar("bz(3)"), -0.007408182207, 1e-10);
    EXPECT_NEAR(evalScalar("az(2)"), -2.628405589, 1e-8);
    EXPECT_NEAR(evalScalar("az(4)"), -0.670320046, 1e-8);
}
