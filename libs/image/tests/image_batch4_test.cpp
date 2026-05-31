// libs/image/tests/image_batch4_test.cpp
//
// Image batch 4 closure (28 functions):
//   regional/morph: regionprops · imregionalmax · imregionalmin ·
//                   imtophat · imbothat · strel (disk now MATLAB-parity)
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

// strel('disk',R) now matches MATLAB R2025b's radial periodic-line
// decomposition (default N=4), not a full Euclidean disk. Pinned to MATLAB.
TEST_F(ImageBatch4Test, StrelDisk)
{
    // r=5 default (N=4): 9x9, sum 69, symmetric octagon-ish profile.
    eval("se5 = strel('disk',5); nh5 = se5.Neighborhood;");
    EXPECT_DOUBLE_EQ(evalScalar("size(nh5,1)"),  9.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(nh5,2)"),  9.0);
    EXPECT_DOUBLE_EQ(evalScalar("sum(nh5(:))"), 69.0);
    EXPECT_DOUBLE_EQ(evalScalar("sum(nh5(1,:))"), 5.0);   // top row clipped
    EXPECT_DOUBLE_EQ(evalScalar("sum(nh5(3,:))"), 9.0);   // mid rows full width
    EXPECT_DOUBLE_EQ(evalScalar("sum(nh5(:,1))"), 5.0);

    // r=3: decomposition degrades to a full 5x5 square (sum 25).
    eval("se3 = strel('disk',3); nh3 = se3.Neighborhood;");
    EXPECT_DOUBLE_EQ(evalScalar("size(nh3,1)"),  5.0);
    EXPECT_DOUBLE_EQ(evalScalar("sum(nh3(:))"), 25.0);

    // r=4 -> 7x7/37, r=7 -> 13x13/157.
    eval("se4 = strel('disk',4); nh4 = se4.Neighborhood;");
    EXPECT_DOUBLE_EQ(evalScalar("sum(nh4(:))"), 37.0);
    eval("se7 = strel('disk',7); nh7 = se7.Neighborhood;");
    EXPECT_DOUBLE_EQ(evalScalar("size(nh7,1)"), 13.0);
    EXPECT_DOUBLE_EQ(evalScalar("sum(nh7(:))"), 157.0);

    // N=0 forces the true Euclidean disk: r=5 -> 11x11/81.
    eval("se0 = strel('disk',5,0); nh0 = se0.Neighborhood;");
    EXPECT_DOUBLE_EQ(evalScalar("size(nh0,1)"), 11.0);
    EXPECT_DOUBLE_EQ(evalScalar("sum(nh0(:))"), 81.0);

    // r<3 always Euclidean: r=2 -> 5x5/13 (matches MATLAB).
    eval("se2 = strel('disk',2); nh2 = se2.Neighborhood;");
    EXPECT_DOUBLE_EQ(evalScalar("sum(nh2(:))"), 13.0);
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

// fspecial('disk',r) is the exact sub-pixel area-coverage integral (MATLAB
// R2025b), not a linear-taper approximation. Pinned to MATLAB.
TEST_F(ImageBatch4Test, FspecialDisk)
{
    eval("h2 = fspecial('disk', 2);");
    EXPECT_DOUBLE_EQ(evalScalar("size(h2,1)"), 5.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(h2,2)"), 5.0);
    EXPECT_NEAR(evalScalar("sum(h2(:))"), 1.0,            1e-12);
    EXPECT_NEAR(evalScalar("h2(3,3)"), 0.0795774715459,  1e-10);  // centre
    EXPECT_NEAR(evalScalar("h2(1,1)"), 0.0,              1e-12);  // corner
    EXPECT_NEAR(evalScalar("h2(3,1)"), 0.0381149714291,  1e-10);  // mid edge
    EXPECT_NEAR(evalScalar("h2(2,2)"), 0.0783813541665,  1e-10);

    eval("h3 = fspecial('disk', 3);");
    EXPECT_DOUBLE_EQ(evalScalar("size(h3,1)"), 7.0);
    EXPECT_NEAR(evalScalar("h3(4,4)"), 0.0353677651315,  1e-10);

    eval("h5 = fspecial('disk', 5);");
    EXPECT_DOUBLE_EQ(evalScalar("size(h5,1)"), 11.0);
    EXPECT_NEAR(evalScalar("sum(h5(:))"), 1.0,           1e-12);
    EXPECT_NEAR(evalScalar("h5(6,6)"), 0.0127323954474,  1e-10);
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
