// toolboxes/stats/tests/cdf_upper_batch_test.cpp
// Batch-closure of nine cdf-family specs that all shared the same gap:
// the trailing `'upper'` string flag was silently ignored. After this
// commit, every adapter strips the flag via stats::detail::stripUpperFlag
// and applies stats::detail::applyUpperInPlace when it was present.
// Closes:
// geocdf, gevcdf, gpcdf, hygecdf,
//                         nakacdf, nbincdf, ncx2cdf, ricecdf}.md
// All hardcoded "upper" expected values are computed by hand as 1 - p
// from the lower-tail value the function would otherwise return.

#include <numkit/core/engine.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class CdfUpperBatchTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// ─── evcdf ──────────────────────────────────────────────────────────

TEST_F(CdfUpperBatchTest, EvcdfUpper)
{
    // Lower tail at x=1, mu=0, sigma=1: F = 1 - exp(-exp(1)) ≈ 0.934012
    EXPECT_NEAR(evalScalar("evcdf(1, 0, 1)"),           0.934011964154687, 1e-12);
    EXPECT_NEAR(evalScalar("evcdf(1, 0, 1, 'upper')"),  1.0 - 0.934011964154687, 1e-12);
    // Sanity: lower + upper == 1
    eval("p = evcdf([-2 0 1 3], 0, 1); pu = evcdf([-2 0 1 3], 0, 1, 'upper');");
    EXPECT_LT(evalScalar("max(abs(p + pu - 1))"), 1e-12);
}

// ─── geocdf ─────────────────────────────────────────────────────────

TEST_F(CdfUpperBatchTest, GeocdfUpper)
{
    // F(0; p=0.3) = p = 0.3
    EXPECT_NEAR(evalScalar("geocdf(0, 0.3)"),           0.3, 1e-12);
    EXPECT_NEAR(evalScalar("geocdf(0, 0.3, 'upper')"),  0.7, 1e-12);
    eval("k = [0 1 2 3 5]; p = geocdf(k, 0.3); pu = geocdf(k, 0.3, 'upper');");
    EXPECT_LT(evalScalar("max(abs(p + pu - 1))"), 1e-12);
}

// ─── gevcdf ─────────────────────────────────────────────────────────

TEST_F(CdfUpperBatchTest, GevcdfUpper)
{
    EXPECT_NEAR(evalScalar("gevcdf(1, 0.5, 1, 0)"),
                evalScalar("1 - gevcdf(1, 0.5, 1, 0, 'upper')"), 1e-12);
    eval("x = [0 1 2 5]; p = gevcdf(x, 0.5, 1, 0); pu = gevcdf(x, 0.5, 1, 0, 'upper');");
    EXPECT_LT(evalScalar("max(abs(p + pu - 1))"), 1e-12);
}

// ─── gpcdf ──────────────────────────────────────────────────────────

TEST_F(CdfUpperBatchTest, GpcdfUpper)
{
    eval("x = [0.1 0.5 1 2]; p = gpcdf(x, 0.5, 1, 0); pu = gpcdf(x, 0.5, 1, 0, 'upper');");
    EXPECT_LT(evalScalar("max(abs(p + pu - 1))"), 1e-12);
    EXPECT_NEAR(evalScalar("gpcdf(0, 0, 1, 0, 'upper')"), 1.0, 1e-12);
}

// ─── hygecdf ────────────────────────────────────────────────────────

TEST_F(CdfUpperBatchTest, HygecdfUpper)
{
    // F(N; M, K, N) == 1 → upper at the boundary == 0
    EXPECT_NEAR(evalScalar("hygecdf(8, 50, 10, 8, 'upper')"), 0.0, 1e-12);
    eval("k = [0 1 3 5 7]; p = hygecdf(k, 50, 10, 8); pu = hygecdf(k, 50, 10, 8, 'upper');");
    EXPECT_LT(evalScalar("max(abs(p + pu - 1))"), 1e-12);
}

// ─── nakacdf ────────────────────────────────────────────────────────

TEST_F(CdfUpperBatchTest, NakacdfUpper)
{
    eval("x = [0.1 0.5 1 2]; p = nakacdf(x, 1, 1); pu = nakacdf(x, 1, 1, 'upper');");
    EXPECT_LT(evalScalar("max(abs(p + pu - 1))"), 1e-12);
    EXPECT_NEAR(evalScalar("nakacdf(0, 1, 1, 'upper')"), 1.0, 1e-12);
}

// ─── nbincdf ────────────────────────────────────────────────────────

TEST_F(CdfUpperBatchTest, NbincdfUpper)
{
    eval("k = [0 1 2 5 10]; p = nbincdf(k, 3, 0.4); pu = nbincdf(k, 3, 0.4, 'upper');");
    EXPECT_LT(evalScalar("max(abs(p + pu - 1))"), 1e-12);
    // F(0; r=3, p=0.4) = p^r = 0.064 ⇒ upper = 0.936
    EXPECT_NEAR(evalScalar("nbincdf(0, 3, 0.4)"),          0.4 * 0.4 * 0.4, 1e-12);
    EXPECT_NEAR(evalScalar("nbincdf(0, 3, 0.4, 'upper')"), 1.0 - 0.4 * 0.4 * 0.4, 1e-12);
}

// ─── ncx2cdf ────────────────────────────────────────────────────────

TEST_F(CdfUpperBatchTest, Ncx2cdfUpper)
{
    // probe: ncx2cdf(2, 3, 1, 'upper') = 0.6917 in MATLAB
    EXPECT_NEAR(evalScalar("ncx2cdf(2, 3, 1, 'upper')"),
                1.0 - evalScalar("ncx2cdf(2, 3, 1)"), 1e-12);
    eval("x = [0.5 1 2 5]; p = ncx2cdf(x, 4, 2); pu = ncx2cdf(x, 4, 2, 'upper');");
    EXPECT_LT(evalScalar("max(abs(p + pu - 1))"), 1e-12);
}

// ─── ricecdf ────────────────────────────────────────────────────────

TEST_F(CdfUpperBatchTest, RicecdfUpper)
{
    eval("x = [0.1 0.5 1 2]; p = ricecdf(x, 1, 1); pu = ricecdf(x, 1, 1, 'upper');");
    EXPECT_LT(evalScalar("max(abs(p + pu - 1))"), 1e-12);
    EXPECT_NEAR(evalScalar("ricecdf(0, 1, 1, 'upper')"), 1.0, 1e-12);
}

// ─── case-insensitivity ─────────────────────────────────────────────

TEST_F(CdfUpperBatchTest, UpperFlagIsCaseInsensitive)
{
    EXPECT_NEAR(evalScalar("evcdf(1, 0, 1, 'UPPER')"),
                evalScalar("evcdf(1, 0, 1, 'upper')"), 1e-15);
    EXPECT_NEAR(evalScalar("evcdf(1, 0, 1, 'Upper')"),
                evalScalar("evcdf(1, 0, 1, 'upper')"), 1e-15);
}
