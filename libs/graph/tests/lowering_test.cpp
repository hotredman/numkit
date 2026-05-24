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
    // Pass tokens so the lowering knows where COMMENT lives and can
    // trim sourceText accordingly (parity with WASM binding).
    return graph::lowerScript(*root, source, tokens);
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
    // M = magic(4); [a, b] = size(M); disp(a); disp(b);
    // M must be assigned BEFORE the size() call for our new "known-
    // producer-only" rule to record it as an input. Built-in names
    // (size, magic, disp) are filtered out automatically.
    auto g = lowerSource("M = magic(4);\n[a, b] = size(M);\ndisp(a);\ndisp(b);\n");

    ASSERT_EQ(g.nodes.size(), 4u);
    EXPECT_EQ(g.nodes[1].kind, graph::NodeKind::Assignment);
    EXPECT_EQ(g.nodes[1].outputs, (std::vector<std::string>{"a", "b"}));
    EXPECT_EQ(g.nodes[1].inputs,  (std::vector<std::string>{"M"}));

    // Both disp() reads should wire from node 1.
    EXPECT_EQ(g.nodes[2].inputs, (std::vector<std::string>{"a"}));
    EXPECT_EQ(g.nodes[3].inputs, (std::vector<std::string>{"b"}));
    EXPECT_EQ(countDataEdges(g, "M"), 1);
    EXPECT_EQ(countDataEdges(g, "a"), 1);
    EXPECT_EQ(countDataEdges(g, "b"), 1);
}

// ── Multi-LHS with ~ (ignored slot) ────────────────────────────────

