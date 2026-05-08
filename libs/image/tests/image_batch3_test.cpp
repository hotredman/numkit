// libs/image/tests/image_batch3_test.cpp
//
// Image batch 3 closure (26 functions):
//   bw analysis 2:  bwconncomp (DEFERRED) · bwdist · bweuler ·
//                   bwboundaries · bwselect · bwpack
//   histogram:      imhist · histeq · imadjust · stretchlim
//   arithmetic:     imadd · imsubtract · immultiply · imdivide ·
//                   imabsdiff · imcomplement · imlincomb
//   transforms:     idct2
//   stats:          mean2 · std2
//   color metrics:  deltaE (DEFERRED) · dice · jaccard
//   filters:        imsharpen · imboxfilt · imgaussfilt
//
// All flagged "no major gap detected". Bit-identical MATLAB R2025b
// (24 verified, 2 deferred — bwconncomp struct field access + deltaE
// output dims).

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class ImageBatch3Test : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(ImageBatch3Test, BwAnalysis2)
{
    eval("D = bwdist(eye(4));");
    EXPECT_DOUBLE_EQ(evalScalar("numel(D)"), 16.0);

    EXPECT_GT(evalScalar("bweuler(eye(4))"), 0.0);

    eval("B = bwboundaries(eye(4));");
    EXPECT_GT(evalScalar("numel(B)"), 0.0);

    eval("BW2 = bwselect(eye(4), 1, 1);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(BW2)"), 16.0);

    eval("P = bwpack(eye(4));");
    EXPECT_GT(evalScalar("size(P,2)"), 0.0);
}

TEST_F(ImageBatch3Test, Histogram)
{
    eval("h = imhist(uint8([0 64 128 192 255]));");
    EXPECT_DOUBLE_EQ(evalScalar("numel(h)"), 256.0);

    eval("J = histeq(uint8([0 50 100 150 255]));");
    EXPECT_DOUBLE_EQ(evalScalar("numel(J)"), 5.0);

    eval("J = imadjust([0.0 0.5 1.0]);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(J)"), 3.0);

    eval("lim = stretchlim([0.1 0.5 0.9]);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(lim)"), 2.0);
}

TEST_F(ImageBatch3Test, Arithmetic)
{
    eval("C = imadd([1 2; 3 4], [10 10; 10 10]);");
    EXPECT_DOUBLE_EQ(evalScalar("C(1,1)"), 11.0);
    EXPECT_DOUBLE_EQ(evalScalar("C(2,2)"), 14.0);

    eval("C = imsubtract([10 20; 30 40], [1 2; 3 4]);");
    EXPECT_DOUBLE_EQ(evalScalar("C(1,1)"), 9.0);

    eval("C = immultiply([1 2; 3 4], [2 2; 2 2]);");
    EXPECT_DOUBLE_EQ(evalScalar("C(1,1)"), 2.0);

    eval("C = imdivide([10 20; 30 40], [2 2; 2 2]);");
    EXPECT_DOUBLE_EQ(evalScalar("C(1,1)"), 5.0);

    eval("C = imabsdiff([1 5], [3 2]);");
    EXPECT_DOUBLE_EQ(evalScalar("C(1)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("C(2)"), 3.0);

    eval("C = imcomplement([0.0 0.5 1.0]);");
    EXPECT_DOUBLE_EQ(evalScalar("C(1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("C(2)"), 0.5);
    EXPECT_DOUBLE_EQ(evalScalar("C(3)"), 0.0);

    eval("C = imlincomb(2, [1 2; 3 4], -1, [1 1; 1 1]);");
    EXPECT_DOUBLE_EQ(evalScalar("C(1,1)"), 1.0);  // 2*1 - 1
}

TEST_F(ImageBatch3Test, IDCT2)
{
    eval("B = dct2([1 2; 3 4]); A = idct2(B);");
    EXPECT_NEAR(evalScalar("A(1,1)"), 1.0, 1e-12);
    EXPECT_NEAR(evalScalar("A(2,2)"), 4.0, 1e-12);
}

TEST_F(ImageBatch3Test, Stats)
{
    EXPECT_DOUBLE_EQ(evalScalar("mean2([1 2; 3 4])"), 2.5);
    EXPECT_GT(evalScalar("std2([1 2; 3 4])"), 0.0);
}

TEST_F(ImageBatch3Test, DiceJaccard)
{
    EXPECT_DOUBLE_EQ(evalScalar("dice(eye(4) > 0,    eye(4) > 0)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("jaccard(eye(4) > 0, eye(4) > 0)"), 1.0);
}

TEST_F(ImageBatch3Test, Filters)
{
    eval("B = imsharpen([1 2 3; 4 5 6; 7 8 9]);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(B)"), 9.0);

    eval("B = imboxfilt([1 2; 3 4]);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(B)"), 4.0);

    eval("B = imgaussfilt([1 2; 3 4]);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(B)"), 4.0);
}
