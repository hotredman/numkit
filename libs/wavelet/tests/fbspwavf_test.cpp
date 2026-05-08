// libs/wavelet/tests/fbspwavf_test.cpp
// Audit ТЗ closure for fbspwavf. Closes audit/findings/wavelet/fbspwavf.md.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class FbspwavfTest : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// Frequency B-spline: ψ(t) = √fb · (sinc(fb·t/m))^m · exp(2πi·fc·t)

TEST_F(FbspwavfTest, M2BasicReference)
{
    eval("[psi, x] = fbspwavf(-5, 5, 8, 2, 1, 1);");
    EXPECT_NEAR(evalScalar("real(psi(4))"), -0.1434850854, 1e-10);
}

TEST_F(FbspwavfTest, M3HigherOrder)
{
    eval("[psi, x] = fbspwavf(-5, 5, 16, 3, 1, 1);");
    EXPECT_NEAR(evalScalar("real(psi(8))"), -0.4703304, 1e-7);
}

TEST_F(FbspwavfTest, AbsAtZeroIsSqrtFb)
{
    // |ψ(0)| = √fb · 1^m · 1 = √fb. fb=1 → 1.
    eval("[psi, x] = fbspwavf(-4, 4, 33, 2, 1, 1);");
    EXPECT_NEAR(evalScalar("real(psi(17))"), 1.0, 1e-10);
    EXPECT_NEAR(evalScalar("imag(psi(17))"), 0.0, 1e-10);
}

TEST_F(FbspwavfTest, RejectsBadOrder)
{
    bool threw = false;
    try { eval("fbspwavf(-5, 5, 8, 0, 1, 1);"); } catch (...) { threw = true; }
    EXPECT_TRUE(threw);
}
