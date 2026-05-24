// libs/graph/include/numkit/graph/node_graph.hpp
//
// NodeGraph IR — statement-level data-flow graph derived from an AST.
// Lives in libs/graph (an offline analysis pass, NOT part of the
// interpreter's eval pipeline). Consumed by:
//   • libs/graph/src/serialize.cpp  → JSON for the IDE
//   • libs/graph/tests/             → unit tests
//
// Design contract (MVP):
//   • Granularity: one node per top-level statement (assignment,
//     expression statement, control-flow region root, jump).
//   • Variable reassignment uses SSA-style versioning behind the
//     scenes — LoweringState.lastProducer tracks the latest node
//     producing each variable name. UI labels stay flat names.
//   • Edges carry the variable name (for data) or are kind-typed
//     (sequence / jump / exception).
//   • Control-flow nodes are REGIONS that own child node ids;
//     branches (if/elseif/else, switch cases) are tracked via
//     branchPartitions — index boundaries inside childNodeIds.
//
// See docs/plans/graph_view_mvp.md (TBD) for the full spec.

#pragma once

#include <optional>
#include <string>
#include <vector>

namespace numkit::graph {

enum class NodeKind {
    Assignment,       // lhs = rhs;
    ExprStmt,         // statement without LHS (side-effect call etc.)
    IfRegion,         // if/elseif/else
    ForRegion,
    WhileRegion,
    SwitchRegion,
    TryRegion,
    JumpContinue,
    JumpBreak,
    JumpReturn,
    Merge,            // φ-equivalent (explicit-mode only)
    GlobalDecl,       // global X
    PersistentDecl,   // persistent Y
    FunctionDef,      // function file: each function gets one
};

/** A handle to one input/output port on a Node. */
struct Port {
    int nodeId    = -1;
    int portIndex = 0;
    std::string name;   // human-readable label (variable name)
};

enum class EdgeKind {
    Data,       // value flows source → target
    Sequence,   // execution order (side-effect statements)
    Jump,       // continue / break / return target
    Exception,  // throw → catch (or function-exit when no try)
};

struct Edge {
    Port source;
    Port target;
    EdgeKind kind = EdgeKind::Data;
    std::string varName;  // for data edges
};

struct Node {
    int id = -1;
    NodeKind kind = NodeKind::ExprStmt;

    // Source mapping — for biject navigation with the text editor.
    int sourceLine = 0;
    int sourceCol  = 0;
    int endLine    = 0;  // for regions: the line of the closing `end`

    // Original text. For Assignment: just the RHS. For ExprStmt:
    // the whole statement. For region roots: the header (e.g.
    // `if x > 0` without the body). UI displays this verbatim.
    std::string sourceText;

    // Port names. For Assignment: outputs = LHS names; inputs =
    // free-vars on the RHS. For ExprStmt: inputs only.
    std::vector<std::string> inputs;
    std::vector<std::string> outputs;

    // Leading comment lines just above the statement (helpful for UI
    // hints). May be empty.
    std::string leadingComment;

    // Region nesting: if this node sits inside an enclosing region
    // (if/for/while/switch/try), parentRegionId points to it.
    std::optional<int> parentRegionId;

    // Region-specific: ids of nodes hosted INSIDE this region. For
    // if/switch the order matches branchPartitions (each partition
    // boundary marks a new branch / case).
    std::vector<int> childNodeIds;
    // For branched regions (if, switch) — partition boundaries inside
    // childNodeIds. E.g. branchPartitions = [0, 3, 5] means branch 0
    // covers childNodeIds[0..3), branch 1 covers [3..5).
    std::vector<int> branchPartitions;
};

struct NodeGraph {
    std::vector<Node> nodes;
    std::vector<Edge> edges;

    // For function files this is the function name; for script files,
    // "<script>". For function files with locals, callers build one
    // NodeGraph per function and key by name.
    std::string functionName = "<script>";
    std::vector<std::string> functionInputs;
    std::vector<std::string> functionOutputs;
};

} // namespace numkit::graph
