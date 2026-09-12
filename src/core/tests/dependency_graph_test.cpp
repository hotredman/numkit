// Statement dependency facts + DAG + dirty propagation (portion A).
// Pure unit tests: parse with the core parser, analyse, assert sets.
// The beamforming-shaped case mirrors the real gallery script — the
// top-level if/elseif block is a WILDCARD there.

#include <nk/core/dependency_graph.hpp>
#include <nk/core/lexer.hpp>
#include <nk/core/parser.hpp>

#include <gtest/gtest.h>
#include <sstream>

using namespace nk;

namespace {

std::vector<StatementFacts> factsOf(const std::string &src)
{
    Lexer lexer(src);
    Parser p(lexer.tokenize());
    auto prog = p.parse();
    return computeStatementFacts(prog.get());
}

}  // namespace

TEST(DepGraph, AssignmentFacts)
{
    auto f = factsOf("y = a + b * sin(t);\n");
    ASSERT_EQ(f.size(), 1u);
    EXPECT_EQ(f[0].writes.count("y"), 1u);
    EXPECT_EQ(f[0].reads.count("a"), 1u);
    EXPECT_EQ(f[0].reads.count("b"), 1u);
    EXPECT_EQ(f[0].reads.count("t"), 1u);
    EXPECT_EQ(f[0].reads.count("sin"), 1u);   // callee is a name read
    EXPECT_FALSE(f[0].wildcardReads);
}

TEST(DepGraph, MergeWriteIndexAssign)
{
    // A(i) = v: A is read AND written (merge-write semantics).
    auto f = factsOf("A(2) = 5;\n");
    ASSERT_EQ(f.size(), 1u);
    EXPECT_EQ(f[0].writes.count("A"), 1u);
    EXPECT_EQ(f[0].reads.count("A"), 1u);
}

TEST(DepGraph, MultiOutputAssign)
{
    auto f = factsOf("[u, s, v] = svd(A, 0);\n");
    ASSERT_EQ(f.size(), 1u);
    EXPECT_EQ(f[0].writes.count("u"), 1u);
    EXPECT_EQ(f[0].writes.count("s"), 1u);
    EXPECT_EQ(f[0].writes.count("v"), 1u);
    EXPECT_EQ(f[0].reads.count("A"), 1u);
}

TEST(DepGraph, LocalFunctionDefinition)
{
    auto f = factsOf("function y = af(n, d)\n  y = n + d;\nend\n");
    ASSERT_EQ(f.size(), 1u);
    EXPECT_EQ(f[0].writes.count("af"), 1u);
    EXPECT_TRUE(f[0].reads.empty());
    EXPECT_FALSE(f[0].wildcardReads);
}

TEST(DepGraph, TopLevelIfIsWildcard)
{
    auto f = factsOf("if x > 1\n  y = 2;\nelse\n  y = 3;\nend\n");
    ASSERT_EQ(f.size(), 1u);
    EXPECT_TRUE(f[0].wildcardReads);
    EXPECT_TRUE(f[0].reads.empty());   // wildcard replaces enumeration
}

TEST(DepGraph, EvalCallIsWildcard)
{
    auto f = factsOf("z = eval('x + 1');\n");
    ASSERT_EQ(f.size(), 1u);
    EXPECT_TRUE(f[0].wildcardReads);
}

TEST(DepGraph, DirtySetLinearChain)
{
    // theta0 change → AF + plot dirty, theta vector stays cached.
    auto f = factsOf(
        "N = 16;\n"
        "theta0 = 25;\n"
        "d = 0.5;\n"
        "theta = -90:0.2:90;\n"
        "AF = theta0 + N + d + theta;\n"
        "plot(theta, AF);\n");
    DependencyGraph g(std::move(f));
    auto dirty = g.dirtySetFor("theta0");
    // statements 4 (AF) and 5 (plot); theta (3) is independent.
    ASSERT_EQ(dirty.size(), 2u);
    EXPECT_EQ(dirty[0], 4u);
    EXPECT_EQ(dirty[1], 5u);
}

