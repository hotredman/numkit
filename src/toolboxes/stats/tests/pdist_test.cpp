// toolboxes/stats/tests/pdist_test.cpp
// pdist.

#include <numkit/core/engine.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class PdistTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override {}
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(PdistTest, EuclideanDefault)
{
    eval("X = [1 1; 2 2; 3 3; 6 5; 7 4]; d = pdist(X);");
    EXPECT_NEAR(evalScalar("d(1)"), 1.4142135624, 1e-9);
    EXPECT_NEAR(evalScalar("d(4)"), 6.7082039325, 1e-9);
    EXPECT_NEAR(evalScalar("d(10)"), 1.4142135624, 1e-9);
}

TEST_F(PdistTest, Cityblock)
{
    eval("X = [1 1; 2 2; 3 3; 6 5; 7 4]; d = pdist(X, 'cityblock');");
    EXPECT_DOUBLE_EQ(evalScalar("d(1)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("d(4)"), 9.0);
}

TEST_F(PdistTest, Minkowski)
{
    eval("X = [1 1; 2 2; 3 3; 6 5; 7 4]; d = pdist(X, 'minkowski', 3);");
    EXPECT_NEAR(evalScalar("d(1)"), 1.2599210499, 1e-9);
    EXPECT_NEAR(evalScalar("d(4)"), 6.2402530734, 1e-5);
}

TEST_F(PdistTest, Cosine)
{
    eval("X = [1 1; 2 2; 3 3; 6 5; 7 4]; d = pdist(X, 'cosine');");
    EXPECT_NEAR(evalScalar("d(3)"), 0.0041067881, 1e-7);
    EXPECT_NEAR(evalScalar("d(4)"), 0.0352361657, 1e-7);
}

// Bug fix 2026-05-08 — added 'mahalanobis' metric (was throwing
// "unknown metric").

TEST_F(PdistTest, MahalanobisDefaultCov)
{
    // No C supplied → uses cov(X). Values match MATLAB at ~1e-5 (Cinv
    // computation introduces small numerical drift).
    eval("X = [1 1; 2 2; 3 3; 6 5; 7 4]; d = pdist(X, 'mahalanobis');");
    EXPECT_NEAR(evalScalar("d(1)"),  0.7953, 1e-3);
    EXPECT_NEAR(evalScalar("d(2)"),  1.5907, 1e-3);
    EXPECT_NEAR(evalScalar("d(4)"),  2.3860, 1e-3);
    EXPECT_NEAR(evalScalar("d(10)"), 2.4928, 1e-3);
}

TEST_F(PdistTest, MahalanobisExplicitC)
{
    // C = identity → equivalent to euclidean distances.
    eval("X = [1 1; 2 2; 3 3; 6 5; 7 4]; d = pdist(X, 'mahalanobis', eye(2));");
    EXPECT_NEAR(evalScalar("d(1)"), 1.4142135624, 1e-9);
    EXPECT_NEAR(evalScalar("d(4)"), 6.7082039325, 1e-9);
}

TEST_F(PdistTest, Pdist2MahalanobisDefault)
{
    // pdist2 uses cov(X) (the FIRST arg) by default for Mahalanobis.
    // Verified via R2025b probe.
    eval("X = [1 1; 2 2; 3 3; 6 5; 7 4]; Y = [0 0; 1 0]; d = pdist2(X, Y, 'mahalanobis');");
    EXPECT_EQ(static_cast<size_t>(evalScalar("size(d, 1)")), 5u);
    EXPECT_EQ(static_cast<size_t>(evalScalar("size(d, 2)")), 2u);
}

// 2026-05-08: pdist2 'Smallest'/'Largest' k mode (closes spec
// gaps #1-#2).

TEST_F(PdistTest, Pdist2SmallestK)
{
    eval("A = [1 2; 3 4; 5 6; 7 8; 9 10]; B = [1 2; 5 5; 9 9];");
    eval("[D, I] = pdist2(A, B, 'euclidean', 'Smallest', 2);");
    EXPECT_EQ(static_cast<size_t>(evalScalar("size(D, 1)")), 2u);
    EXPECT_EQ(static_cast<size_t>(evalScalar("size(D, 2)")), 3u);
    // Per-column smallest (ascending).
    EXPECT_NEAR(evalScalar("D(1,1)"), 0.0, 1e-12);
    EXPECT_NEAR(evalScalar("D(2,1)"), 2.8284271247, 1e-9);
    EXPECT_NEAR(evalScalar("D(1,2)"), 1.0, 1e-12);
    EXPECT_NEAR(evalScalar("D(2,2)"), 2.2360679775, 1e-9);
    EXPECT_NEAR(evalScalar("D(1,3)"), 1.0, 1e-12);
    EXPECT_NEAR(evalScalar("D(2,3)"), 2.2360679775, 1e-9);
    // Indices into A (1-based), per column.
    EXPECT_DOUBLE_EQ(evalScalar("I(1,1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("I(2,1)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("I(1,2)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("I(2,2)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("I(1,3)"), 5.0);
    EXPECT_DOUBLE_EQ(evalScalar("I(2,3)"), 4.0);
}

TEST_F(PdistTest, Pdist2LargestK)
{
    eval("A = [1 2; 3 4; 5 6; 7 8; 9 10]; B = [1 2; 5 5; 9 9];");
    eval("[D, I] = pdist2(A, B, 'euclidean', 'Largest', 2);");
    // Largest 2 per column, descending.
    EXPECT_NEAR(evalScalar("D(1,1)"), 11.3137084990, 1e-9);
    EXPECT_NEAR(evalScalar("D(2,1)"),  8.4852813742, 1e-9);
    EXPECT_NEAR(evalScalar("D(1,2)"),  6.4031242374, 1e-9);
    EXPECT_NEAR(evalScalar("D(2,2)"),  5.0, 1e-12);
    EXPECT_NEAR(evalScalar("D(1,3)"), 10.6301458127, 1e-9);
    EXPECT_NEAR(evalScalar("D(2,3)"),  7.8102496759, 1e-9);
    EXPECT_DOUBLE_EQ(evalScalar("I(1,1)"), 5.0);
    EXPECT_DOUBLE_EQ(evalScalar("I(2,1)"), 4.0);
    EXPECT_DOUBLE_EQ(evalScalar("I(1,2)"), 5.0);
    EXPECT_DOUBLE_EQ(evalScalar("I(2,2)"), 1.0);
}
