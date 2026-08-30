// toolboxes/image/tests/bwmorph_test.cpp
// gtest coverage for bwmorph — binary morphology dispatcher.
// bwmorph is a clean-room implementation: the dispatcher is written
// from public references (public references — Gonzalez &
// Woods, Pratt, Lam/Lee/Suen) and consumes the 512-entry neighbourhood
// truth tables in bwmorph_luts.h. The sum-pixel values below are
// MATLAB R2025b reference output (a 5x5 figure and a deterministic
// 20x20 rng(0) matrix); the property test at the end verifies the
// defining invariants of the operations MATLAB-independently.

#include <numkit/core/engine.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

class BwmorphTest : public ::testing::Test
{
public:
    numkit::StandardEngine engine;
    void   SetUp() override {
                engine.eval("BW = logical([0 0 0 0 0;"
                                 " 0 1 1 0 0;"
                                 " 0 1 1 1 0;"
                                 " 0 1 1 1 0;"
                                 " 0 0 0 0 0]);");
    }
    double eval_scalar(const std::string &c) { return engine.eval(c).toScalar(); }
};

// Single-LUT operations on the 5x5 figure. Expected sums captured
// from MATLAB R2025b.
TEST_F(BwmorphTest, SingleLutOpsSmall)
{
    EXPECT_DOUBLE_EQ(eval_scalar("sum(sum(bwmorph(BW, 'dilate')))"),    24.0);
    EXPECT_DOUBLE_EQ(eval_scalar("sum(sum(bwmorph(BW, 'erode')))"),      0.0);
    EXPECT_DOUBLE_EQ(eval_scalar("sum(sum(bwmorph(BW, 'bridge')))"),     8.0);
    EXPECT_DOUBLE_EQ(eval_scalar("sum(sum(bwmorph(BW, 'clean')))"),      8.0);
    EXPECT_DOUBLE_EQ(eval_scalar("sum(sum(bwmorph(BW, 'diag')))"),       8.0);
    EXPECT_DOUBLE_EQ(eval_scalar("sum(sum(bwmorph(BW, 'fill')))"),       8.0);
    EXPECT_DOUBLE_EQ(eval_scalar("sum(sum(bwmorph(BW, 'hbreak')))"),     8.0);
    EXPECT_DOUBLE_EQ(eval_scalar("sum(sum(bwmorph(BW, 'majority')))"),   5.0);
    EXPECT_DOUBLE_EQ(eval_scalar("sum(sum(bwmorph(BW, 'perim4')))"),     7.0);
    EXPECT_DOUBLE_EQ(eval_scalar("sum(sum(bwmorph(BW, 'perim8')))"),     8.0);
    EXPECT_DOUBLE_EQ(eval_scalar("sum(sum(bwmorph(BW, 'remove')))"),     7.0);
    EXPECT_DOUBLE_EQ(eval_scalar("sum(sum(bwmorph(BW, 'endpoints')))"),  8.0);
    EXPECT_DOUBLE_EQ(eval_scalar("sum(sum(bwmorph(BW, 'fatten')))"),    24.0);
}

// Composite operations.
TEST_F(BwmorphTest, CompositeOpsSmall)
{
    EXPECT_DOUBLE_EQ(eval_scalar("sum(sum(bwmorph(BW, 'open')))"),       0.0);
    EXPECT_DOUBLE_EQ(eval_scalar("sum(sum(bwmorph(BW, 'close')))"),      8.0);
    EXPECT_DOUBLE_EQ(eval_scalar("sum(sum(bwmorph(BW, 'bothat')))"),     0.0);
    EXPECT_DOUBLE_EQ(eval_scalar("sum(sum(bwmorph(BW, 'tophat')))"),     8.0);
}

// Iterated operations.
TEST_F(BwmorphTest, IteratedOpsSmall)
{
    EXPECT_DOUBLE_EQ(eval_scalar("sum(sum(bwmorph(BW, 'skel', Inf)))"),   5.0);
    EXPECT_DOUBLE_EQ(eval_scalar("sum(sum(bwmorph(BW, 'thin', Inf)))"),   1.0);
    EXPECT_DOUBLE_EQ(eval_scalar("sum(sum(bwmorph(BW, 'shrink', Inf)))"), 1.0);
    EXPECT_DOUBLE_EQ(eval_scalar("sum(sum(bwmorph(BW, 'spur', 2)))"),     8.0);
    EXPECT_DOUBLE_EQ(eval_scalar("sum(sum(bwmorph(BW, 'thicken', 1)))"), 19.0);
    EXPECT_DOUBLE_EQ(eval_scalar("sum(sum(bwmorph(BW, 'branchpoints')))"),0.0);
}

