// libs/comm/tests/dpcmopt_test.cpp
//
// Regression guard for dpcmopt() — DPCM parameter optimiser.
// Levinson-Durbin from biased autocorrelation + lloyds() on the
// prediction residual.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class DpcmOptTest : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(DpcmOptTest, KnownPredictorOrder2)
{
    // MATLAB ref:
    //   training = sin(2*pi*0.1*(0:50)) + 0.05*(0:50)
    //   predictor(2:3) = [1.530775, -0.602549]
    eval("training = sin(2*pi*0.1*(0:50)) + 0.05*(0:50);"
         "p = dpcmopt(training, 2);");
    EXPECT_DOUBLE_EQ(evalScalar("p(1)"), 0.0);
    EXPECT_NEAR(evalScalar("p(2)"),  1.530775, 1e-6);
    EXPECT_NEAR(evalScalar("p(3)"), -0.602549, 1e-6);
}

TEST_F(DpcmOptTest, KnownPredictorOrder3)
{
    eval("training = sin(2*pi*0.1*(0:50)) + 0.05*(0:50);"
         "p = dpcmopt(training, 3);");
    EXPECT_DOUBLE_EQ(evalScalar("p(1)"), 0.0);
    EXPECT_NEAR(evalScalar("p(2)"),  1.603410, 1e-6);
    EXPECT_NEAR(evalScalar("p(3)"), -0.787079, 1e-6);
    EXPECT_NEAR(evalScalar("p(4)"),  0.120547, 1e-6);
}

TEST_F(DpcmOptTest, FullThreeOutputForm)
{
    eval("training = sin(2*pi*0.1*(0:50)) + 0.05*(0:50);"
         "[p, c, part] = dpcmopt(training, 2, 4);");
    EXPECT_EQ(static_cast<int>(evalScalar("numel(p)")), 3);
    EXPECT_EQ(static_cast<int>(evalScalar("numel(c)")), 4);
    EXPECT_EQ(static_cast<int>(evalScalar("numel(part)")), 3);
    EXPECT_NEAR(evalScalar("c(1)"), -0.190703, 1e-6);
    EXPECT_NEAR(evalScalar("c(4)"),  0.410908, 1e-6);
    EXPECT_NEAR(evalScalar("part(1)"), -0.091795, 1e-6);
}

TEST_F(DpcmOptTest, RoundTripViaDpcmEncoDeco)
{
    // dpcmopt-tuned parameters should give a reasonable round-trip
    // via dpcmenco/dpcmdeco. MSE depends on signal complexity / K.
    eval("training = sin(2*pi*0.1*(0:50)) + 0.05*(0:50);"
         "[p, c, part] = dpcmopt(training, 2, 4);"
         "[indx, ~] = dpcmenco(training, c, part, p);"
         "[recon, ~] = dpcmdeco(indx, c, p);"
         "mse = mean((training - recon).^2);");
    // Should be small but nonzero (lossy quantization).
    EXPECT_LT(evalScalar("mse"), 0.05);
    EXPECT_GT(evalScalar("mse"), 0.0);
}

TEST_F(DpcmOptTest, IntegerCodebookLengthArg)
{
    // ini_codebook may be a scalar specifying K -- lloyds builds the
    // initial codebook from K linspace bins.
    eval("training = sin(2*pi*0.1*(0:50)) + 0.05*(0:50);"
         "[p, c] = dpcmopt(training, 2, 5);");
    EXPECT_EQ(static_cast<int>(evalScalar("numel(c)")), 5);
}

TEST_F(DpcmOptTest, ExplicitCodebookVectorArg)
{
    eval("training = sin(2*pi*0.1*(0:50)) + 0.05*(0:50);"
         "[p, c] = dpcmopt(training, 2, [-1 0 1]);");
    EXPECT_EQ(static_cast<int>(evalScalar("numel(c)")), 3);
}

TEST_F(DpcmOptTest, RejectsNonPositiveOrder)
{
    bool threw = false;
    try {
        eval("dpcmopt([1 2 3 4 5 6 7], 0);");
    } catch (...) { threw = true; }
    EXPECT_TRUE(threw);
}

TEST_F(DpcmOptTest, RejectsTooShortTraining)
{
    bool threw = false;
    try {
        // ord=3 needs N >= ord+3 = 6; 4 samples -> error
        eval("dpcmopt([1 2 3 4], 3);");
    } catch (...) { threw = true; }
    EXPECT_TRUE(threw);
}
