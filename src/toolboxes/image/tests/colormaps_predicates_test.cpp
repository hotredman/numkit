// toolboxes/image/tests/colormaps_predicates_test.cpp
//
// Coverage for parity-only colormaps + colormap utilities + image-type
// predicates:
//   autumn bone cool copper flag hot lines pink prism spring summer winter
//   white                                   (n x 3 colormaps in [0,1])
//   brighten contrast iscolormap            (colormap utilities)
//   isbw isgray isind isrgb                 (image-type predicates)
// Endpoint colors of the analytic maps are exact; the rest are checked for the
// n x 3 / in-range colormap contract. Verified against the engine.

#include "dual_engine_fixture.hpp"

using namespace m_test;

class ColormapsPredicatesTest : public DualEngineTest
{
    // helper: a colormap call is n x 3 with all entries in [0,1].
protected:
    void expectColormap(const std::string &expr, int n)
    {
        eval("cm_ = " + expr + ";");
        EXPECT_EQ(static_cast<int>(evalScalar("size(cm_,1)")), n) << expr;
        EXPECT_EQ(static_cast<int>(evalScalar("size(cm_,2)")), 3) << expr;
        EXPECT_DOUBLE_EQ(evalScalar("all(cm_(:) >= 0 & cm_(:) <= 1)"), 1.0) << expr;
    }
};

TEST_P(ColormapsPredicatesTest, AnalyticColormapEndpoints)
{
    eval("a = autumn(4);");                 // R=1, G ramps 0->1, B=0
    EXPECT_DOUBLE_EQ(evalScalar("a(1,1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("a(1,2)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("a(4,2)"), 1.0);
    eval("c = cool(4);");                   // [0 1 1] -> [1 0 1]
    EXPECT_DOUBLE_EQ(evalScalar("c(1,1)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("c(1,2)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("c(4,1)"), 1.0);
    eval("s = spring(4);");                 // [1 0 1] -> [1 1 0]
    EXPECT_DOUBLE_EQ(evalScalar("s(1,1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("s(1,3)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("all(white(5)(:) == 1)"), 1.0);  // white = ones
}

TEST_P(ColormapsPredicatesTest, AllColormapsShapeAndRange)
{
    expectColormap("autumn(8)", 8);
    expectColormap("bone(8)", 8);
    expectColormap("cool(8)", 8);
    expectColormap("copper(8)", 8);
    expectColormap("flag(8)", 8);
    expectColormap("hot(8)", 8);
    expectColormap("lines(8)", 8);
    expectColormap("pink(8)", 8);
    expectColormap("prism(8)", 8);
    expectColormap("spring(8)", 8);
    expectColormap("summer(8)", 8);
    expectColormap("winter(8)", 8);
    expectColormap("white(8)", 8);
}

TEST_P(ColormapsPredicatesTest, BrightenContrastIscolormap)
{
    expectColormap("brighten(gray(8), 0.5)", 8);   // brightened map stays a colormap
    expectColormap("brighten(gray(8), -0.5)", 8);  // darkened too
    EXPECT_EQ(static_cast<int>(evalScalar("size(contrast(reshape(1:16,4,4)), 2)")), 3);
    EXPECT_DOUBLE_EQ(evalScalar("iscolormap([0 0 0; 1 1 1])"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("iscolormap([1 2])"), 0.0);   // not n x 3
}

TEST_P(ColormapsPredicatesTest, TypePredicates)
{
    EXPECT_DOUBLE_EQ(evalScalar("isgray(0.5 * ones(4))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("isrgb(rand(4, 4, 3))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("isind(uint8([1 2; 3 4]))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("isbw(logical([0 1; 1 0]))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("isrgb(0.5 * ones(4))"), 0.0);   // 2-D is not RGB
}

INSTANTIATE_DUAL(ColormapsPredicatesTest);
