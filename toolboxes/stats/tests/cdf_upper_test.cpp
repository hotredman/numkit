// toolboxes/stats/tests/cdf_upper_test.cpp
// Joint regression test for the 'upper' flag added to all stats.dist
// CDFs. covers:
//   normcdf, chi2cdf, tcdf, fcdf, betacdf, gamcdf, expcdf, raylcdf,
//   logncdf, wblcdf, unifcdf, unidcdf, binocdf, poisscdf
// Reference values from MATLAB R2025b probes.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class CdfUpperTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(CdfUpperTest, NormcdfUpper)
{
    EXPECT_NEAR(evalScalar("normcdf(1.96, 0, 1, 'upper')"), 0.024997895148, 1e-9);
    EXPECT_NEAR(evalScalar("normcdf(1.96)"), 0.975002104852, 1e-9);
}

TEST_F(CdfUpperTest, Chi2cdfUpper)
{
    EXPECT_NEAR(evalScalar("chi2cdf(3.84, 1, 'upper')"), 0.0500435212, 1e-9);
}

TEST_F(CdfUpperTest, TcdfUpper)
{
    EXPECT_NEAR(evalScalar("tcdf(2.0, 10, 'upper')"), 0.036694017, 1e-7);
}

TEST_F(CdfUpperTest, FcdfUpper)
{
    EXPECT_NEAR(evalScalar("fcdf(2, 5, 10, 'upper')"), 0.1641949509, 1e-9);
}

TEST_F(CdfUpperTest, BetacdfUpper)
{
    EXPECT_NEAR(evalScalar("betacdf(0.5, 2, 3, 'upper')"), 0.3125, 1e-12);
}

TEST_F(CdfUpperTest, GamcdfUpper)
{
    // gamcdf(2, 1, 1) = 1 - exp(-2) ≈ 0.8647; upper = exp(-2) ≈ 0.1353
    EXPECT_NEAR(evalScalar("gamcdf(2, 1, 1, 'upper')"), std::exp(-2.0), 1e-9);
}

TEST_F(CdfUpperTest, ExpcdfUpper)
{
    EXPECT_NEAR(evalScalar("expcdf(1, 1, 'upper')"), std::exp(-1.0), 1e-12);
}

TEST_F(CdfUpperTest, RaylcdfUpper)
{
    EXPECT_NEAR(evalScalar("raylcdf(1, 1, 'upper')"), std::exp(-0.5), 1e-9);
}

TEST_F(CdfUpperTest, LogncdfUpper)
{
    EXPECT_NEAR(evalScalar("logncdf(1, 0, 1, 'upper')"), 0.5, 1e-12);
}

TEST_F(CdfUpperTest, WblcdfUpper)
{
    EXPECT_NEAR(evalScalar("wblcdf(1, 1, 1, 'upper')"), std::exp(-1.0), 1e-12);
}

TEST_F(CdfUpperTest, UnifcdfUpper)
{
    EXPECT_NEAR(evalScalar("unifcdf(0.3, 0, 1, 'upper')"), 0.7, 1e-12);
}

TEST_F(CdfUpperTest, UnidcdfUpper)
{
    EXPECT_NEAR(evalScalar("unidcdf(3, 5, 'upper')"), 0.4, 1e-12);
}

TEST_F(CdfUpperTest, BinocdfUpper)
{
    // P(X >= 3) for Bin(5, 0.3) = sum_{k=3..5} C(5,k) 0.3^k 0.7^(5-k)
    // = 10*0.027*0.49 + 5*0.0081*0.7 + 0.00243 = 0.13230 + 0.02835 + 0.00243 = 0.16308
    EXPECT_NEAR(evalScalar("binocdf(2, 5, 0.3, 'upper')"), 0.16308, 1e-5);
}

TEST_F(CdfUpperTest, PoisscdfUpper)
{
    // poisscdf(2, 3) ≈ 0.4232; upper ≈ 0.5768
    EXPECT_NEAR(evalScalar("poisscdf(2, 3, 'upper')"), 0.5768099, 1e-6);
}

TEST_F(CdfUpperTest, LowerTailUnchanged)
{
    // Sanity: existing default-tail behavior must not regress.
    EXPECT_NEAR(evalScalar("normcdf(0)"), 0.5, 1e-15);
    EXPECT_NEAR(evalScalar("expcdf(0, 1)"), 0.0, 1e-15);
}
