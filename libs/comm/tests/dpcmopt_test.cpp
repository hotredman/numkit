// libs/comm/tests/dpcmopt_test.cpp
// gtest coverage for dpcmopt() — DPCM parameter optimiser.
// dpcmopt is a clean-room implementation from public references
// (Makhoul 1975; Proakis & Manolakis; Jayant & Noll 1984 — see
//): the autocorrelation method of linear
// prediction (Yule-Walker via Levinson-Durbin) plus lloyds() on the
// prediction residual. The hardcoded predictor / codebook values are
// MATLAB R2025b reference output; the property test at the end checks
// correctness MATLAB-independently (it recovers the coefficients of a
// known AR process).

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

// ── MATLAB-independent correctness test ───────────────────────────────
// The predictor of an order-p autoregressive signal is, by definition,
// its AR coefficients. Generate a training signal from a KNOWN AR(2)
// process  x[n] = 1.4*x[n-1] - 0.5*x[n-2] + e[n]  (a strongly
// predictable, stable process) and check that dpcmopt(x, 2) recovers a
// predictor close to [0, 1.4, -0.5], and that the prediction residual
// has far less energy than the training signal.
TEST_F(DpcmOptTest, RecoversKnownARProcess)
{
    eval("rng(12345);\n"
         "e = randn(1, 6000);\n"
         "x = filter(1, [1 -1.4 0.5], e);\n"   // AR(2): a1=1.4, a2=-0.5
         "p = dpcmopt(x, 2);");
    EXPECT_DOUBLE_EQ(evalScalar("p(1)"), 0.0);
    // Recovered predictor coefficients ≈ the true AR coefficients.
    EXPECT_NEAR(evalScalar("p(2)"),  1.4, 0.06);
    EXPECT_NEAR(evalScalar("p(3)"), -0.5, 0.06);

    // The predictor removes most of the signal energy: the residual
    // variance is far below the training-signal variance.
    eval("xhat = filter([0 p(2) p(3)], 1, x);\n"
         "resid = x - xhat;\n"
         "vratio = var(resid) / var(x);");
    EXPECT_LT(evalScalar("vratio"), 0.3);
}
