// libs/builtin/tests/setops_test.cpp
// Phase 8: unique / ismember / union / intersect / setdiff / histcounts / discretize

#include "dual_engine_fixture.hpp"
#include <cmath>

using namespace m_test;

class SetOpsTest : public DualEngineTest
{};

// ── unique ──────────────────────────────────────────────────

TEST_P(SetOpsTest, UniqueBasic)
{
    eval("u = unique([3 1 2 1 3 2]);");
    auto *u = getVarPtr("u");
    EXPECT_EQ(rows(*u), 1u);
    EXPECT_EQ(cols(*u), 3u);
    EXPECT_DOUBLE_EQ(u->doubleData()[0], 1.0);
    EXPECT_DOUBLE_EQ(u->doubleData()[1], 2.0);
    EXPECT_DOUBLE_EQ(u->doubleData()[2], 3.0);
}

TEST_P(SetOpsTest, UniqueAlreadySorted)
{
    eval("u = unique([1 2 3 4 5]);");
    auto *u = getVarPtr("u");
    EXPECT_EQ(u->numel(), 5u);
}

TEST_P(SetOpsTest, UniqueAllSame)
{
    eval("u = unique([5 5 5 5]);");
    auto *u = getVarPtr("u");
    EXPECT_EQ(u->numel(), 1u);
    EXPECT_DOUBLE_EQ(u->doubleData()[0], 5.0);
}

TEST_P(SetOpsTest, UniqueEmpty)
{
    eval("u = unique([]);");
    auto *u = getVarPtr("u");
    EXPECT_EQ(u->numel(), 0u);
}

TEST_P(SetOpsTest, UniqueWithIndices)
{
    eval("function [a, b, c] = wrap(x)\n"
         "  [a, b, c] = unique(x);\n"
         "end");
    eval("[U, ia, ic] = wrap([3 1 2 1 3 2]);");
    auto *U  = getVarPtr("U");
    auto *ia = getVarPtr("ia");
    auto *ic = getVarPtr("ic");
    EXPECT_EQ(U->numel(), 3u);
    EXPECT_DOUBLE_EQ(U->doubleData()[0], 1.0);
    EXPECT_DOUBLE_EQ(U->doubleData()[1], 2.0);
    EXPECT_DOUBLE_EQ(U->doubleData()[2], 3.0);
    // ia: index of first occurrence of each unique val (1-based)
    // For [3 1 2 1 3 2]: 1 first at idx 2, 2 first at idx 3, 3 first at idx 1
    EXPECT_DOUBLE_EQ(ia->doubleData()[0], 2.0);
    EXPECT_DOUBLE_EQ(ia->doubleData()[1], 3.0);
    EXPECT_DOUBLE_EQ(ia->doubleData()[2], 1.0);
    // ic: position of each X(i) in U (1-based)
    // X = [3 1 2 1 3 2] → ic = [3 1 2 1 3 2]
    EXPECT_DOUBLE_EQ(ic->doubleData()[0], 3.0);
    EXPECT_DOUBLE_EQ(ic->doubleData()[1], 1.0);
    EXPECT_DOUBLE_EQ(ic->doubleData()[2], 2.0);
    EXPECT_DOUBLE_EQ(ic->doubleData()[5], 2.0);
}

TEST_P(SetOpsTest, UniqueMatrixFlattens)
{
    // unique flattens column-major
    eval("u = unique([3 1; 2 3]);");
    auto *u = getVarPtr("u");
    EXPECT_EQ(u->numel(), 3u);
    EXPECT_DOUBLE_EQ(u->doubleData()[0], 1.0);
    EXPECT_DOUBLE_EQ(u->doubleData()[1], 2.0);
    EXPECT_DOUBLE_EQ(u->doubleData()[2], 3.0);
}

// ── ismember ────────────────────────────────────────────────

