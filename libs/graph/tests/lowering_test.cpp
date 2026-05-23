// libs/graph/tests/lowering_test.cpp — Phase 1 lowering coverage.
//
// Verifies that small scripts produce the expected NodeGraph shape:
//   • Per-statement node counts + kinds
//   • Data-edge wiring across reassignment + side-effect statements
//   • lastProducer correctness for indexed / field LHS (read-modify-
//     write should count the root identifier as BOTH input AND output)

#include <numkit/core/lexer.hpp>
#include <numkit/core/parser.hpp>
#include <numkit/graph/lowering.hpp>
#include <numkit/graph/serialize.hpp>

#include <gtest/gtest.h>

using namespace numkit;

namespace {

graph::NodeGraph lowerSource(const std::string &source)
{
    Lexer lex(source);
    auto tokens = lex.tokenize();
    Parser parser(tokens);
    auto root = parser.parse();
    return graph::lowerScript(*root, source);
}

// Count edges that wire a specific variable name.
int countDataEdges(const graph::NodeGraph &g, const std::string &var)
{
    int n = 0;
    for (const auto &e : g.edges) {
        if (e.kind == graph::EdgeKind::Data && e.varName == var) ++n;
    }
    return n;
}

// Find first node by kind (or -1).
int firstNodeOfKind(const graph::NodeGraph &g, graph::NodeKind k)
{
    for (size_t i = 0; i < g.nodes.size(); ++i) {
        if (g.nodes[i].kind == k) return static_cast<int>(i);
    }
    return -1;
}

} // namespace

// ── Smoke: 3-statement DSP pipeline ────────────────────────────────

TEST(GraphLowering, ThreeStatementPipeline)
{
    // x = 1; y = x + 2; plot(y);
    // Expected:
    //   node 0: Assignment   outputs=[x]       inputs=[]
    //   node 1: Assignment   outputs=[y]       inputs=[x]
    //   node 2: ExprStmt                       inputs=[y]
    // Edges:
    //   0 -> 1 (var x)
    //   1 -> 2 (var y)
    auto g = lowerSource("x = 1;\ny = x + 2;\nplot(y);\n");

    ASSERT_EQ(g.nodes.size(), 3u);
    EXPECT_EQ(g.nodes[0].kind, graph::NodeKind::Assignment);
    EXPECT_EQ(g.nodes[1].kind, graph::NodeKind::Assignment);
    EXPECT_EQ(g.nodes[2].kind, graph::NodeKind::ExprStmt);

    EXPECT_EQ(g.nodes[0].outputs, (std::vector<std::string>{"x"}));
    EXPECT_TRUE(g.nodes[0].inputs.empty());

    EXPECT_EQ(g.nodes[1].outputs, (std::vector<std::string>{"y"}));
    EXPECT_EQ(g.nodes[1].inputs,  (std::vector<std::string>{"x"}));

    EXPECT_TRUE(g.nodes[2].outputs.empty());
    EXPECT_EQ(g.nodes[2].inputs,  (std::vector<std::string>{"y"}));

    EXPECT_EQ(countDataEdges(g, "x"), 1);
    EXPECT_EQ(countDataEdges(g, "y"), 1);
}

// ── Reassignment: SSA-like behind the scenes ───────────────────────

TEST(GraphLowering, ReassignmentChainsThroughLastProducer)
{
    // x = 1; x = x + 1; plot(x);
    // node 0: x = 1
    // node 1: x = x + 1     reads x@0, writes x@1
    // node 2: plot(x)       reads x@1
    auto g = lowerSource("x = 1;\nx = x + 1;\nplot(x);\n");

    ASSERT_EQ(g.nodes.size(), 3u);
    // Edges: 0→1 (x), 1→2 (x). The (re-)assignment counts as both
    // read and write but lastProducer was updated only after wiring.
    ASSERT_EQ(g.edges.size(), 2u);
    EXPECT_EQ(g.edges[0].source.nodeId, 0);
    EXPECT_EQ(g.edges[0].target.nodeId, 1);
    EXPECT_EQ(g.edges[0].varName, "x");
    EXPECT_EQ(g.edges[1].source.nodeId, 1);
    EXPECT_EQ(g.edges[1].target.nodeId, 2);
    EXPECT_EQ(g.edges[1].varName, "x");
}

