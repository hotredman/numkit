// toolboxes/builtin/tests/setops_test.cpp
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

// Complex unique: MATLAB orders unique values by magnitude |z| then phase
// angle arg(z); dedup by exact equality. Was unsupported (threw "Not a
// double array"). vs MATLAB R2025b. 2026-05-29.
TEST_P(SetOpsTest, UniqueComplex)
{
    eval("function [a, b, c] = wrapc(x)\n"
         "  [a, b, c] = unique(x);\n"
         "end");
    eval("[U, ia, ic] = wrapc([3+4i 1 3+4i 5i]);");
    auto *U  = getVarPtr("U");
    auto *ia = getVarPtr("ia");
    auto *ic = getVarPtr("ic");
    ASSERT_NE(U, nullptr);
    EXPECT_EQ(U->numel(), 3u);
    // [1, 3+4i, 5i] (by |z| then angle)
    EXPECT_DOUBLE_EQ(U->complexData()[0].real(), 1.0);
    EXPECT_DOUBLE_EQ(U->complexData()[0].imag(), 0.0);
    EXPECT_DOUBLE_EQ(U->complexData()[1].real(), 3.0);
    EXPECT_DOUBLE_EQ(U->complexData()[1].imag(), 4.0);
    EXPECT_DOUBLE_EQ(U->complexData()[2].imag(), 5.0);
    EXPECT_DOUBLE_EQ(ia->doubleData()[0], 2.0);   // 1 first at idx 2
    EXPECT_DOUBLE_EQ(ia->doubleData()[1], 1.0);   // 3+4i first at idx 1
    EXPECT_DOUBLE_EQ(ia->doubleData()[2], 4.0);   // 5i first at idx 4
    EXPECT_DOUBLE_EQ(ic->doubleData()[0], 2.0);   // X(1)=3+4i -> U(2)
    EXPECT_DOUBLE_EQ(ic->doubleData()[2], 2.0);   // X(3)=3+4i -> U(2)
    EXPECT_DOUBLE_EQ(ic->doubleData()[3], 3.0);   // X(4)=5i   -> U(3)

    // 'stable' keeps first-occurrence order.
    eval("cs = unique([3+4i 1 3+4i 5i], 'stable');");
    auto *cs = getVarPtr("cs");
    ASSERT_NE(cs, nullptr);
    EXPECT_EQ(cs->numel(), 3u);
    EXPECT_DOUBLE_EQ(cs->complexData()[0].real(), 3.0);   // 3+4i first
    EXPECT_DOUBLE_EQ(cs->complexData()[1].real(), 1.0);
    EXPECT_DOUBLE_EQ(cs->complexData()[2].imag(), 5.0);
}

