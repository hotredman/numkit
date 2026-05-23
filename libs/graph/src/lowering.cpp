// libs/graph/src/lowering.cpp
//
// AST → NodeGraph lowering. Phase-1 scope:
//   • Top-level BLOCK iteration.
//   • Assignment (single LHS, multi LHS via MULTI_ASSIGN, indexed
//     LHS via CALL/CELL_INDEX, field LHS via FIELD_ACCESS).
//   • ExprStmt (statements without LHS).
//   • Data edges via lastProducer map.
//
// Phase-2+ adds control-flow regions (IF/FOR/WHILE/SWITCH/TRY) and
// jump nodes. Until then those statement kinds are emitted as opaque
// ExprStmt nodes whose sourceText is the header line — they show up
// in the graph but their bodies don't get walked. Good-enough for
// scripts without nested control flow during the proof-of-concept.

#include <numkit/graph/lowering.hpp>

#include <algorithm>
#include <functional>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace numkit::graph {
namespace {

// ── Helpers ──────────────────────────────────────────────────────────

/** Best-effort source-text slice. Lines/cols are 1-indexed. Returns
 *  empty when the slice is out of range. For Phase 1 we slice just
 *  the start line — multi-line statements show only their first line
 *  in the node body; full slicing comes when we wire endLine for
 *  every node. */
std::string sliceLine(const std::string &source, int line)
{
    if (line <= 0 || source.empty()) return {};
    int curLine = 1;
    size_t start = 0;
    while (start < source.size() && curLine < line) {
        if (source[start] == '\n') ++curLine;
        ++start;
    }
    if (curLine != line) return {};
    size_t end = source.find('\n', start);
    if (end == std::string::npos) end = source.size();
    // Strip trailing \r (Windows line endings).
    while (end > start && (source[end - 1] == '\r')) --end;
    return source.substr(start, end - start);
}

/** Walk an expression subtree and collect identifier names that are
 *  READS (right-hand-side uses), in source order, deduplicated. Used
 *  to wire data edges from a statement's RHS to its lastProducer
 *  ancestors.
 *
 *  Subtleties:
 *    • IDENTIFIER → record.
 *    • CALL / CELL_INDEX → the head identifier is parser-ambiguous:
 *      could be a function call (`plot(y)` — plot is a function name,
 *      not a read) OR array/cell indexing of a local variable
 *      (`A(i)` — A is a read of the previously-assigned variable).
 *      `isKnownVar(name)` resolves the ambiguity — if the name is
 *      already a known producer in scope, treat it as a variable
 *      read; otherwise skip it (it's a function name).
 *      Args (children[1..N]) recurse normally regardless.
 *    • FIELD_ACCESS → recurse into the object child (which is a read).
 *    • Literals / END_VAL → no reads.
 *    • ANON_FUNC → opaque for MVP (its captures handled in v2). */
void collectReads(const ASTNode *node,
                  std::unordered_set<std::string> &seen,
                  std::vector<std::string> &order,
                  const std::function<bool(const std::string &)> &isKnownVar)
{
    if (!node) return;
    if (node->type == NodeType::IDENTIFIER) {
        const auto &name = node->strValue;
        if (!name.empty() && seen.insert(name).second) order.push_back(name);
        return;
    }
    if (node->type == NodeType::CALL
     || node->type == NodeType::CELL_INDEX
     || node->type == NodeType::INDEX) {
        if (!node->children.empty()) {
            const auto &head = node->children[0];
            if (head->type == NodeType::IDENTIFIER) {
                // Ambiguous head — record only if known variable.
                const auto &headName = head->strValue;
                if (!headName.empty() && isKnownVar(headName)) {
                    if (seen.insert(headName).second) order.push_back(headName);
                }
            } else {
                // Compound head (e.g. namespace.fn, struct.method) —
                // recurse so any base identifiers get picked up.
                collectReads(head.get(), seen, order, isKnownVar);
            }
            for (size_t i = 1; i < node->children.size(); ++i) {
                collectReads(node->children[i].get(), seen, order, isKnownVar);
            }
        }
        return;
    }
    // Skip literals / end-val / anon-func bodies (the latter is its
    // own scope; for MVP we treat its captures as opaque).
    if (node->type == NodeType::NUMBER_LITERAL
     || node->type == NodeType::IMAG_LITERAL
     || node->type == NodeType::STRING_LITERAL
     || node->type == NodeType::DQSTRING_LITERAL
     || node->type == NodeType::BOOL_LITERAL
     || node->type == NodeType::END_VAL
     || node->type == NodeType::ANON_FUNC) {
        return;
    }
    for (const auto &c : node->children) collectReads(c.get(), seen, order, isKnownVar);
}

std::vector<std::string> collectReads(
        const ASTNode *node,
        const std::function<bool(const std::string &)> &isKnownVar)
{
    std::unordered_set<std::string> seen;
    std::vector<std::string> order;
    collectReads(node, seen, order, isKnownVar);
    return order;
}

/** Extract LHS target names from an ASSIGN's children[0]. For
 *  indexed/field LHS the base identifier is BOTH a read (old value)
 *  AND a write (new version) — caller decides how to wire that.
 *  Returns the list of plain identifier names that become outputs. */
std::vector<std::string> assignLhsOutputs(const ASTNode *lhs)
{
    if (!lhs) return {};
    if (lhs->type == NodeType::IDENTIFIER) {
        return { lhs->strValue };
    }
    // Indexed assignment: the call's first child is the array name.
    // Field assignment: the field's children[0] is the object identifier.
    // Both produce a new version of the SAME root identifier.
    if (lhs->type == NodeType::CALL
     || lhs->type == NodeType::CELL_INDEX
     || lhs->type == NodeType::INDEX) {
        if (!lhs->children.empty()
         && lhs->children[0]->type == NodeType::IDENTIFIER) {
            return { lhs->children[0]->strValue };
        }
    }
    if (lhs->type == NodeType::FIELD_ACCESS
     || lhs->type == NodeType::DYNAMIC_FIELD_ACCESS) {
        // Recurse left until we hit the root identifier.
        const ASTNode *cur = lhs;
        while (cur && !cur->children.empty()
            && (cur->type == NodeType::FIELD_ACCESS
             || cur->type == NodeType::DYNAMIC_FIELD_ACCESS)) {
            cur = cur->children[0].get();
        }
        if (cur && cur->type == NodeType::IDENTIFIER) {
            return { cur->strValue };
        }
    }
    return {};
}

/** Is this LHS shape one that ALSO reads the old version (indexed,
 *  field, cell)? Plain identifier LHS doesn't. */
bool lhsIsReadModifyWrite(const ASTNode *lhs)
{
    if (!lhs) return false;
    return lhs->type == NodeType::CALL
        || lhs->type == NodeType::CELL_INDEX
        || lhs->type == NodeType::INDEX
        || lhs->type == NodeType::FIELD_ACCESS
        || lhs->type == NodeType::DYNAMIC_FIELD_ACCESS;
}

// ── Lowering state ──────────────────────────────────────────────────

struct LoweringState {
    NodeGraph graph;
    // Most recent node that produced each variable. Edges read from
    // this; assignments update it on completion.
    std::unordered_map<std::string, int> lastProducer;
    // Source text for slicing per-node sourceText.
    const std::string *source = nullptr;

