// libs/wavelet/tests/shanwavf_test.cpp
// shanwavf.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class ShanwavfTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// Shannon wavelet: ψ(t) = √fb · sinc(fb·t) · exp(2πi·fc·t)

TEST_F(ShanwavfTest, BasicReferenceValues)
{
    eval("[psi, x] = shanwavf(-5, 5, 8, 1, 1);");
    EXPECT_NEAR(evalScalar("real(psi(2))"),  0.0782871436, 1e-10);
    EXPECT_NEAR(evalScalar("real(psi(4))"), -0.0775286446, 1e-10);
}

TEST_F(ShanwavfTest, AbsAtZeroIsSqrtFb)
{
    // |ψ(0)| = √fb. fb=1 → 1.
    eval("[psi, x] = shanwavf(-4, 4, 33, 1, 1);");
    EXPECT_NEAR(evalScalar("real(psi(17))"), 1.0, 1e-10);
    EXPECT_NEAR(evalScalar("imag(psi(17))"), 0.0, 1e-10);
}

TEST_F(ShanwavfTest, NonDefaultBandwidth)
{
    eval("[psi, x] = shanwavf(-5, 5, 16, 0.5, 2);");
    EXPECT_NEAR(evalScalar("real(psi(1))"), 0.0900316, 1e-7);
    EXPECT_NEAR(evalScalar("real(psi(8))"), -0.337619, 1e-6);
}
