// toolboxes/stats/tests/chi2inv_test.cpp
// chi2inv. Reference values from MATLAB R2025b.

#include <numkit/core/engine.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class Chi2invTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override {}
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(Chi2invTest, K1Quantiles)
{
    // Chi²(1) at p=0.95 → 3.84 is the famous critical value.
    eval("x = chi2inv([0.05 0.5 0.95], 1);");
    EXPECT_NEAR(evalScalar("x(1)"), 0.0039321400000195, 1e-12);
    EXPECT_NEAR(evalScalar("x(2)"), 0.4549364231195725, 1e-12);
    EXPECT_NEAR(evalScalar("x(3)"), 3.8414588206941191, 1e-12);
}

TEST_F(Chi2invTest, K5Quantiles)
{
    eval("x = chi2inv([0.05 0.5 0.95], 5);");
    EXPECT_NEAR(evalScalar("x(1)"),  1.1454762260617692, 1e-12);
    EXPECT_NEAR(evalScalar("x(2)"),  4.3514601910955264, 1e-12);
    EXPECT_NEAR(evalScalar("x(3)"), 11.0704976935163550, 1e-12);
}

TEST_F(Chi2invTest, K30Quantiles)
{
    eval("x = chi2inv([0.05 0.5 0.95], 30);");
    EXPECT_NEAR(evalScalar("x(1)"), 18.4926609819535166, 1e-12);
    EXPECT_NEAR(evalScalar("x(2)"), 29.3360315166615706, 1e-12);
    EXPECT_NEAR(evalScalar("x(3)"), 43.7729718257421894, 1e-12);
}

TEST_F(Chi2invTest, BoundaryProbabilities)
{
    EXPECT_DOUBLE_EQ(evalScalar("chi2inv(0.0, 5)"), 0.0);
    EXPECT_TRUE(std::isinf(evalScalar("chi2inv(1.0, 5)")));
}

TEST_F(Chi2invTest, OutOfRangeProbReturnsNaN)
{
    EXPECT_TRUE(std::isnan(evalScalar("chi2inv(-0.1, 5)")));
    EXPECT_TRUE(std::isnan(evalScalar("chi2inv( 1.5, 5)")));
}

TEST_F(Chi2invTest, KEqualsZeroDegenerateGivesZero)
{
    // Degenerate Chi²(0) has all mass at 0 → quantile = 0 for any p in [0,1].
    EXPECT_DOUBLE_EQ(evalScalar("chi2inv(0.5, 0)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("chi2inv(0.0, 0)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("chi2inv(1.0, 0)"), 0.0);
    // Out-of-range p still NaN.
    EXPECT_TRUE(std::isnan(evalScalar("chi2inv(-0.1, 0)")));
}

TEST_F(Chi2invTest, NegativeKReturnsNaN)
{
    EXPECT_TRUE(std::isnan(evalScalar("chi2inv(0.5, -1)")));
}
