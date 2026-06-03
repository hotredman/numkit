// libs/image/tests/known_bugs_test.cpp
//
// One DISABLED_ test per OPEN bug in bugs/image/*.md. Disabled until fixed;
// remove `DISABLED_` to turn into a live regression guard. MATLAB R2025b.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class ImageKnownBug : public ::testing::Test
{
public:
    Engine engine;
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