// ── Multi-LHS (MULTI_ASSIGN) ───────────────────────────────────────

TEST(GraphLowering, MultiAssignProducesMultipleOutputs)
{
    // [a, b] = size(M); disp(a); disp(b);
    auto g = lowerSource("[a, b] = size(M);\ndisp(a);\ndisp(b);\n");

    ASSERT_EQ(g.nodes.size(), 3u);
    EXPECT_EQ(g.nodes[0].kind, graph::NodeKind::Assignment);
    EXPECT_EQ(g.nodes[0].outputs, (std::vector<std::string>{"a", "b"}));
    EXPECT_EQ(g.nodes[0].inputs,  (std::vector<std::string>{"M"}));

    // Both disp() reads should wire from node 0.
    EXPECT_EQ(g.nodes[1].inputs, (std::vector<std::string>{"a"}));
    EXPECT_EQ(g.nodes[2].inputs, (std::vector<std::string>{"b"}));
    EXPECT_EQ(countDataEdges(g, "a"), 1);
    EXPECT_EQ(countDataEdges(g, "b"), 1);
}

// ── Multi-LHS with ~ (ignored slot) ────────────────────────────────

TEST(GraphLowering, MultiAssignIgnoresTildeSlots)
{
    // [~, b] = size(M); disp(b);
    auto g = lowerSource("[~, b] = size(M);\ndisp(b);\n");
    ASSERT_EQ(g.nodes.size(), 2u);
    EXPECT_EQ(g.nodes[0].outputs, (std::vector<std::string>{"~", "b"}));
    // No edge wires `~` — only `b`.
    EXPECT_EQ(countDataEdges(g, "b"), 1);
    EXPECT_EQ(countDataEdges(g, "~"), 0);
}

// ── Indexed LHS: read-modify-write ─────────────────────────────────

TEST(GraphLowering, IndexedAssignTreatsRootAsBothReadAndWrite)
{
    // A = zeros(3); A(1, 2) = 5; disp(A);
    // node 1 (A(1,2)=5) reads OLD A (from node 0) AND produces new A,
    // node 2 (disp) reads new A (from node 1).
    auto g = lowerSource("A = zeros(3);\nA(1, 2) = 5;\ndisp(A);\n");

    ASSERT_EQ(g.nodes.size(), 3u);
    EXPECT_EQ(g.nodes[1].outputs, (std::vector<std::string>{"A"}));
    // A appears as an input on node 1 (read-modify-write).
    auto &ins = g.nodes[1].inputs;
    EXPECT_NE(std::find(ins.begin(), ins.end(), "A"), ins.end());

    // Two edges on var A: 0→1, 1→2.
    EXPECT_EQ(countDataEdges(g, "A"), 2);
}

// ── Field LHS ──────────────────────────────────────────────────────

TEST(GraphLowering, FieldAssignTreatsRootAsBothReadAndWrite)
{
    // s = struct(); s.x = 1; disp(s);
    auto g = lowerSource("s = struct();\ns.x = 1;\ndisp(s);\n");

    ASSERT_EQ(g.nodes.size(), 3u);
    EXPECT_EQ(g.nodes[1].outputs, (std::vector<std::string>{"s"}));
    auto &ins = g.nodes[1].inputs;
    EXPECT_NE(std::find(ins.begin(), ins.end(), "s"), ins.end());
    EXPECT_EQ(countDataEdges(g, "s"), 2);
}

// ── Side-effect statement with no LHS ──────────────────────────────

