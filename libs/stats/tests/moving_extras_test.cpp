// libs/stats/tests/moving_extras_test.cpp
//
// Tests for B1 (moving stats: mov*, smoothdata, hampel) and B2
// (descriptive extras: bounds, iqr, maxk, mink, rmse).

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <cmath>
#include <gtest/gtest.h>

using namespace numkit;

class MovingTest : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// ── B1 — moving stats ────────────────────────────────────────────────

TEST_F(MovingTest, MovmeanCenteredK3)
{
    // movmean([1 2 3 4 5], 3) → [1.5, 2, 3, 4, 4.5]   (centred, shrink edges)
    eval("y = movmean([1 2 3 4 5], 3);");
    EXPECT_NEAR(evalScalar("y(1)"), 1.5, 1e-12);
    EXPECT_NEAR(evalScalar("y(2)"), 2.0, 1e-12);
    EXPECT_NEAR(evalScalar("y(3)"), 3.0, 1e-12);
    EXPECT_NEAR(evalScalar("y(4)"), 4.0, 1e-12);
    EXPECT_NEAR(evalScalar("y(5)"), 4.5, 1e-12);
}

TEST_F(MovingTest, MovsumK2)
{
    // k=2 → kb=0, kf=1 → window [i, i+1] truncated → asymmetric trailing.
    // [1 2 3 4] → [3, 5, 7, 4]
    eval("y = movsum([1 2 3 4], 2);");
    EXPECT_NEAR(evalScalar("y(1)"), 3.0, 1e-12);
    EXPECT_NEAR(evalScalar("y(2)"), 5.0, 1e-12);
    EXPECT_NEAR(evalScalar("y(3)"), 7.0, 1e-12);
    EXPECT_NEAR(evalScalar("y(4)"), 4.0, 1e-12);
}

TEST_F(MovingTest, MovminMovmaxConstant)
{
    eval("a = movmin([3 1 4 1 5], 3);");
    EXPECT_NEAR(evalScalar("a(3)"), 1.0, 1e-12);   // window {1,4,1} min = 1
    eval("b = movmax([3 1 4 1 5], 3);");
    EXPECT_NEAR(evalScalar("b(3)"), 4.0, 1e-12);   // window {1,4,1} max = 4
    EXPECT_NEAR(evalScalar("b(4)"), 5.0, 1e-12);
}

TEST_F(MovingTest, MovmedianOddOddWindow)
{
    eval("y = movmedian([1 100 2 100 3], 3);");
    EXPECT_NEAR(evalScalar("y(2)"), 2.0, 1e-12);    // median {1,100,2}=2
    EXPECT_NEAR(evalScalar("y(3)"), 100.0, 1e-12);  // median {100,2,100}=100
    EXPECT_NEAR(evalScalar("y(4)"), 3.0, 1e-12);
}

TEST_F(MovingTest, MovvarConstantWindowZero)
{
    eval("v = movvar([5 5 5 5 5], 3);");
    EXPECT_NEAR(evalScalar("v(3)"), 0.0, 1e-12);
}

TEST_F(MovingTest, MovstdMatchesSqrtMovvar)
{
    eval("v = movvar([1 2 3 4 5], 3); s = movstd([1 2 3 4 5], 3);");
    for (int i = 1; i <= 5; ++i) {
        const double vi = evalScalar("v(" + std::to_string(i) + ")");
        const double si = evalScalar("s(" + std::to_string(i) + ")");
        EXPECT_NEAR(si, std::sqrt(vi), 1e-12);
    }
}

TEST_F(MovingTest, MovprodWindow)
{
    // movprod([1 2 3 4], 2) — k=2 centred window = [i, i+1] →
    //   i=1 {1,2}=2, i=2 {2,3}=6, i=3 {3,4}=12, i=4 {4}=4
    eval("y = movprod([1 2 3 4], 2);");
    EXPECT_NEAR(evalScalar("y(1)"),  2.0, 1e-12);
    EXPECT_NEAR(evalScalar("y(2)"),  6.0, 1e-12);
    EXPECT_NEAR(evalScalar("y(3)"), 12.0, 1e-12);
    EXPECT_NEAR(evalScalar("y(4)"),  4.0, 1e-12);
}

TEST_F(MovingTest, MovmadAroundConstantIsZero)
{
    eval("y = movmad([3 3 3 3 3], 3);");
    EXPECT_NEAR(evalScalar("y(3)"), 0.0, 1e-12);
}

TEST_F(MovingTest, SmoothdataMovmeanDefault)
{
    // smoothdata with no method defaults to movmean.
    eval("y = smoothdata([1 2 3 4 5]);");
    EXPECT_EQ(eval("y").numel(), 5u);
}

TEST_F(MovingTest, SmoothdataGaussian)
{
    eval("y = smoothdata([1 2 3 4 5], 'gaussian', 3);");
    EXPECT_EQ(eval("y").numel(), 5u);
    // Centre value is the gaussian-weighted mean of {2, 3, 4} ≈ 3.
    EXPECT_NEAR(evalScalar("y(3)"), 3.0, 1e-9);
}

TEST_F(MovingTest, HampelReplacesOutlier)
{
    // Insert one big outlier; hampel with default k=3 should replace it.
    eval("x = [1 2 3 100 5 6 7]; y = hampel(x);");
    EXPECT_LT(evalScalar("y(4)"), 50.0);   // outlier got knocked down
}

