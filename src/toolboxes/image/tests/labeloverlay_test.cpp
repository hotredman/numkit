// toolboxes/image/tests/labeloverlay_test.cpp
//
// Regression guard for labeloverlay — colour-blend a label / mask
// matrix over a 2-D grayscale or RGB base. All reference values come
// from MATLAB R2025b labeloverlay (probes saved in tmp/lo_probe*.m).
//
// Default ColorAssignment is 'auto' which → 'shuffle' for the default
// 'jet' colormap. The shuffle uses rng('default') (MT19937 seed 0,
// state[0]=5489). The C++ side uses numkit's MatlabMT19937 +
// stable-sort-of-rand to be bit-identical with MATLAB's randperm.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class LabelOverlayTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override
    {
        engine.eval(
            "import compat.*;"
            "A = uint8([10 20 30 40 50; 60 70 80 90 100;"
            "          110 120 130 140 150; 160 170 180 190 200;"
            "          210 220 230 240 250]);"
            "L = uint8([0 0 1 1 2; 0 1 1 2 2;"
            "          1 1 0 2 3; 1 0 0 3 3; 0 0 3 3 3]);");
    }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// ── Default: grayscale → RGB, jet shuffle, Transparency=0.5 ─────

TEST_F(LabelOverlayTest, DefaultClassAndSize)
{
    eval("B = labeloverlay(A, L);");
    EXPECT_EQ(eval("class(B)").toString(), "uint8");
    EXPECT_EQ(static_cast<int>(evalScalar("size(B, 1)")), 5);
    EXPECT_EQ(static_cast<int>(evalScalar("size(B, 2)")), 5);
    EXPECT_EQ(static_cast<int>(evalScalar("size(B, 3)")), 3);
}

TEST_F(LabelOverlayTest, DefaultPixels)
{
    eval("B = labeloverlay(A, L);");
    // label 0 → passthrough
    EXPECT_EQ(static_cast<int>(evalScalar("double(B(1,1,1))")), 10);
    EXPECT_EQ(static_cast<int>(evalScalar("double(B(1,1,2))")), 10);
    EXPECT_EQ(static_cast<int>(evalScalar("double(B(1,1,3))")), 10);
    // label 1 (jet shuffle row 2 = original row 3 = [1 1 0] = [255 255 0])
    // 0.5*30 + 0.5*255 = 142.5 → 143
    EXPECT_EQ(static_cast<int>(evalScalar("double(B(1,3,1))")), 143);
    EXPECT_EQ(static_cast<int>(evalScalar("double(B(1,3,2))")), 143);
    EXPECT_EQ(static_cast<int>(evalScalar("double(B(1,3,3))")), 15);
    // label 2 (jet shuffle row 3 = original row 1 = [0 0 1] = [0 0 255])
    EXPECT_EQ(static_cast<int>(evalScalar("double(B(1,5,1))")), 25);
    EXPECT_EQ(static_cast<int>(evalScalar("double(B(1,5,2))")), 25);
    EXPECT_EQ(static_cast<int>(evalScalar("double(B(1,5,3))")), 153);
    // label 3 (jet shuffle row 4 = original row 2 = [0 1 1] = [0 255 255])
    EXPECT_EQ(static_cast<int>(evalScalar("double(B(3,5,1))")), 75);
    EXPECT_EQ(static_cast<int>(evalScalar("double(B(3,5,2))")), 203);
    EXPECT_EQ(static_cast<int>(evalScalar("double(B(3,5,3))")), 203);
}

// ── Transparency boundary values ────────────────────────────────

TEST_F(LabelOverlayTest, TransparencyZeroIsPureColour)
{
    eval("B = labeloverlay(A, L, 'Transparency', 0);");
    EXPECT_EQ(static_cast<int>(evalScalar("double(B(1,3,1))")), 255);
    EXPECT_EQ(static_cast<int>(evalScalar("double(B(1,3,2))")), 255);
    EXPECT_EQ(static_cast<int>(evalScalar("double(B(1,3,3))")), 0);
    EXPECT_EQ(static_cast<int>(evalScalar("double(B(1,5,1))")), 0);
    EXPECT_EQ(static_cast<int>(evalScalar("double(B(1,5,2))")), 0);
    EXPECT_EQ(static_cast<int>(evalScalar("double(B(1,5,3))")), 255);
}

TEST_F(LabelOverlayTest, TransparencyOneIsPassthrough)
{
    eval("B = labeloverlay(A, L, 'Transparency', 1);");
    EXPECT_EQ(static_cast<int>(evalScalar("double(B(1,3,1))")), 30);
    EXPECT_EQ(static_cast<int>(evalScalar("double(B(1,3,2))")), 30);
    EXPECT_EQ(static_cast<int>(evalScalar("double(B(1,3,3))")), 30);
}

// ── Logical mask ────────────────────────────────────────────────

