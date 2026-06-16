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

// bugs/image/regionprops-perimeter.md — Perimeter currently silently dropped.
TEST_F(ImageKnownBug, DISABLED_RegionpropsPerimeter)
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

// bugs/image/imresize-interp.md — bilinear/bicubic grid + boundary + antialias.
TEST_F(ImageKnownBug, DISABLED_ImresizeBilinear)
{
    eval("r = imresize([1 2; 3 4], 2, 'bilinear');");
    EXPECT_NEAR(evalScalar("r(1,1)"), 1.0,  1e-4);   // numkit 0.5625
    EXPECT_NEAR(evalScalar("r(1,2)"), 1.25, 1e-4);   // numkit 0.9375
    EXPECT_NEAR(evalScalar("r(4,4)"), 4.0,  1e-4);   // numkit 2.25
}

// bugs/image/corner.md — corner-point detection (cornermetric exists, corner doesn't).
TEST_F(ImageKnownBug, DISABLED_Corner)
{
    eval("I = zeros(20,20); I(6:15,6:15) = 1; C = corner(I);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(C,2)")), 2);   // [x y] coords
    EXPECT_GE(static_cast<int>(evalScalar("size(C,1)")), 4);   // 4 block corners
}

// bugs/image/adapthisteq-mapping.md — CLAHE output ~54% too bright vs MATLAB
// (per-tile CDF not anchored at the display minimum). On a deterministic
// textured 64x64 image MATLAB R2025b gives J(32,32)=128, min(J)=20, max=235;
// numkit currently 205 / 127 / 255.
TEST_F(ImageKnownBug, DISABLED_AdapthisteqMapping)
{
    eval("[xx,yy] = meshgrid(1:64,1:64); I = uint8(120 + 40*sin(xx/8) + 30*cos(yy/6)); J = adapthisteq(I);");
    EXPECT_NEAR(evalScalar("double(J(32,32))"), 128.0, 3.0);    // numkit 205
    EXPECT_NEAR(evalScalar("double(min(J(:)))"), 20.0, 4.0);    // numkit 127
    EXPECT_NEAR(evalScalar("double(max(J(:)))"), 235.0, 4.0);   // numkit 255
}
