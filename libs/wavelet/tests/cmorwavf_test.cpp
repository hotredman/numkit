// libs/wavelet/tests/cmorwavf_test.cpp
// cmorwavf.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class CmorwavfTest : public ::testing::Test
{
public:
    StdEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// ψ(t) = (1/√(π·fb))·exp(2πi·fc·t)·exp(-t²/fb)

TEST_F(CmorwavfTest, DefaultArgs)
{
    // Bug fix 2026-05-08: 3-arg form was throwing instead of using
    // defaults fb=1, fc=1 (MATLAB R2025b behavior).
    eval("[psi, x] = cmorwavf(-5, 5, 8);");
    EXPECT_NEAR(evalScalar("real(psi(4))"), -0.0753732289, 1e-9);
    EXPECT_NEAR(evalScalar("imag(psi(4))"),  0.3302316928, 1e-9);
}

TEST_F(CmorwavfTest, ExplicitArgs)
{
    eval("[psi, x] = cmorwavf(-5, 5, 8, 1.5, 1);");
    EXPECT_NEAR(evalScalar("real(psi(4))"), -0.0729509743, 1e-9);
    EXPECT_NEAR(evalScalar("imag(psi(4))"),  0.3196191020, 1e-9);
}

TEST_F(CmorwavfTest, AbsAtZeroIsPeak)
{
    // |ψ(0)| = 1/√(π·fb); for fb=1 ≈ 0.5642.
    eval("[psi, x] = cmorwavf(-4, 4, 33, 1, 1);");
    EXPECT_NEAR(evalScalar("abs(psi(17))"), 0.5641895835, 1e-9);
}

TEST_F(CmorwavfTest, Empty3argThrowsClean)
{
    bool threw = false;
    try { eval("cmorwavf(0);"); } catch (...) { threw = true; }
    EXPECT_TRUE(threw);
}