TEST(GraphLowering, BareExpressionIsExprStmtNoOutputs)
{
    // x = 5; x + 1; disp(x);
    auto g = lowerSource("x = 5;\nx + 1;\ndisp(x);\n");

    ASSERT_EQ(g.nodes.size(), 3u);
    EXPECT_EQ(g.nodes[1].kind, graph::NodeKind::ExprStmt);
    EXPECT_TRUE(g.nodes[1].outputs.empty());
    EXPECT_EQ(g.nodes[1].inputs, (std::vector<std::string>{"x"}));
    // Edges: 0→1 (x), 0→2 (x). lastProducer unchanged by ExprStmt.
    EXPECT_EQ(countDataEdges(g, "x"), 2);
}

// ── Undefined inputs don't create edges ────────────────────────────

TEST(GraphLowering, UndefinedReadEmitsNoEdge)
{
    // y = z + 1;   (z never assigned — treat as function param /
    // undefined source; UI will show a dangling input handle).
    auto g = lowerSource("y = z + 1;\n");
    ASSERT_EQ(g.nodes.size(), 1u);
    EXPECT_EQ(g.nodes[0].inputs, (std::vector<std::string>{"z"}));
    EXPECT_TRUE(g.edges.empty());
}

// ── sourceLine wiring ─────────────────────────────────────────────

TEST(GraphLowering, SourceLineMatchesScriptLines)
{
    auto g = lowerSource("x = 1;\ny = x;\n");
    ASSERT_EQ(g.nodes.size(), 2u);
    EXPECT_EQ(g.nodes[0].sourceLine, 1);
    EXPECT_EQ(g.nodes[1].sourceLine, 2);
    EXPECT_EQ(g.nodes[0].sourceText, "x = 1;");
    EXPECT_EQ(g.nodes[1].sourceText, "y = x;");
}

// ── Global declaration ────────────────────────────────────────────

TEST(GraphLowering, GlobalDeclRegistersProducer)
{
    auto g = lowerSource("global X;\ny = X + 1;\n");
    ASSERT_EQ(g.nodes.size(), 2u);
    EXPECT_EQ(g.nodes[0].kind, graph::NodeKind::GlobalDecl);
    EXPECT_EQ(g.nodes[0].outputs, (std::vector<std::string>{"X"}));
    // y = X + 1 reads X — should wire from the global decl.
    EXPECT_EQ(countDataEdges(g, "X"), 1);
}

// ── JSON serialization round-trip-ish ─────────────────────────────

TEST(GraphLowering, JsonContainsExpectedKeys)
{
    auto g = lowerSource("x = 1;\ny = x;\n");
    std::string j = graph::toJSON(g);
    // Spot-check the keys are present.
    EXPECT_NE(j.find("\"functionName\""), std::string::npos);
    EXPECT_NE(j.find("\"nodes\""),        std::string::npos);
    EXPECT_NE(j.find("\"edges\""),        std::string::npos);
    EXPECT_NE(j.find("\"Assignment\""),   std::string::npos);
    EXPECT_NE(j.find("\"Data\""),         std::string::npos);
    EXPECT_NE(j.find("\"sourceLine\":1"), std::string::npos);
    EXPECT_NE(j.find("\"sourceLine\":2"), std::string::npos);
}

// ── Stubbed control-flow: opaque ExprStmt (Phase 2 fills it in) ───

TEST(GraphLowering, IfStmtPhase1IsOpaqueExprStmt)
{
    // Until Phase 2 control-flow lowering lands, `if` blocks emit a
    // single opaque node so the graph still renders. Verify the
    // contract: one node, kind = ExprStmt, sourceLine on the `if`.
    auto g = lowerSource("if x > 0\n  y = 1;\nend\n");
    ASSERT_EQ(g.nodes.size(), 1u);
    EXPECT_EQ(g.nodes[0].kind, graph::NodeKind::ExprStmt);
    EXPECT_EQ(g.nodes[0].sourceLine, 1);
}
