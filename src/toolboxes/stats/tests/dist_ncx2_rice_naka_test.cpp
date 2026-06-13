// toolboxes/stats/tests/dist_ncx2_rice_naka_test.cpp
//
// gtest coverage for three less-common distributions that shipped parity-only:
// noncentral chi-square (ncx2*), Rice/Rician (rice*) and Nakagami (naka*) --
// pdf / cdf / inv / stat / rnd. Reference values are numkit's parity-validated
// output; inv is checked by the cdf(inv(p)) == p round-trip; ncx2 stat matches
// the closed form (mean = k + lambda, var = 2(k + 2*lambda)).

#include "dual_engine_fixture.hpp"

#include <cmath>

using namespace m_test;

class DistNcx2RiceNakaTest : public DualEngineTest
{};

// ── noncentral chi-square ───────────────────────────────────

TEST_P(DistNcx2RiceNakaTest, Ncx2PdfCdfInvStat)
{
    EXPECT_NEAR(evalScalar("ncx2pdf(5, 3, 2)"), 0.1004419818, 1e-9);
    EXPECT_NEAR(evalScalar("ncx2cdf(5, 3, 2)"), 0.5934051801, 1e-9);
    EXPECT_NEAR(evalScalar("ncx2cdf(ncx2inv(0.3, 3, 2), 3, 2)"), 0.3, 1e-9);  // inv round-trip
    eval("[m, v] = ncx2stat(3, 2);");
    EXPECT_NEAR(evalScalar("m"), 5.0, 1e-9);    // k + lambda
    EXPECT_NEAR(evalScalar("v"), 14.0, 1e-9);   // 2*(k + 2*lambda)
}

TEST_P(DistNcx2RiceNakaTest, Ncx2RndShapeAndMean)
{
    eval("rng(0); y = ncx2rnd(3, 2, 2000, 1); mu = mean(y);");
    EXPECT_EQ(eval("y").numel(), 2000u);
    EXPECT_NEAR(evalScalar("mu"), 5.0, 0.5);  // sample mean ~ k + lambda
}

// ── Rice / Rician ───────────────────────────────────────────

TEST_P(DistNcx2RiceNakaTest, RicePdfCdfInvStat)
{
    EXPECT_NEAR(evalScalar("ricepdf(2, 1, 1)"), 0.3742395128, 1e-9);
    EXPECT_NEAR(evalScalar("ricecdf(2, 1, 1)"), 0.7309873435, 1e-9);
    EXPECT_NEAR(evalScalar("ricecdf(riceinv(0.3, 1, 1), 1, 1)"), 0.3, 1e-9);
    eval("[m, v] = ricestat(1, 1);");
    EXPECT_NEAR(evalScalar("m"), 1.548572461, 1e-6);
    EXPECT_NEAR(evalScalar("v"), 0.6019233344, 1e-6);
}

TEST_P(DistNcx2RiceNakaTest, RiceRndShapeAndMean)
{
    eval("rng(0); y = ricernd(1, 1, 2000, 1); mu = mean(y);");
    EXPECT_EQ(eval("y").numel(), 2000u);
    EXPECT_NEAR(evalScalar("mu"), 1.5486, 0.1);
}

// ── Nakagami ────────────────────────────────────────────────

TEST_P(DistNcx2RiceNakaTest, NakaPdfCdfInvStat)
{
    EXPECT_NEAR(evalScalar("nakapdf(1.5, 1, 2)"), 0.486978701, 1e-9);
    EXPECT_NEAR(evalScalar("nakacdf(1.5, 1, 2)"), 0.6753475326, 1e-9);
    EXPECT_NEAR(evalScalar("nakacdf(nakainv(0.3, 1, 2), 1, 2)"), 0.3, 1e-9);
    eval("[m, v] = nakastat(1, 2);");
    EXPECT_NEAR(evalScalar("m"), 1.253314137, 1e-6);
    EXPECT_NEAR(evalScalar("v"), 0.4292036732, 1e-6);
}

TEST_P(DistNcx2RiceNakaTest, NakaRndShapeAndMean)
{
    eval("rng(0); y = nakarnd(1, 2, 2000, 1); mu = mean(y);");
    EXPECT_EQ(eval("y").numel(), 2000u);
    EXPECT_NEAR(evalScalar("mu"), 1.2533, 0.1);
}

INSTANTIATE_DUAL(DistNcx2RiceNakaTest);
