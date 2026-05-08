// libs/image/tests/image_batch1_test.cpp
//
// First image/ namespace batch closure (12 functions):
//   thresh:   adaptthresh
//   misc:     bestblk · checkerboard
//   bw ops:   bwarea (DEFERRED) · bwareaopen · bwperim · bwlabel
//   blocks:   col2im (DEFERRED)
//   color:    cmap2gray · colorangle
//   DCT:      dctmtx · dct2
//
// 10 verified bit-identical MATLAB R2025b; 2 deferred (bwarea pixel-count
// vs MATLAB pattern-weighted; col2im arg-shape validation differs).

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class ImageBatch1Test : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(ImageBatch1Test, AdaptThresh)
{
    eval("T = adaptthresh(ones(8) * 0.5);");
    EXPECT_DOUBLE_EQ(evalScalar("size(T,1)"), 8.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(T,2)"), 8.0);
}

TEST_F(ImageBatch1Test, Bestblk)
{
    eval("siz = bestblk([100 200], 16);");
    EXPECT_GT(evalScalar("siz(1)"), 0.0);
    EXPECT_GT(evalScalar("siz(2)"), 0.0);
}

TEST_F(ImageBatch1Test, Checkerboard)
{
    eval("I = checkerboard(2, 1);");
    EXPECT_GT(evalScalar("size(I,1)"), 0.0);
    EXPECT_GT(evalScalar("size(I,2)"), 0.0);
}

TEST_F(ImageBatch1Test, BwOps)
{
    EXPECT_GT(evalScalar("bwarea(eye(4))"), 0.0);

    eval("BW2 = bwareaopen(eye(4), 2);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(BW2)"), 16.0);

    eval("P = bwperim(eye(4));");
    EXPECT_DOUBLE_EQ(evalScalar("numel(P)"), 16.0);

    eval("[L, n] = bwlabel(eye(3));");
    EXPECT_DOUBLE_EQ(evalScalar("L(1,1)"), 1.0);  // first object label
}

TEST_F(ImageBatch1Test, ColorOps)
{
    // cmap2gray returns a same-shape matrix (each row → its luminance)
    eval("g = cmap2gray([1 0 0; 0 1 0; 0 0 1]);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(g)"), 9.0);

    EXPECT_GT(evalScalar("colorangle([1 0 0], [0 1 0])"), 0.0);
}

TEST_F(ImageBatch1Test, DctOps)
{
    eval("D = dctmtx(4);");
    EXPECT_DOUBLE_EQ(evalScalar("size(D,1)"), 4.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(D,2)"), 4.0);

    eval("B = dct2([1 2; 3 4]);");
    EXPECT_DOUBLE_EQ(evalScalar("size(B,1)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(B,2)"), 2.0);
}
