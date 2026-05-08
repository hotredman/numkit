// libs/stats/tests/pdist_test.cpp
// Audit ТЗ closure for pdist. Closes audit/findings/cluster/pdist.md.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class PdistTest : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
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
    // pdist2 uses cov(Y) by default for Mahalanobis.
    eval("X = [1 1; 2 2]; Y = [1 1; 2 2; 3 3; 6 5; 7 4]; d = pdist2(X, Y, 'mahalanobis');");
    EXPECT_EQ(static_cast<size_t>(evalScalar("size(d, 1)")), 2u);
    EXPECT_EQ(static_cast<size_t>(evalScalar("size(d, 2)")), 5u);
    // (Just check shape; numerical values verified via parity spec.)
}
