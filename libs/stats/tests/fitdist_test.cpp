// libs/stats/tests/fitdist_test.cpp
//
// Regression guard for fitdist (probability-distribution struct).

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>
#include <cmath>

using namespace numkit;

class FitdistTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(FitdistTest, NormalReturnsStruct)
{
    eval("x = [1.2; 0.8; 1.5; 0.9; 1.1; 1.3; 1.0; 0.7; 1.4; 1.0]; "
         "pd = fitdist(x, 'Normal');");
    EXPECT_TRUE(eval("isstruct(pd)").toBool());
    EXPECT_NEAR(evalScalar("pd.ParameterValues(1)"), 1.09, 1e-12);
    // sample std (N-1) per MATLAB fitdist convention
    const double sigma = evalScalar("pd.ParameterValues(2)");
    EXPECT_NEAR(sigma, 0.260128, 1e-5);
}

TEST_F(FitdistTest, NormalHasMatlabFields)
{
    eval("pd = fitdist([1 2 3 4 5]', 'Normal');");
    EXPECT_TRUE(eval("strcmp(pd.DistributionName, 'Normal')").toBool());
    EXPECT_EQ(static_cast<int>(evalScalar("size(pd.ParameterValues, 2)")), 2);
    EXPECT_EQ(static_cast<int>(evalScalar("size(pd.ParameterNames, 2)")), 2);
    EXPECT_DOUBLE_EQ(evalScalar("pd.NumObservations"), 5.0);
}

TEST_F(FitdistTest, ExponentialReturnsMu)
{
    eval("xe = [0.5; 1.0; 1.5; 2.0; 2.5; 0.3; 0.8; 1.2; 1.7; 0.6]; "
         "pd = fitdist(xe, 'Exponential');");
    EXPECT_NEAR(evalScalar("pd.ParameterValues(1)"), 1.21, 1e-12);
    EXPECT_TRUE(eval("strcmp(pd.DistributionName, 'Exponential')").toBool());
}

TEST_F(FitdistTest, PoissonReturnsLambda)
{
    eval("xp = [0; 1; 2; 3; 4; 5; 1; 2; 3; 2]; "
         "pd = fitdist(xp, 'Poisson');");
    EXPECT_NEAR(evalScalar("pd.ParameterValues(1)"), 2.3, 1e-12);
}

TEST_F(FitdistTest, LognormalReturnsMuSigma)
{
    eval("xl = exp([0.1; 0.2; 0.3; 0.4; 0.5]); "
         "pd = fitdist(xl, 'Lognormal');");
    EXPECT_NEAR(evalScalar("pd.ParameterValues(1)"), 0.3, 1e-12);
    // sample std (N-1) of [0.1 0.2 0.3 0.4 0.5]: std = sqrt(0.025) ≈ 0.158114
    EXPECT_NEAR(evalScalar("pd.ParameterValues(2)"), std::sqrt(0.025), 1e-12);
}

TEST_F(FitdistTest, UnknownDistRejected)
{
    EXPECT_THROW(eval("fitdist([1 2 3], 'Gamma');"), std::exception);
}

TEST_F(FitdistTest, NonStringNameRejected)
{
    EXPECT_THROW(eval("fitdist([1 2 3], 5);"), std::exception);
}

TEST_F(FitdistTest, TooSmallSampleRejected)
{
    EXPECT_THROW(eval("fitdist([1], 'Normal');"), std::exception);
}
