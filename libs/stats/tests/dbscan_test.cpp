// libs/stats/tests/dbscan_test.cpp
// dbscan.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class DbscanTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override {
        engine.eval("import compat.*;");
        engine.eval("X = [0 0; 0.1 0; 0 0.1; 0.1 0.1; "
                    "5 5; 5.1 5; 5 5.1; 5.1 5.1; "
                    "10 0; 10.1 0; 10 0.1; 20 20];");
    }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// Three tight blobs + one isolated point. minpts=3, eps=0.5 →
// blob1=[1..4]=cluster 1, blob2=[5..8]=cluster 2, blob3=[9..11]=cluster 3
// (only 3 points so all three are core), point 12 is noise (-1).
// MATLAB R2025b uses -1 for noise (not 0).

TEST_F(DbscanTest, BasicEuclidean)
{
    eval("[idx, core] = dbscan(X, 0.5, 3);");
    EXPECT_DOUBLE_EQ(evalScalar("idx(1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("idx(4)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("idx(5)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("idx(8)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("idx(9)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("idx(11)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("idx(12)"), -1.0);
    // Core points: blob1+blob2+blob3 all dense enough to be core; the
    // isolated point 12 is not core.
    EXPECT_DOUBLE_EQ(evalScalar("double(core(1))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(core(5))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(core(9))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(core(12))"), 0.0);
}

// 2026-05-08 — gap closure: noise was previously 0, MATLAB uses -1.
TEST_F(DbscanTest, NoiseLabelIsMinusOne)
{
    eval("idx = dbscan(X, 0.5, 3);");
    EXPECT_DOUBLE_EQ(evalScalar("idx(12)"), -1.0);
}

// gap closure: 'Distance', 'precomputed' supported.
TEST_F(DbscanTest, PrecomputedDistanceMatrix)
{
    eval("D = pdist2(X, X);");
    eval("idx = dbscan(D, 0.5, 3, 'Distance', 'precomputed');");
    EXPECT_DOUBLE_EQ(evalScalar("idx(1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("idx(5)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("idx(9)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("idx(12)"), -1.0);
}

// gap closure: 'P' parameter for Minkowski.
TEST_F(DbscanTest, MinkowskiP)
{
    eval("idx = dbscan(X, 0.5, 3, 'Distance', 'minkowski', 'P', 3);");
    EXPECT_DOUBLE_EQ(evalScalar("idx(1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("idx(5)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("idx(9)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("idx(12)"), -1.0);
}

// gap closure: 'Distance' N-V keyword (was previously positional only).
TEST_F(DbscanTest, DistanceCityblockKeyword)
{
    eval("idx = dbscan(X, 1.0, 3, 'Distance', 'cityblock');");
    EXPECT_DOUBLE_EQ(evalScalar("idx(1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("idx(5)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("idx(9)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("idx(12)"), -1.0);
}
