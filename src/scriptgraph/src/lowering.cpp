// scriptgraph/src/lowering.cpp
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

#include <numkit/scriptgraph/lowering.hpp>

#include <algorithm>
#include <functional>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace numkit::scriptgraph {
namespace {

// ── Helpers ──────────────────────────────────────────────────────────

/** Slice a single source line from `startCol` (inclusive) to
 *  `endCol` (exclusive). Both 1-indexed; 0 / out-of-range values mean
 *  "no bound on that side". Trailing whitespace after the right cut
 *  is trimmed so multi-statement lines like `a=1; b=2; c=3;` slice
 *  to clean `a=1;`, `b=2;`, `c=3;` segments without straggler spaces. */
std::string sliceLine(const std::string &source, int line,
                      int startCol = 1, int endCol = 0)
{
    if (line <= 0 || source.empty()) return {};
    int curLine = 1;
    size_t lineStart = 0;
    while (lineStart < source.size() && curLine < line) {
        if (source[lineStart] == '\n') ++curLine;
        ++lineStart;
    }
    if (curLine != line) return {};
    size_t lineEnd = source.find('\n', lineStart);
    if (lineEnd == std::string::npos) lineEnd = source.size();
    while (lineEnd > lineStart && source[lineEnd - 1] == '\r') --lineEnd;
    size_t start = lineStart + static_cast<size_t>(std::max(0, startCol - 1));
    if (start > lineEnd) start = lineEnd;
    size_t end = lineEnd;
    if (endCol > 0) {
        size_t capPos = lineStart + static_cast<size_t>(endCol - 1);
        if (capPos < end) end = capPos;
        while (end > start && (source[end - 1] == ' ' || source[end - 1] == '\t')) --end;
    }
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
        // Only record an IDENTIFIER as a variable read when it's a
        // KNOWN producer in scope (set by an earlier assignment,
        // global/persistent decl, or — eventually — a function
        // parameter). Filters out built-in function names + constants
        // (`pi`, `cos`, `eps`, `clear`) that appear as bare
        // identifiers but aren't data-flow sources. Same rule we
        // already apply to CALL/CELL_INDEX/INDEX head identifiers;
        // generalised here to ALL identifiers so the graph only shows
        // real data dependencies.
        const auto &name = node->strValue;
        if (!name.empty() && isKnownVar(name) && seen.insert(name).second) {
            order.push_back(name);
        }
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

/** Walk an AST subtree and collect names of variables ASSIGNED
 *  anywhere inside (ASSIGN, MULTI_ASSIGN, for-iter vars, global /
 *  persistent decls). Used to identify loop-carried variables —
 *  the set of names a loop body writes to, which combined with the
 *  pre-loop producer state tells us which need explicit φ-nodes
 *  at the loop header (Phase 2e). */
void collectAssignedNames(const ASTNode *node, std::set<std::string> &out)
{
    if (!node) return;
    if (node->type == NodeType::ASSIGN) {
        if (!node->children.empty()) {
            for (const auto &n : assignLhsOutputs(node->children[0].get())) {
                out.insert(n);
            }
        }
        // RHS could contain anon-funcs etc; recursing into it is safe.
        for (size_t i = 1; i < node->children.size(); ++i) {
            collectAssignedNames(node->children[i].get(), out);
        }
        return;
    }
    if (node->type == NodeType::MULTI_ASSIGN) {
        for (const auto &n : node->returnNames) {
            if (!n.empty() && n != "~") out.insert(n);
        }
        for (const auto &c : node->children) collectAssignedNames(c.get(), out);
        return;
    }
    if (node->type == NodeType::FOR_STMT && !node->strValue.empty()) {
        out.insert(node->strValue);  // iter var is an assignment too
    }
    if ((node->type == NodeType::GLOBAL_STMT
      || node->type == NodeType::PERSISTENT_STMT)) {
        for (const auto &n : node->paramNames)  out.insert(n);
        for (const auto &n : node->returnNames) out.insert(n);
    }
    if (node->type == NodeType::TRY_STMT && !node->strValue.empty()) {
        // catch var (e.g. ME) — written when the catch fires; we
        // count it so the outer loop knows the try-region modified
        // a local name even if the user never assigns to it
        // explicitly elsewhere. (It still stays scope-local thanks
        // to the catch-var leak fix in lowerTry.)
    }
    for (const auto &c : node->children) collectAssignedNames(c.get(), out);
    for (const auto &br : node->branches) {
        collectAssignedNames(br.first.get(),  out);
        collectAssignedNames(br.second.get(), out);
    }
    if (node->elseBranch) collectAssignedNames(node->elseBranch.get(), out);
}

// ── Lowering state ──────────────────────────────────────────────────

struct LoweringState {
    NodeGraph graph;
    // Most recent node that produced each variable. Edges read from
    // this; assignments update it on completion. Snapshot/restored
    // around control-flow region recursion (push/popScope helpers).
    std::unordered_map<std::string, int> lastProducer;
    // Source text for slicing per-node sourceText.
    const std::string *source = nullptr;
    // First COMMENT-token column per source line (1-indexed). Built
    // from the lexer's COMMENT-token stream. Looked up in sliceLine
    // to cut trailing `% ...` off the body text. Empty when no
    // tokens were provided (caller skipped trimming).
    std::unordered_map<int, int> firstCommentColPerLine;
    // Innermost region we're currently lowering INTO. Every node
    // emitted inherits this as its parentRegionId, and gets pushed
    // onto the region's childNodeIds. nullopt at script top-level.
    std::optional<int> currentRegion;

    int firstCommentColOnLine(int line) const {
        auto it = firstCommentColPerLine.find(line);
        return it == firstCommentColPerLine.end() ? 0 : it->second;
    }

    /** Pick the effective end-col cap for a slice on `line`. Caller
     *  passes its own explicit cap (e.g. stmt.endCol from the parser
     *  for simple statements, or body[0].col for compound headers).
     *  The trailing-comment col from the lexer is folded in via min,
     *  so a `% comment` after a statement still gets trimmed even
     *  when the parser-supplied cap would have kept it. 0 = no cap. */
    int sliceCap(int line, int explicitEndCol) const {
        int cap = explicitEndCol;
        int comm = firstCommentColOnLine(line);
        if (comm > 0 && (cap == 0 || comm < cap)) cap = comm;
        return cap;
    }

    int addNode(Node n)
    {
        n.id = static_cast<int>(graph.nodes.size());
        // Inherit current region as parent so the renderer can
        // nest us inside its compound frame. The region itself
        // records us in its childNodeIds (below).
        if (currentRegion && !n.parentRegionId) {
            n.parentRegionId = currentRegion;
        }
        graph.nodes.push_back(std::move(n));
        int id = graph.nodes.back().id;
        if (currentRegion) {
            graph.nodes[*currentRegion].childNodeIds.push_back(id);
        }
        return id;
    }

    // Scope helpers — push/pop currentRegion + snapshot/restore
    // lastProducer at region boundaries. Used by control-flow
    // lowering to keep branches isolated from each other and from
    // the enclosing scope.
    struct ScopeFrame {
        std::optional<int> savedRegion;
        std::unordered_map<std::string, int> savedProducers;
    };
    ScopeFrame enterRegion(int regionId)
    {
        ScopeFrame f{ currentRegion, lastProducer };
        currentRegion = regionId;
        return f;
    }
    void leaveRegion(ScopeFrame &f)
    {
        currentRegion = f.savedRegion;
        lastProducer = std::move(f.savedProducers);
    }

    /** Wire one data edge from `producer` (-1 = no known producer)
     *  to (targetNode, targetPort). When producer is missing we
     *  emit no edge — UI treats the input as an undefined / function-
     *  parameter source. */
    void addDataEdge(int producerId, int targetId, int targetPortIdx,
                     const std::string &varName)
    {
        if (producerId < 0) return;
        // Auto-resolve the source port from the producer's outputs[]
        // by varName match. Single-output nodes always resolve to 0
        // (the old behavior); multi-output nodes (e.g. ForRegion with
        // iter + loop-carried vars) need the right slot picked so the
        // edge attaches to the correct output handle in the renderer.
        int sourcePort = 0;
        if (producerId >= 0 && producerId < static_cast<int>(graph.nodes.size())) {
            const auto &n = graph.nodes[producerId];
            for (size_t i = 0; i < n.outputs.size(); ++i) {
                if (n.outputs[i] == varName) { sourcePort = static_cast<int>(i); break; }
            }
        }
        Edge e;
        e.source = { producerId, sourcePort, varName };
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
    n.sourceText = S.source ? sliceLine(*S.source, stmt.line, stmt.col, S.sliceCap(stmt.line, stmt.endCol)) : "";

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
    n.sourceText = S.source ? sliceLine(*S.source, stmt.line, stmt.col, S.sliceCap(stmt.line, stmt.endCol)) : "";
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
    n.sourceText = S.source ? sliceLine(*S.source, stmt.line, stmt.col, S.sliceCap(stmt.line, stmt.endCol)) : "";
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

// Forward decl — region lowerings call back into lowerStatement.
void lowerStatement(LoweringState &S, const ASTNode &stmt);

/** Build a region-node skeleton (kind, source slice, endLine, inputs
 *  from a "header expression" like an if-cond / for-range / switch-
 *  expr). Caller adds outputs (iter var) and recurses children. */
/** First-child source position of a compound's body — used to cap
 *  the header slice when the body sits on the SAME line as the
 *  header (compact form: `for k=1:3, body, end`). Returns (0,0) when
 *  there's no body or its position is unknown. */
std::pair<int,int> bodyStartPos(const ASTNode &stmt)
{
    const ASTNode *body = nullptr;
    switch (stmt.type) {
        case NodeType::FOR_STMT:
        case NodeType::WHILE_STMT:
        case NodeType::TRY_STMT:
            body = stmt.children.size() > 1 ? stmt.children[1].get() : nullptr;
            break;
        case NodeType::IF_STMT:
        case NodeType::SWITCH_STMT:
            if (!stmt.branches.empty()) body = stmt.branches[0].second.get();
            break;
        default: break;
    }
    if (!body) return {0, 0};
    if (body->type == NodeType::BLOCK && !body->children.empty()
     && body->children[0]) {
        return {body->children[0]->line, body->children[0]->col};
    }
    return {body->line, body->col};
}

int addRegionNode(LoweringState &S, const ASTNode &stmt, NodeKind kind,
                  const ASTNode *headerExpr)
{
    Node n;
    n.kind = kind;
    n.sourceLine = stmt.line;
    n.sourceCol  = stmt.col;
    n.endLine    = stmt.endLine;
    // Header slice ends right before the body's first statement when
    // the body shares the header's line (compact `for k=1:3, ... end`
    // form). Otherwise → no explicit cap, slice runs to end of line.
    int headerCap = 0;
    auto [bLn, bCol] = bodyStartPos(stmt);
    if (bLn == stmt.line && bCol > stmt.col) headerCap = bCol;
    n.sourceText = S.source
        ? sliceLine(*S.source, stmt.line, stmt.col, S.sliceCap(stmt.line, headerCap))
        : "";
    // Strip trailing `;` / `,` separator that lived BETWEEN the header
    // and the body in the compact form (slice left it in even after
    // ws-trim). For multi-line compounds this is a no-op.
    while (!n.sourceText.empty()
        && (n.sourceText.back() == ',' || n.sourceText.back() == ';'
         || n.sourceText.back() == ' ' || n.sourceText.back() == '\t')) {
        n.sourceText.pop_back();
    }

    // Inputs = free vars from the header expression (cond/range/switch-expr).
    if (headerExpr) {
        auto isKnownVar = [&](const std::string &name) {
            return S.lastProducer.count(name) > 0;
        };
        n.inputs = collectReads(headerExpr, isKnownVar);
    }
    int rid = S.addNode(std::move(n));

    // Wire data edges from lastProducer to each input.
    auto &nodeRef = S.graph.nodes[rid];
    for (size_t i = 0; i < nodeRef.inputs.size(); ++i) {
        auto it = S.lastProducer.find(nodeRef.inputs[i]);
        if (it != S.lastProducer.end()) {
            S.addDataEdge(it->second, rid, static_cast<int>(i), nodeRef.inputs[i]);
        }
    }
    return rid;
}

/** Lower a body BLOCK (or single statement) under the given region.
 *  Returns the index right after the last child added — caller uses
 *  this to compute branchPartitions for if/switch. Each statement
 *  carries its own endCol from the parser, so no extra capping is
 *  needed here for multi-stmt-on-one-line cases — they Just Work. */
int lowerBodyInto(LoweringState &S, int regionId, const ASTNode *body)
{
    if (!body) return static_cast<int>(S.graph.nodes[regionId].childNodeIds.size());
    if (body->type == NodeType::BLOCK) {
        for (const auto &c : body->children) {
            if (c) lowerStatement(S, *c);
        }
    } else {
        lowerStatement(S, *body);
    }
    return static_cast<int>(S.graph.nodes[regionId].childNodeIds.size());
}

/** Phase 2c: at the merge point of a branched region (after if /
 *  switch / try `end`), for each variable assigned in ≥1 branch,
 *  emit an explicit Merge node — a proper SSA φ that fans in from
 *  every branch's writer to a single output. External reads of the
 *  variable then route through this Merge, not directly to any one
 *  branch's writer (which was the Phase-2a "last-writer-wins"
 *  costyl).
 *
 *  Branch-coverage rules:
 *    - If a branch HAS a writer for the var → its writer is the
 *      input for that branch slot.
 *    - If a branch DOESN'T write the var → the slot takes the
 *      pre-region producer of the var (the value flows through
 *      that branch unchanged). When the var didn't exist pre-region
 *      either, the slot has no edge (undefined-source).
 *    - `hasFallThrough` adds an extra slot for the "no branch
 *      matched" case (if without else, switch without otherwise) —
 *      that slot always reads from the pre-region producer.
 *
 *  Side effect: S.lastProducer is reset to preRegionProducers and
 *  then [name] is rebound to the new Merge node id for every
 *  affected variable. Callers (lowerIf/Switch/Try) should NOT
 *  touch lastProducer after calling this. */
void emitMergeNodes(
        LoweringState &S,
        const std::unordered_map<std::string, int> &preRegionProducers,
        const std::vector<std::unordered_map<std::string, int>> &branchProducers,
        bool hasFallThrough,
        const ASTNode &stmt)
{
    // Collect names that any branch wrote with a producer DIFFERENT
    // from the pre-region one (i.e. genuinely modified). Sorted for
    // deterministic node ordering — graph output reproducibility.
    std::set<std::string> assigned;
    for (const auto &bp : branchProducers) {
        for (const auto &[name, id] : bp) {
            auto pIt = preRegionProducers.find(name);
            if (pIt == preRegionProducers.end() || pIt->second != id) {
                assigned.insert(name);
            }
        }
    }

    // Start the post-region producer state from the pre-region map.
    S.lastProducer = preRegionProducers;

    for (const auto &name : assigned) {
        // Build the per-slot writer id list. One slot per branch in
        // branchProducers order, plus optional fall-through slot.
        std::vector<int> writers;
        std::vector<std::string> labels;
        writers.reserve(branchProducers.size() + (hasFallThrough ? 1 : 0));
        labels.reserve(branchProducers.size()  + (hasFallThrough ? 1 : 0));

        auto preIt = preRegionProducers.find(name);
        int preProd = preIt != preRegionProducers.end() ? preIt->second : -1;

        for (size_t i = 0; i < branchProducers.size(); ++i) {
            auto it = branchProducers[i].find(name);
            int wId = (it != branchProducers[i].end()) ? it->second : preProd;
            writers.push_back(wId);
            labels.push_back(name);  // same name on every input — the
                                     // branch is conveyed by port index
        }
        if (hasFallThrough) {
            writers.push_back(preProd);
            labels.push_back(name);
        }

        // Spawn the Merge node at the parent scope (S.currentRegion
        // was already restored to the enclosing region by the caller
        // via leaveRegion() — so addNode wires parentRegionId for us).
        Node mn;
        mn.kind        = NodeKind::Merge;
        mn.sourceLine  = stmt.endLine > 0 ? stmt.endLine : stmt.line;
        mn.sourceCol   = 0;
        mn.sourceText  = name;
        mn.inputs      = std::move(labels);
        mn.outputs     = { name };
        int mid = S.addNode(std::move(mn));

        for (size_t i = 0; i < writers.size(); ++i) {
            if (writers[i] >= 0) {
                S.addDataEdge(writers[i], mid, static_cast<int>(i), name);
            }
        }
        S.lastProducer[name] = mid;
    }
}

void lowerIf(LoweringState &S, const ASTNode &stmt)
{
    // First branch's cond is the "header expression" for the IfRegion
    // node. Inputs from cond → region. Subsequent elseif conds are
    // additional inputs but their free vars merge into the region's
    // input list (we walk them after the region node is created).
    const ASTNode *firstCond = nullptr;
    if (!stmt.branches.empty()) firstCond = stmt.branches[0].first.get();
    int rid = addRegionNode(S, stmt, NodeKind::IfRegion, firstCond);

    // For each branch: snapshot pre-state, process branch body,
    // capture per-branch producers, mark branchPartitions boundary.
    auto preProducers = S.lastProducer;
    std::vector<std::unordered_map<std::string, int>> branchProducers;

    auto frame = S.enterRegion(rid);
    // Partition boundaries: each entry = childNodeIds.size() just
    // BEFORE the branch starts adding children. The renderer slices
    // childNodeIds by these boundaries to lay out branches in columns.
    std::vector<int> partitions;
    partitions.push_back(0);

    for (size_t i = 0; i < stmt.branches.size(); ++i) {
        S.lastProducer = preProducers;
        // For elseif (i > 0): walk the elseif's cond for additional reads.
        // These need to be wired as region inputs too — append + connect.
        if (i > 0 && stmt.branches[i].first) {
            auto isKnownVar = [&](const std::string &name) {
                return preProducers.count(name) > 0;
            };
            for (const auto &name : collectReads(stmt.branches[i].first.get(), isKnownVar)) {
                auto &reg = S.graph.nodes[rid];
                if (std::find(reg.inputs.begin(), reg.inputs.end(), name) == reg.inputs.end()) {
                    int portIdx = static_cast<int>(reg.inputs.size());
                    reg.inputs.push_back(name);
                    auto it = preProducers.find(name);
                    if (it != preProducers.end()) {
                        S.addDataEdge(it->second, rid, portIdx, name);
                    }
                }
            }
        }
        lowerBodyInto(S, rid, stmt.branches[i].second.get());
        branchProducers.push_back(S.lastProducer);
        partitions.push_back(static_cast<int>(S.graph.nodes[rid].childNodeIds.size()));
    }
    // Else branch (no cond).
    if (stmt.elseBranch) {
        S.lastProducer = preProducers;
        lowerBodyInto(S, rid, stmt.elseBranch.get());
        branchProducers.push_back(S.lastProducer);
        partitions.push_back(static_cast<int>(S.graph.nodes[rid].childNodeIds.size()));
    }
    S.leaveRegion(frame);

    S.graph.nodes[rid].branchPartitions = std::move(partitions);
    // Phi-style merge at the join point — proper SSA join for vars
    // written in any branch. If there's no `else`, add a fall-through
    // slot so the pre-region value of the var participates in the
    // merge (covers the "no branch matched" case).
    emitMergeNodes(S, preProducers, branchProducers,
                   /*hasFallThrough=*/ !stmt.elseBranch, stmt);
}

/** Phase 2e: build the loop-carried passthrough scaffolding for a
 *  for/while region. For each variable that is BOTH pre-existing
 *  OUTSIDE the loop AND assigned INSIDE the body, we materialise an
 *  EXPLICIT in/out port pair on the region edge and route data
 *  through a five-step path:
 *
 *    pre-producer → region.in[k]      (cross-hierarchy entering)
 *    region.in[k] → φ.in[0]           (internal passthrough)
 *    body writer  → φ.in[1]           (back-edge, wired by caller
 *                                       after body is lowered)
 *    φ            → region.out[k]     (internal passthrough)
 *    region.out[k] → consumer         (cross-hierarchy exiting,
 *                                       wired by caller via lastProducer)
 *
 *  Returns:
 *    .phiByVar  — map varName → φ-node id (caller wires back-edges
 *                  and post-loop lastProducer through this)
 *    .inPort    — map varName → region input port index
 *    .outPort   — map varName → region output port index
 *
 *  Caller responsibilities:
 *    1. Call this function INSIDE the loop region scope
 *       (S.currentRegion == regionId).
 *    2. After body lowering, wire each phi.in[1] from the body's
 *       last writer for that var.
 *    3. After body lowering, set lastProducer[var] = regionId so
 *       post-loop consumers route through the region's output port. */
struct LoopCarriedScaffold {
    std::unordered_map<std::string, int> phiByVar;
    std::unordered_map<std::string, int> inPort;
    std::unordered_map<std::string, int> outPort;
};

LoopCarriedScaffold emitLoopPhis(
        LoweringState &S,
        int regionId,
        const std::unordered_map<std::string, int> &preProducers,
        const std::set<std::string> &assignedInBody,
        const std::string &iterName,
        int loopLine)
{
    LoopCarriedScaffold sc;
    for (const auto &name : assignedInBody) {
        if (name == iterName) continue;  // for-iter handled separately
        auto preIt = preProducers.find(name);
        if (preIt == preProducers.end()) continue;  // not loop-carried

        // 1. Region input port. REUSE the existing slot if the header
        //    expression already declared this name (e.g. `while x < 10`
        //    + loop-carried `x` — same name, same port, addRegionNode
        //    already wired the pre-producer → region edge). Otherwise
        //    append a new input port and wire it explicitly.
        auto &reg = S.graph.nodes[regionId];
        int inIdx = -1;
        for (size_t i = 0; i < reg.inputs.size(); ++i) {
            if (reg.inputs[i] == name) { inIdx = static_cast<int>(i); break; }
        }
        if (inIdx < 0) {
            inIdx = static_cast<int>(reg.inputs.size());
            reg.inputs.push_back(name);
            // Cross-hierarchy edge entering the region:
            //   pre-producer → region.in[inIdx]
            S.addDataEdge(preIt->second, regionId, inIdx, name);
        }
        // Output port — always a fresh slot; regions don't surface
        // outputs from header processing.
        int outIdx = static_cast<int>(reg.outputs.size());
        reg.outputs.push_back(name);
        sc.inPort[name]  = inIdx;
        sc.outPort[name] = outIdx;

        // 3. Spawn the φ inside the region.
        Node mn;
        mn.kind       = NodeKind::Merge;
        mn.sourceLine = loopLine;
        mn.sourceCol  = 0;
        mn.sourceText = name;
        mn.inputs     = { name, name };   // [0] pre-loop, [1] back-edge
        mn.outputs    = { name };
        int phiId = S.addNode(std::move(mn));
        sc.phiByVar[name] = phiId;

        // 4. Internal passthrough region.in[inIdx] → φ.in[0]. Encoded
        //    as an Edge whose SOURCE is the region but with a port
        //    index that matches the INPUT slot. The renderer detects
        //    intra-region passthrough by `target.parent == source` and
        //    routes through the region's `inp-N` (source-type) handle.
        Edge e;
        e.source  = { regionId, inIdx, name };
        e.target  = { phiId,    0,     name };
        e.kind    = EdgeKind::Data;
        e.varName = name;
        S.graph.edges.push_back(std::move(e));
    }
    return sc;
}

/** Wire the right-hand passthrough φ → region.out[k] for every loop-
 *  carried var. Called by the loop lowerer AFTER body is lowered. */
void wireLoopPhiOutputs(
        LoweringState &S,
        int regionId,
        const LoopCarriedScaffold &sc)
{
    for (const auto &[name, phiId] : sc.phiByVar) {
        auto it = sc.outPort.find(name);
        if (it == sc.outPort.end()) continue;
        Edge e;
        e.source  = { phiId,    0,         name };
        e.target  = { regionId, it->second, name };
        e.kind    = EdgeKind::Data;
        e.varName = name;
        S.graph.edges.push_back(std::move(e));
    }
}

void lowerFor(LoweringState &S, const ASTNode &stmt)
{
    // Header = `for k = range`. children[0] = range expr,
    // children[1] = body BLOCK, strValue = loop var name.
    const ASTNode *range = stmt.children.size() > 0 ? stmt.children[0].get() : nullptr;
    const ASTNode *body  = stmt.children.size() > 1 ? stmt.children[1].get() : nullptr;
    int rid = addRegionNode(S, stmt, NodeKind::ForRegion, range);

    // Register the loop variable as an OUTPUT of the region (port 0) +
    // a producer in the body scope. MATLAB semantics: k is visible
    // INSIDE the loop (this iteration's value) AND AFTER the loop
    // (last iteration's value). So we set lastProducer[k] = rid both
    // inside the region recursion AND in the post-region state.
    const std::string &iterName = stmt.strValue;
    if (!iterName.empty()) {
        S.graph.nodes[rid].outputs.push_back(iterName);
    }

    auto preProducers = S.lastProducer;
    auto frame = S.enterRegion(rid);

    // Phase 2e: emit loop-carried in/out ports on the region with
    // an internal φ for each. See emitLoopPhis for the 5-step routing
    // (pre → region.in → φ → region.out → consumer).
    std::set<std::string> assignedInBody;
    if (body) collectAssignedNames(body, assignedInBody);
    auto sc = emitLoopPhis(S, rid, preProducers, assignedInBody,
                           iterName, stmt.line);
    // Body reads of loop-carried vars resolve to the φ.
    for (const auto &[name, phiId] : sc.phiByVar) {
        S.lastProducer[name] = phiId;
    }

    if (!iterName.empty()) {
        S.lastProducer[iterName] = rid;  // body reads of `k` wire to ForRegion
    }
    lowerBodyInto(S, rid, body);
    auto bodyProducers = S.lastProducer;

    // Back-edges: body's last writer of each loop-carried var feeds
    // φ.in[1]. Skip vars whose body producer IS the φ itself (no
    // actual write happened — the var was only read).
    for (const auto &[name, phiId] : sc.phiByVar) {
        auto it = bodyProducers.find(name);
        if (it != bodyProducers.end() && it->second != phiId) {
            S.addDataEdge(it->second, phiId, /*portIndex=*/ 1, name);
        }
    }
    // φ → region.out[k] internal passthrough (right side of the loop).
    wireLoopPhiOutputs(S, rid, sc);

    S.leaveRegion(frame);

    // Post-loop scope:
    //   - Loop-carried vars route external reads through the REGION
    //     itself (addDataEdge auto-resolves source port via outputs[]
    //     name lookup, picking the dedicated output port slot).
    //   - Vars NEW in body (no pre-producer) keep their body-last-writer
    //     for external reads — that's their only producer.
    //   - iterName is special — see the shadowing rule below.
    S.lastProducer = preProducers;
    for (const auto &[name, id] : bodyProducers) {
        if (name == iterName) continue;
        auto phiIt = sc.phiByVar.find(name);
        S.lastProducer[name] = (phiIt != sc.phiByVar.end()) ? rid : id;
    }
    // Iter-var visibility after the loop:
    //   • If preProducers already bound iterName to an OUTER ForRegion
    //     (nested `for k` inside `for k`), restore that outer binding —
    //     graph-level shadowing keeps the outer body's reads wired to
    //     the outer loop, not the just-exited inner.
    //   • Otherwise expose ForRegion as the producer (MATLAB semantic:
    //     k is visible after the loop with its last iteration value).
    if (!iterName.empty()) {
        auto preIt = preProducers.find(iterName);
        bool shadowingOuterFor = false;
        if (preIt != preProducers.end()) {
            int prev = preIt->second;
            if (prev >= 0 && prev < static_cast<int>(S.graph.nodes.size())
                && S.graph.nodes[prev].kind == NodeKind::ForRegion) {
                shadowingOuterFor = true;
            }
        }
        S.lastProducer[iterName] = shadowingOuterFor ? preIt->second : rid;
    }
}

void lowerWhile(LoweringState &S, const ASTNode &stmt)
{
    // Header = `while cond`. children[0] = cond, children[1] = body.
    const ASTNode *cond = stmt.children.size() > 0 ? stmt.children[0].get() : nullptr;
    const ASTNode *body = stmt.children.size() > 1 ? stmt.children[1].get() : nullptr;
    int rid = addRegionNode(S, stmt, NodeKind::WhileRegion, cond);

    // No implicit iter var.
    auto preProducers = S.lastProducer;
    auto frame = S.enterRegion(rid);

    std::set<std::string> assignedInBody;
    if (body) collectAssignedNames(body, assignedInBody);
    auto sc = emitLoopPhis(S, rid, preProducers, assignedInBody,
                           /*iterName=*/ "", stmt.line);
    for (const auto &[name, phiId] : sc.phiByVar) {
        S.lastProducer[name] = phiId;
    }

    lowerBodyInto(S, rid, body);
    auto bodyProducers = S.lastProducer;

    for (const auto &[name, phiId] : sc.phiByVar) {
        auto it = bodyProducers.find(name);
        if (it != bodyProducers.end() && it->second != phiId) {
            S.addDataEdge(it->second, phiId, /*portIndex=*/ 1, name);
        }
    }
    wireLoopPhiOutputs(S, rid, sc);

    S.leaveRegion(frame);

    S.lastProducer = preProducers;
    for (const auto &[name, id] : bodyProducers) {
        auto phiIt = sc.phiByVar.find(name);
        S.lastProducer[name] = (phiIt != sc.phiByVar.end()) ? rid : id;
    }
}

void lowerSwitch(LoweringState &S, const ASTNode &stmt)
{
    // Header = `switch x`. children[0] = switch expr, branches[] =
    // (case-expr, body), elseBranch = otherwise body. Case-expressions
    // are literals/cells in practice — but we still scan them for
    // free vars (rare: `case foo` where foo is a variable).
    const ASTNode *swExpr = stmt.children.size() > 0 ? stmt.children[0].get() : nullptr;
    int rid = addRegionNode(S, stmt, NodeKind::SwitchRegion, swExpr);

    auto preProducers = S.lastProducer;
    std::vector<std::unordered_map<std::string, int>> branchProducers;
    auto frame = S.enterRegion(rid);
    std::vector<int> partitions;
    partitions.push_back(0);

    for (const auto &branch : stmt.branches) {
        S.lastProducer = preProducers;
        // case-expr free vars → region inputs (uncommon, but cover
        // it for completeness).
        if (branch.first) {
            auto isKnownVar = [&](const std::string &name) {
                return preProducers.count(name) > 0;
            };
            for (const auto &name : collectReads(branch.first.get(), isKnownVar)) {
                auto &reg = S.graph.nodes[rid];
                if (std::find(reg.inputs.begin(), reg.inputs.end(), name) == reg.inputs.end()) {
                    int portIdx = static_cast<int>(reg.inputs.size());
                    reg.inputs.push_back(name);
                    auto it = preProducers.find(name);
                    if (it != preProducers.end()) {
                        S.addDataEdge(it->second, rid, portIdx, name);
                    }
                }
            }
        }
        lowerBodyInto(S, rid, branch.second.get());
        branchProducers.push_back(S.lastProducer);
        partitions.push_back(static_cast<int>(S.graph.nodes[rid].childNodeIds.size()));
    }
    if (stmt.elseBranch) {
        S.lastProducer = preProducers;
        lowerBodyInto(S, rid, stmt.elseBranch.get());
        branchProducers.push_back(S.lastProducer);
        partitions.push_back(static_cast<int>(S.graph.nodes[rid].childNodeIds.size()));
    }
    S.leaveRegion(frame);

    S.graph.nodes[rid].branchPartitions = std::move(partitions);
    // Switch without an `otherwise` — fall-through slot represents
    // the "no case matched" path (in MATLAB the switch is a no-op then,
    // so the pre-region value of the var is what flows out).
    emitMergeNodes(S, preProducers, branchProducers,
                   /*hasFallThrough=*/ !stmt.elseBranch, stmt);
}

void lowerTry(LoweringState &S, const ASTNode &stmt)
{
    // try body = children[0]; catch body (optional) = children[1];
    // catch var name (optional) = strValue. The catch var is an
    // implicit producer in catch scope only — not visible outside.
    const ASTNode *tryBody   = stmt.children.size() > 0 ? stmt.children[0].get() : nullptr;
    const ASTNode *catchBody = stmt.children.size() > 1 ? stmt.children[1].get() : nullptr;

    int rid = addRegionNode(S, stmt, NodeKind::TryRegion, nullptr);

    auto preProducers = S.lastProducer;
    std::vector<std::unordered_map<std::string, int>> branchProducers;
    auto frame = S.enterRegion(rid);
    std::vector<int> partitions;
    partitions.push_back(0);

    // try body
    S.lastProducer = preProducers;
    lowerBodyInto(S, rid, tryBody);
    branchProducers.push_back(S.lastProducer);
    partitions.push_back(static_cast<int>(S.graph.nodes[rid].childNodeIds.size()));

    // catch body — register catch var (e.g. `ME`) as producer rooted
    // at the TryRegion (Phase 2c may give it its own sub-port). Only
    // visible to the catch body scope: we drop it from the branch's
    // producer snapshot before merging so subsequent code outside the
    // try/catch can't bind to it.
    if (catchBody) {
        S.lastProducer = preProducers;
        if (!stmt.strValue.empty()) {
            S.graph.nodes[rid].outputs.push_back(stmt.strValue);
            S.lastProducer[stmt.strValue] = rid;
        }
        lowerBodyInto(S, rid, catchBody);
        auto catchProducers = S.lastProducer;
        if (!stmt.strValue.empty()) {
            catchProducers.erase(stmt.strValue);  // ME doesn't leak past `end`
        }
        branchProducers.push_back(std::move(catchProducers));
        partitions.push_back(static_cast<int>(S.graph.nodes[rid].childNodeIds.size()));
    }
    // Phase 2e: a single Exception edge from the last try-body
    // statement to the first catch-body statement, representing
    // "if anything in try throws, control resumes here". Avoids
    // per-statement edges (which would clutter the view); the
    // single arc is enough to convey the try→catch relationship
    // at the data-flow layer, on top of the TryRegion's branch
    // partition layout which already shows the structure.
    if (catchBody && partitions.size() >= 3) {
        const auto &childIds = S.graph.nodes[rid].childNodeIds;
        int pTryEnd   = partitions[1];
        int pCatchEnd = partitions[2];
        if (pTryEnd > 0
         && pCatchEnd > pTryEnd
         && pTryEnd <= static_cast<int>(childIds.size())) {
            int lastTry    = childIds[pTryEnd - 1];
            int firstCatch = childIds[pTryEnd];
            Edge e;
            e.source  = { lastTry,    /*portIndex*/ 0, "" };
            e.target  = { firstCatch, /*portIndex*/ 0, "" };
            e.kind    = EdgeKind::Exception;
            e.varName = "throws";
            S.graph.edges.push_back(std::move(e));
        }
    }

    S.leaveRegion(frame);

    S.graph.nodes[rid].branchPartitions = std::move(partitions);
    // Try without `catch`: never happens in practice (parser rejects)
    // but covered defensively — no fall-through, try body is its own
    // path. With `catch`, the two branches naturally form the join
    // (try-succeeds vs caught-exception), no fall-through either.
    emitMergeNodes(S, preProducers, branchProducers,
                   /*hasFallThrough=*/ false, stmt);
}

/** Phase 2d: function definition as a proper compound region.
 *
 *  Lays out:
 *    - FunctionDef region with input ports = paramNames and output
 *      ports = returnNames.
 *    - Inside the region: every parameter is registered as a
 *      producer rooted at the FunctionDef itself, so body reads of
 *      a parameter wire from the corresponding region input port.
 *    - The function body has its OWN local scope: MATLAB functions
 *      do not see the enclosing script's variables (no closures).
 *      So lastProducer is cleared on entry and restored on exit.
 *    - At function exit, for each return name we look up its last
 *      writer inside the body and emit a Data edge writer →
 *      FunctionDef.out[i]. Multi-return functions get one such
 *      edge per output port.
 *
 *  Approximation worth noting: when a body has `return` mid-way
 *  PLUS code after it that overrides the return var, the last
 *  in-source-order writer wins — early-return paths are not
 *  cross-merged with the fall-through writer. That's accepted as
 *  Phase 2d/e split: proper return-path merging belongs with
 *  loop-carried merges in Phase 2e. */
void lowerFunctionDef(LoweringState &S, const ASTNode &stmt)
{
    int rid = addRegionNode(S, stmt, NodeKind::FunctionDef, nullptr);

    // Set the region's port lists. params on the left, returns
    // on the right — the renderer derives handles from these.
    {
        auto &reg = S.graph.nodes[rid];
        reg.inputs  = stmt.paramNames;
        reg.outputs = stmt.returnNames;
    }

    // Function body has its own scope — clear lastProducer on entry
    // so script variables don't bleed in (MATLAB workspace isolation).
    auto outerProducers = S.lastProducer;
    auto frame = S.enterRegion(rid);
    S.lastProducer.clear();

    // Parameters become implicit producers rooted at FunctionDef.
    // A body read of `x` (where x is a param) wires from
    // FunctionDef.out[?] — addDataEdge uses portIndex=0 by default
    // which lands on the FunctionDef's FIRST output, but that's the
    // wrong semantic: params are INPUTS of the function and their
    // values originate from the FunctionDef as if it were the caller.
    // For graph purposes we model each param read as an edge from
    // the FunctionDef node (its source "port" is the param's
    // INPUT slot — we use the param index so the renderer can
    // attach the edge to the correct input handle in a future pass).
    for (size_t i = 0; i < stmt.paramNames.size(); ++i) {
        S.lastProducer[stmt.paramNames[i]] = rid;
    }

    const ASTNode *body = stmt.children.empty() ? nullptr : stmt.children[0].get();
    lowerBodyInto(S, rid, body);

    // Wire return values: writer → FunctionDef.out[i]. The edge
    // crosses the region boundary (source is inside, target is the
    // region itself).
    for (size_t i = 0; i < stmt.returnNames.size(); ++i) {
        const auto &rname = stmt.returnNames[i];
        auto it = S.lastProducer.find(rname);
        if (it == S.lastProducer.end()) continue;
        Edge e;
        e.source  = { it->second, /*portIndex*/ 0, rname };
        e.target  = { rid,        static_cast<int>(i), rname };
        e.kind    = EdgeKind::Data;
        e.varName = rname;
        S.graph.edges.push_back(std::move(e));
    }

    S.leaveRegion(frame);
    // Function-local writes do NOT leak to the outer scope — restore
    // the pre-function producer state verbatim.
    S.lastProducer = outerProducers;
}

/** Walk up the parentRegionId chain from the innermost active region
 *  and return the id of the first ancestor whose kind matches one of
 *  the given kinds. Returns -1 when no such ancestor exists (e.g.
 *  `break` outside any loop, or `return` at script top level). */
int findEnclosingRegion(const LoweringState &S,
                        std::initializer_list<NodeKind> kinds)
{
    if (!S.currentRegion) return -1;
    int rid = *S.currentRegion;
    while (rid >= 0) {
        const auto &node = S.graph.nodes[rid];
        for (auto k : kinds) {
            if (node.kind == k) return rid;
        }
        if (!node.parentRegionId) return -1;
        rid = *node.parentRegionId;
    }
    return -1;
}

void lowerJump(LoweringState &S, const ASTNode &stmt, NodeKind kind)
{
    // Emit the jump leaf with its source text (e.g. `break;`).
    Node n;
    n.kind = kind;
    n.sourceLine = stmt.line;
    n.sourceCol  = stmt.col;
    n.sourceText = S.source ? sliceLine(*S.source, stmt.line, stmt.col, S.sliceCap(stmt.line, stmt.endCol)) : "";
    int jid = S.addNode(std::move(n));

    // Phase 2b: wire a Jump edge from this node to the relevant
    // enclosing region:
    //   - break    → innermost ForRegion / WhileRegion (exit-sink)
    //   - continue → innermost ForRegion / WhileRegion (loop header)
    //   - return   → innermost FunctionDef (function exit)
    // When no such ancestor exists, no edge is emitted — the jump
    // is dangling, which is itself a useful signal in the view
    // (`break` at script top level is a semantic error).
    int target = -1;
    const char *label = "";
    switch (kind) {
        case NodeKind::JumpBreak:
            target = findEnclosingRegion(S, {NodeKind::ForRegion, NodeKind::WhileRegion});
            label  = "break";
            break;
        case NodeKind::JumpContinue:
            target = findEnclosingRegion(S, {NodeKind::ForRegion, NodeKind::WhileRegion});
            label  = "continue";
            break;
        case NodeKind::JumpReturn:
            target = findEnclosingRegion(S, {NodeKind::FunctionDef});
            label  = "return";
            break;
        default: break;
    }
    if (target >= 0) {
        Edge e;
        e.source   = { jid,    /*portIndex*/ 0, "" };
        e.target   = { target, /*portIndex*/ 0, "" };
        e.kind     = EdgeKind::Jump;
        e.varName  = label;
        S.graph.edges.push_back(std::move(e));
    }
}

void lowerStatement(LoweringState &S, const ASTNode &stmt)
{
    switch (stmt.type) {
    case NodeType::ASSIGN:
    case NodeType::MULTI_ASSIGN:    lowerAssign(S, stmt);            return;
    case NodeType::EXPR_STMT:       lowerExprStmt(S, stmt);          return;
    case NodeType::GLOBAL_STMT:     lowerGlobalDecl(S, stmt, NodeKind::GlobalDecl);     return;
    case NodeType::PERSISTENT_STMT: lowerGlobalDecl(S, stmt, NodeKind::PersistentDecl); return;
    case NodeType::IF_STMT:         lowerIf(S, stmt);                return;
    case NodeType::FOR_STMT:        lowerFor(S, stmt);               return;
    case NodeType::WHILE_STMT:      lowerWhile(S, stmt);             return;
    case NodeType::SWITCH_STMT:     lowerSwitch(S, stmt);            return;
    case NodeType::TRY_STMT:        lowerTry(S, stmt);               return;
    case NodeType::BREAK_STMT:      lowerJump(S, stmt, NodeKind::JumpBreak);    return;
    case NodeType::CONTINUE_STMT:   lowerJump(S, stmt, NodeKind::JumpContinue); return;
    case NodeType::RETURN_STMT:     lowerJump(S, stmt, NodeKind::JumpReturn);   return;
    case NodeType::FUNCTION_DEF:    lowerFunctionDef(S, stmt);       return;
    default:
        // Unrecognised — skip rather than crash. Phase-2 may turn
        // this into an assertion once every statement kind is covered.
        return;
    }
}

} // namespace

NodeGraph lowerScript(const ASTNode &root,
                      const std::string &sourceText,
                      const std::vector<Token> &tokens)
{
    LoweringState S;
    S.source = sourceText.empty() ? nullptr : &sourceText;
    S.graph.functionName = "<script>";

    // Build per-line "first COMMENT column" map for sourceText
    // trimming. Comments come from the lexer (single source of truth
    // for MATLAB syntax — handles quote/transpose, block comments,
    // line continuations correctly), passed in by the caller as the
    // raw token stream. The parser ALREADY pre-filters COMMENT tokens
    // so the AST is unaffected.
    for (const auto &t : tokens) {
        if (t.type != TokenType::COMMENT) continue;
        auto [it, inserted] = S.firstCommentColPerLine.try_emplace(t.line, t.col);
        if (!inserted && t.col < it->second) it->second = t.col;
    }

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

} // namespace numkit::scriptgraph
