#include <numkit/core/dependency_graph.hpp>

#include <algorithm>

namespace numkit {

namespace {

// ── Expression walker: collect every identifier evaluated ──────────
struct ReadCollector {
    std::set<std::string> reads;
    bool wildcard = false;   // eval/feval/assignin/clear or unresolved

    void expr(const ASTNode *n)
    {
        if (!n)
            return;
        switch (n->type) {
        case NodeType::IDENTIFIER:
            reads.insert(n->strValue);
            return;
        case NodeType::CALL:
        case NodeType::COMMAND_CALL: {
            // Callee name (strValue on CALL) counts as a read when it
            // is not a local function — calling a workspace function
            // value (`f = @sin; f(x)`). Dynamic calls through values
            // are fine: the identifier was already read. Some call
            // forms carry the callee as an IDENTIFIER child instead of
            // strValue — check both.
            std::string callee = n->strValue;
            if (callee.empty() && !n->children.empty()
                && n->children[0]->type == NodeType::IDENTIFIER)
                callee = n->children[0]->strValue;
            if (!callee.empty())
                reads.insert(callee);
            if (callee == "eval" || callee == "feval"
                || callee == "assignin" || callee == "clear"
                || callee == "load" || callee == "run")
                wildcard = true;
            for (const auto &c : n->children)
                expr(c.get());
            return;
        }
        case NodeType::INDEX:
        case NodeType::CELL_INDEX:
            // A(i) = v handled by the caller (merge-write); as an
            // rvalue the container identifier is a plain read.
            for (const auto &c : n->children)
                expr(c.get());
            return;
        case NodeType::FIELD_ACCESS:
        case NodeType::DYNAMIC_FIELD_ACCESS:
            // s.f reads s; the field name is not a variable. Dynamic
            // field s.(expr) also evaluates expr.
            expr(n->children[0].get());
            if (n->type == NodeType::DYNAMIC_FIELD_ACCESS
                && n->children.size() > 1)
                expr(n->children[1].get());
            return;
        case NodeType::ANON_FUNC:
            // @(x) … — parameters are bound locally; the body reads
            // captured variables. Walk every child, then drop params.
            for (const auto &c : n->children)
                expr(c.get());
            for (const auto &p : n->paramNames)
                reads.erase(p);
            return;
        default:
            for (const auto &c : n->children)
                expr(c.get());
            return;
        }
    }
};

// ── Statement facts ─────────────────────────────────────────────────
bool isOpaqueStatement(const ASTNode *s)
{
    switch (s->type) {
    case NodeType::IF_STMT:
    case NodeType::FOR_STMT:
    case NodeType::WHILE_STMT:
    case NodeType::SWITCH_STMT:
    case NodeType::TRY_STMT:
    case NodeType::GLOBAL_STMT:
    case NodeType::PERSISTENT_STMT:
        return true;
    default:
        return false;
    }
}

StatementFacts factsFor(const ASTNode *s)
{
    StatementFacts f;
    if (!s)
        return f;
    if (isOpaqueStatement(s)) {
        f.wildcardReads = true;
        return f;
    }
    switch (s->type) {
    case NodeType::FUNCTION_DEF:
        f.writes.insert(s->strValue);
        return f;
    case NodeType::CLASSDEF_DEF:
        f.writes.insert(s->strValue);
        return f;
    case NodeType::ASSIGN: {
        const ASTNode *lhs = s->children[0].get();
        ReadCollector rc;
        rc.expr(s->children[1].get());
        if (lhs->type == NodeType::IDENTIFIER) {
            f.writes.insert(lhs->strValue);
        } else {
            // A(i) = v / s.f = v / c{i} = v — merge-write: the base
            // variable is BOTH read and written.
            ReadCollector lc;
            lc.expr(lhs);
            f.reads = lc.reads;
            f.writes = lc.reads;
            f.wildcardReads = lc.wildcard;
        }
        f.reads.insert(rc.reads.begin(), rc.reads.end());
        f.wildcardReads = f.wildcardReads || rc.wildcard;
        return f;
    }
    case NodeType::MULTI_ASSIGN: {
        ReadCollector rc;
        for (const auto &t : s->lhsTargets)
            if (t) {
                ReadCollector lc;
                lc.expr(t.get());
                for (const auto &w : lc.reads)
                    f.writes.insert(w);   // plain ids + merge-writes
                if (lc.wildcard)
                    f.wildcardReads = true;
            }
        for (const auto &r : s->returnNames)
            if (!r.empty() && r != "~")
                f.writes.insert(r);
        // RHS is the CALL child (last child).
        if (!s->children.empty())
            rc.expr(s->children.back().get());
        f.reads.insert(rc.reads.begin(), rc.reads.end());
        f.wildcardReads = f.wildcardReads || rc.wildcard;
        return f;
    }
    case NodeType::DELETE_ASSIGN: {
        // delete s.f — reads s.
        ReadCollector rc;
        rc.expr(s->children[0].get());
        f.reads = rc.reads;
        return f;
    }
    case NodeType::EXPR_STMT: {
        ReadCollector rc;
        rc.expr(s->children.empty() ? s : s->children[0].get());
        f.reads = rc.reads;
        f.wildcardReads = rc.wildcard;
        return f;
    }
    case NodeType::RETURN_STMT:
    case NodeType::BREAK_STMT:
    case NodeType::CONTINUE_STMT:
        return f;
    default: {
        // Bare expression at top level (rare — parser wraps in
        // EXPR_STMT) — treat as reads-only.
        ReadCollector rc;
        rc.expr(s);
        f.reads = rc.reads;
        f.wildcardReads = rc.wildcard;
        return f;
    }
    }
}

}  // namespace

std::vector<StatementFacts> computeStatementFacts(const ASTNode *program)
{
    std::vector<StatementFacts> out;
    if (!program)
        return out;
    for (const auto &s : program->children)
        out.push_back(factsFor(s.get()));
    return out;
}

DependencyGraph::DependencyGraph(std::vector<StatementFacts> facts)
    : facts_(std::move(facts))
{
    downstream_.resize(facts_.size());
    // Writes-kill: for statement j, each read of var x resolves to the
    // NEAREST earlier writer. Track the last writer per variable.
    std::set<std::string> everWritten;
    for (size_t j = 0; j < facts_.size(); ++j) {
        if (facts_[j].wildcardReads) {
            // Depends on every prior statement.
            for (size_t i = 0; i < j; ++i)
                downstream_[i].push_back(j);
        } else {
            for (const auto &x : facts_[j].reads) {
                // nearest def below j
                for (size_t i = j; i-- > 0;) {
                    if (facts_[i].writes.count(x)) {
                        downstream_[i].push_back(j);
                        break;
                    }
                    if (facts_[i].wildcardReads) {
                        // Wildcard may write x invisibly — depend on it.
                        downstream_[i].push_back(j);
                        break;
                    }
                }
            }
        }
        for (const auto &w : facts_[j].writes)
            everWritten.insert(w);
    }
    (void)everWritten;
    // Dedup edges.
    for (auto &d : downstream_) {
        std::sort(d.begin(), d.end());
        d.erase(std::unique(d.begin(), d.end()), d.end());
    }
}

std::vector<size_t> DependencyGraph::dirtySetFor(const std::string &var) const
{
    std::vector<char> dirty(facts_.size(), 0);
    std::vector<size_t> work;
    for (size_t i = 0; i < facts_.size(); ++i) {
        // A PURE DEFINITION of the parameter (writes var, never reads
        // it) is REPLACED by the setParam write — re-running it would
        // clobber the slider value back to the script default. Only
        // statements that READ var (or merge-write it) are affected.
        bool pureDef = facts_[i].writes.count(var) > 0
                       && facts_[i].reads.count(var) == 0
                       && !facts_[i].wildcardReads;
        if (pureDef)
            continue;
        bool affected = facts_[i].writes.count(var) > 0
                        || facts_[i].reads.count(var) > 0;
        // A wildcard statement before ANY writer of var may itself
        // write var — conservatively treat wildcards as affected.
        if (!affected && facts_[i].wildcardReads) {
            for (size_t k = 0; k <= i; ++k)
                if (facts_[k].writes.count(var)) {
                    affected = true;
                    break;
                }
        }
        if (affected) {
            dirty[i] = 1;
            work.push_back(i);
        }
    }
    while (!work.empty()) {
        size_t i = work.back();
        work.pop_back();
        for (size_t j : downstream_[i])
            if (!dirty[j]) {
                dirty[j] = 1;
                work.push_back(j);
            }
    }
    std::vector<size_t> out;
    for (size_t i = 0; i < facts_.size(); ++i)
        if (dirty[i])
            out.push_back(i);
    return out;
}

}  // namespace numkit
