// libs/signal/tests/besself_test.cpp
//
// DEEP-PROBE 2026-06: besself ran a digital (bilinear) path by default and
// returned binomial (s+Wo)^n garbage; only besself(...,'s') was correct.
// MATLAB besself is ALWAYS analog (no digital Bessel), so besself now forces
// the analog path. Reference coefficients: MATLAB R2025b.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class BesselfTest : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// Default (no 's') must give the ANALOG Bessel filter — was [1 3 3 1].
TEST_F(BesselfTest, LowpassDefaultIsAnalog)
{
    eval("[b, a] = besself(3, 1);");
    EXPECT_NEAR(evalScalar("a(1)"), 1.0,        1e-9);
    EXPECT_NEAR(evalScalar("a(2)"), 2.432881,   1e-5);
    EXPECT_NEAR(evalScalar("a(3)"), 2.466212,   1e-5);
    EXPECT_NEAR(evalScalar("a(4)"), 1.0,        1e-9);
    // Numerator is a constant (unit DC gain): b = [0 0 0 1].
    EXPECT_NEAR(evalScalar("b(4)"), 1.0,        1e-9);
    EXPECT_NEAR(evalScalar("b(1)"), 0.0,        1e-12);
}

// Explicit 's' gives the same (it is redundant for besself).
TEST_F(BesselfTest, ExplicitSMatchesDefault)
{
    eval("[bd, ad] = besself(3, 1); [bs, as] = besself(3, 1, 's');");
    EXPECT_DOUBLE_EQ(evalScalar("isequal(ad, as)"), 1.0);
}

// Order 2 and a scaled cutoff.
TEST_F(BesselfTest, Order2AndScaled)
{
    eval("[b2, a2] = besself(2, 1);");
    EXPECT_NEAR(evalScalar("a2(2)"), 1.732051, 1e-5);
    eval("[b4, a4] = besself(4, 2);");
    EXPECT_NEAR(evalScalar("a4(2)"), 6.247880,  1e-4);
    EXPECT_NEAR(evalScalar("a4(5)"), 16.0,      1e-6);
}

// Highpass transform.
TEST_F(BesselfTest, Highpass)
{
    eval("[bh, ah] = besself(3, 2, 'high');");
    EXPECT_NEAR(evalScalar("ah(2)"), 4.932424, 1e-4);
    EXPECT_NEAR(evalScalar("ah(4)"), 8.0,      1e-5);
}

// Direct C++ API.
TEST_F(BesselfTest, PublicApi)
{
    eval("[b, a] = besself(2, 1);");
    Value a = *engine.getVariable("a");
    ASSERT_EQ(a.numel(), 3u);
    EXPECT_NEAR(a.elemAsDouble(1), 1.732051, 1e-5);
}
