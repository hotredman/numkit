// toolboxes/stats/tests/dist_ev_gev_gp_pdf_test.cpp
//
// Fills the one remaining gtest gap for the extreme-value / Pareto family:
// the probability *densities* evpdf / gevpdf / gppdf. The matching CDFs are
// already guarded in cdf_upper_batch_test.cpp (lower + 'upper' tails) and the
// inverse CDFs are exercised by the fit round-trips in evfit_gpfit_test.cpp /
// gevfit_test.cpp, so this file deliberately covers only the densities.
//
//   ev*   extreme value (Gumbel, minima)   evpdf(x, mu, sigma)
//   gev*  generalized extreme value        gevpdf(x, k, sigma, mu)
//   gp*   generalized Pareto               gppdf(x, k, sigma, theta)
//
// Reference values verified against the numkit engine (parity-validated).

#include "dual_engine_fixture.hpp"

using namespace m_test;

class DistEvGevGpPdfTest : public DualEngineTest
{};

// ── extreme value (Gumbel, minima convention), infinite support ─────────
TEST_P(DistEvGevGpPdfTest, Evpdf)
{
    EXPECT_NEAR(evalScalar("evpdf(0, 0, 1)"), 0.3678794412, 1e-9);   // exp(-1) at x=mu
    EXPECT_NEAR(evalScalar("evpdf(1, 0, 1)"), 0.1793740787, 1e-9);
    EXPECT_NEAR(evalScalar("evpdf(-1, 0, 1)"), 0.2546463800, 1e-9);
    // shifted / scaled: evpdf(x,mu,sigma) = (1/sigma)*evpdf((x-mu)/sigma,0,1)
    EXPECT_NEAR(evalScalar("evpdf(2, 2, 3)"), 0.3678794412 / 3.0, 1e-9);
}

// ── generalized EV, shape k>0 has lower-bounded support x > mu - sigma/k ─
TEST_P(DistEvGevGpPdfTest, Gevpdf)
{
    EXPECT_NEAR(evalScalar("gevpdf(0, 0.1, 1, 0)"), 0.3678794412, 1e-9);  // exp(-1) at x=mu
    EXPECT_NEAR(evalScalar("gevpdf(1, 0.1, 1, 0)"), 0.2383642609, 1e-9);
    EXPECT_DOUBLE_EQ(evalScalar("gevpdf(-11, 0.1, 1, 0)"), 0.0);          // below support (> -10)
    // k -> 0 limit coincides with the Gumbel density at the mode.
    EXPECT_NEAR(evalScalar("gevpdf(0, 0, 1, 0)"), 0.3678794412, 1e-9);
}

// ── generalized Pareto, support x >= theta ──────────────────────────────
TEST_P(DistEvGevGpPdfTest, Gppdf)
{
    EXPECT_DOUBLE_EQ(evalScalar("gppdf(0, 0.1, 1, 0)"), 1.0);             // peak at the threshold
    EXPECT_NEAR(evalScalar("gppdf(1, 0.1, 1, 0)"), 0.3504938995, 1e-9);
    EXPECT_DOUBLE_EQ(evalScalar("gppdf(-1, 0.1, 1, 0)"), 0.0);            // below threshold
    // k == 0 reduces to exponential(sigma): pdf(x) = (1/sigma) exp(-x/sigma).
    EXPECT_NEAR(evalScalar("gppdf(0, 0, 1, 0)"), 1.0, 1e-12);
    EXPECT_NEAR(evalScalar("gppdf(1, 0, 1, 0)"), 0.3678794412, 1e-9);
}

INSTANTIATE_DUAL(DistEvGevGpPdfTest);
