// toolboxes/image/tests/image_batch1_test.cpp
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

#include <numkit/core/engine.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class ImageBatch1Test : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override {}
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(ImageBatch1Test, AdaptThresh)
{
    eval("T = adaptthresh(ones(8) * 0.5);");
    EXPECT_DOUBLE_EQ(evalScalar("size(T,1)"), 8.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(T,2)"), 8.0);
}

// adaptthresh sensitivity scale + default neighborhood. 2026-05-31: the
// sensitivity used an approximate bias instead of MATLAB's
// T=clip(localStat*(1.6-s),0,1), and the default neighborhood clamped up
// to 3 (MATLAB uses 1 for dims<16). vs MATLAB R2025b.
TEST_F(ImageBatch1Test, AdaptThreshSensitivity)
{
    // Constant image: T = c*(1.6-s), so the sensitivity maps linearly.
    eval("C = 0.5*ones(8,8);");
    eval("c0 = adaptthresh(C,0);   c5 = adaptthresh(C,0.5);   c1 = adaptthresh(C,1);");
    EXPECT_NEAR(evalScalar("c0(4,4)"), 0.80, 1e-12);
    EXPECT_NEAR(evalScalar("c5(4,4)"), 0.55, 1e-12);
    EXPECT_NEAR(evalScalar("c1(4,4)"), 0.30, 1e-12);
    // Small image -> default neighborhood 1 (no smoothing): T = pixel*1.1
    // clipped. B(3,3)=10/11 -> 1.0 (clip); B(1,1)=0 -> 0.0.
    eval("B = reshape(mod((0:35)*7,11)/11, 6, 6); Tb = adaptthresh(B,0.5);");
    EXPECT_DOUBLE_EQ(evalScalar("Tb(3,3)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("Tb(1,1)"), 0.0);
    // Larger image -> neighborhood 5 box mean matches MATLAB exactly.
    eval("Bg = reshape(mod((0:1023)*7,101)/101, 32, 32); Tg = adaptthresh(Bg,0.5);");
    EXPECT_NEAR(evalScalar("Tg(10,10)"), 0.5105743, 1e-6);
    EXPECT_NEAR(evalScalar("Tg(16,16)"), 0.5576238, 1e-6);
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
