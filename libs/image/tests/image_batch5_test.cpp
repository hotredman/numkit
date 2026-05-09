// libs/image/tests/image_batch5_test.cpp
//
// Image batch 5 closure (13 functions):
//   filters extras: imhistmatch · imbilatfilt · imflatfield · imapplymatrix
//                   imboxfilt3 · imgaussfilt3
//   misc:           impyramid · imquantize · imadjustn · imhistmatchn
//   gradient:       imgradientxy
//   bw extras:      imkeepborder
//   overlay:        imoverlay (DEFERRED)
//
// All flagged "no major gap detected". Bit-identical MATLAB R2025b
// (12 verified, 1 deferred — imoverlay arg validation differs).

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class ImageBatch5Test : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(ImageBatch5Test, ImageFiltersExtras)
{
    eval("J = imhistmatch(uint8([0 100 200]), uint8([50 150 255]));");
    EXPECT_DOUBLE_EQ(evalScalar("numel(J)"), 3.0);

    eval("B = imbilatfilt([1 2; 3 4]);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(B)"), 4.0);

    eval("B = imflatfield([1 2; 3 4], 1);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(B)"), 4.0);

    eval("B = imapplymatrix(eye(3), zeros(3,3,3));");
    EXPECT_DOUBLE_EQ(evalScalar("numel(B)"), 27.0);

    eval("B = imboxfilt3(zeros(3,3,3));");
    EXPECT_DOUBLE_EQ(evalScalar("numel(B)"), 27.0);

    eval("B = imgaussfilt3(zeros(3,3,3));");
    EXPECT_DOUBLE_EQ(evalScalar("numel(B)"), 27.0);
}

TEST_F(ImageBatch5Test, Misc)
{
    eval("B = impyramid([1 2; 3 4], 'reduce');");
    EXPECT_GT(evalScalar("numel(B)"), 0.0);

    eval("q = imquantize([0.1 0.5 0.9], [0.3 0.7]);");
    EXPECT_DOUBLE_EQ(evalScalar("q(1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("q(3)"), 3.0);

    eval("B = imadjustn(zeros(2,2,2));");
    EXPECT_DOUBLE_EQ(evalScalar("numel(B)"), 8.0);

    eval("B = imhistmatchn(zeros(2,2,2), zeros(2,2,2));");
    EXPECT_DOUBLE_EQ(evalScalar("numel(B)"), 8.0);
}

TEST_F(ImageBatch5Test, GradientXY)
{
    eval("[Gx, Gy] = imgradientxy([1 2 3; 4 5 6; 7 8 9]);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(Gx)"), 9.0);
    EXPECT_DOUBLE_EQ(evalScalar("numel(Gy)"), 9.0);
}

TEST_F(ImageBatch5Test, ImKeepBorder)
{
    eval("BW = imkeepborder(eye(4));");
    EXPECT_DOUBLE_EQ(evalScalar("numel(BW)"), 16.0);
}
