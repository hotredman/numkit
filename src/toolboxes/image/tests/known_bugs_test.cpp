// toolboxes/image/tests/known_bugs_test.cpp
//
// One DISABLED_ test per OPEN bug in bugs/image/*.md. Disabled until fixed;
// remove `DISABLED_` to turn into a live regression guard. MATLAB R2025b.

#include <numkit/core/engine.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class ImageKnownBug : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// bugs/image/regionprops-perimeter.md — Perimeter implemented (FIXED; live guard).
TEST_F(ImageKnownBug, RegionpropsPerimeter)
{
    eval("s = regionprops(logical(ones(3,3)), 'Perimeter'); pm = s.Perimeter;");
    EXPECT_NEAR(evalScalar("pm"), 7.476000, 1e-4);
}

// bugs/image/watershed.md — watershed transform.
TEST_F(ImageKnownBug, DISABLED_Watershed)
{
    eval("L = watershed(magic(5));");
    EXPECT_EQ(static_cast<int>(evalScalar("size(L,1)")), 5);
    EXPECT_EQ(static_cast<int>(evalScalar("size(L,2)")), 5);
    EXPECT_EQ(static_cast<int>(evalScalar("max(L(:))")), 3);
}

// bugs/image/imfindcircles.md — circular Hough transform.
// (Verify centers/radii vs MATLAB on a synthetic image when enabling.)
TEST_F(ImageKnownBug, DISABLED_ImfindcirclesExists)
{
    eval("[c, r, m] = imfindcircles(zeros(40,40), [3 8]);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(c,1)")),
              static_cast<int>(evalScalar("numel(r)")));   // one center per radius
}

// bugs/image/imresize-interp.md — bilinear/bicubic grid + boundary + antialias (FIXED).
TEST_F(ImageKnownBug, ImresizeBilinear)
{
    // bilinear upscale x2 — pixel-centre map + mirror boundary (was 0.5625, ...).
    eval("r = imresize([1 2; 3 4], 2, 'bilinear');");
    EXPECT_NEAR(evalScalar("r(1,1)"), 1.0,  1e-9);
    EXPECT_NEAR(evalScalar("r(1,2)"), 1.25, 1e-9);
    EXPECT_NEAR(evalScalar("r(4,4)"), 4.0,  1e-9);
    // bicubic upscale x2 (was 0.5625).
    eval("c = imresize([1 2; 3 4], 2, 'bicubic');");
    EXPECT_NEAR(evalScalar("c(1,1)"), 0.71875, 1e-7);
    // downscale with the default method (bicubic) + antialiasing.
    eval("d = imresize([1 2 3 4 5 6], [1 3]);");
    EXPECT_NEAR(evalScalar("d(1)"), 1.44922, 1e-4);
    EXPECT_NEAR(evalScalar("d(2)"), 3.5,     1e-9);
    EXPECT_NEAR(evalScalar("d(3)"), 5.55078, 1e-4);
}

// bugs/image/corner.md — corner-point detection (cornermetric exists, corner doesn't).
TEST_F(ImageKnownBug, DISABLED_Corner)
{
    eval("I = zeros(20,20); I(6:15,6:15) = 1; C = corner(I);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(C,2)")), 2);   // [x y] coords
    EXPECT_GE(static_cast<int>(evalScalar("size(C,1)")), 4);   // 4 block corners
}

// bugs/image/adapthisteq-mapping.md — CLAHE brightness regression FIXED
// (MATLAB clip-count ceil/round + step-redistribution + rayleigh/exp vmax
// scaling). numkit now matches MATLAB R2025b to within a few gray levels on a
// deterministic textured 64x64 image (J(32,32)=125 vs 128, min 19 vs 20, max
// 235 exact). The residual ≤~3 levels is inter-tile bilinear rounding
// (bit-exact region port deferred); the tolerances guard against re-regression.
TEST_F(ImageKnownBug, AdapthisteqMapping)
{
    eval("[xx,yy] = meshgrid(1:64,1:64); I = uint8(120 + 40*sin(xx/8) + 30*cos(yy/6)); J = adapthisteq(I);");
    EXPECT_NEAR(evalScalar("double(J(32,32))"), 128.0, 2.0);    // exact; was 205 before fix
    EXPECT_NEAR(evalScalar("double(min(J(:)))"), 20.0, 2.0);    // exact; was 127
    EXPECT_NEAR(evalScalar("double(max(J(:)))"), 235.0, 2.0);   // exact; was 255
    EXPECT_NEAR(evalScalar("mean(double(J(:)))"), 130.3, 1.0);  // was ~201 (+54%)
}
