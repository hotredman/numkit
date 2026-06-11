// toolboxes/stats/tests/cholcov_test.cpp
//
// Regression guard for cholcov() — Cholesky-like factor of (possibly
// singular) covariance matrix.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class CholcovTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(CholcovTest, PositiveDefiniteUpperTriangular)
{
    // SIGMA = [4 2 0; 2 5 1; 0 1 3] -> standard chol output.
    eval("[T, p] = cholcov([4 2 0; 2 5 1; 0 1 3]);");
    EXPECT_DOUBLE_EQ(evalScalar("p"), 0.0);
    EXPECT_EQ(static_cast<int>(evalScalar("size(T, 1)")), 3);
    EXPECT_EQ(static_cast<int>(evalScalar("size(T, 2)")), 3);
    EXPECT_DOUBLE_EQ(evalScalar("T(1, 1)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("T(2, 1)"), 0.0);  // upper-triangular
    EXPECT_NEAR(evalScalar("T(3, 3)"), 1.6583123951777, 1e-12);
    eval("err = max(max(abs(T'*T - [4 2 0; 2 5 1; 0 1 3])));");
    EXPECT_LT(evalScalar("err"), 1e-12);
}

TEST_F(CholcovTest, IdentityIsItsOwnFactor)
{
    eval("[T, p] = cholcov(eye(3));");
    EXPECT_DOUBLE_EQ(evalScalar("p"), 0.0);
    eval("err = max(max(abs(T - eye(3))));");
    EXPECT_LT(evalScalar("err"), 1e-12);
}

TEST_F(CholcovTest, RankOnePSDGivesRowFactor)
{
    // [4 2; 2 1] = [2;1]*[2 1] -> T = [2 1] (1×2)
    eval("[T, p] = cholcov([4 2; 2 1]);");
    EXPECT_DOUBLE_EQ(evalScalar("p"), 0.0);
    EXPECT_EQ(static_cast<int>(evalScalar("size(T, 1)")), 1);
    EXPECT_EQ(static_cast<int>(evalScalar("size(T, 2)")), 2);
    eval("err = max(max(abs(T'*T - [4 2; 2 1])));");
    EXPECT_LT(evalScalar("err"), 1e-12);
}

TEST_F(CholcovTest, IndefiniteReturnsEmptyTPositiveP)
{
    // [1 0; 0 -1] has 1 negative eigvalue -> p = 1, T empty.
    eval("[T, p] = cholcov([1 0; 0 -1]);");
    EXPECT_DOUBLE_EQ(evalScalar("p"), 1.0);
    EXPECT_EQ(static_cast<int>(evalScalar("size(T, 1)")), 0);
}

TEST_F(CholcovTest, NegativeDefiniteCountsAllEigvals)
{
    // -eye(2) has 2 negative eigvals -> p = 2.
    eval("[T, p] = cholcov(-eye(2));");
    EXPECT_DOUBLE_EQ(evalScalar("p"), 2.0);
    EXPECT_EQ(static_cast<int>(evalScalar("size(T, 1)")), 0);
}

TEST_F(CholcovTest, EmptyInput)
{
    eval("[T, p] = cholcov(zeros(0, 0));");
    EXPECT_DOUBLE_EQ(evalScalar("p"), 0.0);
    EXPECT_EQ(static_cast<int>(evalScalar("size(T, 1)")), 0);
}

TEST_F(CholcovTest, RejectsNonSquare)
{
    bool threw = false;
    try { eval("cholcov([1 2; 3 4; 5 6]);"); } catch (...) { threw = true; }
    EXPECT_TRUE(threw);
}

TEST_F(CholcovTest, ReconstructionForSymmetricPSD)
{
    // Build a known PSD matrix from M*M' with low-rank M.
    eval("M = [1 2 3; 2 1 0];"
         "S = M' * M;"  // 3×3 PSD rank 2
         "[T, p] = cholcov(S);"
         "err = max(max(abs(T'*T - S)));");
    EXPECT_DOUBLE_EQ(evalScalar("p"), 0.0);
    EXPECT_LT(evalScalar("err"), 1e-10);
}