// unique(x,'stable') keeps first-occurrence order (was no-op -> sorted). vs MATLAB.
TEST_P(SetOpsTest, UniqueStable)
{
    eval("u = unique([3 1 4 1 5 9 2 6 5 3], 'stable');");
    auto *u = getVarPtr("u");
    EXPECT_EQ(u->numel(), 7u);
    EXPECT_DOUBLE_EQ(u->doubleData()[0], 3.0);
    EXPECT_DOUBLE_EQ(u->doubleData()[1], 1.0);
    EXPECT_DOUBLE_EQ(u->doubleData()[2], 4.0);
    EXPECT_DOUBLE_EQ(u->doubleData()[6], 6.0);
    // [u,ia,ic] = unique(...,'stable').
    eval("function [a,b,c] = wrapStable(x)\n"
         "  [a,b,c] = unique(x, 'stable');\n"
         "end");
    eval("[U, ia, ic] = wrapStable([3 1 4 1 5]);");
    auto *U = getVarPtr("U"); auto *ia = getVarPtr("ia"); auto *ic = getVarPtr("ic");
    EXPECT_DOUBLE_EQ(U->doubleData()[0], 3.0);   // first-occurrence order
    EXPECT_DOUBLE_EQ(U->doubleData()[1], 1.0);
    EXPECT_DOUBLE_EQ(ia->doubleData()[3], 5.0);  // 5 first at idx 5
    EXPECT_DOUBLE_EQ(ic->doubleData()[3], 2.0);  // X(4)=1 -> u position 2
    // 'sorted' (default) still sorts.
    eval("s = unique([3 1 4 1 5], 'sorted');");
    EXPECT_DOUBLE_EQ(getVarPtr("s")->doubleData()[0], 1.0);
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

// ── unique output orientation (MATLAB: ia/ic always columns; u matches
//    the input orientation). Regression: ia/ic came back as rows and u was
//    always a row even for a column input. ───────────────────────────────
TEST_P(SetOpsTest, UniqueRowInputColumnIndices)
{
    eval("function [a,b,c] = wrapO(x)\n  [a,b,c] = unique(x);\nend");
    eval("[u, ia, ic] = wrapO([3 1 2 1 3]);");
    // u keeps the row orientation of the input
    EXPECT_DOUBLE_EQ(evalScalar("size(u,1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(u,2)"), 3.0);
    // ia / ic are column vectors
    EXPECT_DOUBLE_EQ(evalScalar("size(ia,1)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(ia,2)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(ic,1)"), 5.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(ic,2)"), 1.0);
    // values unchanged
    EXPECT_DOUBLE_EQ(evalScalar("ia(1)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("ic(1)"), 3.0);
}

TEST_P(SetOpsTest, UniqueColumnInputColumnValues)
{
    eval("function [a,b,c] = wrapC(x)\n  [a,b,c] = unique(x);\nend");
    eval("[u, ia, ic] = wrapC([3;1;2;1;3]);");
    // column input → column u
    EXPECT_DOUBLE_EQ(evalScalar("size(u,1)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(u,2)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(ia,2)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(ic,2)"), 1.0);
    // single-output column form too
    EXPECT_DOUBLE_EQ(evalScalar("size(unique([3;1;2]),2)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(unique([3 1 2]),1)"), 1.0); // row stays row
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

// ── set-operation index outputs (ia/ib) — were unimplemented ────────────
TEST_P(SetOpsTest, IntersectIndices)
{
    eval("function [a,b,c]=wIx(x,y)\n  [a,b,c]=intersect(x,y);\nend");
    eval("[c, ia, ib] = wIx([3 1 2 5], [2 4 1]);");
    // c = [1 2]; ia indexes A, ib indexes B (both 1-based, columns)
    EXPECT_DOUBLE_EQ(evalScalar("c(1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("c(2)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("ia(1)"), 2.0); // 1 at A(2)
    EXPECT_DOUBLE_EQ(evalScalar("ia(2)"), 3.0); // 2 at A(3)
    EXPECT_DOUBLE_EQ(evalScalar("ib(1)"), 3.0); // 1 at B(3)
    EXPECT_DOUBLE_EQ(evalScalar("ib(2)"), 1.0); // 2 at B(1)
    EXPECT_DOUBLE_EQ(evalScalar("size(ia,2)"), 1.0); // columns
    EXPECT_DOUBLE_EQ(evalScalar("size(ib,2)"), 1.0);
}

TEST_P(SetOpsTest, SetdiffIndices)
{
    eval("function [a,b]=wSd(x,y)\n  [a,b]=setdiff(x,y);\nend");
    eval("[d, ia] = wSd([3 1 2 5], [2 4 1]);");
    EXPECT_DOUBLE_EQ(evalScalar("d(1)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("d(2)"), 5.0);
    EXPECT_DOUBLE_EQ(evalScalar("ia(1)"), 1.0); // 3 at A(1)
    EXPECT_DOUBLE_EQ(evalScalar("ia(2)"), 4.0); // 5 at A(4)
    EXPECT_DOUBLE_EQ(evalScalar("size(ia,2)"), 1.0);
}

TEST_P(SetOpsTest, UnionIndices)
{
    eval("function [a,b,c]=wUn(x,y)\n  [a,b,c]=union(x,y);\nend");
    eval("[u, ia, ib] = wUn([3 1 2], [2 4 1]);");
    // u = [1 2 3 4]; ia indexes the A-sourced elements (1,2,3),
    // ib indexes the B-only element (4 at B(2)).
    EXPECT_DOUBLE_EQ(evalScalar("ia(1)"), 2.0); // 1 at A(2)
    EXPECT_DOUBLE_EQ(evalScalar("ia(2)"), 3.0); // 2 at A(3)
    EXPECT_DOUBLE_EQ(evalScalar("ia(3)"), 1.0); // 3 at A(1)
    EXPECT_DOUBLE_EQ(evalScalar("ib(1)"), 2.0); // 4 at B(2)
    EXPECT_DOUBLE_EQ(evalScalar("numel(ia)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("numel(ib)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(ia,2)"), 1.0);
}

// Complex intersect/union/setdiff (C output): exact-equality membership,
// values ordered by |z| then angle ('sorted'); 'stable' keeps first-occur
// order. Was unsupported (threw "Not a double array"). vs MATLAB R2025b.
TEST_P(SetOpsTest, SetOpsComplex)
{
    // intersect sorted -> [3+4i 5i]
    eval("ci = intersect([1 5i 3+4i 2], [3+4i 5i 7]);");
    auto *ci = getVarPtr("ci");
    ASSERT_NE(ci, nullptr); ASSERT_EQ(ci->numel(), 2u);
    EXPECT_DOUBLE_EQ(ci->complexData()[0].real(), 3.0);
    EXPECT_DOUBLE_EQ(ci->complexData()[0].imag(), 4.0);
    EXPECT_DOUBLE_EQ(ci->complexData()[1].imag(), 5.0);
    // intersect stable -> [5i 3+4i] (A-order)
    eval("cis = intersect([1 5i 3+4i 2], [3+4i 5i 7], 'stable');");
    auto *cis = getVarPtr("cis");
    ASSERT_NE(cis, nullptr);
    EXPECT_DOUBLE_EQ(cis->complexData()[0].imag(), 5.0);
    EXPECT_DOUBLE_EQ(cis->complexData()[1].real(), 3.0);

    // union sorted -> [1 3+4i 5i]
    eval("cu = union([1 5i], [3+4i 1]);");
    auto *cu = getVarPtr("cu");
    ASSERT_NE(cu, nullptr); ASSERT_EQ(cu->numel(), 3u);
    EXPECT_DOUBLE_EQ(cu->complexData()[0].real(), 1.0);
    EXPECT_DOUBLE_EQ(cu->complexData()[1].real(), 3.0);
    EXPECT_DOUBLE_EQ(cu->complexData()[2].imag(), 5.0);
    // union stable -> [1 5i 3+4i]
    eval("cus = union([1 5i], [3+4i 1], 'stable');");
    auto *cus = getVarPtr("cus");
    ASSERT_NE(cus, nullptr);
    EXPECT_DOUBLE_EQ(cus->complexData()[1].imag(), 5.0);
    EXPECT_DOUBLE_EQ(cus->complexData()[2].real(), 3.0);

    // setdiff sorted -> [1 3+4i]
    eval("cd = setdiff([1 5i 3+4i], [5i]);");
    auto *cd = getVarPtr("cd");
    ASSERT_NE(cd, nullptr); ASSERT_EQ(cd->numel(), 2u);
    EXPECT_DOUBLE_EQ(cd->complexData()[0].real(), 1.0);
    EXPECT_DOUBLE_EQ(cd->complexData()[1].imag(), 4.0);
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
    // Non-monotonic explicit edges are invalid.
    EXPECT_THROW(eval("histcounts([1 2 3], [3 2 1]);"), std::runtime_error);
    // A scalar second argument is the bin COUNT (MATLAB), not a 1-element edge
    // vector: histcounts(x, 1) -> a single bin (no throw).
    eval("h1 = histcounts([1 2 3], 1);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(h1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("h1(1)"), 3.0);
}

// ── histcounts 'Normalization' ──────────────────────────────
// x has 9 elements, 2 out of range → N = 9 (numel, not in-range count).
TEST_P(SetOpsTest, HistcountsNormProbability)
{
    eval("h = histcounts([1 2 2 3 3 3 5 99 -7], [0 2 4 6], "
         "'Normalization', 'probability');");
    auto *h = getVarPtr("h");
    ASSERT_NE(h, nullptr);
    EXPECT_NEAR(h->doubleData()[0], 1.0 / 9.0, 1e-12);
    EXPECT_NEAR(h->doubleData()[1], 5.0 / 9.0, 1e-12);
    EXPECT_NEAR(h->doubleData()[2], 1.0 / 9.0, 1e-12);
}

TEST_P(SetOpsTest, HistcountsNormCumcountAndCdf)
{
    eval("cc = histcounts([1 2 2 3 3 3 5 99 -7], [0 2 4 6], "
         "'Normalization', 'cumcount');");
    auto *cc = getVarPtr("cc");
    ASSERT_NE(cc, nullptr);
    EXPECT_DOUBLE_EQ(cc->doubleData()[0], 1.0);
    EXPECT_DOUBLE_EQ(cc->doubleData()[1], 6.0);
    EXPECT_DOUBLE_EQ(cc->doubleData()[2], 7.0);
    eval("cf = histcounts([1 2 2 3 3 3 5 99 -7], [0 2 4 6], "
         "'Normalization', 'cdf');");
    auto *cf = getVarPtr("cf");
    EXPECT_NEAR(cf->doubleData()[2], 7.0 / 9.0, 1e-12);  // not 1.0: 2 out of range
}

TEST_P(SetOpsTest, HistcountsNormCountDensityAndPdf)
{
    // Nonuniform edges [0 1 4 6] → binwidths [1 3 2]; data 4 pts all in range.
    eval("cd = histcounts([0.5 2 3 5], [0 1 4 6], "
         "'Normalization', 'countdensity');");
    auto *cd = getVarPtr("cd");
    ASSERT_NE(cd, nullptr);
    EXPECT_NEAR(cd->doubleData()[0], 1.0, 1e-12);          // 1/1
    EXPECT_NEAR(cd->doubleData()[1], 2.0 / 3.0, 1e-12);    // 2/3
    EXPECT_NEAR(cd->doubleData()[2], 0.5, 1e-12);          // 1/2
    eval("pf = histcounts([0.5 2 3 5], [0 1 4 6], "
         "'Normalization', 'pdf');");
    auto *pf = getVarPtr("pf");
    EXPECT_NEAR(pf->doubleData()[0], 0.25, 1e-12);         // 1/(4*1)
    EXPECT_NEAR(pf->doubleData()[1], 2.0 / 12.0, 1e-12);   // 2/(4*3)
    EXPECT_NEAR(pf->doubleData()[2], 1.0 / 8.0, 1e-12);    // 1/(4*2)
}

TEST_P(SetOpsTest, HistcountsNormUnknownThrows)
{
    EXPECT_THROW(eval("histcounts([1 2 3], [0 2 4], 'Normalization', 'bogus');"),
                 std::runtime_error);
}

// ── histcounts 'BinEdges' name-value + [n, edges] second output ──────
TEST_P(SetOpsTest, HistcountsBinEdgesNameValue)
{
    // 'BinEdges' is equivalent to passing the edges positionally. Regression:
    // numkit used to treat 'BinEdges' as the edges vector → "Not a double".
    eval("[n, e] = histcounts([1 2 3 4 5], 'BinEdges', [0 2 4 6]);");
    auto *n = getVarPtr("n");
    auto *e = getVarPtr("e");
    ASSERT_NE(n, nullptr);
    ASSERT_NE(e, nullptr);
    EXPECT_DOUBLE_EQ(n->doubleData()[0], 1.0);
    EXPECT_DOUBLE_EQ(n->doubleData()[1], 2.0);
    EXPECT_DOUBLE_EQ(n->doubleData()[2], 2.0);
    // second output = edges, returned as a row vector
    EXPECT_EQ(e->numel(), 4u);
    EXPECT_DOUBLE_EQ(e->doubleData()[0], 0.0);
    EXPECT_DOUBLE_EQ(e->doubleData()[3], 6.0);
    // 'BinEdges' composes with 'Normalization'
    eval("p = histcounts([1 2 3 4 5], 'BinEdges', [0 2 4 6], "
         "'Normalization', 'probability');");
    auto *p = getVarPtr("p");
    ASSERT_NE(p, nullptr);
    EXPECT_NEAR(p->doubleData()[0], 0.2, 1e-12);
    EXPECT_NEAR(p->doubleData()[1], 0.4, 1e-12);
    EXPECT_NEAR(p->doubleData()[2], 0.4, 1e-12);
}

TEST_P(SetOpsTest, HistcountsSecondOutputEdges)
{
    // [n, e] with positional edges also returns the edges (was previously
    // a single-output-only function).
    eval("[n, e] = histcounts([1 2 3 4 5], [0 2 4 6]);");
    auto *e = getVarPtr("e");
    ASSERT_NE(e, nullptr);
    EXPECT_EQ(e->numel(), 4u);
    EXPECT_DOUBLE_EQ(e->doubleData()[2], 4.0);
}

// Automatic bin selection (MATLAB binpicker rules) — was an unsupported stub
// (bugs/math/histcounts-autobinning). Values verified against MATLAB R2025b.
TEST_P(SetOpsTest, HistcountsAutomaticBinning)
{
    // 'BinWidth': fixed-width bins on multiples of the width covering the data.
    eval("[n, e] = histcounts([1 5 2 8 3], 'BinWidth', 2);");   // e = [0 2 4 6 8]
    EXPECT_DOUBLE_EQ(evalScalar("numel(e)"), 5.0);
    EXPECT_DOUBLE_EQ(evalScalar("e(1)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("e(2)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("sum(n)"), 5.0);

    // 'BinLimits' + 'NumBins': nbins uniform bins over the limits. n2 = [1 2 2].
    eval("[n2, e2] = histcounts(1:5, 'BinLimits', [0 6], 'NumBins', 3);");
    EXPECT_DOUBLE_EQ(evalScalar("e2(1)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("e2(3)"), 4.0);
    EXPECT_DOUBLE_EQ(evalScalar("e2(4)"), 6.0);
    EXPECT_DOUBLE_EQ(evalScalar("n2(2)"), 2.0);

    // Default 'auto' on continuous data → Scott's rule; binpicker snaps the
    // width to a nice value (5) → 6 bins on [0, 30].
    eval("[n3, e3] = histcounts((1:200)/7);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(n3)"), 6.0);
    EXPECT_DOUBLE_EQ(evalScalar("e3(1)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("e3(end)"), 30.0);

    // sturges / sqrt / fd: the rule's bin count is re-derived through binpicker.
    EXPECT_DOUBLE_EQ(evalScalar("numel(histcounts((1:200)/7,'BinMethod','sturges'))"), 10.0);
    EXPECT_DOUBLE_EQ(evalScalar("numel(histcounts((1:200)/7,'BinMethod','sqrt'))"), 15.0);
    EXPECT_DOUBLE_EQ(evalScalar("numel(histcounts((1:200)/7,'BinMethod','fd'))"), 6.0);
}

// DEEP-PROBE 2026-05-31: 'BinMethod','integers' (one unit-width bin centered
// on each integer in [round(min),round(max)]); other BinMethods still throw.
TEST_P(SetOpsTest, HistcountsBinMethodIntegers)
{
    eval("[n, e] = histcounts([1 1 2 3 3 3], 'BinMethod', 'integers');");
    auto *n = getVarPtr("n");
    auto *e = getVarPtr("e");
    ASSERT_NE(n, nullptr);
    ASSERT_NE(e, nullptr);
    EXPECT_EQ(n->numel(), 3u);
    EXPECT_DOUBLE_EQ(n->doubleData()[0], 2.0);
    EXPECT_DOUBLE_EQ(n->doubleData()[1], 1.0);
    EXPECT_DOUBLE_EQ(n->doubleData()[2], 3.0);
    EXPECT_EQ(e->numel(), 4u);
    EXPECT_DOUBLE_EQ(e->doubleData()[0], 0.5);
    EXPECT_DOUBLE_EQ(e->doubleData()[3], 3.5);

    // Gaps create empty interior bins; non-integer data rounds to centers.
    eval("[m, em] = histcounts([2 5 5 7], 'BinMethod', 'integers');");
    auto *m = getVarPtr("m");
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->numel(), 6u);            // integers 2..7
    EXPECT_DOUBLE_EQ(m->doubleData()[0], 1.0);   // the 2
    EXPECT_DOUBLE_EQ(m->doubleData()[3], 2.0);   // the two 5s
    EXPECT_DOUBLE_EQ(m->doubleData()[5], 1.0);   // the 7

    eval("[d, ed] = histcounts([1.2 2.8 2.9 3.1], 'BinMethod', 'integers');");
    auto *d = getVarPtr("d");
    auto *ed = getVarPtr("ed");
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->numel(), 3u);            // round(1.2)=1 .. round(3.1)=3
    EXPECT_DOUBLE_EQ(ed->doubleData()[0], 0.5);
    EXPECT_DOUBLE_EQ(d->doubleData()[2], 3.0);   // 2.8,2.9,3.1 -> integer 3

    // Negative range.
    eval("[k, ek] = histcounts([-2 -1 -1 0], 'BinMethod', 'integers');");
    auto *ek = getVarPtr("ek");
    auto *k = getVarPtr("k");
    ASSERT_NE(ek, nullptr);
    EXPECT_DOUBLE_EQ(ek->doubleData()[0], -2.5);
    EXPECT_DOUBLE_EQ(k->doubleData()[1], 2.0);   // the two -1s

    // Composes with Normalization.
    eval("p = histcounts([1 1 2 3], 'BinMethod', 'integers', "
         "'Normalization', 'probability');");
    auto *p = getVarPtr("p");
    ASSERT_NE(p, nullptr);
    EXPECT_NEAR(p->doubleData()[0], 0.5, 1e-12);

    // 'auto' on small integer data resolves to the integer rule (was a stub
    // throw before bugs/math/histcounts-autobinning was fixed).
    eval("[a, ea] = histcounts([1 2 3], 'BinMethod', 'auto');");
    auto *a = getVarPtr("a");
    ASSERT_NE(a, nullptr);
    EXPECT_EQ(a->numel(), 3u);
    EXPECT_DOUBLE_EQ(getVarPtr("ea")->doubleData()[0], 0.5);
}

// ── histc (legacy) ──────────────────────────────────────────
// n has length(edges); last bin = exact-equal to edges(end). vs MATLAB.
TEST_P(SetOpsTest, HistcCounts)
{
    eval("h = histc([1 2 2 3 5], [0 2 4 6]);");
    auto *h = getVarPtr("h");
    EXPECT_EQ(h->numel(), 4u);                  // length(edges), not edges-1
    EXPECT_DOUBLE_EQ(h->doubleData()[0], 1.0);  // [0,2): {1}
    EXPECT_DOUBLE_EQ(h->doubleData()[1], 3.0);  // [2,4): {2,2,3}
    EXPECT_DOUBLE_EQ(h->doubleData()[2], 1.0);  // [4,6): {5}
    EXPECT_DOUBLE_EQ(h->doubleData()[3], 0.0);  // == 6: none
}

TEST_P(SetOpsTest, HistcBinIndexAndColumnwise)
{
    // 2nd output: 1-based bin index of each element (0 if out of range).
    eval("function [a,b] = hcb(x, e)\n  [a,b] = histc(x, e);\nend");
    eval("[n, bin] = hcb([1 2 2 3 5], [0 2 4 6]);");
    auto *bin = getVarPtr("bin");
    EXPECT_DOUBLE_EQ(bin->doubleData()[0], 1.0);
    EXPECT_DOUBLE_EQ(bin->doubleData()[1], 2.0);
    EXPECT_DOUBLE_EQ(bin->doubleData()[4], 3.0);
    // Matrix: column-wise, length(edges) x ncols.
    eval("H = histc([1 5; 2 6; 3 7], [0 4 8]);");
    auto *H = getVarPtr("H");
    EXPECT_EQ(H->dims().rows(), 3u);
    EXPECT_EQ(H->dims().cols(), 2u);
    EXPECT_DOUBLE_EQ(evalScalar("H(1,1)"), 3.0);  // col1 [1;2;3] all in [0,4)
    EXPECT_DOUBLE_EQ(evalScalar("H(2,2)"), 3.0);  // col2 [5;6;7] all in [4,8)
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
// custom DoubleHashEq0 in discrete.cpp must collapse them. These
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

// unique(M,'rows','stable'): first-occurrence order instead of lex sort.
// DEEP-PROBE 2026-05-31 — the 'rows' path previously dropped 'stable'.
// vs MATLAB R2025b: M=[3 0;1 0;2 0;1 0;3 0] -> C=[3 0;1 0;2 0],
// ia=[1;2;3], ic=[1;2;3;2;1].
TEST_P(SetOpsTest, UniqueRowsStableOrder)
{
    eval("M = [3 0; 1 0; 2 0; 1 0; 3 0]; [C, ia, ic] = unique(M, 'rows', 'stable');");
    EXPECT_DOUBLE_EQ(evalScalar("size(C, 1);"), 3.0);
    // Rows kept in first-appearance order (NOT sorted).
    EXPECT_DOUBLE_EQ(evalScalar("C(1,1);"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("C(2,1);"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("C(3,1);"), 2.0);
    // ia indexes the first occurrence of each distinct row.
    EXPECT_DOUBLE_EQ(evalScalar("ia(1);"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("ia(2);"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("ia(3);"), 3.0);
    // ic maps every row back to its unique entry.
    EXPECT_DOUBLE_EQ(evalScalar("ic(1);"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("ic(2);"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("ic(3);"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("ic(4);"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("ic(5);"), 1.0);
    // Single-output form keeps the same order.
    eval("D = unique([5 5; 1 1; 5 5; 9 9; 1 1], 'rows', 'stable');");
    EXPECT_DOUBLE_EQ(evalScalar("size(D,1);"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("D(1,1);"), 5.0);
    EXPECT_DOUBLE_EQ(evalScalar("D(2,1);"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("D(3,1);"), 9.0);
    // 'sorted' (default) is unchanged.
    eval("S = unique(M, 'rows');");
    EXPECT_DOUBLE_EQ(evalScalar("S(1,1);"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("S(3,1);"), 3.0);
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

// 'stable' setOrder for setdiff/union/intersect: keep first-occurrence
// (A-then-B) order instead of sorting. (Was ignored -> always sorted.)
// vs MATLAB R2025b.
TEST_P(SetOpsTest, BinarySetopsStableOrder)
{
    eval("sd = setdiff([3 1 2 5 4], [2 5], 'stable');");   // -> [3 1 4]
    EXPECT_DOUBLE_EQ(evalScalar("sd(1);"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("sd(2);"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("sd(3);"), 4.0);
    eval("un = union([3 1], [2 1], 'stable');");           // -> [3 1 2]
    EXPECT_DOUBLE_EQ(evalScalar("un(1);"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("un(3);"), 2.0);
    eval("ii = intersect([4 2 3 1], [1 2 4], 'stable');"); // -> [4 2 1]
    EXPECT_DOUBLE_EQ(evalScalar("ii(1);"), 4.0);
    EXPECT_DOUBLE_EQ(evalScalar("ii(3);"), 1.0);
    // default 'sorted' unchanged.
    eval("ss = setdiff([3 1 2 5 4], [2 5]);");
    EXPECT_DOUBLE_EQ(evalScalar("ss(1);"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("ss(3);"), 4.0);
}

// ismember 2nd output loc = LOWEST 1-based index in B (0 if absent).
// (Was missing -> [tf,loc]=ismember(...) errored.) vs MATLAB R2025b.
TEST_P(SetOpsTest, IsmemberLocSecondOutput)
{
    eval("[tf, loc] = ismember([2 5 8 1], [5 2 9]);");
    EXPECT_DOUBLE_EQ(evalScalar("tf(1);"),  1.0);
    EXPECT_DOUBLE_EQ(evalScalar("tf(3);"),  0.0);
    EXPECT_DOUBLE_EQ(evalScalar("loc(1);"), 2.0);   // 2 is at B index 2
    EXPECT_DOUBLE_EQ(evalScalar("loc(2);"), 1.0);   // 5 is at B index 1
    EXPECT_DOUBLE_EQ(evalScalar("loc(3);"), 0.0);   // 8 absent
    // Tie: B has duplicate values -> loc is the LOWEST index.
    eval("[~, l2] = ismember([3 1 2], [2 1 3 1]);");
    EXPECT_DOUBLE_EQ(evalScalar("l2(1);"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("l2(2);"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("l2(3);"), 1.0);
}

// Complex ismember: membership by EXACT equality (real AND imag); Locb is
// the lowest 1-based index in B; NaN component never matches; reals vs
// complex compare as z+0i. Was unsupported (threw "Not a double array").
// vs MATLAB R2025b. 2026-05-29.
TEST_P(SetOpsTest, IsmemberComplex)
{
    eval("[tf, loc] = ismember([1 5i 3+4i 2], [3+4i 5i 1]);");
    EXPECT_DOUBLE_EQ(evalScalar("tf(1);"),  1.0);
    EXPECT_DOUBLE_EQ(evalScalar("tf(4);"),  0.0);
    EXPECT_DOUBLE_EQ(evalScalar("loc(1);"), 3.0);   // 1 at B(3)
    EXPECT_DOUBLE_EQ(evalScalar("loc(2);"), 2.0);   // 5i at B(2)
    EXPECT_DOUBLE_EQ(evalScalar("loc(3);"), 1.0);   // 3+4i at B(1)
    EXPECT_DOUBLE_EQ(evalScalar("loc(4);"), 0.0);   // 2 absent
    // duplicate in B -> lowest index.
    eval("[~, l2] = ismember(5i, [5i 2 5i]);");
    EXPECT_DOUBLE_EQ(evalScalar("l2;"), 1.0);
    // real query against complex set compares as z+0i.
    EXPECT_DOUBLE_EQ(evalScalar("double(ismember(2, [2+0i 5i]));"), 1.0);
    // NaN component never matches.
    EXPECT_DOUBLE_EQ(evalScalar("double(ismember(complex(nan,1), [complex(nan,1) 2]));"), 0.0);
}

// setdiff/intersect/union with the 'rows' flag: each row is one element, the
// result is the sorted set of unique rows. Was throwing (setdiff) or ignoring
// the flag and flattening (intersect/union). vs MATLAB R2025b.
// DEEP-PROBE 2026-05-31.
TEST_P(SetOpsTest, SetOpsRows)
{
    eval("Ar = [1 2;3 4;5 6]; Br = [3 4;9 9;1 2];");
    // setdiff rows: rows of A not in B -> [5 6].
    eval("d = setdiff(Ar, Br, 'rows');");
    EXPECT_DOUBLE_EQ(evalScalar("size(d,1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(d,2)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("d(1,1)"), 5.0);
    EXPECT_DOUBLE_EQ(evalScalar("d(1,2)"), 6.0);
    // intersect rows: common rows, sorted -> [1 2;3 4].
    eval("c = intersect(Ar, Br, 'rows');");
    EXPECT_DOUBLE_EQ(evalScalar("size(c,1)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("c(1,1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("c(2,1)"), 3.0);
    // row-distinguishing: [1 2;3 4] vs [2 1;3 4] -> only [3 4] (1x2, not 1x4).
    eval("dd = intersect([1 2;3 4], [2 1;3 4], 'rows');");
    EXPECT_DOUBLE_EQ(evalScalar("size(dd,1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(dd,2)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("dd(1,1)"), 3.0);
    // union rows: unique rows of [A;B], sorted -> [1 2;3 4;5 6;9 9].
    eval("u = union(Ar, Br, 'rows');");
    EXPECT_DOUBLE_EQ(evalScalar("size(u,1)"), 4.0);
    EXPECT_DOUBLE_EQ(evalScalar("u(4,1)"), 9.0);
    EXPECT_DOUBLE_EQ(evalScalar("u(4,2)"), 9.0);
    // 'rows' index outputs are deferred (must throw).
    EXPECT_THROW(eval("[dd2, ia] = setdiff(Ar, Br, 'rows');"), std::exception);
}

// ismember(A,B,'rows'): row-wise membership. Was IGNORING 'rows' and doing
// element-wise membership (returned a MATRIX the size of A). With 'rows' each
// row is one element; tf and loc are COLUMNS of height size(A,1). vs MATLAB
// R2025b. DEEP-PROBE 2026-05-31.
TEST_P(SetOpsTest, IsmemberRows)
{
    eval("[tf, loc] = ismember([1 2; 5 6; 3 4], [3 4; 1 2; 7 8], 'rows');");
    EXPECT_DOUBLE_EQ(evalScalar("size(tf,1)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(tf,2)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(tf(1))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(tf(2))"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(tf(3))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("loc(1)"), 2.0);   // [1 2] is B row 2
    EXPECT_DOUBLE_EQ(evalScalar("loc(2)"), 0.0);   // [5 6] absent
    EXPECT_DOUBLE_EQ(evalScalar("loc(3)"), 1.0);   // [3 4] is B row 1
    // duplicate row in B -> LOWEST index.
    eval("[~, l2] = ismember([2 2; 1 1], [1 1; 3 3; 1 1; 2 2], 'rows');");
    EXPECT_DOUBLE_EQ(evalScalar("l2(1)"), 4.0);
    EXPECT_DOUBLE_EQ(evalScalar("l2(2)"), 1.0);
    // single-output scalar form.
    EXPECT_DOUBLE_EQ(evalScalar("double(ismember([10 20], [10 20; 1 2], 'rows'))"), 1.0);
    // NaN-containing row never matches.
    EXPECT_DOUBLE_EQ(evalScalar("double(ismember([nan 2], [nan 2; 1 2], 'rows'))"), 0.0);
    // mismatched column counts throw.
    EXPECT_THROW(eval("ismember([1 2 3], [1 2], 'rows');"), std::exception);
}

// setxor(A,B,'rows'): symmetric difference of the row sets (rows in exactly
// one input), sorted. Was IGNORING 'rows' and flattening element-wise to a
// 1xN vector. vs MATLAB R2025b. DEEP-PROBE 2026-05-31.
TEST_P(SetOpsTest, SetxorRows)
{
    eval("x = setxor([1 2;3 4;5 6], [3 4;9 9;1 2], 'rows');");
    EXPECT_DOUBLE_EQ(evalScalar("size(x,1)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(x,2)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("x(1,1)"), 5.0);   // only in A
    EXPECT_DOUBLE_EQ(evalScalar("x(1,2)"), 6.0);
    EXPECT_DOUBLE_EQ(evalScalar("x(2,1)"), 9.0);   // only in B
    EXPECT_DOUBLE_EQ(evalScalar("x(2,2)"), 9.0);
    // interleaved only-in-A / only-in-B, all sorted together.
    eval("y = setxor([5 6;1 1], [1 1;2 2;7 8], 'rows');");
    EXPECT_DOUBLE_EQ(evalScalar("size(y,1)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(1,1)"), 2.0);   // [2 2] (B)
    EXPECT_DOUBLE_EQ(evalScalar("y(2,1)"), 5.0);   // [5 6] (A)
    EXPECT_DOUBLE_EQ(evalScalar("y(3,1)"), 7.0);   // [7 8] (B)
    // element-wise (non-rows) path unchanged.
    eval("e = setxor([1 2 3 4], [3 4 5 6]);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(e)"), 4.0);
    EXPECT_DOUBLE_EQ(evalScalar("e(1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("e(4)"), 6.0);
    // mismatched columns throw; 'rows' index outputs deferred.
    EXPECT_THROW(eval("setxor([1 2 3], [1 2], 'rows');"), std::exception);
    EXPECT_THROW(eval("[c, ia] = setxor([1 2;3 4], [3 4;9 9], 'rows');"), std::exception);
}

INSTANTIATE_DUAL(SetOpsTest);
