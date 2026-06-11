// toolboxes/stats/tests/moving_extras_test.cpp
//
// Tests for B1 (moving stats: mov*, smoothdata, hampel) and B2
// (descriptive extras: bounds, iqr, maxk, mink, rmse).

#include <numkit/core/engine.hpp>
#include <numkit/core/engine.hpp>
#include <cmath>
#include <gtest/gtest.h>

using namespace numkit;

class MovingTest : public ::testing::Test
{
public:
    StandardEngine engine;
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
    // MATLAB: even k centres on current+previous → kb=1, kf=0 → window [i-1, i].
    // movsum([1 2 3 4], 2) = [1, 3, 5, 7]. (Was forward [3 5 7 4]; fixed 2026-05-29.)
    eval("y = movsum([1 2 3 4], 2);");
    EXPECT_NEAR(evalScalar("y(1)"), 1.0, 1e-12);
    EXPECT_NEAR(evalScalar("y(2)"), 3.0, 1e-12);
    EXPECT_NEAR(evalScalar("y(3)"), 5.0, 1e-12);
    EXPECT_NEAR(evalScalar("y(4)"), 7.0, 1e-12);
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
    // movprod([1 2 3 4], 2) — MATLAB even k = backward window [i-1, i] →
    //   i=1 {1}=1, i=2 {1,2}=2, i=3 {2,3}=6, i=4 {3,4}=12 -> [1 2 6 12].
    eval("y = movprod([1 2 3 4], 2);");
    EXPECT_NEAR(evalScalar("y(1)"),  1.0, 1e-12);
    EXPECT_NEAR(evalScalar("y(2)"),  2.0, 1e-12);
    EXPECT_NEAR(evalScalar("y(3)"),  6.0, 1e-12);
    EXPECT_NEAR(evalScalar("y(4)"), 12.0, 1e-12);
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

// smoothdata 'gaussian' kernel: MATLAB R2025b uses sigma = windowLength/5,
// centred on the CURRENT sample, with 'shrink' endpoints (truncate +
// renormalise). Was sigma=(k-1)/4 with a mis-aligned edge kernel -> wrong at
// the boundaries and interior. vs MATLAB R2025b. DEEP-PROBE 2026-05-31.
TEST_F(MovingTest, SmoothdataGaussianMatchesMatlab)
{
    // smoothdata([1 5 2 8 3],'gaussian',3) = [1.79834 3.83535 3.49741 6.16984 3.99793]
    eval("y = smoothdata([1 5 2 8 3], 'gaussian', 3);");
    EXPECT_NEAR(evalScalar("y(1)"), 1.798335, 1e-5);  // left edge (truncated+renorm)
    EXPECT_NEAR(evalScalar("y(2)"), 3.835353, 1e-5);
    EXPECT_NEAR(evalScalar("y(3)"), 3.497406, 1e-5);
    EXPECT_NEAR(evalScalar("y(5)"), 3.997931, 1e-5);  // right edge
    // window 5 interior + edges.
    eval("z = smoothdata([1 5 2 8 3 9 4], 'gaussian', 5);");
    EXPECT_NEAR(evalScalar("z(1)"), 2.470528, 1e-5);
    EXPECT_NEAR(evalScalar("z(4)"), 5.204815, 1e-5);
    EXPECT_NEAR(evalScalar("z(7)"), 5.663341, 1e-5);
}

TEST_F(MovingTest, HampelReplacesOutlier)
{
    // Insert one big outlier; hampel with default k=3 should replace it.
    eval("x = [1 2 3 100 5 6 7]; y = hampel(x);");
    EXPECT_LT(evalScalar("y(4)"), 50.0);   // outlier got knocked down
}

// [y,i,xmedian,xsigma] = hampel(x): the 2nd/3rd/4th outputs (outlier mask,
// local median, local 1.4826*MAD) were unimplemented. vs MATLAB R2025b on
// x = [1 2 100 3 4]. DEEP-PROBE 2026-05-31.
TEST_F(MovingTest, HampelMultiOutput)
{
    eval("[y, i, xmed, xsig] = hampel([1 2 100 3 4]);");
    // y filtered.
    EXPECT_DOUBLE_EQ(evalScalar("y(3)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(1)"), 1.0);
    // i = logical outlier mask.
    EXPECT_DOUBLE_EQ(evalScalar("double(i(3))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("sum(double(i))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(islogical(i))"), 1.0);
    // xmedian = local window median.
    EXPECT_DOUBLE_EQ(evalScalar("xmed(1)"), 2.5);
    EXPECT_DOUBLE_EQ(evalScalar("xmed(5)"), 3.5);
    // xsigma = MATLAB-exact 1.4826...*MAD (here MAD=1).
    EXPECT_NEAR(evalScalar("xsig(1)"), 1.482602218505602, 1e-12);
    // k=2 with two outliers: both flagged, scaled sigma at the second.
    eval("[y2, i2, m2, s2] = hampel([1 2 3 100 5 6 7 200 9 10], 2);");
    EXPECT_DOUBLE_EQ(evalScalar("sum(double(i2))"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("y2(4)"), 5.0);
    EXPECT_DOUBLE_EQ(evalScalar("y2(8)"), 9.0);
    EXPECT_NEAR(evalScalar("s2(4)"), 2.965204437011204, 1e-10);
}

TEST_F(MovingTest, MovmeanMatrixDim1)
{
    // 4×2 column-wise (dim=1) movmean, k=2 → MATLAB backward window [i-1, i].
    // col 1: {1}=1, {1,2}=1.5, {2,3}=2.5, {3,4}=3.5
    // col 2: {10}=10, {10,20}=15, {20,30}=25, {30,40}=35
    eval("M = [1 10; 2 20; 3 30; 4 40];");
    eval("y = movmean(M, 2, 1);");
    EXPECT_NEAR(evalScalar("y(1,1)"), 1.0,  1e-12);
    EXPECT_NEAR(evalScalar("y(2,1)"), 1.5,  1e-12);
    EXPECT_NEAR(evalScalar("y(3,1)"), 2.5,  1e-12);
    EXPECT_NEAR(evalScalar("y(4,2)"), 35.0, 1e-12);
}

TEST_F(MovingTest, MovmeanMatrixDim2)
{
    // 2×3 row-wise (dim=2) movmean, k=2 → MATLAB backward window [j-1, j].
    // row 1: {1}=1, {1,2}=1.5, {2,3}=2.5
    // row 2: {4}=4, {4,5}=4.5, {5,6}=5.5
    eval("M = [1 2 3; 4 5 6];");
    eval("y = movmean(M, 2, 2);");
    EXPECT_NEAR(evalScalar("y(1,1)"), 1.0, 1e-12);
    EXPECT_NEAR(evalScalar("y(1,2)"), 1.5, 1e-12);
    EXPECT_NEAR(evalScalar("y(2,3)"), 5.5, 1e-12);
}

// MATLAB even-length scalar windows lean BACKWARD (centre = current+previous,
// kb=k/2, kf=k/2-1) across the whole mov* family. Odd windows stay symmetric.
// Regression for the 2026-05-29 fix (numkit had even windows leaning forward).
TEST_F(MovingTest, EvenWindowLeansBackwardLikeMATLAB)
{
    eval("s = movsum([1 2 3 4], 2);");      // [1 3 5 7]
    EXPECT_NEAR(evalScalar("s(1)"), 1.0, 1e-12);
    EXPECT_NEAR(evalScalar("s(4)"), 7.0, 1e-12);
    eval("mx = movmax([1 5 2 8], 2);");     // [1 5 5 8]
    EXPECT_NEAR(evalScalar("mx(1)"), 1.0, 1e-12);
    EXPECT_NEAR(evalScalar("mx(3)"), 5.0, 1e-12);
    eval("mn = movmin([4 1 3 2], 2);");     // [4 1 1 2]
    EXPECT_NEAR(evalScalar("mn(1)"), 4.0, 1e-12);
    EXPECT_NEAR(evalScalar("mn(2)"), 1.0, 1e-12);
    eval("md = movmedian([1 2 3 4], 2);");  // [1 1.5 2.5 3.5]
    EXPECT_NEAR(evalScalar("md(1)"), 1.0, 1e-12);
    EXPECT_NEAR(evalScalar("md(4)"), 3.5, 1e-12);
    // k=4 backward window [i-2, i+1]: movsum([1 2 3 4 5 6],4)=[3 6 10 14 18 15]
    eval("s4 = movsum([1 2 3 4 5 6], 4);");
    EXPECT_NEAR(evalScalar("s4(1)"),  3.0, 1e-12);
    EXPECT_NEAR(evalScalar("s4(4)"), 14.0, 1e-12);
    EXPECT_NEAR(evalScalar("s4(6)"), 15.0, 1e-12);
    // odd window unchanged (symmetric): movsum([1 2 3 4],3)=[3 6 9 7]
    eval("s3 = movsum([1 2 3 4], 3);");
    EXPECT_NEAR(evalScalar("s3(2)"), 6.0, 1e-12);
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
    // iqr uses MATLAB's default quantile method ((i-0.5)/n). For 1:9 that
    // gives q1=2.75, q3=7.25, so iqr = 4.5 (not the textbook q1=3,q3=7).
    EXPECT_NEAR(evalScalar("iqr(1:9)"), 4.5, 1e-12);
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
