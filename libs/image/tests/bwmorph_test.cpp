// libs/image/tests/bwmorph_test.cpp
//
// gtest coverage for bwmorph. The function ports MATLAB's
// algbwmorph.m semantics 1:1 via dumped LUTs; this test pins the
// expected sum-pixel output for every documented operation on a
// small 5x5 figure and on a deterministic 20x20 random matrix
// matching MATLAB rng(0).

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

class BwmorphTest : public ::testing::Test
{
public:
    numkit::Engine engine;
    void   SetUp() override {
        engine.eval("import compat.*;");
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
