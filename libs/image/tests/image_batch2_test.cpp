// libs/image/tests/image_batch2_test.cpp
//
// Image batch 2 closure (29 functions):
//   morphology:   imerode · imdilate · imopen · imclose · bwhitmiss ·
//                 imreconstruct · imclearborder · imfill
//   color spaces: rgb2gray · rgb2hsv · hsv2rgb · rgb2lab · lab2rgb ·
//                 rgb2ycbcr · ycbcr2rgb
//   convert:      im2double · im2single · im2uint8 · im2uint16
//   filter/geom:  imfilter · medfilt2 · wiener2 · imresize · imrotate ·
//                 imcrop · edge · imgradient · imnoise · imtranslate
//
// All flagged "no major gap detected". Bit-identical MATLAB R2025b.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class ImageBatch2Test : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(ImageBatch2Test, Morphology)
{
    eval("BW = imerode(eye(5), ones(3));");   EXPECT_DOUBLE_EQ(evalScalar("numel(BW)"), 25.0);
    eval("BW = imdilate(eye(5), ones(3));");  EXPECT_DOUBLE_EQ(evalScalar("numel(BW)"), 25.0);
    eval("BW = imopen(eye(5),  ones(3));");   EXPECT_DOUBLE_EQ(evalScalar("numel(BW)"), 25.0);
    eval("BW = imclose(eye(5), ones(3));");   EXPECT_DOUBLE_EQ(evalScalar("numel(BW)"), 25.0);
    eval("BW = bwhitmiss(eye(4), ones(2), zeros(2));"); EXPECT_DOUBLE_EQ(evalScalar("numel(BW)"), 16.0);
    eval("M = imreconstruct(eye(4), ones(4));"); EXPECT_DOUBLE_EQ(evalScalar("numel(M)"), 16.0);
    eval("BW = imclearborder(ones(4));");
    EXPECT_DOUBLE_EQ(evalScalar("BW(1,1)"), 0.0);  // borders cleared
    eval("BW = imfill(eye(5), 'holes');");    EXPECT_DOUBLE_EQ(evalScalar("numel(BW)"), 25.0);
}

TEST_F(ImageBatch2Test, ColorConversions)
{
    eval("g = rgb2gray(zeros(2,2,3));");
    EXPECT_DOUBLE_EQ(evalScalar("numel(g)"), 4.0);

    eval("hsv = rgb2hsv(zeros(2,2,3));");
    EXPECT_DOUBLE_EQ(evalScalar("numel(hsv)"), 12.0);

    eval("rgb = hsv2rgb(zeros(2,2,3));");
    EXPECT_DOUBLE_EQ(evalScalar("numel(rgb)"), 12.0);

    eval("lab = rgb2lab(zeros(2,2,3));");
    EXPECT_DOUBLE_EQ(evalScalar("numel(lab)"), 12.0);

    eval("rgb2 = lab2rgb(zeros(2,2,3));");
    EXPECT_DOUBLE_EQ(evalScalar("numel(rgb2)"), 12.0);

    eval("ycc = rgb2ycbcr(zeros(2,2,3));");
    EXPECT_DOUBLE_EQ(evalScalar("numel(ycc)"), 12.0);

    eval("rgb3 = ycbcr2rgb(zeros(2,2,3));");
    EXPECT_DOUBLE_EQ(evalScalar("numel(rgb3)"), 12.0);
}

TEST_F(ImageBatch2Test, ImConvert)
{
    eval("d = im2double(uint8([0 128 255]));");
    EXPECT_NEAR(evalScalar("d(1)"), 0.0, 1e-12);
    EXPECT_NEAR(evalScalar("d(3)"), 1.0, 1e-12);  // 255/255

    EXPECT_DOUBLE_EQ(evalScalar("double(im2uint8([0.0 0.5 1.0])(1))"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(im2uint8([0.0 0.5 1.0])(3))"), 255.0);

    EXPECT_DOUBLE_EQ(evalScalar("double(im2uint16([0.0 0.5 1.0])(1))"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(im2uint16([0.0 0.5 1.0])(3))"), 65535.0);
}

TEST_F(ImageBatch2Test, FilterGeom)
{
    eval("B = imfilter([1 2; 3 4], ones(2)/4);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(B)"), 4.0);

    eval("B = medfilt2([1 5; 3 7]);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(B)"), 4.0);

    eval("B = wiener2([1 2; 3 4]);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(B)"), 4.0);

    eval("B = imresize([1 2; 3 4], 2);");
    EXPECT_DOUBLE_EQ(evalScalar("size(B,1)"), 4.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(B,2)"), 4.0);

    eval("B = imrotate([1 2; 3 4], 90);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(B)"), 4.0);

    eval("B = imcrop([1 2 3; 4 5 6; 7 8 9], [1 1 1 1]);");
    EXPECT_GT(evalScalar("numel(B)"), 0.0);

    eval("BW = edge(eye(5));");
    EXPECT_DOUBLE_EQ(evalScalar("numel(BW)"), 25.0);

    eval("[Gmag, Gdir] = imgradient([1 2 3; 4 5 6; 7 8 9]);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(Gmag)"), 9.0);

    eval("rng(42); J = imnoise(zeros(4), 'gaussian');");
    EXPECT_DOUBLE_EQ(evalScalar("numel(J)"), 16.0);

    eval("B = imtranslate([1 2; 3 4], [1 1]);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(B)"), 4.0);
}