    int addNode(Node n)
    {
        n.id = static_cast<int>(graph.nodes.size());
        graph.nodes.push_back(std::move(n));
        return graph.nodes.back().id;
    }

    /** Wire one data edge from `producer` (-1 = no known producer)
     *  to (targetNode, targetPort). When producer is missing we
     *  emit no edge — UI treats the input as an undefined / function-
     *  parameter source. */
    void addDataEdge(int producerId, int targetId, int targetPortIdx,
                     const std::string &varName)
    {
        if (producerId < 0) return;
        Edge e;
        e.source = { producerId, /*portIndex*/ 0, varName };
        e.target = { targetId, targetPortIdx, varName };
        e.kind = EdgeKind::Data;
        e.varName = varName;
        graph.edges.push_back(std::move(e));
    }
};

// ── Statement lowering ──────────────────────────────────────────────

void lowerStatement(LoweringState &S, const ASTNode &stmt);

void lowerAssign(LoweringState &S, const ASTNode &stmt)
{
    // children[0] = LHS, children[1] = RHS (for ASSIGN);
    // MULTI_ASSIGN has returnNames + children[0] = RHS.
    Node n;
    n.kind = NodeKind::Assignment;
    n.sourceLine = stmt.line;
    n.sourceCol  = stmt.col;
    n.sourceText = S.source ? sliceLine(*S.source, stmt.line) : "";

    const ASTNode *lhs = nullptr;
    const ASTNode *rhs = nullptr;
    if (stmt.type == NodeType::MULTI_ASSIGN) {
        n.outputs = stmt.returnNames;
        rhs = stmt.children.empty() ? nullptr : stmt.children[0].get();
    } else {
        lhs = stmt.children.size() > 0 ? stmt.children[0].get() : nullptr;
        rhs = stmt.children.size() > 1 ? stmt.children[1].get() : nullptr;
        n.outputs = assignLhsOutputs(lhs);
    }

    // Inputs: free vars from RHS. For read-modify-write LHS shapes
    // (indexed / field), ALSO include the root identifier — it's
    // read before the new version is written.
    auto isKnownVar = [&](const std::string &name) {
        return S.lastProducer.count(name) > 0;
    };
    n.inputs = collectReads(rhs, isKnownVar);
    if (lhs && lhsIsReadModifyWrite(lhs)) {
        // Add the root name if it's not already present from RHS.
        auto lhsRoots = assignLhsOutputs(lhs);
        for (const auto &name : lhsRoots) {
            if (std::find(n.inputs.begin(), n.inputs.end(), name) == n.inputs.end()) {
                n.inputs.push_back(name);
            }
        }
    }
    // For indexed LHS, also walk the index expressions for free vars.
    if (lhs && (lhs->type == NodeType::CALL
             || lhs->type == NodeType::CELL_INDEX
             || lhs->type == NodeType::INDEX)) {
        for (size_t i = 1; i < lhs->children.size(); ++i) {
            for (const auto &name : collectReads(lhs->children[i].get(), isKnownVar)) {
                if (std::find(n.inputs.begin(), n.inputs.end(), name) == n.inputs.end()) {
                    n.inputs.push_back(name);
                }
            }
        }
    }

    int nid = S.addNode(std::move(n));

    // Wire data edges from lastProducer of each input.
    auto &nodeRef = S.graph.nodes[nid];
    for (size_t i = 0; i < nodeRef.inputs.size(); ++i) {
        const auto &varName = nodeRef.inputs[i];
        auto it = S.lastProducer.find(varName);
        if (it != S.lastProducer.end()) {
            S.addDataEdge(it->second, nid, static_cast<int>(i), varName);
        }
    }

    // Update lastProducer for every output. `~` slots in MULTI_ASSIGN
    // are skipped (they're ignored sinks in MATLAB).
    for (const auto &name : nodeRef.outputs) {
        if (name.empty() || name == "~") continue;
        S.lastProducer[name] = nid;
    }
}

void lowerExprStmt(LoweringState &S, const ASTNode &stmt)
{
    Node n;
    n.kind = NodeKind::ExprStmt;
    n.sourceLine = stmt.line;
    n.sourceCol  = stmt.col;
    n.sourceText = S.source ? sliceLine(*S.source, stmt.line) : "";
    const ASTNode *expr = stmt.children.empty() ? nullptr : stmt.children[0].get();
    auto isKnownVar = [&](const std::string &name) {
        return S.lastProducer.count(name) > 0;
    };
    n.inputs = collectReads(expr, isKnownVar);

    int nid = S.addNode(std::move(n));
    auto &nodeRef = S.graph.nodes[nid];
    for (size_t i = 0; i < nodeRef.inputs.size(); ++i) {
        const auto &varName = nodeRef.inputs[i];
        auto it = S.lastProducer.find(varName);
        if (it != S.lastProducer.end()) {
            S.addDataEdge(it->second, nid, static_cast<int>(i), varName);
        }
    }
}

void lowerGlobalDecl(LoweringState &S, const ASTNode &stmt, NodeKind kind)
{
    // `global X Y` / `persistent X` — declaration nodes. Names live
    // on returnNames in our AST (paramNames in some builds — both
    // are checked).
    Node n;
    n.kind = kind;
    n.sourceLine = stmt.line;
    n.sourceCol  = stmt.col;
    n.sourceText = S.source ? sliceLine(*S.source, stmt.line) : "";
    // Decl names live on paramNames (the parser stores them there).
    n.outputs = stmt.paramNames.empty() ? stmt.returnNames : stmt.paramNames;
    int nid = S.addNode(std::move(n));
    auto &nodeRef = S.graph.nodes[nid];
    // These names become available for subsequent reads — register
    // each as a producer rooted at this decl node.
    for (const auto &name : nodeRef.outputs) {
        if (!name.empty()) S.lastProducer[name] = nid;
    }
}

void lowerStatement(LoweringState &S, const ASTNode &stmt)
{
    switch (stmt.type) {
    case NodeType::ASSIGN:
    case NodeType::MULTI_ASSIGN:
        lowerAssign(S, stmt);
        return;
    case NodeType::EXPR_STMT:
        lowerExprStmt(S, stmt);
        return;
    case NodeType::GLOBAL_STMT:
        lowerGlobalDecl(S, stmt, NodeKind::GlobalDecl);
        return;
    case NodeType::PERSISTENT_STMT:
        lowerGlobalDecl(S, stmt, NodeKind::PersistentDecl);
        return;
    // Phase-2 territory — emit as opaque ExprStmt for now so the
    // node is visible in the graph (sourceText = the header line).
    // Body recursion + region wiring lands in the next phase.
    case NodeType::IF_STMT:
    case NodeType::FOR_STMT:
    case NodeType::WHILE_STMT:
    case NodeType::SWITCH_STMT:
    case NodeType::TRY_STMT:
    case NodeType::BREAK_STMT:
    case NodeType::CONTINUE_STMT:
    case NodeType::RETURN_STMT:
    case NodeType::FUNCTION_DEF:
        lowerExprStmt(S, stmt);
        return;
    default:
        // Unrecognised — skip rather than crash. Phase-2 may turn
        // this into an assertion once every statement kind is covered.
        return;
    }
}

} // namespace

NodeGraph lowerScript(const ASTNode &root, const std::string &sourceText)
{
    LoweringState S;
    S.source = sourceText.empty() ? nullptr : &sourceText;
    S.graph.functionName = "<script>";

    // Top-level parse() returns a BLOCK whose children are the
    // statements. Some callers may hand us a single statement
    // directly — handle both.
    if (root.type == NodeType::BLOCK) {
        for (const auto &child : root.children) {
            if (child) lowerStatement(S, *child);
        }
    } else {
        lowerStatement(S, root);
    }

    return std::move(S.graph);
}

} // namespace numkit::graph