TEST(GraphLowering, MultiAssignIgnoresTildeSlots)
{
    // M = magic(4); [~, b] = size(M); disp(b);
    // Pre-assign M so it gets recorded as a known producer.
    auto g = lowerSource("M = magic(4);\n[~, b] = size(M);\ndisp(b);\n");
    ASSERT_EQ(g.nodes.size(), 3u);
    EXPECT_EQ(g.nodes[1].outputs, (std::vector<std::string>{"~", "b"}));
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

TEST(GraphLowering, UndefinedReadIsNotRecordedAsInput)
{
    // y = z + 1;   (z never assigned anywhere — built-in / workspace /
    // typo). Under the strict "known-producer-only" rule, z is NOT
    // recorded as an input. The user sees a node `y = z + 1` with no
    // input ports — they can read the source text to spot z.
    //
    // Trade-off documented: this hides workspace vars / function
    // params not seen as producers. Phase 2 will add function-param
    // tracking which registers params as producers at function entry.
    auto g = lowerSource("y = z + 1;\n");
    ASSERT_EQ(g.nodes.size(), 1u);
    EXPECT_TRUE(g.nodes[0].inputs.empty());
    EXPECT_TRUE(g.edges.empty());
}

TEST(GraphLowering, BuiltinFunctionNamesNotInputs)
{
    // y = sin(x) + cos(x);   sin/cos are functions, not variables.
    // x is known via prior assignment → should be the only input.
    auto g = lowerSource("x = 1;\ny = sin(x) + cos(x);\n");
    ASSERT_EQ(g.nodes.size(), 2u);
    EXPECT_EQ(g.nodes[1].inputs, (std::vector<std::string>{"x"}));
    // No sin / cos as inputs (function names, never producers).
}

TEST(GraphLowering, BuiltinConstantsLikePiNotInputs)
{
    // theta = pi / 3;  beta = 2 * pi * d * cos(theta);
    // pi is a built-in constant (bare IDENTIFIER), not a variable.
    // Pre-assign d for it to appear as input on the second statement.
    auto g = lowerSource("theta = pi / 3;\nd = 0.5;\nbeta = 2 * pi * d * cos(theta);\n");
    ASSERT_EQ(g.nodes.size(), 3u);
    // theta = pi / 3 — pi is built-in → no inputs recorded.
    EXPECT_TRUE(g.nodes[0].inputs.empty());
    // beta = 2 * pi * d * cos(theta) — only d, theta (pi + cos
    // filtered as built-ins). Order: encountered = d, then theta.
    EXPECT_EQ(g.nodes[2].inputs, (std::vector<std::string>{"d", "theta"}));
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

TEST(GraphLowering, SourceTextStripsTrailingComment)
{
    // Comments are dropped from sourceText (we feed COMMENT-token
    // positions from the lexer into lowering). The raw source line
    // has `% element spacing in wavelengths` trailing; sourceText
    // must end with `d = 0.5;` exactly (no `%`, no trailing
    // whitespace).
    auto g = lowerSource("d = 0.5;  % element spacing in wavelengths\n");
    ASSERT_EQ(g.nodes.size(), 1u);
    EXPECT_EQ(g.nodes[0].sourceText, "d = 0.5;");
}

TEST(GraphLowering, SourceTextPreservesPercentInsideStrings)
{
    // The lexer's COMMENT emission already handled the string-vs-comment
    // distinction (quote-tracking, transpose disambiguation). `% loss`
    // inside a single-quote string is NOT a COMMENT token, so our
    // sliceLine sees no cut col and the whole RHS survives.
    auto g = lowerSource("msg = 'foo % loss';\n");
    ASSERT_EQ(g.nodes.size(), 1u);
    EXPECT_EQ(g.nodes[0].sourceText, "msg = 'foo % loss';");
}

TEST(GraphLowering, MultipleStatementsOnOneLineGetSeparateSlices)
{
    // Three statements separated by `;` on a single source line —
    // each node's sourceText must contain ONLY its own statement,
    // not the whole shared line. The block iterator caps each slice
    // at the next sibling's start column.
    auto g = lowerSource("subplot(1,3,1); imshow(rgb); title('Original RGB');\n");
    ASSERT_EQ(g.nodes.size(), 3u);
    EXPECT_EQ(g.nodes[0].sourceText, "subplot(1,3,1);");
    EXPECT_EQ(g.nodes[1].sourceText, "imshow(rgb);");
    EXPECT_EQ(g.nodes[2].sourceText, "title('Original RGB');");
}

TEST(GraphLowering, MultipleStatementsOneLineInsideForBody)
{
    // Same multi-statement slicing must apply inside a region body.
    auto g = lowerSource("for k=1:3\n  a=k; b=k+1;\nend\n");
    ASSERT_EQ(g.nodes.size(), 3u);
    EXPECT_EQ(g.nodes[1].sourceText, "a=k;");
    EXPECT_EQ(g.nodes[2].sourceText, "b=k+1;");
}

TEST(GraphLowering, CompactForLoopOneLineSliceClean)
{
    // Compact `for ... end` on a single line — each child stmt's
    // endCol comes from the parser, and the region header slice is
    // capped at body[0].col, so all three nodes get clean text:
    //   region:  "for k=1:3"   (trailing `,` stripped)
    //   a=k:     "a=k;"
    //   b=k+1:   "b=k+1;"
    auto g = lowerSource("for k=1:3, a=k; b=k+1; end\n");
    ASSERT_EQ(g.nodes.size(), 3u);
    EXPECT_EQ(g.nodes[0].kind, graph::NodeKind::ForRegion);
    EXPECT_EQ(g.nodes[0].sourceText, "for k=1:3");
    EXPECT_EQ(g.nodes[1].sourceText, "a=k;");
    EXPECT_EQ(g.nodes[2].sourceText, "b=k+1;");
}

// ── Phase 2a: control-flow region recursion ───────────────────────

TEST(GraphLowering, IfRegionRecursesIntoBodies)
{
    // x = 1; if x > 0; y = 2; else; y = 3; end; disp(y);
    auto g = lowerSource("x = 1;\nif x > 0\n  y = 2;\nelse\n  y = 3;\nend\ndisp(y);\n");
    // 6 nodes: x=1, IfRegion, y=2, y=3, Merge(y), disp(y).
    // (Phase 2c: the post-branch Merge sits between body assignments
    //  and disp(y) so the read routes through it.)
    ASSERT_EQ(g.nodes.size(), 6u);
    EXPECT_EQ(g.nodes[0].kind, graph::NodeKind::Assignment);   // x = 1
    EXPECT_EQ(g.nodes[1].kind, graph::NodeKind::IfRegion);
    EXPECT_EQ(g.nodes[2].kind, graph::NodeKind::Assignment);   // y = 2
    EXPECT_EQ(g.nodes[3].kind, graph::NodeKind::Assignment);   // y = 3
    EXPECT_EQ(g.nodes[4].kind, graph::NodeKind::Merge);        // φ(y)
    EXPECT_EQ(g.nodes[5].kind, graph::NodeKind::ExprStmt);     // disp(y)

    // IfRegion has x as input + children for both branches.
    EXPECT_EQ(g.nodes[1].inputs, (std::vector<std::string>{"x"}));
    EXPECT_EQ(g.nodes[1].childNodeIds, (std::vector<int>{2, 3}));
    EXPECT_EQ(g.nodes[1].branchPartitions, (std::vector<int>{0, 1, 2}));

    // Body children get parentRegionId = IfRegion's id.
    EXPECT_EQ(g.nodes[2].parentRegionId, std::optional<int>(1));
    EXPECT_EQ(g.nodes[3].parentRegionId, std::optional<int>(1));

    // disp(y) reads y → routes through the Merge node (NOT directly
    // from y=3 as it did under the old last-writer-wins approximation).
    EXPECT_EQ(g.nodes[5].inputs, (std::vector<std::string>{"y"}));
}

TEST(GraphLowering, ForRegionRegistersIterVarAsProducer)
{
    auto g = lowerSource("s = 0;\nfor k = 1:10\n  disp(k);\nend\ndisp(s);\ndisp(k);\n");
    // 5 nodes: s=0, ForRegion, disp(k) inside, disp(s) after, disp(k) after
    ASSERT_EQ(g.nodes.size(), 5u);
    EXPECT_EQ(g.nodes[1].kind, graph::NodeKind::ForRegion);
    EXPECT_EQ(g.nodes[1].outputs, (std::vector<std::string>{"k"}));
    // disp(k) INSIDE body
    EXPECT_EQ(g.nodes[2].inputs, (std::vector<std::string>{"k"}));
    EXPECT_EQ(g.nodes[2].parentRegionId, std::optional<int>(1));
    // disp(k) AFTER loop — k still visible (MATLAB semantic).
    EXPECT_EQ(g.nodes[4].inputs, (std::vector<std::string>{"k"}));
    // Two `k` edges: ForRegion → disp(k)inside + ForRegion → disp(k)after.
    EXPECT_EQ(countDataEdges(g, "k"), 2);
}

TEST(GraphLowering, WhileRegionHasNoIterVar)
{
    auto g = lowerSource("x = 0;\nwhile x < 10\n  x = x + 1;\nend\ndisp(x);\n");
    ASSERT_GE(g.nodes.size(), 4u);
    EXPECT_EQ(g.nodes[1].kind, graph::NodeKind::WhileRegion);
    EXPECT_TRUE(g.nodes[1].outputs.empty());
    EXPECT_EQ(g.nodes[1].inputs, (std::vector<std::string>{"x"}));
}

TEST(GraphLowering, SwitchRegionPartitionsCases)
{
    auto g = lowerSource(
        "x = 1;\n"
        "switch x\n"
        "  case 1, y = 10;\n"
        "  case 2, y = 20;\n"
        "  otherwise, y = 0;\n"
        "end\ndisp(y);\n");
    // 7 nodes: x=1, SwitchRegion, y=10, y=20, y=0, Merge(y), disp(y).
    ASSERT_EQ(g.nodes.size(), 7u);
    EXPECT_EQ(g.nodes[1].kind, graph::NodeKind::SwitchRegion);
    EXPECT_EQ(g.nodes[1].inputs, (std::vector<std::string>{"x"}));
    EXPECT_EQ(g.nodes[1].branchPartitions, (std::vector<int>{0, 1, 2, 3}));
    EXPECT_EQ(g.nodes[1].childNodeIds.size(), 3u);
    // Merge sits at parent scope, NOT inside the SwitchRegion.
    EXPECT_EQ(g.nodes[5].kind, graph::NodeKind::Merge);
    EXPECT_FALSE(g.nodes[5].parentRegionId.has_value());
}

TEST(GraphLowering, TryRegionCatchVarVisibleOnlyInCatchScope)
{
    auto g = lowerSource(
        "try\n  x = 1;\ncatch ME\n  disp(ME);\nend\ndisp(ME);\n");
    int tryRegionId = -1;
    for (size_t i = 0; i < g.nodes.size(); ++i) {
        if (g.nodes[i].kind == graph::NodeKind::TryRegion) { tryRegionId = (int)i; break; }
    }
    ASSERT_NE(tryRegionId, -1);
    auto &outs = g.nodes[tryRegionId].outputs;
    EXPECT_NE(std::find(outs.begin(), outs.end(), "ME"), outs.end());
    // disp(ME) inside catch → 1 edge. disp(ME) outside → 0 edges
    // (ME out of scope after end).
    EXPECT_EQ(countDataEdges(g, "ME"), 1);
}

TEST(GraphLowering, NestedForRegionsShadowIterVar)
{
    auto g = lowerSource(
        "for k = 1:3\n"
        "  for k = 1:5\n"
        "    disp(k);\n"
        "  end\n"
        "  disp(k);\n"
        "end\n");
    int innerFor = -1, outerFor = -1;
    for (size_t i = 0; i < g.nodes.size(); ++i) {
        if (g.nodes[i].kind == graph::NodeKind::ForRegion) {
            if (outerFor < 0) outerFor = (int)i;
            else              innerFor = (int)i;
        }
    }
    ASSERT_GE(outerFor, 0);
    ASSERT_GE(innerFor, 0);
    int innerDisp = -1, outerDisp = -1;
    for (size_t i = 0; i < g.nodes.size(); ++i) {
        if (g.nodes[i].kind != graph::NodeKind::ExprStmt) continue;
        if (g.nodes[i].parentRegionId == innerFor)      innerDisp = (int)i;
        else if (g.nodes[i].parentRegionId == outerFor) outerDisp = (int)i;
    }
    ASSERT_GE(innerDisp, 0);
    ASSERT_GE(outerDisp, 0);
    bool sawInnerEdge = false, sawOuterEdge = false;
    for (const auto &e : g.edges) {
        if (e.varName != "k") continue;
        if (e.source.nodeId == innerFor && e.target.nodeId == innerDisp) sawInnerEdge = true;
        if (e.source.nodeId == outerFor && e.target.nodeId == outerDisp) sawOuterEdge = true;
    }
    EXPECT_TRUE(sawInnerEdge);
    EXPECT_TRUE(sawOuterEdge);
}

// ── Phase 2c: merge (φ) nodes at branched join points ─────────────

TEST(GraphLowering, IfElseEmitsMergeNodeForBranchedVar)
{
    // x = 0; if x > 0; y = 1; else; y = 2; end; disp(y);
    // Expected: 6 nodes — x=0, IfRegion, y=1, y=2, Merge(y), disp(y).
    // disp(y) reads y via the Merge, NOT directly from y=2 (the old
    // last-writer-wins approximation).
    auto g = lowerSource("x = 0;\nif x > 0\n  y = 1;\nelse\n  y = 2;\nend\ndisp(y);\n");
    ASSERT_EQ(g.nodes.size(), 6u);
    int mergeId = firstNodeOfKind(g, graph::NodeKind::Merge);
    ASSERT_GE(mergeId, 0);
    EXPECT_EQ(g.nodes[mergeId].outputs, (std::vector<std::string>{"y"}));
    // Two inputs into the merge — one per branch.
    EXPECT_EQ(g.nodes[mergeId].inputs.size(), 2u);
    // disp(y) wires from the merge, not directly from y=2.
    int dispId = -1;
    for (size_t i = 0; i < g.nodes.size(); ++i) {
        if (g.nodes[i].kind == graph::NodeKind::ExprStmt
         && g.nodes[i].sourceText.find("disp") != std::string::npos) {
            dispId = (int)i; break;
        }
    }
    ASSERT_GE(dispId, 0);
    bool wiredFromMerge = false;
    for (const auto &e : g.edges) {
        if (e.varName == "y" && e.target.nodeId == dispId) {
            EXPECT_EQ(e.source.nodeId, mergeId);
            wiredFromMerge = true;
        }
    }
    EXPECT_TRUE(wiredFromMerge);
}

TEST(GraphLowering, IfWithoutElseMergeHasFallThroughSlot)
{
    // y = 0; if x > 0; y = 1; end; disp(y);
    // No `else` → the merge needs a fall-through input from the
    // pre-region y (`y = 0`). Merge has TWO inputs: the if-branch
    // writer and the pre-region producer.
    auto g = lowerSource("x = 0;\ny = 0;\nif x > 0\n  y = 1;\nend\ndisp(y);\n");
    int mergeId = firstNodeOfKind(g, graph::NodeKind::Merge);
    ASSERT_GE(mergeId, 0);
    EXPECT_EQ(g.nodes[mergeId].inputs.size(), 2u);  // 1 branch + 1 fall-through
    // Two `y` edges land on the merge — from y=1 (branch) and y=0 (pre).
    int edgesToMerge = 0;
    for (const auto &e : g.edges) {
        if (e.varName == "y" && e.target.nodeId == mergeId) ++edgesToMerge;
    }
    EXPECT_EQ(edgesToMerge, 2);
}

TEST(GraphLowering, SwitchEmitsMergeForCaseAssignedVar)
{
    auto g = lowerSource(
        "x = 1;\n"
        "switch x\n"
        "  case 1, y = 10;\n"
        "  case 2, y = 20;\n"
        "  otherwise, y = 0;\n"
        "end\ndisp(y);\n");
    int mergeId = firstNodeOfKind(g, graph::NodeKind::Merge);
    ASSERT_GE(mergeId, 0);
    EXPECT_EQ(g.nodes[mergeId].outputs, (std::vector<std::string>{"y"}));
    // Three case-branches → 3 merge inputs, no fall-through (otherwise covers all).
    EXPECT_EQ(g.nodes[mergeId].inputs.size(), 3u);
}

TEST(GraphLowering, TryCatchEmitsMergeForBothBranchAssignments)
{
    auto g = lowerSource("try\n  y = 1;\ncatch\n  y = 2;\nend\ndisp(y);\n");
    int mergeId = firstNodeOfKind(g, graph::NodeKind::Merge);
    ASSERT_GE(mergeId, 0);
    EXPECT_EQ(g.nodes[mergeId].outputs, (std::vector<std::string>{"y"}));
    EXPECT_EQ(g.nodes[mergeId].inputs.size(), 2u);
}

TEST(GraphLowering, NoMergeWhenVarAssignedInOnlyOneBranch_LegacyNote)
{
    // Phase 2a behavior was: write in any branch sets lastProducer.
    // Phase 2c semantics: even if ONLY one branch writes the var,
    // we still emit a merge with a fall-through input — because
    // semantically the other branch leaves the pre-region value
    // intact, which is a different φ-source.
    auto g = lowerSource("y = 0;\nif x > 0\n  y = 1;\nend\ndisp(y);\n");
    // Merge expected (covered by IfWithoutElseMergeHasFallThroughSlot).
    EXPECT_GE(firstNodeOfKind(g, graph::NodeKind::Merge), 0);
}

TEST(GraphLowering, BreakAndContinueAreOwnNodes)
{
    auto g = lowerSource("for k = 1:10\n  if k > 5\n    break;\n  end\n  continue;\nend\n");
    int brk = -1, cnt = -1;
    for (size_t i = 0; i < g.nodes.size(); ++i) {
        if (g.nodes[i].kind == graph::NodeKind::JumpBreak)    brk = (int)i;
        if (g.nodes[i].kind == graph::NodeKind::JumpContinue) cnt = (int)i;
    }
    EXPECT_GE(brk, 0);
    EXPECT_GE(cnt, 0);
}