TEST_F(LabelOverlayTest, LogicalMask)
{
    eval("BW = logical([0 0 1 1 1; 0 1 1 1 0;"
         "             1 1 0 0 1; 0 0 1 1 1; 1 1 1 0 0]);"
         "B = labeloverlay(A, BW);");
    // BW=1 → label 1 → default jet shuffle of jet(2). Verified
    // against MATLAB: B(1,3,:) = [15 15 143].
    EXPECT_EQ(static_cast<int>(evalScalar("double(B(1,3,1))")), 15);
    EXPECT_EQ(static_cast<int>(evalScalar("double(B(1,3,2))")), 15);
    EXPECT_EQ(static_cast<int>(evalScalar("double(B(1,3,3))")), 143);
    // BW=0 → passthrough
    EXPECT_EQ(static_cast<int>(evalScalar("double(B(1,1,1))")), 10);
}

// ── Custom Nx3 numeric Colormap ─────────────────────────────────

TEST_F(LabelOverlayTest, NumericColormapNoShuffle)
{
    // Numeric cmap → ColorAssignment 'auto' picks 'noshuffle'.
    eval("cmap = [1 0 0; 0 1 0; 0 0 1; 1 1 0];"
         "B = labeloverlay(A, L, 'Colormap', cmap, 'Transparency', 0);");
    // label 1 → cmap(1,:) = [1 0 0]
    EXPECT_EQ(static_cast<int>(evalScalar("double(B(1,3,1))")), 255);
    EXPECT_EQ(static_cast<int>(evalScalar("double(B(1,3,2))")), 0);
    EXPECT_EQ(static_cast<int>(evalScalar("double(B(1,3,3))")), 0);
    // label 2 → cmap(2,:) = [0 1 0]
    EXPECT_EQ(static_cast<int>(evalScalar("double(B(1,5,1))")), 0);
    EXPECT_EQ(static_cast<int>(evalScalar("double(B(1,5,2))")), 255);
    EXPECT_EQ(static_cast<int>(evalScalar("double(B(1,5,3))")), 0);
    // label 3 → cmap(3,:) = [0 0 1]
    EXPECT_EQ(static_cast<int>(evalScalar("double(B(3,5,1))")), 0);
    EXPECT_EQ(static_cast<int>(evalScalar("double(B(3,5,2))")), 0);
    EXPECT_EQ(static_cast<int>(evalScalar("double(B(3,5,3))")), 255);
}

// ── IncludedLabels filter ───────────────────────────────────────

TEST_F(LabelOverlayTest, IncludedLabelsSkipsExcluded)
{
    eval("B = labeloverlay(A, L, 'IncludedLabels', [1 3]);");
    // label 1 included → colour blend
    EXPECT_EQ(static_cast<int>(evalScalar("double(B(1,3,1))")), 143);
    // label 2 excluded → passthrough (A(1,5)=50)
    EXPECT_EQ(static_cast<int>(evalScalar("double(B(1,5,1))")), 50);
    EXPECT_EQ(static_cast<int>(evalScalar("double(B(1,5,2))")), 50);
    EXPECT_EQ(static_cast<int>(evalScalar("double(B(1,5,3))")), 50);
    // label 3 included
    EXPECT_EQ(static_cast<int>(evalScalar("double(B(3,5,1))")), 75);
}

TEST_F(LabelOverlayTest, IncludedLabelsContainsZero)
{
    // When 0 is in IncludedLabels, no prepend → label 0 is coloured
    // by cmap(1,:) (= jet(3) row 1 here after shuffle).
    eval("Asmall = uint8([100 100 100]); Lsmall = uint8([0 1 2]);"
         "B = labeloverlay(Asmall, Lsmall, "
         "                 'IncludedLabels', [0 1 2], 'Transparency', 0);");
    // For this input maxLabel=2 totalLabels=3. jet(3) shuffle =
    // jet(3)([3 1 2], :) = [[1 1 0]; [0 0 1]; [0 1 1]].
    EXPECT_EQ(static_cast<int>(evalScalar("double(B(1,1,1))")), 255);
    EXPECT_EQ(static_cast<int>(evalScalar("double(B(1,1,2))")), 255);
    EXPECT_EQ(static_cast<int>(evalScalar("double(B(1,1,3))")), 0);
    EXPECT_EQ(static_cast<int>(evalScalar("double(B(1,2,1))")), 0);
    EXPECT_EQ(static_cast<int>(evalScalar("double(B(1,2,2))")), 0);
    EXPECT_EQ(static_cast<int>(evalScalar("double(B(1,2,3))")), 255);
    EXPECT_EQ(static_cast<int>(evalScalar("double(B(1,3,1))")), 0);
    EXPECT_EQ(static_cast<int>(evalScalar("double(B(1,3,2))")), 255);
    EXPECT_EQ(static_cast<int>(evalScalar("double(B(1,3,3))")), 255);
}

