// libs/stats/tests/gevlike_test.cpp
// Backfill gtest + gevlike. Reference values
// from MATLAB R2025b probe.
// Note: gevlike's ACOV uses the analytical observed-Fisher Hessian
// at k != 0 (matches FD). At exactly k=0 MATLAB uses an analytical
// Gumbel-limit Hessian that differs from FD straddling; numkit's FD
// approach reports the FD value (~0.030, 0.098, -1.622) and not
// MATLAB's analytical ACOV — documented as a known gap.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class GevlikeTest : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override
    {
        engine.eval("import compat.*;");
        engine.eval("x = [1 2 3 4 5]';");
    }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(GevlikeTest, NLogLKPositive)
{
    EXPECT_NEAR(evalScalar("gevlike([0.5, 1, 0], x)"), 14.1460230417, 1e-9);
}

TEST_F(GevlikeTest, NLogLKZeroGumbelLimit)
{
    EXPECT_NEAR(evalScalar("gevlike([0, 1, 0], x)"), 15.5780553787, 1e-9);
}

TEST_F(GevlikeTest, ACovKPositive)
{
    eval("[nL, ac] = gevlike([0.5, 1, 0], x);");
    EXPECT_NEAR(evalScalar("ac(1,1)"),  0.7323243670, 1e-6);
    EXPECT_NEAR(evalScalar("ac(1,2)"), -0.4845307553, 1e-6);
    EXPECT_NEAR(evalScalar("ac(1,3)"), -0.0703687586, 1e-6);
    EXPECT_NEAR(evalScalar("ac(2,2)"),  0.3841928178, 1e-6);
    EXPECT_NEAR(evalScalar("ac(2,3)"),  0.3851466902, 1e-6);
    EXPECT_NEAR(evalScalar("ac(3,3)"), -1.2357793582, 1e-6);
    // Symmetry checks.
    EXPECT_DOUBLE_EQ(evalScalar("ac(1,2)"), evalScalar("ac(2,1)"));
    EXPECT_DOUBLE_EQ(evalScalar("ac(1,3)"), evalScalar("ac(3,1)"));
    EXPECT_DOUBLE_EQ(evalScalar("ac(2,3)"), evalScalar("ac(3,2)"));
}

TEST_F(GevlikeTest, SupportViolationReturnsNaN)
{
    EXPECT_TRUE(std::isnan(evalScalar("gevlike([0.5, 1, 0], [-100; -1; 0])")));
}

TEST_F(GevlikeTest, NegativeSigmaReturnsNaN)
{
    EXPECT_TRUE(std::isnan(evalScalar("gevlike([0.5, -1, 0], x)")));
}

TEST_F(GevlikeTest, EmptyDataReturnsInf)
{
    EXPECT_TRUE(std::isinf(evalScalar("gevlike([0.5, 1, 0], [])")));
}
