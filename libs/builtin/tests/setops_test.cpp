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

// ── Phase P3 hash-set edge cases ─────────────────────────────
//
// The default std::hash<double> distinguishes +0 and -0 by bit pattern,
// putting them in different buckets and breaking equality lookup. The
// custom DoubleHashEq0 in MStdSetOps.cpp must collapse them. These
// tests pin that semantic so a future "simplification" of the hash
// can't silently regress.

TEST_P(SetOpsTest, UniqueCollapsesPositiveAndNegativeZero)
{
    // 0 and -0 are == per IEEE 754; MATLAB and the hash set must
    // collapse them to a single output slot.
    eval("u = unique([0 -0 1 -0 0]);");
    auto *u = getVarPtr("u");
    EXPECT_EQ(u->numel(), 2u);
    EXPECT_DOUBLE_EQ(u->doubleData()[0], 0.0);
    EXPECT_DOUBLE_EQ(u->doubleData()[1], 1.0);
}

TEST_P(SetOpsTest, IsmemberMatchesAcrossPlusMinusZero)
{
    eval("v = ismember([-0 0 1], [0]);");
    auto *v = getVarPtr("v");
    // Both -0 and +0 must hash-collide with the +0 in B.
    EXPECT_TRUE (v->logicalData()[0] != 0);
    EXPECT_TRUE (v->logicalData()[1] != 0);
    EXPECT_FALSE(v->logicalData()[2] != 0);
}

TEST_P(SetOpsTest, UnionCollapsesPlusMinusZero)
{
    eval("u = union([-0 1], [0 2]);");
    auto *u = getVarPtr("u");
    EXPECT_EQ(u->numel(), 3u);
    EXPECT_DOUBLE_EQ(u->doubleData()[0], 0.0);
    EXPECT_DOUBLE_EQ(u->doubleData()[1], 1.0);
    EXPECT_DOUBLE_EQ(u->doubleData()[2], 2.0);
}

// Larger N exercises rehash + xorshift mixer in DoubleHashEq0. With
// 200 distinct integers in [0,200) repeated 5x, the hash table must
// dedupe down to exactly 200 entries.
TEST_P(SetOpsTest, UniqueLargeNHeavyDuplication)
{
    eval("x = mod(0:999, 200); u = unique(x);");
    auto *u = getVarPtr("u");
    EXPECT_EQ(u->numel(), 200u);
    // Sorted ascending: 0, 1, 2, ..., 199.
    for (size_t i = 0; i < 200; ++i)
        EXPECT_DOUBLE_EQ(u->doubleData()[i], static_cast<double>(i));
}

// uniqueWithIndices: ia (first-occurrence index per unique), ic
// (mapping back) under heavy duplication. Catches a future regression
// in the firstIdx.try_emplace path or the rankByValue construction.
TEST_P(SetOpsTest, UniqueWithIndicesLargeNRoundTrip)
{
    eval("function [a, b, c] = w(x)\n  [a, b, c] = unique(x);\nend");
    eval("x = mod(0:99, 10); [u, ia, ic] = w(x);");
    auto *u  = getVarPtr("u");
    auto *ia = getVarPtr("ia");
    auto *ic = getVarPtr("ic");
    EXPECT_EQ(u->numel(), 10u);
    // Round-trip: x(ia) must equal u, u(ic) must equal x.
    eval("rt1 = x(ia); rt2 = u(ic);");
    auto *rt1 = getVarPtr("rt1");
    auto *rt2 = getVarPtr("rt2");
    for (size_t i = 0; i < 10; ++i)
        EXPECT_DOUBLE_EQ(rt1->doubleData()[i], u->doubleData()[i]);
    for (size_t i = 0; i < 100; ++i)
        EXPECT_DOUBLE_EQ(rt2->doubleData()[i], static_cast<double>(i % 10));
}

TEST_P(SetOpsTest, IsmemberLargeNExhaustive)
{
    // A = 0..999; B = 500..1499. Membership in B is exactly i >= 500
    // for each A[i]. Catches a hash collision / equality regression at
    // a size that exercises rehash.
    eval("A = 0:999; B = 500:1499; v = ismember(A, B);");
    auto *v = getVarPtr("v");
    EXPECT_EQ(v->numel(), 1000u);
    for (size_t i = 0; i < 1000; ++i) {
        const bool expected = (i >= 500);
        EXPECT_EQ(v->logicalData()[i] != 0, expected) << "at i=" << i;
    }
}

// ── Phase P4 uniform-edge fast path ──────────────────────────
//
// Beyond the basic histcounts/discretize tests above, the new
// uniform-edge path needs explicit coverage on (a) the FP-rounding
// guard and (b) parity with the irregular-fallback path.

TEST_P(SetOpsTest, HistcountsUniformAndIrregularAgree)
{
    // Same data, two equivalent edge specs (one regular, one re-
    // expressed with a tiny perturbation that fails edgesAreUniform).
    // Bin counts must be identical -- the fast and fallback paths
    // are required to agree on integer-valued data within the bins.
    eval("x = (0:99) + 0.5;");
    eval("eA = 0:10:100;");
    // Same edges but one slot perturbed by 0 (still uniform per check)
    eval("eB = [0 10 20 30 40 50 60 70 80 90 100.0001];");
    eval("hA = histcounts(x, eA); hB = histcounts(x, eB);");
    auto *hA = getVarPtr("hA");
    auto *hB = getVarPtr("hB");
    EXPECT_EQ(hA->numel(), hB->numel());
    for (size_t i = 0; i < hA->numel(); ++i)
        EXPECT_DOUBLE_EQ(hA->doubleData()[i], hB->doubleData()[i])
            << "at bin " << i;
}

