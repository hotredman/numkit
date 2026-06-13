// toolboxes/image/tests/cast_util_metrics_test.cpp
//
// Coverage for parity-only image casts, layout utilities and quality metrics:
//   mat2gray im2single im2int16 imcast intlut          (scaling / type casts)
//   label2idx padarray col2im bwunpack axes2pix
//   fchcode iptnum2ordinal phantom                      (layout / misc utils)
//   corr2 psnr ssim                                     (quality metrics)
// Anchored on exact transforms (mat2gray normalises to [0,1]; im2single of
// uint8 128 = 128/255; identical-image metrics: corr2/ssim = 1, psnr = Inf).

#include "dual_engine_fixture.hpp"

using namespace m_test;

class CastUtilMetricsTest : public DualEngineTest
{};

TEST_P(CastUtilMetricsTest, ScalingCasts)
{
    eval("g = mat2gray([0 5 10]);");
    EXPECT_DOUBLE_EQ(evalScalar("g(1)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("g(2)"), 0.5);
    EXPECT_DOUBLE_EQ(evalScalar("g(3)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("isa(im2single(uint8(128)), 'single')"), 1.0);
    EXPECT_NEAR(evalScalar("im2single(uint8(128))"), 128.0 / 255.0, 1e-6);
    EXPECT_DOUBLE_EQ(evalScalar("isa(im2int16(uint8(255)), 'int16')"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("imcast(uint8(255), 'double')"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("isa(imcast(uint8(255), 'double'), 'double')"), 1.0);
    eval("lut = intlut(uint8([1 2 3]), uint8(0:255));");   // identity LUT
    EXPECT_DOUBLE_EQ(evalScalar("double(lut(2))"), 2.0);
}

TEST_P(CastUtilMetricsTest, LayoutUtilities)
{
    // label2idx: linear indices of each label.
    eval("idx = label2idx([1 1 2; 0 2 2]);");
    EXPECT_DOUBLE_EQ(evalScalar("iscell(idx)"), 1.0);
    EXPECT_EQ(static_cast<int>(evalScalar("numel(idx)")), 2);
    EXPECT_EQ(static_cast<int>(evalScalar("numel(idx{2})")), 3);   // three '2' pixels
    // padarray: symmetric zero pad by 1 on each side.
    eval("p = padarray([1 2; 3 4], [1 1]);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(p,1)")), 4);
    EXPECT_DOUBLE_EQ(evalScalar("p(1,1)"), 0.0);     // padded corner
    EXPECT_DOUBLE_EQ(evalScalar("p(2,2)"), 1.0);     // original (1,1)
    // col2im inverts im2col (distinct blocks).
    eval("A = reshape(1:16, 4, 4); B = im2col(A, [2 2], 'distinct');");
    eval("C = col2im(B, [2 2], [4 4], 'distinct');");
    EXPECT_LT(evalScalar("max(abs(C(:) - A(:)))"), 1e-12);
    // bwunpack inverts bwpack.
    eval("M = logical([1 0 1; 0 1 0; 1 1 0]);");
    EXPECT_DOUBLE_EQ(evalScalar("isequal(bwunpack(bwpack(M), 3), M)"), 1.0);
    // axes2pix: data coord -> pixel coord.
    EXPECT_NEAR(evalScalar("axes2pix(5, [1 5], 3)"), 3.0, 1e-9);
}

TEST_P(CastUtilMetricsTest, MiscUtils)
{
    EXPECT_DOUBLE_EQ(evalScalar("isstruct(fchcode([0 0; 0 1; 1 1]))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("strcmp(iptnum2ordinal(3), 'third')"), 1.0);
    eval("ph = phantom(16);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(ph,1)")), 16);
    EXPECT_EQ(static_cast<int>(evalScalar("size(ph,2)")), 16);
    EXPECT_NEAR(evalScalar("max(ph(:))"), 1.0, 1e-9);
}

TEST_P(CastUtilMetricsTest, QualityMetrics)
{
    EXPECT_NEAR(evalScalar("corr2([1 2; 3 4], [1 2; 3 4])"), 1.0, 1e-12);     // identical
    EXPECT_NEAR(evalScalar("corr2([1 2; 3 4], [4 3; 2 1])"), -1.0, 1e-12);    // anti-correlated
    EXPECT_TRUE(eval("isinf(psnr([1 2; 3 4]/4, [1 2; 3 4]/4))").toBool());    // identical -> Inf
    EXPECT_NEAR(evalScalar("ssim([1 2 3 4 5 6 7 8]/8, [1 2 3 4 5 6 7 8]/8)"), 1.0, 1e-9);
}

INSTANTIATE_DUAL(CastUtilMetricsTest);
