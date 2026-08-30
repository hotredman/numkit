// toolboxes/wavelet/tests/cgauwavf_test.cpp
//
// Backfill gtest for toolboxes/wavelet/src/shape/gauss.cpp::cgauwavf.

#include <numkit/core/engine.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class CgauwavfTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override {}
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// MATLAB R2025b reference (from cycle 59 probe):
//   p=1: real(psi(t=-1)) = 0.4598   imag(psi(t=-1)) = 0.2734
//   p=2: real(psi(0))    = -0.8088

TEST_F(CgauwavfTest, P1Default)
{
    eval("[psi, x] = cgauwavf(-5, 5, 11);");
    EXPECT_NEAR(evalScalar("real(psi(5))"),  0.4598, 1e-3);
    EXPECT_NEAR(evalScalar("imag(psi(5))"),  0.2734, 1e-3);
}

TEST_F(CgauwavfTest, P2RealAtZero)
{
    eval("[psi, x] = cgauwavf(-5, 5, 11, 2);");
    EXPECT_NEAR(evalScalar("real(psi(6))"), -0.8088, 1e-3);
    EXPECT_NEAR(evalScalar("imag(psi(6))"),  0.0,    1e-3);
}

TEST_F(CgauwavfTest, GridLength)
{
    eval("[psi, x] = cgauwavf(-5, 5, 11);");
    EXPECT_EQ(static_cast<size_t>(evalScalar("numel(psi)")), 11u);
    EXPECT_DOUBLE_EQ(evalScalar("x(1)"), -5);
    EXPECT_DOUBLE_EQ(evalScalar("x(11)"), 5);
}

TEST_F(CgauwavfTest, GridDependentNormalisation)
{
    // cgauwavf normalisation is trapezoidal on the grid → values
    // change with grid density (matches MATLAB behavior).
    eval("[a,~] = cgauwavf(-5, 5, 11); [b,~] = cgauwavf(-5, 5, 101);");
    EXPECT_NE(evalScalar("real(a(5))"), evalScalar("real(b(41))"));
}

// Bug fix 2026-05-08 — added 'cgauN' wname form.

TEST_F(CgauwavfTest, WnameForm)
{
    eval("[psi, x] = cgauwavf(-5, 5, 8, 'cgau3');");
    EXPECT_NEAR(evalScalar("real(psi(4))"), -0.4766724202, 1e-9);
}

TEST_F(CgauwavfTest, WnameMatchesIntegerForm)
{
    eval("[a, ~] = cgauwavf(-5, 5, 16, 4); [b, ~] = cgauwavf(-5, 5, 16, 'cgau4');");
    EXPECT_DOUBLE_EQ(evalScalar("max(abs(real(a - b)))"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("max(abs(imag(a - b)))"), 0.0);
}
