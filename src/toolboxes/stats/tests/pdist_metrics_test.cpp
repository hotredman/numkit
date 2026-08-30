// toolboxes/stats/tests/pdist_metrics_test.cpp
//
// Regression guard for bugs/stats/pdist-metrics.md (FIXED): pdist/pdist2 gain
// the 'seuclidean' and 'spearman' metrics, and the 'cosine'/'correlation'
// metrics return NaN (not 1) on a zero-norm / constant row. MATLAB R2025b
// reference values.

#include <numkit/core/engine.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

class PdistMetricsTest : public ::testing::Test
{
public:
    numkit::StandardEngine engine;
    void SetUp() override {}
    numkit::Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
    bool evalBool(const std::string &c) { return eval(c).toBool(); }
};

// 'seuclidean' default scale = per-column sample std (n-1) of X.
TEST_F(PdistMetricsTest, SeuclideanDefault)
{
    eval("A = [1 2 3; 4 5 7; 1 0 2]; d = pdist(A, 'seuclidean');");
    EXPECT_NEAR(evalScalar("d(1)"), 2.58974263533912, 1e-11);
    EXPECT_NEAR(evalScalar("d(2)"), 0.880020505571071, 1e-11);
    EXPECT_NEAR(evalScalar("d(3)"), 3.24326949118959, 1e-11);
}

// 'seuclidean' with an explicit scale vector S divides each diff by S(k).
TEST_F(PdistMetricsTest, SeuclideanExplicitScale)
{
    eval("A = [1 2 3; 4 5 7; 1 0 2]; d = pdist(A, 'seuclidean', [1 2 3]);");
    EXPECT_NEAR(evalScalar("d(1)"), 3.60940130461795, 1e-11);
    EXPECT_NEAR(evalScalar("d(2)"), 1.05409255338946, 1e-11);
    EXPECT_NEAR(evalScalar("d(3)"), 4.245913067619,   1e-11);
}

// 'spearman' (no ties): tiedrank rows -> correlation distance. Identical rank
// rows give ~0; the other two pairs give 0.5.
TEST_F(PdistMetricsTest, SpearmanNoTies)
{
    eval("A = [1 2 3; 4 5 7; 1 0 2]; d = pdist(A, 'spearman');");
    EXPECT_NEAR(evalScalar("d(1)"), 0.0, 1e-9);   // MATLAB ~2.2e-16
    EXPECT_NEAR(evalScalar("d(2)"), 0.5, 1e-12);
    EXPECT_NEAR(evalScalar("d(3)"), 0.5, 1e-12);
}

// 'spearman' with ties: average ranks. [1 1 2;3 2 2] -> distance 1.5.
TEST_F(PdistMetricsTest, SpearmanTies)
{
    eval("d = pdist([1 1 2; 3 2 2], 'spearman');");
    EXPECT_NEAR(evalScalar("d(1)"), 1.5, 1e-12);
}

// 'cosine' with a zero-norm row -> NaN (was clamped to 1).
TEST_F(PdistMetricsTest, CosineZeroRowNaN)
{
    EXPECT_TRUE(evalBool("isnan(pdist([0 0; 3 4], 'cosine'))"));
    // a non-degenerate pair still yields a finite cosine distance
    EXPECT_TRUE(evalBool("isfinite(pdist([1 1; 3 4], 'cosine'))"));
}

// 'correlation' with a constant row -> NaN (MATLAB returns NaN, not 1).
TEST_F(PdistMetricsTest, CorrelationConstantRowNaN)
{
    EXPECT_TRUE(evalBool("isnan(pdist([1 1; 3 4], 'correlation'))"));
    EXPECT_TRUE(evalBool("isnan(pdist([1 1; 2 2], 'correlation'))"));
}

// pdist2 'seuclidean': default scale = std(X) (the first arg).
TEST_F(PdistMetricsTest, Pdist2Seuclidean)
{
    eval("D = pdist2([1 2 3; 4 5 7], [1 0 2; 2 2 2], 'seuclidean');");
    EXPECT_NEAR(evalScalar("D(1,1)"), 1.00692049779955, 1e-11);
    EXPECT_NEAR(evalScalar("D(2,1)"), 3.26811192518793, 1e-11);
    EXPECT_NEAR(evalScalar("D(1,2)"), 0.58925565098879, 1e-11);
    EXPECT_NEAR(evalScalar("D(2,2)"), 2.45232316159369, 1e-11);
}

// pdist2 'spearman': ranks each side; constant Y row -> NaN column.
TEST_F(PdistMetricsTest, Pdist2SpearmanNaN)
{
    eval("Q = pdist2([1 2 3; 4 5 7], [1 0 2; 2 2 2], 'spearman');");
    EXPECT_NEAR(evalScalar("Q(1,1)"), 0.5, 1e-12);
    EXPECT_NEAR(evalScalar("Q(2,1)"), 0.5, 1e-12);
    EXPECT_TRUE(evalBool("isnan(Q(1,2))"));   // Y row 2 = [2 2 2] constant
    EXPECT_TRUE(evalBool("isnan(Q(2,2))"));
}