// ── RGB input ───────────────────────────────────────────────────

TEST_F(LabelOverlayTest, RGBInputPreservesPassthrough)
{
    eval("Arg = uint8(cat(3, "
         "  [10 20 30; 40 50 60; 70 80 90],"
         "  [100 110 120; 130 140 150; 160 170 180],"
         "  [200 205 210; 215 220 225; 230 235 240]));"
         "Lrg = uint8([0 1 1; 0 1 0; 2 2 0]);"
         "B = labeloverlay(Arg, Lrg);");
    EXPECT_EQ(eval("class(B)").toString(), "uint8");
    // RGB passthrough at label 0
    EXPECT_EQ(static_cast<int>(evalScalar("double(B(1,1,1))")), 10);
    EXPECT_EQ(static_cast<int>(evalScalar("double(B(1,1,2))")), 100);
    EXPECT_EQ(static_cast<int>(evalScalar("double(B(1,1,3))")), 200);
    // label 1
    EXPECT_EQ(static_cast<int>(evalScalar("double(B(1,2,1))")), 138);
    EXPECT_EQ(static_cast<int>(evalScalar("double(B(1,2,2))")), 183);
    EXPECT_EQ(static_cast<int>(evalScalar("double(B(1,2,3))")), 103);
    // label 2
    EXPECT_EQ(static_cast<int>(evalScalar("double(B(3,1,1))")), 35);
    EXPECT_EQ(static_cast<int>(evalScalar("double(B(3,1,2))")), 80);
    EXPECT_EQ(static_cast<int>(evalScalar("double(B(3,1,3))")), 243);
}

// ── ColorAssignment options ─────────────────────────────────────

TEST_F(LabelOverlayTest, NoShuffleJet)
{
    // 'noshuffle' on default 'jet' keeps jet(N+1) order.
    eval("A0 = uint8(zeros(1,3));"
         "L0 = uint8([1 2 3]);"
         "B = labeloverlay(A0, L0, 'Transparency', 0,"
         "                'ColorAssignment', 'noshuffle');");
    // After prepend: cmap = [jet(4)(1,:); jet(4)]. Label 1 → cmap(2,:)
    // = jet(4)(1,:) = [0 0 1] = [0 0 255].
    EXPECT_EQ(static_cast<int>(evalScalar("double(B(1,1,1))")), 0);
    EXPECT_EQ(static_cast<int>(evalScalar("double(B(1,1,2))")), 0);
    EXPECT_EQ(static_cast<int>(evalScalar("double(B(1,1,3))")), 255);
    // Label 2 → cmap(3,:) = jet(4)(2,:) = [0 1 1] = [0 255 255].
    EXPECT_EQ(static_cast<int>(evalScalar("double(B(1,2,1))")), 0);
    EXPECT_EQ(static_cast<int>(evalScalar("double(B(1,2,2))")), 255);
    EXPECT_EQ(static_cast<int>(evalScalar("double(B(1,2,3))")), 255);
    // Label 3 → cmap(4,:) = jet(4)(3,:) = [1 1 0] = [255 255 0].
    EXPECT_EQ(static_cast<int>(evalScalar("double(B(1,3,1))")), 255);
    EXPECT_EQ(static_cast<int>(evalScalar("double(B(1,3,2))")), 255);
    EXPECT_EQ(static_cast<int>(evalScalar("double(B(1,3,3))")), 0);
}

// ── Errors ──────────────────────────────────────────────────────

TEST_F(LabelOverlayTest, NegativeLabelThrows)
{
    EXPECT_THROW(eval("labeloverlay(A, int8([0 1 -1 2 3;"
                      "                     0 1 1 2 2;"
                      "                     1 1 0 2 3;"
                      "                     1 0 0 3 3;"
                      "                     0 0 3 3 3]));"),
                 std::exception);
}

TEST_F(LabelOverlayTest, MismatchedSizeThrows)
{
    EXPECT_THROW(eval("labeloverlay(A, uint8([1 2; 3 4]));"), std::exception);
}

TEST_F(LabelOverlayTest, ColormapTooSmallThrows)
{
    EXPECT_THROW(eval("labeloverlay(A, L, 'Colormap', [1 0 0; 0 1 0]);"),
                 std::exception);
}

TEST_F(LabelOverlayTest, BadColorAssignmentThrows)
{
    EXPECT_THROW(eval("labeloverlay(A, L, 'ColorAssignment', 'invalid');"),
                 std::exception);
}

TEST_F(LabelOverlayTest, TransparencyOutOfRangeThrows)
{
    EXPECT_THROW(eval("labeloverlay(A, L, 'Transparency', 1.5);"),
                 std::exception);
    EXPECT_THROW(eval("labeloverlay(A, L, 'Transparency', -0.1);"),
                 std::exception);
}
