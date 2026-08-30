// toolboxes/stats/tests/dist_dispatch_test.cpp
//
// Regression guard for the generic distribution dispatchers cdf/pdf/icdf/random.
// Expected values from MATLAB R2025b. bugs/stats/distribution-dispatchers.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class DistDispatchTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override {}
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(DistDispatchTest, CdfFamilies)
{
    EXPECT_NEAR(evalScalar("cdf('Normal',1,0,1)"),    0.8413447461, 1e-9);
    EXPECT_NEAR(evalScalar("cdf('Exponential',1,2)"), 0.3934693403, 1e-9);
    EXPECT_NEAR(evalScalar("cdf('Gamma',2,3,1)"),     0.3233235838, 1e-9);
    EXPECT_NEAR(evalScalar("cdf('T',1,5)"),           0.8183912662, 1e-9);
    EXPECT_NEAR(evalScalar("cdf('Weibull',1,1,2)"),   0.6321205588, 1e-9);
    EXPECT_NEAR(evalScalar("cdf('Uniform',0.3,0,1)"), 0.3,          1e-9);
    EXPECT_NEAR(evalScalar("cdf('Rayleigh',1,1)"),    0.3934693403, 1e-9);
    EXPECT_NEAR(evalScalar("cdf('norm',1,0,1)"),      0.8413447461, 1e-9);  // alias
}

TEST_F(DistDispatchTest, PdfFamilies)
{
    EXPECT_NEAR(evalScalar("pdf('Poisson',2,3)"),      0.2240418077, 1e-9);
    EXPECT_NEAR(evalScalar("pdf('Binomial',2,5,0.3)"), 0.3087,       1e-9);
    EXPECT_NEAR(evalScalar("pdf('Beta',0.5,2,3)"),     1.5,          1e-9);
    EXPECT_NEAR(evalScalar("pdf('Geometric',2,0.3)"),  0.147,        1e-9);
}

TEST_F(DistDispatchTest, IcdfFamilies)
{
    EXPECT_NEAR(evalScalar("icdf('Normal',0.975,0,1)"), 1.959963985, 1e-8);
    EXPECT_NEAR(evalScalar("icdf('Chisquare',0.95,3)"), 7.814727903, 1e-7);
    EXPECT_NEAR(evalScalar("icdf('Poisson',0.5,3)"),    3.0,         1e-12);
}

TEST_F(DistDispatchTest, RandomSizeAndUnknownThrows)
{
    eval("r = random('Normal', 0, 1, 1, 3);");
    EXPECT_EQ(eval("r").numel(), 3u);              // random forwards size args to normrnd
    EXPECT_THROW(eval("cdf('NotADistribution', 1)"), std::exception);
}