TEST(DepGraph, DirtySetThroughWildcardTail)
{
    // win feeds a top-level if → the if is wildcard → the tail from
    // the if re-runs; theta BEFORE the if stays cached (real gallery
    // shape).
    auto f = factsOf(
        "N = 16;\n"
        "win = 'hamming';\n"
        "theta = -90:0.2:90;\n"
        "if strcmp(win, 'chebwin')\n"
        "  w = ones(N, 1);\n"
        "else\n"
        "  w = hamming(N);\n"
        "end\n"
        "AF = w + theta;\n"
        "polarplot(theta, AF);\n");
    DependencyGraph g(std::move(f));
    auto dirty = g.dirtySetFor("win");
    // WILDCARD-PREFIX rule (portion B2): the if is a wildcard that may
    // accumulate on earlier state, so its re-run pulls the earlier
    // prefix in — N(0) + theta(2) re-run too. win's OWN pure def (1)
    // stays SKIPPED (setParam replaces it; re-running would reset the
    // slider). Tail: if(3) + AF(4) + polarplot(5).
    ASSERT_EQ(dirty.size(), 5u);
    EXPECT_EQ(dirty[0], 0u);
    EXPECT_EQ(dirty[1], 2u);
    EXPECT_EQ(dirty[2], 3u);
    EXPECT_EQ(dirty[3], 4u);
    EXPECT_EQ(dirty[4], 5u);
}

TEST(DepGraph, PureDefsReplacedByParamWrite)
{
    // x's two pure definitions are REPLACED by the param write (they
    // never read x); only the reader re-runs. A param redefinition
    // mid-script (`x = x + 1`) reads x and would re-run — covered by
    // the merge-write tests.
    auto f = factsOf(
        "x = 1;\n"
        "x = 2;\n"
        "y = x;\n");
    DependencyGraph g(std::move(f));
    auto dirty = g.dirtySetFor("x");
    ASSERT_EQ(dirty.size(), 1u);
    EXPECT_EQ(dirty[0], 2u);
}

TEST(DepGraph, ClearIsWildcard)
{
    auto f = factsOf("clear x;\n");
    ASSERT_EQ(f.size(), 1u);
    EXPECT_TRUE(f[0].wildcardReads);
}

TEST(DepGraph, AnonymousFunctionCaptures)
{
    auto f = factsOf("f = @(t) A * sin(w * t);\n");
    ASSERT_EQ(f.size(), 1u);
    EXPECT_EQ(f[0].writes.count("f"), 1u);
    EXPECT_EQ(f[0].reads.count("A"), 1u);
    EXPECT_EQ(f[0].reads.count("w"), 1u);
    EXPECT_EQ(f[0].reads.count("t"), 0u);   // parameter, not captured
}

// ── Portion B1: element regions + index-aware dirty sets ────────────

TEST(ElemRegions, LiteralExtractionForms)
{
    auto f = factsOf(
        "a = C{5,2};\n"          // elem
        "b = C(1:5, 3);\n"       // rows 1..5 col 3
        "d = C(1:2:9, 3);\n"     // stepped → covering 1..9
        "e = C(:, 2);\n"         // full rows, col 2
        "g = C(i, 1);\n"         // identifier idx → ALL
        "h = C(7);\n"            // linear → ALL
        "k = size(C);\n");       // size probe → ALL
    ASSERT_EQ(f.size(), 7u);
    EXPECT_TRUE(f[0].elemReads.at("C").rects.size() == 1u);
    ElemRegion r0 = f[0].elemReads.at("C").rects[0];
    EXPECT_FALSE(f[0].elemReads.at("C").all);
    EXPECT_EQ(r0.r0, 5u); EXPECT_EQ(r0.r1, 5u);
    EXPECT_EQ(r0.c0, 2u); EXPECT_EQ(r0.c1, 2u);
    // rows 1..5 × col 3
    ElemRegion r1 = f[1].elemReads.at("C").rects[0];
    EXPECT_EQ(r1.r0, 1u); EXPECT_EQ(r1.r1, 5u);
    EXPECT_EQ(r1.c0, 3u); EXPECT_EQ(r1.c1, 3u);
    // stepped covers 1..9
    ElemRegion r2 = f[2].elemReads.at("C").rects[0];
    EXPECT_EQ(r2.r1, 9u);
    // full rows: r1 == SIZE_MAX
    ElemRegion r3 = f[3].elemReads.at("C").rects[0];
    EXPECT_EQ(r3.r1, (size_t)-1);
    EXPECT_EQ(r3.c0, 2u);
    // Paren forms without end/colon args are AST-ambiguous with
    // function calls (C(7) ≡ foo(3)): NO region entry — the coarse
    // read makes them whole-var readers (conservative, same dirty
    // behaviour as entry.all).
    EXPECT_EQ(f[4].elemReads.count("C"), 0u);  // C(i,1)
    EXPECT_EQ(f[5].elemReads.count("C"), 0u);  // C(7)
    EXPECT_TRUE(f[6].elemReads.at("C").all);   // size(C)
}