TEST_P(SetOpsTest, IsmemberBasic)
{
    eval("v = ismember([1 2 3 4 5], [2 4 6]);");
    auto *v = getVarPtr("v");
    EXPECT_EQ(v->numel(), 5u);
    EXPECT_FALSE(v->logicalData()[0] != 0);  // 1 not in B
    EXPECT_TRUE (v->logicalData()[1] != 0);  // 2 in B
    EXPECT_FALSE(v->logicalData()[2] != 0);  // 3 not in B
    EXPECT_TRUE (v->logicalData()[3] != 0);  // 4 in B
    EXPECT_FALSE(v->logicalData()[4] != 0);  // 5 not in B
}

TEST_P(SetOpsTest, IsmemberPreservesShape)
{
    eval("v = ismember([1 2; 3 4], [2 3]);");
    auto *v = getVarPtr("v");
    EXPECT_EQ(rows(*v), 2u);
    EXPECT_EQ(cols(*v), 2u);
    // 1: false, 3: true (col-major), 2: true, 4: false
    EXPECT_FALSE(v->logicalData()[0] != 0);
    EXPECT_TRUE (v->logicalData()[1] != 0);
    EXPECT_TRUE (v->logicalData()[2] != 0);
    EXPECT_FALSE(v->logicalData()[3] != 0);
}

TEST_P(SetOpsTest, IsmemberNanNeverMatches)
{
    eval("v = ismember([NaN 1 NaN], [NaN 1]);");
    auto *v = getVarPtr("v");
    EXPECT_FALSE(v->logicalData()[0] != 0);  // NaN never matches
    EXPECT_TRUE (v->logicalData()[1] != 0);  // 1 matches
    EXPECT_FALSE(v->logicalData()[2] != 0);
}

// ── union / intersect / setdiff ─────────────────────────────

TEST_P(SetOpsTest, UnionBasic)
{
    eval("u = union([1 3 5], [2 3 4]);");
    auto *u = getVarPtr("u");
    EXPECT_EQ(u->numel(), 5u);
    EXPECT_DOUBLE_EQ(u->doubleData()[0], 1.0);
    EXPECT_DOUBLE_EQ(u->doubleData()[1], 2.0);
    EXPECT_DOUBLE_EQ(u->doubleData()[2], 3.0);
    EXPECT_DOUBLE_EQ(u->doubleData()[3], 4.0);
    EXPECT_DOUBLE_EQ(u->doubleData()[4], 5.0);
}

TEST_P(SetOpsTest, UnionRemovesDuplicates)
{
    eval("u = union([1 1 2 2 3], [3 3 4]);");
    auto *u = getVarPtr("u");
    EXPECT_EQ(u->numel(), 4u);
    EXPECT_DOUBLE_EQ(u->doubleData()[0], 1.0);
    EXPECT_DOUBLE_EQ(u->doubleData()[3], 4.0);
}

TEST_P(SetOpsTest, IntersectBasic)
{
    eval("u = intersect([1 2 3 4 5], [3 4 5 6 7]);");
    auto *u = getVarPtr("u");
    EXPECT_EQ(u->numel(), 3u);
    EXPECT_DOUBLE_EQ(u->doubleData()[0], 3.0);
    EXPECT_DOUBLE_EQ(u->doubleData()[1], 4.0);
    EXPECT_DOUBLE_EQ(u->doubleData()[2], 5.0);
}

TEST_P(SetOpsTest, IntersectDisjoint)
{
    eval("u = intersect([1 2 3], [4 5 6]);");
    auto *u = getVarPtr("u");
    EXPECT_EQ(u->numel(), 0u);
}

TEST_P(SetOpsTest, SetdiffBasic)
{
    eval("u = setdiff([1 2 3 4 5], [2 4]);");
    auto *u = getVarPtr("u");
    EXPECT_EQ(u->numel(), 3u);
    EXPECT_DOUBLE_EQ(u->doubleData()[0], 1.0);
    EXPECT_DOUBLE_EQ(u->doubleData()[1], 3.0);
    EXPECT_DOUBLE_EQ(u->doubleData()[2], 5.0);
}

