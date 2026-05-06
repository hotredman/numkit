// libs/wavelet/tests/gauswavf_test.cpp
//
// Backfill gtest for libs/wavelet/src/shape/gauss.cpp::gauswavf.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class GauswavfTest : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// MATLAB R2025b reference (verified during cycle 59):
//   p=1: psi(t=-1) = +0.6572  psi(t=+1) = -0.6572
//   p=2: psi(0)    = +1.0314
//   p=4: psi(0)    = +1.0461

TEST_F(GauswavfTest, P1Default)
{
    eval("[psi, x] = gauswavf(-5, 5, 11);");
    EXPECT_NEAR(evalScalar("psi(5)"),  0.6572, 1e-3);
    EXPECT_NEAR(evalScalar("psi(7)"), -0.6572, 1e-3);
}

TEST_F(GauswavfTest, P2EvenSymmetric)
{
    eval("[psi, x] = gauswavf(-5, 5, 11, 2);");
    EXPECT_NEAR(evalScalar("psi(6)"),  1.0314, 1e-3);
    EXPECT_NEAR(evalScalar("psi(5)"), -0.3794, 1e-3);
}

TEST_F(GauswavfTest, P4EvenPeak)
{
    eval("[psi, x] = gauswavf(-5, 5, 11, 4);");
    EXPECT_NEAR(evalScalar("psi(6)"),  1.0461, 1e-3);
}

TEST_F(GauswavfTest, GridLength)
{
    eval("[psi, x] = gauswavf(-5, 5, 11);");
    EXPECT_EQ(static_cast<size_t>(evalScalar("numel(psi)")), 11u);
    EXPECT_EQ(static_cast<size_t>(evalScalar("numel(x)")), 11u);
    EXPECT_DOUBLE_EQ(evalScalar("x(1)"), -5);
    EXPECT_DOUBLE_EQ(evalScalar("x(11)"), 5);
}