TEST_F(MovingTest, MovmeanMatrixDim1)
{
    // 4×2 column-wise (dim=1) movmean, k=2 → centred window [i, i+1].
    // col 1: {1,2}=1.5, {2,3}=2.5, {3,4}=3.5, {4}=4
    // col 2: {10,20}=15, {20,30}=25, {30,40}=35, {40}=40
    eval("M = [1 10; 2 20; 3 30; 4 40];");
    eval("y = movmean(M, 2, 1);");
    EXPECT_NEAR(evalScalar("y(1,1)"), 1.5,  1e-12);
    EXPECT_NEAR(evalScalar("y(2,1)"), 2.5,  1e-12);
    EXPECT_NEAR(evalScalar("y(3,1)"), 3.5,  1e-12);
    EXPECT_NEAR(evalScalar("y(4,2)"), 40.0, 1e-12);
}

TEST_F(MovingTest, MovmeanMatrixDim2)
{
    // 2×3 row-wise (dim=2) movmean, k=2 → window [j, j+1].
    // row 1: {1,2}=1.5, {2,3}=2.5, {3}=3
    // row 2: {4,5}=4.5, {5,6}=5.5, {6}=6
    eval("M = [1 2 3; 4 5 6];");
    eval("y = movmean(M, 2, 2);");
    EXPECT_NEAR(evalScalar("y(1,1)"), 1.5, 1e-12);
    EXPECT_NEAR(evalScalar("y(1,2)"), 2.5, 1e-12);
    EXPECT_NEAR(evalScalar("y(2,3)"), 6.0, 1e-12);
}

TEST_F(MovingTest, MovingAsymmetricWindow)
{
    // [kb kf] form: trailing window of 3 ([i-2, i]).
    eval("y = movsum([1 2 3 4 5], [2 0]);");
    // [1, 3, 6, 9, 12]
    EXPECT_NEAR(evalScalar("y(1)"), 1.0, 1e-12);
    EXPECT_NEAR(evalScalar("y(2)"), 3.0, 1e-12);
    EXPECT_NEAR(evalScalar("y(3)"), 6.0, 1e-12);
    EXPECT_NEAR(evalScalar("y(4)"), 9.0, 1e-12);
    EXPECT_NEAR(evalScalar("y(5)"), 12.0, 1e-12);
}

// ── B2 — descriptive extras ──────────────────────────────────────────

TEST_F(MovingTest, BoundsTwoOutputs)
{
    eval("[lo, hi] = bounds([3 1 4 1 5 9 2 6]);");
    EXPECT_NEAR(evalScalar("lo"), 1.0, 1e-12);
    EXPECT_NEAR(evalScalar("hi"), 9.0, 1e-12);
}

TEST_F(MovingTest, IqrUniformVector)
{
    // [1..9], q1=3, q3=7, iqr=4.
    EXPECT_NEAR(evalScalar("iqr(1:9)"), 4.0, 1e-12);
}

TEST_F(MovingTest, MaxkTopThree)
{
    // maxk([3 1 4 1 5 9 2 6], 3) → [9, 6, 5]
    eval("y = maxk([3 1 4 1 5 9 2 6], 3);");
    EXPECT_NEAR(evalScalar("y(1)"), 9.0, 1e-12);
    EXPECT_NEAR(evalScalar("y(2)"), 6.0, 1e-12);
    EXPECT_NEAR(evalScalar("y(3)"), 5.0, 1e-12);
}

TEST_F(MovingTest, MinkBottomTwo)
{
    eval("y = mink([3 1 4 1 5 9 2 6], 2);");
    EXPECT_NEAR(evalScalar("y(1)"), 1.0, 1e-12);
    EXPECT_NEAR(evalScalar("y(2)"), 1.0, 1e-12);
}

TEST_F(MovingTest, RmseScalarVsVector)
{
    // rmse([1 2 3 4], 0) → sqrt(mean([1 4 9 16])) = sqrt(7.5)
    EXPECT_NEAR(evalScalar("rmse([1 2 3 4], 0)"), std::sqrt(7.5), 1e-12);
}

TEST_F(MovingTest, RmseTwoVectorsZeroOnIdentical)
{
    EXPECT_NEAR(evalScalar("rmse([1 2 3], [1 2 3])"), 0.0, 1e-12);
}

// ── Pack 36: mape ─────────────────────────────────────────────────
TEST_F(MovingTest, MapeKnownExample)
{
    // F = [98 90 110 120], A = 100 → per-elem |Δ|/A = [0.02 0.10 0.10 0.20]
    // mean × 100 = 10.5
    EXPECT_NEAR(evalScalar("mape([98 90 110 120], [100 100 100 100])"), 10.5, 1e-12);
}

TEST_F(MovingTest, MapeIdenticalIsZero)
{
    EXPECT_NEAR(evalScalar("mape([1 2 3 4], [1 2 3 4])"), 0.0, 1e-12);
}

TEST_F(MovingTest, MapeScalarBroadcast)
{
    // F scalar 0, A = [10 20] → mean(|10|/10, |20|/20)*100 = 100.
    EXPECT_NEAR(evalScalar("mape(0, [10 20])"), 100.0, 1e-12);
}

TEST_F(MovingTest, MapeAlongDim2)
{
    // 2x2 -> mape per row.
    eval("M = mape([1 2; 3 4], [2 4; 6 8], 2);");
    EXPECT_NEAR(evalScalar("M(1)"), 50.0, 1e-12);  // (|1/2|+|2/4|)/2 *100 = 50
    EXPECT_NEAR(evalScalar("M(2)"), 50.0, 1e-12);
}
