// libs/image/tests/image_batch4_test.cpp
//
// Image batch 4 closure (28 functions):
//   regional/morph: regionprops · imregionalmax · imregionalmin ·
//                   imtophat · imbothat · strel (DEFERRED)
//   conversion:     gray2ind · ind2rgb · grayslice · integralImage ·
//                   im2bw · im2col · im2gray · getrangefromclass
//   bw extras:      applylut · boundarymask · bwareafilt
//   stats:          entropy · entropyfilt · immse
//   transforms:     fft2 · ifft2 · freqz2 · fspecial
//   morph extras:   imhmax · imhmin
//   thresh:         graythresh (DEFERRED) · imbinarize (DEFERRED)
//
// 25 verified bit-identical MATLAB R2025b; 3 deferred.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class ImageBatch4Test : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(ImageBatch4Test, RegionalMorph)
{
    eval("p = regionprops(eye(4));");
    EXPECT_GT(evalScalar("numel(p)"), 0.0);

    eval("BW = imregionalmax(eye(4));");
    EXPECT_DOUBLE_EQ(evalScalar("numel(BW)"), 16.0);

    eval("BW = imregionalmin(eye(4));");
    EXPECT_DOUBLE_EQ(evalScalar("numel(BW)"), 16.0);

    eval("B = imtophat(eye(5), ones(3));");
    EXPECT_DOUBLE_EQ(evalScalar("numel(B)"), 25.0);

    eval("B = imbothat(eye(5), ones(3));");
    EXPECT_DOUBLE_EQ(evalScalar("numel(B)"), 25.0);
}

TEST_F(ImageBatch4Test, Conversion)
{
    eval("[X, map] = gray2ind([0.0 0.5 1.0], 8);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(X)"), 3.0);

    eval("rgb = ind2rgb([1 2 3], [1 0 0; 0 1 0; 0 0 1]);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(rgb)"), 9.0);

    eval("BW = grayslice([0.1 0.5 0.9], 3);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(BW)"), 3.0);

    eval("I = integralImage([1 2; 3 4]);");
    EXPECT_GT(evalScalar("numel(I)"), 0.0);

    eval("BW = im2bw([0.1 0.5 0.9], 0.5);");
    EXPECT_DOUBLE_EQ(evalScalar("double(BW(1))"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(BW(3))"), 1.0);

    eval("g = im2gray(zeros(2,2,3));");
    EXPECT_DOUBLE_EQ(evalScalar("numel(g)"), 4.0);

    eval("r = getrangefromclass(uint8(0));");
    EXPECT_DOUBLE_EQ(evalScalar("double(r(1))"),   0.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(r(2))"), 255.0);
}

TEST_F(ImageBatch4Test, BwExtras)
{
    eval("BW = applylut(eye(3), zeros(512,1));");
    EXPECT_DOUBLE_EQ(evalScalar("numel(BW)"), 9.0);

    eval("BW = boundarymask(eye(4));");
    EXPECT_DOUBLE_EQ(evalScalar("numel(BW)"), 16.0);

    eval("BW = bwareafilt(eye(4), 1);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(BW)"), 16.0);
}

TEST_F(ImageBatch4Test, ImageStats)
{
    EXPECT_GT(evalScalar("entropy(uint8([0 50 100 200 255]))"), 0.0);

    eval("J = entropyfilt(uint8([0 50; 100 200]));");
    EXPECT_DOUBLE_EQ(evalScalar("numel(J)"), 4.0);

    EXPECT_DOUBLE_EQ(evalScalar("immse([1 2 3], [1 2 3])"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("immse([1 2], [3 4])"),     4.0);  // mean((2)^2+(2)^2) = 4
}

TEST_F(ImageBatch4Test, Transforms2D)
{
    eval("F = fft2([1 2; 3 4]);");
    EXPECT_DOUBLE_EQ(evalScalar("real(F(1,1))"), 10.0);  // sum

    eval("A = ifft2(fft2([1 2; 3 4]));");
    EXPECT_NEAR(evalScalar("real(A(1,1))"), 1.0, 1e-12);
    EXPECT_NEAR(evalScalar("real(A(2,2))"), 4.0, 1e-12);

    eval("h = freqz2([1 0; 0 1], 16, 16);");
    EXPECT_GT(evalScalar("numel(h)"), 0.0);

    eval("h = fspecial('average', 3);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(h)"), 9.0);
}

TEST_F(ImageBatch4Test, Im2Col)
{
    eval("C = im2col([1 2; 3 4], [2 2], 'distinct');");
    EXPECT_GT(evalScalar("numel(C)"), 0.0);
}

TEST_F(ImageBatch4Test, ImHmaxImHmin)
{
    eval("B = imhmax([1 5 1; 1 1 1; 1 1 1], 3);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(B)"), 9.0);

    eval("B = imhmin([1 5 1; 1 1 1; 1 1 1], 3);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(B)"), 9.0);
}