TEST_P(SetOpsTest, SetdiffEmpty)
{
    eval("u = setdiff([1 2 3], [1 2 3]);");
    auto *u = getVarPtr("u");
    EXPECT_EQ(u->numel(), 0u);
}

// ── histcounts ──────────────────────────────────────────────

TEST_P(SetOpsTest, HistcountsBasic)
{
    // edges [0 2 4 6] → bins [0,2), [2,4), [4,6]
    eval("h = histcounts([1 2 3 4 5], [0 2 4 6]);");
    auto *h = getVarPtr("h");
    EXPECT_EQ(h->numel(), 3u);
    EXPECT_DOUBLE_EQ(h->doubleData()[0], 1.0);  // {1}
    EXPECT_DOUBLE_EQ(h->doubleData()[1], 2.0);  // {2, 3}
    EXPECT_DOUBLE_EQ(h->doubleData()[2], 2.0);  // {4, 5}
}

TEST_P(SetOpsTest, HistcountsLastBinClosed)
{
    // edge value v == last edge falls into the last bin
    eval("h = histcounts([0 5 10], [0 5 10]);");
    auto *h = getVarPtr("h");
    EXPECT_EQ(h->numel(), 2u);
    EXPECT_DOUBLE_EQ(h->doubleData()[0], 1.0);  // 0 in [0,5)
    EXPECT_DOUBLE_EQ(h->doubleData()[1], 2.0);  // 5,10 in [5,10]
}

TEST_P(SetOpsTest, HistcountsOutOfRangeIgnored)
{
    eval("h = histcounts([-1 0 5 10 11], [0 5 10]);");
    auto *h = getVarPtr("h");
    EXPECT_DOUBLE_EQ(h->doubleData()[0], 1.0);  // {0} (5 goes to last bin)
    EXPECT_DOUBLE_EQ(h->doubleData()[1], 2.0);  // {5, 10}
}

TEST_P(SetOpsTest, HistcountsBadEdgesThrows)
{
    EXPECT_THROW(eval("histcounts([1 2 3], [3 2 1]);"), std::runtime_error);
    EXPECT_THROW(eval("histcounts([1 2 3], [1]);"), std::runtime_error);
}

// ── discretize ──────────────────────────────────────────────

TEST_P(SetOpsTest, DiscretizeBasic)
{
    eval("b = discretize([1 2 3 4 5], [0 2 4 6]);");
    auto *b = getVarPtr("b");
    EXPECT_EQ(b->numel(), 5u);
    // 1 → bin 1 [0,2), 2 → bin 2 [2,4), 3 → bin 2, 4 → bin 3 [4,6], 5 → bin 3
    EXPECT_DOUBLE_EQ(b->doubleData()[0], 1.0);
    EXPECT_DOUBLE_EQ(b->doubleData()[1], 2.0);
    EXPECT_DOUBLE_EQ(b->doubleData()[2], 2.0);
    EXPECT_DOUBLE_EQ(b->doubleData()[3], 3.0);
    EXPECT_DOUBLE_EQ(b->doubleData()[4], 3.0);
}

TEST_P(SetOpsTest, DiscretizeOutOfRangeIsNaN)
{
    eval("b = discretize([-1 0 7], [0 2 6]);");
    auto *b = getVarPtr("b");
    EXPECT_TRUE(std::isnan(b->doubleData()[0]));
    EXPECT_DOUBLE_EQ(b->doubleData()[1], 1.0);  // 0 → bin 1
    EXPECT_TRUE(std::isnan(b->doubleData()[2]));
}

TEST_P(SetOpsTest, DiscretizePreservesShape)
{
    eval("b = discretize([1 2; 3 4], [0 2 4 6]);");
    auto *b = getVarPtr("b");
    EXPECT_EQ(rows(*b), 2u);
    EXPECT_EQ(cols(*b), 2u);
}

INSTANTIATE_DUAL(SetOpsTest);
