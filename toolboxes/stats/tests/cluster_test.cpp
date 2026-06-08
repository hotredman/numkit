// toolboxes/stats/tests/cluster_test.cpp
// cluster.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class ClusterTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override {
        engine.eval("import compat.*;");
        engine.eval("X = [1 1; 1.5 1.5; 5 5; 5.5 5.5; 10 10; 1 2; 6 6; 11 11];");
        engine.eval("Z = linkage(pdist(X));");
    }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// MATLAB and numkit can assign different cluster label IDs for the same
// partition; we test partition equivalence via same-cluster boolean
// queries instead of exact labels.

TEST_F(ClusterTest, MaxclustThreePartition)
{
    eval("T = cluster(Z, 'maxclust', 3);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(unique(T))"), 3.0);
    // Expected partition: {1,2,6} {3,4,7} {5,8}.
    EXPECT_DOUBLE_EQ(evalScalar("double(T(1)==T(2))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(T(1)==T(6))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(T(3)==T(4))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(T(3)==T(7))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(T(5)==T(8))"), 1.0);
    // Different clusters must differ.
    EXPECT_DOUBLE_EQ(evalScalar("double(T(1)==T(3))"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(T(1)==T(5))"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(T(3)==T(5))"), 0.0);
}

// 2026-05-08 — gap #1 closure: 'cutoff' default criterion is 'inconsistent'.
TEST_F(ClusterTest, CutoffInconsistencyDefault)
{
    // Inconsistency cutoff 0.5 → 3 clusters (root inc = 0.7259 > 0.5,
    // both subtrees deeper down satisfy inconsistency).
    eval("T = cluster(Z, 'cutoff', 0.5);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(unique(T))"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(T(1)==T(2))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(T(3)==T(4))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(T(5)==T(8))"), 1.0);
}

TEST_F(ClusterTest, CutoffInconsistencyMergesEverything)
{
    // Inconsistency cutoff 5 → 1 cluster (root inc 0.7259 < 5).
    eval("T = cluster(Z, 'cutoff', 5);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(unique(T))"), 1.0);
}

TEST_F(ClusterTest, CutoffInconsistencyVsDistanceDiffers)
{
    // gap #1: default was distance, MATLAB default is inconsistency.
    // For cutoff=0.5: with inconsistency we get 3 clusters; with
    // distance (every link is 0.7071 > 0.5) we get 8 singleton clusters.
    eval("Td = cluster(Z, 'cutoff', 0.5, 'criterion', 'distance');");
    eval("Ti = cluster(Z, 'cutoff', 0.5);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(unique(Td))"), 8.0);
    EXPECT_DOUBLE_EQ(evalScalar("numel(unique(Ti))"), 3.0);
}

// gap #2 closure: 'criterion' N-V is parsed.
TEST_F(ClusterTest, CriterionDistanceNVPaired)
{
    eval("T = cluster(Z, 'cutoff', 2, 'criterion', 'distance');");
    EXPECT_DOUBLE_EQ(evalScalar("numel(unique(T))"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(T(1)==T(2))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(T(3)==T(4))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(T(5)==T(8))"), 1.0);
}

// gap #3 closure: 'depth' N-V parsed (and propagated to inconsistent calc).
TEST_F(ClusterTest, DepthNVDoesNotCrash)
{
    eval("T = cluster(Z, 'cutoff', 0.5, 'depth', 4);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(unique(T))"), 3.0);
}
