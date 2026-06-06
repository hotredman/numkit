// libs/stats/tests/kmedoids_test.cpp
// kmedoids.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class KmedoidsTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override {
        engine.eval("import compat.*;");
        engine.eval("X = [0 0; 0.1 0; 0 0.1; 0.1 0.1; "
                    "5 5; 5.1 5; 5 5.1; 5.1 5.1; "
                    "10 0; 10.1 0; 10 0.1; 10.1 0.1];");
    }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// MATLAB and numkit assign different cluster IDs (RNG cascade depends
// on normrnd parity — out of scope for this spec); test partition
// equivalence and output shapes instead of literal labels.

TEST_F(KmedoidsTest, ThreeClustersPartition)
{
    eval("[idx, C] = kmedoids(X, 3);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(unique(idx))"), 3.0);
    // First 4 → cluster A, next 4 → B, last 4 → C.
    EXPECT_DOUBLE_EQ(evalScalar("double(idx(1)==idx(2))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(idx(1)==idx(4))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(idx(5)==idx(8))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(idx(9)==idx(12))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(idx(1)==idx(5))"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(C, 1)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(C, 2)"), 2.0);
}

// 2026-05-08 — gap closure: 4-output form (D matrix).
TEST_F(KmedoidsTest, FourOutputDistanceMatrix)
{
    eval("[idx, C, sumd, D] = kmedoids(X, 3);");
    EXPECT_DOUBLE_EQ(evalScalar("size(D, 1)"), 12.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(D, 2)"), 3.0);
    // Each row's minimum distance must be the distance to its own
    // assigned medoid; idx(i) selects that column.
    eval("min_d_per_row = min(D, [], 2);");
    EXPECT_GE(evalScalar("min_d_per_row(1)"), 0.0);
    EXPECT_LE(evalScalar("min_d_per_row(1)"),
              evalScalar("D(1, idx(1))") + 1e-12);
}

// gap closure: 5-output form (midx — medoid row indices).
TEST_F(KmedoidsTest, FiveOutputMidx)
{
    eval("[idx, C, sumd, D, midx] = kmedoids(X, 3);");
    EXPECT_DOUBLE_EQ(evalScalar("size(midx, 1)"), 3.0);
    // midx values must be valid row indices into X (1-based, 1..N).
    EXPECT_GE(evalScalar("midx(1)"), 1.0);
    EXPECT_LE(evalScalar("midx(1)"), 12.0);
    EXPECT_GE(evalScalar("midx(2)"), 1.0);
    EXPECT_LE(evalScalar("midx(2)"), 12.0);
    EXPECT_GE(evalScalar("midx(3)"), 1.0);
    EXPECT_LE(evalScalar("midx(3)"), 12.0);
}

// gap closure: 6-output form (info struct).
TEST_F(KmedoidsTest, SixOutputInfoStruct)
{
    eval("[idx, C, sumd, D, midx, info] = kmedoids(X, 3);");
    eval("alg = info.algorithm;");
    eval("dst = info.distance;");
    eval("its = info.iterations;");
    EXPECT_GE(evalScalar("its"), 1.0);
}

// gap closure: default Distance is 'sqeuclidean' (per MATLAB R2025b);
// previously numkit defaulted to 'euclidean'.
TEST_F(KmedoidsTest, DefaultDistanceSqEuclidean)
{
    eval("[idx, C, sumd, D, midx, info] = kmedoids(X, 3);");
    eval("dist_str = info.distance;");
    // info.distance must be 'sqeuclidean' string.
    eval("ok = strcmp(dist_str, 'sqeuclidean');");
    EXPECT_DOUBLE_EQ(evalScalar("double(ok)"), 1.0);
}

// gap closure: case-insensitive N-V keys + 'Algorithm' / 'Start' parse.
TEST_F(KmedoidsTest, AlgorithmStartNVAccepted)
{
    eval("[idx] = kmedoids(X, 3, 'algorithm', 'pam', 'start', 'plus', "
         "'replicates', 2);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(unique(idx))"), 3.0);
}

TEST_F(KmedoidsTest, DistanceCityblock)
{
    eval("[idx2] = kmedoids(X, 3, 'Distance', 'cityblock');");
    EXPECT_DOUBLE_EQ(evalScalar("numel(unique(idx2))"), 3.0);
}