// 20x20 random pattern from rng(0) — must give identical results
// to MATLAB rng(0) since the seed-driven matrix is reproducible.
TEST_F(BwmorphTest, RandomPatternMatchesMatlab)
{
    engine.eval("rng(0); BW2 = rand(20, 20) > 0.5;");
    EXPECT_DOUBLE_EQ(eval_scalar("sum(sum(bwmorph(BW2, 'dilate')))"),       400.0);
    EXPECT_DOUBLE_EQ(eval_scalar("sum(sum(bwmorph(BW2, 'skel', Inf)))"),    158.0);
    EXPECT_DOUBLE_EQ(eval_scalar("sum(sum(bwmorph(BW2, 'thin', Inf)))"),    147.0);
    EXPECT_DOUBLE_EQ(eval_scalar("sum(sum(bwmorph(BW2, 'thicken', 3)))"),   222.0);
    EXPECT_DOUBLE_EQ(eval_scalar("sum(sum(bwmorph(BW2, 'spur', 5)))"),      162.0);
    EXPECT_DOUBLE_EQ(eval_scalar("sum(sum(bwmorph(BW2, 'shrink', Inf)))"),   89.0);
    EXPECT_DOUBLE_EQ(eval_scalar("sum(sum(bwmorph(BW2, 'branchpoints')))"),  70.0);
}

// Unknown operation must throw.
TEST_F(BwmorphTest, UnknownOpThrows)
{
    EXPECT_THROW(engine.eval("bwmorph(BW, 'bogus');"), std::exception);
}

// n=0 returns the input unchanged.
TEST_F(BwmorphTest, ZeroIterationsNoOp)
{
    engine.eval("J = bwmorph(BW, 'dilate', 0);");
    EXPECT_DOUBLE_EQ(eval_scalar("sum(sum(BW - J))"), 0.0);
}

// ── MATLAB-independent correctness test ───────────────────────────────
// Verify the defining invariants of the morphological operations on a
// known shape — no reference engine involved.
TEST_F(BwmorphTest, MorphologicalInvariants)
{
    engine.eval("rng(7); A = rand(24, 24) > 0.4;");

    // Dilation is extensive, erosion is anti-extensive:
    //   erode(A) ⊆ A ⊆ dilate(A).
    engine.eval("D = bwmorph(A, 'dilate'); E = bwmorph(A, 'erode');");
    EXPECT_DOUBLE_EQ(eval_scalar("nnz(A & ~D)"), 0.0);  // A ⊆ dilate(A)
    EXPECT_DOUBLE_EQ(eval_scalar("nnz(E & ~A)"), 0.0);  // erode(A) ⊆ A
    EXPECT_GE(eval_scalar("nnz(D)"), eval_scalar("nnz(A)"));
    EXPECT_LE(eval_scalar("nnz(E)"), eval_scalar("nnz(A)"));

    // Perimeter pixels are a subset of the foreground.
    engine.eval("P4 = bwmorph(A, 'perim4'); P8 = bwmorph(A, 'perim8');");
    EXPECT_DOUBLE_EQ(eval_scalar("nnz(P4 & ~A)"), 0.0);
    EXPECT_DOUBLE_EQ(eval_scalar("nnz(P8 & ~A)"), 0.0);

    // Skeleton / thinning of a solid block: a subset of the block with
    // strictly fewer pixels (the block is reduced to a thin figure).
    engine.eval("blk = false(20, 20); blk(5:16, 5:16) = true;"
                "sk = bwmorph(blk, 'skel', Inf);"
                "th = bwmorph(blk, 'thin', Inf);");
    EXPECT_DOUBLE_EQ(eval_scalar("nnz(sk & ~blk)"), 0.0);
    EXPECT_DOUBLE_EQ(eval_scalar("nnz(th & ~blk)"), 0.0);
    EXPECT_LT(eval_scalar("nnz(sk)"), eval_scalar("nnz(blk)"));
    EXPECT_LT(eval_scalar("nnz(th)"), eval_scalar("nnz(blk)"));

    // shrink ∞ of a single solid blob leaves exactly one pixel.
    engine.eval("one = bwmorph(blk, 'shrink', Inf);");
    EXPECT_DOUBLE_EQ(eval_scalar("nnz(one)"), 1.0);

    // A solid rectangle, strictly interior to a zero border, is its own
    // opening and closing (a rectangle is already morphologically
    // open and closed). This exercises open / close end-to-end.
    engine.eval("O = bwmorph(blk, 'open'); Cl = bwmorph(blk, 'close');");
    EXPECT_DOUBLE_EQ(eval_scalar("nnz(O ~= blk)"), 0.0);   // open(blk)  == blk
    EXPECT_DOUBLE_EQ(eval_scalar("nnz(Cl ~= blk)"), 0.0);  // close(blk) == blk
}