TEST(ElemRegions, MergeWriteRecordsWriteRegionOnly)
{
    auto f = factsOf("C{5,2} = x + 1;\n");
    ASSERT_EQ(f.size(), 1u);
    EXPECT_FALSE(f[0].elemWrites.at("C").all);
    ElemRegion w = f[0].elemWrites.at("C").rects[0];
    EXPECT_EQ(w.r0, 5u); EXPECT_EQ(w.c0, 2u);
    // The LHS is NOT an rvalue read: no elemReads entry.
    EXPECT_EQ(f[0].elemReads.count("C"), 0u);
    EXPECT_EQ(f[0].lhsMergeBases.count("C"), 1u);
}

TEST(ElemRegions, SheetDirtySetElement)
{
    // The table-editor core case: editing ONE cell recomputes only the
    // formulas reading that cell (or ranges covering it); definitions
    // of other cells and formulas on other columns stay clean.
    auto f = factsOf(
        "C = cell(10, 4);\n"                 // 0: init (whole-var def)
        "C{1,1} = 5;\n"                      // 1: value def (1,1)
        "C{1,2} = 7;\n"                      // 2: value def (1,2)
        "C{2,1} = C{1,1} * 2;\n"             // 3: reads (1,1)
        "C{2,2} = sum(cell2mat(C(:,2)));\n"  // 4: reads col 2
        "C{3,1} = C{2,1} + C{1,2};\n");      // 5: reads (2,1),(1,2)
    DependencyGraph g(std::move(f));
    // Edit cell (1,1): formula 3 (reads 1,1) + 5 (reads 2,1 via C{2,1}
    // elem) re-run. Formula 4 (col 2) and defs 2 stay clean. Init 0 is
    // a whole-var pure def → skipped.
    auto dirty = g.dirtySetForElement("C", 1, 1);
    ASSERT_EQ(dirty.size(), 2u);
    EXPECT_EQ(dirty[0], 3u);
    EXPECT_EQ(dirty[1], 5u);
    // Edit cell (1,2): formula 4 (col-2 reader) + formula 5 re-run.
    auto dirty2 = g.dirtySetForElement("C", 1, 2);
    ASSERT_EQ(dirty2.size(), 2u);
    EXPECT_EQ(dirty2[0], 4u);
    EXPECT_EQ(dirty2[1], 5u);
    // Edit cell (9,9) (empty region): nothing but wildcards — none.
    auto dirty3 = g.dirtySetForElement("C", 9, 9);
    EXPECT_TRUE(dirty3.empty());
}

TEST(ElemRegions, WholeVarReaderAccumulates)
{
    // D = C reads the WHOLE C — an edit anywhere dirties it; and it
    // depends on every writer (merge semantics), not nearest-def.
    auto f = factsOf(
        "C = cell(5,5);\n"
        "C{1,1} = 1;\n"
        "C{2,2} = 2;\n"
        "D = C;\n");
    DependencyGraph g(std::move(f));
    auto dirty = g.dirtySetForElement("C", 2, 2);
    // D (3) reads whole C → affected by any cell edit; C(2,2) def (2)
    // is a pure cell def → clean.
    ASSERT_EQ(dirty.size(), 1u);
    EXPECT_EQ(dirty[0], 3u);
}

TEST(ElemRegions, EndIndexIsAll)
{
    auto f = factsOf("x = C(end, 1);\n");
    ASSERT_EQ(f.size(), 1u);
    EXPECT_TRUE(f[0].elemReads.at("C").all);
}
