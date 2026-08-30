// toolboxes/wavelet/tests/upcoef_test.cpp
//
// upcoef(O, X, wname, N[, L]) — direct single-branch reconstruction of a
// coefficient vector up N levels. bugs/wavelet/upcoef.md. Reference values
// from MATLAB R2025b.

#include <numkit/bundle/standard_engine.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class UpcoefTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override {}
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// Approximation, Haar, 2 levels: a single coefficient 5 → [2.5 2.5 2.5 2.5].
TEST_F(UpcoefTest, ApproxHaarTwoLevels)
{
    eval("y = upcoef('a', 5, 'db1', 2);");
    EXPECT_EQ(static_cast<int>(evalScalar("numel(y)")), 4);
    EXPECT_NEAR(evalScalar("y(1)"), 2.5, 1e-12);
    EXPECT_NEAR(evalScalar("y(4)"), 2.5, 1e-12);
}

// Detail branch uses the highpass on the first level → sign flip in the tail.
TEST_F(UpcoefTest, DetailHaarTwoLevels)
{
    eval("y = upcoef('d', 5, 'db1', 2);");
    EXPECT_EQ(static_cast<int>(evalScalar("numel(y)")), 4);
    EXPECT_NEAR(evalScalar("y(1)"),  2.5, 1e-12);
    EXPECT_NEAR(evalScalar("y(4)"), -2.5, 1e-12);
}

// db2, one level: longer filter keeps the full length (2n−1 + |F|−1).
TEST_F(UpcoefTest, ApproxDb2OneLevel)
{
    eval("y = upcoef('a', [1 2], 'db2', 1);");
    EXPECT_EQ(static_cast<int>(evalScalar("numel(y)")), 6);
    EXPECT_NEAR(evalScalar("y(1)"),  0.48296291, 1e-7);
    EXPECT_NEAR(evalScalar("y(4)"),  1.54362318, 1e-7);
    EXPECT_NEAR(evalScalar("y(6)"), -0.25881905, 1e-7);
}

// Vector approximation, Haar, one level.
TEST_F(UpcoefTest, VectorApproxHaar)
{
    eval("y = upcoef('a', [1 2 3], 'db1', 1);");
    EXPECT_EQ(static_cast<int>(evalScalar("numel(y)")), 6);
    EXPECT_NEAR(evalScalar("y(1)"), 0.70710678, 1e-7);
    EXPECT_NEAR(evalScalar("y(5)"), 2.12132034, 1e-7);
}

// N = 0 returns X unchanged.
TEST_F(UpcoefTest, ZeroLevelsIsIdentity)
{
    eval("y = upcoef('a', [3 4 5], 'db1', 0);");
    EXPECT_EQ(static_cast<int>(evalScalar("numel(y)")), 3);
    EXPECT_NEAR(evalScalar("y(2)"), 4.0, 1e-12);
}

// Bad branch type throws.
TEST_F(UpcoefTest, BadTypeThrows)
{
    EXPECT_THROW(eval("upcoef('x', 5, 'db1', 1);"), std::exception);
}
