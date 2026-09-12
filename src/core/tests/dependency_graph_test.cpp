// Statement dependency facts + DAG + dirty propagation (portion A).
// Pure unit tests: parse with the core parser, analyse, assert sets.
// The beamforming-shaped case mirrors the real gallery script — the
// top-level if/elseif block is a WILDCARD there.

#include <numkit/core/dependency_graph.hpp>
#include <numkit/core/lexer.hpp>
#include <numkit/core/parser.hpp>

#include <gtest/gtest.h>
#include <sstream>

using namespace numkit;

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
    // The if WITH its body is ONE top-level statement (index 3); the
    // tail is AF(4) + polarplot(5); theta(2) untouched.
    ASSERT_EQ(dirty.size(), 3u);
    EXPECT_EQ(dirty[0], 3u);
    EXPECT_EQ(dirty[1], 4u);
    EXPECT_EQ(dirty[2], 5u);
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
