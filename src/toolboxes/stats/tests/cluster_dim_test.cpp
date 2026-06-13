// toolboxes/stats/tests/cluster_dim_test.cpp
//
// Coverage for previously gtest-uncovered stats files (parity-spec only):
// cluster/knnsearch.cpp (knnsearch, silhouette) and dim/pca.cpp (pca, pcares).
// Reference values come from numkit's parity-validated output (specs
// knnsearch/silhouette/pca/pcares.json), cross-checked against closed form
// (e.g. column means mu = 3.6, recon + res = X).

#include "dual_engine_fixture.hpp"

#include <cmath>

using namespace m_test;

class ClusterDimTest : public DualEngineTest
{};

// ── knnsearch: K nearest neighbours ─────────────────────────

TEST_P(ClusterDimTest, KnnsearchK3)
{
    eval("X = [1 1; 1 2; 2 1; 8 8; 9 8; 8 9]; Y = [1.2 1.8; 8.7 8.2]; "
         "[idx, D] = knnsearch(X, Y, 'K', 3);");
    EXPECT_DOUBLE_EQ(evalScalar("idx(1,1)"), 2.0);  // nearest to [1.2 1.8] is row 2
    EXPECT_DOUBLE_EQ(evalScalar("idx(1,2)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("idx(2,1)"), 5.0);  // nearest to [8.7 8.2] is row 5
    EXPECT_NEAR(evalScalar("D(1,1)"), 0.282842712475, 1e-9);
}

// ── silhouette: cohesion/separation in [-1,1] ───────────────

TEST_P(ClusterDimTest, SilhouetteWellSeparated)
{
    eval("X = [1 1; 1 2; 2 1; 8 8; 9 8; 8 9]; cl = [1 1 1 2 2 2]'; s = silhouette(X, cl);");
    EXPECT_EQ(eval("s").numel(), 6u);
    EXPECT_GT(evalScalar("min(s)"), 0.9);  // tight, far-apart clusters → ~1
    EXPECT_LE(evalScalar("max(s)"), 1.0);
}

// ── pca: coeff / latent / explained / mu / tsquared ─────────

TEST_P(ClusterDimTest, PcaDecomposition)
{
    eval("X = [1 2 3; 2 1 5; 3 4 1; 5 6 2; 7 5 4]; "
         "[coeff, score, latent, tsq, expl, mu] = pca(X);");
    EXPECT_NEAR(evalScalar("abs(coeff(1,1))"), 0.740886390786, 1e-9);
    EXPECT_NEAR(evalScalar("latent(1)"), 9.35740385499, 1e-8);
    EXPECT_NEAR(evalScalar("latent(2)"), 3.11992714849, 1e-8);
    EXPECT_NEAR(evalScalar("latent(3)"), 0.122668996516, 1e-8);
    EXPECT_NEAR(evalScalar("expl(1)"), 74.2651099602, 1e-6);
    EXPECT_NEAR(evalScalar("mu(1)"), 3.6, 1e-12);  // column mean
    EXPECT_NEAR(evalScalar("tsq(1)"), 1.35357766143, 1e-8);
}

// ── pcares: residuals + reconstruction (recon + res = X) ────

TEST_P(ClusterDimTest, PcaResReconstruction)
{
    eval("X = [1 2 3; 2 1 5; 3 4 1; 5 6 2; 7 5 4]; [res, recon] = pcares(X, 2);");
    EXPECT_NEAR(evalScalar("res(1,1)"), -0.0886138033704, 1e-9);
    EXPECT_NEAR(evalScalar("recon(1,1)"), 1.08861380337, 1e-9);
    EXPECT_NEAR(evalScalar("recon(1,1) + res(1,1)"), 1.0, 1e-9);  // == X(1,1)
}

INSTANTIATE_DUAL(ClusterDimTest);
