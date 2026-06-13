// toolboxes/image/tests/filtering_morphology_test.cpp
//
// Coverage for parity-only image filtering / correlation / morphology:
//   ordfilt2 rangefilt medfilt3 imsmooth        (order-stat / smoothing)
//   normxcorr2 fftconv2 convmtx2                 (correlation / convolution)
//   mmgradm grayconnected imextendedmax
//   imextendedmin imimposemin                    (morphology)
//   imsplit imoverlay otsuthresh                 (channels / overlay / threshold)
// Anchored on a 3x3 ramp where order statistics are exact (median of 1..9 = 5,
// local range = 8) and self-consistency checks (fftconv2 == conv2,
// normxcorr2 peak = 1 at a perfect match).

#include "dual_engine_fixture.hpp"

using namespace m_test;

class FilteringMorphologyTest : public DualEngineTest
{
protected:
    void SetUp() override
    {
        DualEngineTest::SetUp();
        eval("A = [1 2 3; 4 5 6; 7 8 9];");
    }
};

TEST_P(FilteringMorphologyTest, OrderStatAndSmooth)
{
    eval("o = ordfilt2(A, 5, ones(3,3));");
    EXPECT_DOUBLE_EQ(evalScalar("o(2,2)"), 5.0);    // median of the 3x3 neighbourhood
    eval("rf = rangefilt(A);");
    EXPECT_DOUBLE_EQ(evalScalar("rf(2,2)"), 8.0);   // local max - min = 9 - 1
    eval("s = imsmooth(A, 1);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(s,1)")), 3);
    EXPECT_DOUBLE_EQ(evalScalar("all(isfinite(s(:)))"), 1.0);
    eval("v = medfilt3(repmat(reshape(1:9,3,3), [1 1 3]));");
    EXPECT_EQ(static_cast<int>(evalScalar("size(v,3)")), 3);
}

TEST_P(FilteringMorphologyTest, CorrelationConvolution)
{
    eval("c = normxcorr2([1 2; 3 4], [1 2; 3 4]);");
    EXPECT_NEAR(evalScalar("max(c(:))"), 1.0, 1e-6);              // perfect match -> 1
    // fftconv2 agrees with the direct conv2.
    eval("K = [1 1; 1 1]; fc = fftconv2(K, A); cc = conv2(K, A);");
    EXPECT_LT(evalScalar("max(abs(fc(:) - cc(:)))"), 1e-9);
    // convmtx2: (mh+m-1)(nh+n-1) x (m*n) for an mh x nh kernel and m x n image.
    eval("M = convmtx2([1 2; 3 4], 3, 3);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(M,1)")), 16);
    EXPECT_EQ(static_cast<int>(evalScalar("size(M,2)")), 9);
}

TEST_P(FilteringMorphologyTest, Morphology)
{
    eval("mg = mmgradm(A);");                       // morphological gradient
    EXPECT_EQ(static_cast<int>(evalScalar("size(mg,1)")), 3);
    EXPECT_DOUBLE_EQ(evalScalar("all(isfinite(mg(:)))"), 1.0);
    eval("g = grayconnected(A, 2, 2, 2);");         // flood fill from (2,2), tol 2
    EXPECT_DOUBLE_EQ(evalScalar("islogical(g)"), 1.0);
    EXPECT_GT(evalScalar("sum(g(:))"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("islogical(imextendedmax(A, 1))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("islogical(imextendedmin(A, 1))"), 1.0);
    // imimposemin forces the marked pixel to be the global minimum.
    eval("ii = imimposemin(A, logical([0 0 0; 0 1 0; 0 0 0]));");
    EXPECT_DOUBLE_EQ(evalScalar("ii(2,2) == min(ii(:))"), 1.0);
}

TEST_P(FilteringMorphologyTest, ChannelsOverlayThreshold)
{
    eval("[R, G, B] = imsplit(cat(3, A, A*2, A*3));");
    EXPECT_DOUBLE_EQ(evalScalar("R(2,2)"), 5.0);
    EXPECT_DOUBLE_EQ(evalScalar("G(2,2)"), 10.0);
    EXPECT_DOUBLE_EQ(evalScalar("B(2,2)"), 15.0);
    eval("ov = imoverlay(A/9, logical([1 0 0; 0 0 0; 0 0 0]), [1 0 0]);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(ov,3)")), 3);    // RGB output
    EXPECT_NEAR(evalScalar("otsuthresh([10 20 30 5 5 40 20 10])"), 0.428571, 1e-5);
}

INSTANTIATE_DUAL(FilteringMorphologyTest);