TEST_P(SetOpsTest, HistcountsLargeNUniformIntegrity)
{
    // 1000 evenly-spaced integers into 10 uniform bins of width 100
    // → exactly 100 per bin. Catches off-by-one in the FP-rounding
    // guard or the last-bin closure.
    eval("x = 0:999; e = 0:100:1000; h = histcounts(x, e);");
    auto *h = getVarPtr("h");
    EXPECT_EQ(h->numel(), 10u);
    for (size_t i = 0; i < 10; ++i)
        EXPECT_DOUBLE_EQ(h->doubleData()[i], 100.0) << "at bin " << i;
}

// ── unique 'rows' flag (post-parity round 10) ──────────────────────

TEST_P(SetOpsTest, UniqueRowsBasic)
{
    // 4 rows, 2nd row duplicates 1st → 3 unique rows lex-sorted
    // [1 2; 3 4; 1 2; 5 6] → [1 2; 3 4; 5 6]
    eval("M = [1 2; 3 4; 1 2; 5 6]; C = unique(M, 'rows');");
    EXPECT_DOUBLE_EQ(evalScalar("size(C, 1);"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(C, 2);"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("C(1, 1);"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("C(1, 2);"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("C(2, 1);"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("C(3, 1);"), 5.0);
}

TEST_P(SetOpsTest, UniqueRowsLexSort)
{
    // Lex sort: by col 1 first, then col 2, etc.
    eval("M = [2 1; 1 9; 2 0; 1 9]; C = unique(M, 'rows');");
    // distinct: (1,9), (2,0), (2,1) → lex sorted
    EXPECT_DOUBLE_EQ(evalScalar("size(C, 1);"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("C(1, 1);"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("C(1, 2);"), 9.0);
    EXPECT_DOUBLE_EQ(evalScalar("C(2, 1);"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("C(2, 2);"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("C(3, 1);"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("C(3, 2);"), 1.0);
}

TEST_P(SetOpsTest, UniqueRowsThreeOutputs)
{
    eval("M = [1 2; 3 4; 1 2; 5 6]; [C, ia, ic] = unique(M, 'rows');");
    EXPECT_DOUBLE_EQ(evalScalar("size(C, 1);"), 3.0);
    // ia: original row index per unique row
    // sorted unique = [(1,2) row 1, (3,4) row 2, (5,6) row 4]
    EXPECT_DOUBLE_EQ(evalScalar("ia(1);"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("ia(2);"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("ia(3);"), 4.0);
    // ic: each original row → rank in unique
    // M(1,:) and M(3,:) are unique #1; M(2,:) is #2; M(4,:) is #3
    EXPECT_DOUBLE_EQ(evalScalar("ic(1);"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("ic(2);"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("ic(3);"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("ic(4);"), 3.0);
}

TEST_P(SetOpsTest, UniqueRowsNanRowsKeptDistinct)
{
    // Each NaN-row stays as its own unique slot, appended at the end.
    eval("M = [1 2; NaN 0; 1 2; NaN 0]; C = unique(M, 'rows');");
    // Non-NaN unique = [(1,2)]; NaN rows: 2 of them, each distinct
    EXPECT_DOUBLE_EQ(evalScalar("size(C, 1);"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("C(1, 1);"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("C(1, 2);"), 2.0);
    EXPECT_TRUE(std::isnan(evalScalar("C(2, 1);")));
    EXPECT_DOUBLE_EQ(evalScalar("C(2, 2);"), 0.0);
    EXPECT_TRUE(std::isnan(evalScalar("C(3, 1);")));
}

TEST_P(SetOpsTest, UniqueRowsEmpty)
{
    eval("M = zeros(0, 3); C = unique(M, 'rows');");
    EXPECT_DOUBLE_EQ(evalScalar("size(C, 1);"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(C, 2);"), 3.0);
}

TEST_P(SetOpsTest, UniqueRowsNegativeZeroNormalized)
{
    // -0 and +0 must hash to the same slot (otherwise [-0 1] and [0 1]
    // would be treated as distinct rows).
    eval("M = [-0 1; 0 1; 0 1]; C = unique(M, 'rows');");
    EXPECT_DOUBLE_EQ(evalScalar("size(C, 1);"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("C(1, 2);"), 1.0);
}

TEST_P(SetOpsTest, UniqueRowsAllSame)
{
    eval("M = [7 8; 7 8; 7 8]; C = unique(M, 'rows');");
    EXPECT_DOUBLE_EQ(evalScalar("size(C, 1);"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("C(1, 1);"), 7.0);
    EXPECT_DOUBLE_EQ(evalScalar("C(1, 2);"), 8.0);
}

TEST_P(SetOpsTest, UniqueRowsNDThrows)
{
    // 'rows' flag is 2D-only.
    eval("A = reshape(1:24, [2, 3, 4]);");
    EXPECT_THROW(eval("C = unique(A, 'rows');"), std::exception);
}

TEST_P(SetOpsTest, UniqueRowsBadFlagThrows)
{
    eval("M = [1 2; 3 4];");
    EXPECT_THROW(eval("C = unique(M, 'banana');"), std::exception);
}

TEST_P(SetOpsTest, UniqueAcceptsNoOpFlags)
{
    // 'first', 'sorted' etc. are MATLAB-recognised but no-op for our impl.
    eval("v = [3 1 2 1]; c = unique(v, 'sorted');");
    EXPECT_DOUBLE_EQ(evalScalar("c(1);"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("c(3);"), 3.0);
}

INSTANTIATE_DUAL(SetOpsTest);
