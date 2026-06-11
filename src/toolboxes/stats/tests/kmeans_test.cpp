// toolboxes/stats/tests/kmeans_test.cpp
// kmeans.

#include <numkit/core/engine.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class KmeansTest : public ::testing::Test
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

TEST_F(KmeansTest, ThreeClustersPartition)
{
    eval("[idx, C] = kmeans(X, 3, 'Replicates', 5);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(unique(idx))"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(idx(1)==idx(4))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(idx(5)==idx(8))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(idx(9)==idx(12))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(idx(1)==idx(5))"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(C, 1)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(C, 2)"), 2.0);
}

// 2026-05-08 — gap closure: 4-output form (D matrix N×K).
TEST_F(KmeansTest, FourOutputDistanceMatrix)
{
    eval("[idx, C, sumd, D] = kmeans(X, 3, 'Replicates', 5);");
    EXPECT_DOUBLE_EQ(evalScalar("size(D, 1)"), 12.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(D, 2)"), 3.0);
    // D values are squared distances (default Distance = 'sqeuclidean').
    // The minimum per row corresponds to the assigned cluster.
    eval("min_d_row1 = min(D(1, :));");
    EXPECT_NEAR(evalScalar("D(1, idx(1))"),
                evalScalar("min_d_row1"), 1e-12);
}

// gap closure: case-insensitive N-V parsing.
TEST_F(KmeansTest, MixedCaseNV)
{
    eval("[idx] = kmeans(X, 3, 'maxiter', 200, 'replicates', 5);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(unique(idx))"), 3.0);
}

// gap closure: 'Display' / 'EmptyAction' / 'OnlinePhase' silent accept.
TEST_F(KmeansTest, SilentNV)
{
    eval("[idx] = kmeans(X, 3, 'Display', 'off', 'EmptyAction', 'singleton', "
         "'OnlinePhase', 'on');");
    EXPECT_DOUBLE_EQ(evalScalar("numel(unique(idx))"), 3.0);
}

// 'Distance' = sqeuclidean accepted; other metrics raise a clear error.
TEST_F(KmeansTest, DistanceSqeuclideanAccepted)
{
    eval("[idx] = kmeans(X, 3, 'Distance', 'sqeuclidean');");
    EXPECT_DOUBLE_EQ(evalScalar("numel(unique(idx))"), 3.0);
}

TEST_F(KmeansTest, DistanceCityblockRejected)
{
    bool threw = false;
    try {
        eval("kmeans(X, 3, 'Distance', 'cityblock');");
    } catch (const std::exception &) {
        threw = true;
    }
    EXPECT_TRUE(threw);
}

TEST_F(KmeansTest, StartPlusAccepted)
{
    eval("[idx] = kmeans(X, 3, 'Start', 'plus');");
    EXPECT_DOUBLE_EQ(evalScalar("numel(unique(idx))"), 3.0);
}
