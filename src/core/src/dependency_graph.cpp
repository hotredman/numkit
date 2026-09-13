#include <nk/core/dependency_graph.hpp>

#include <algorithm>

namespace nk {

namespace depdetail {
struct AxisRange;
void recordAccess(ElemAccessMap &m, const std::string &base,
                  const ASTNode *n);
bool isSizeCallee(const std::string &callee);
}  // namespace depdetail

namespace {

// A WILDCARD statement (opaque for/while/eval/…) may ACCUMULATE on
// earlier state (`AF = AF + …` inside a for over a previously defined
// AF) — re-running it in isolation double-counts. Its re-run therefore
// requires the whole earlier prefix to have executed on the SAME param
// values: mark [0..w] dirty. Worst case this degrades to a full
// prefix re-run — the documented wildcard contract (correct over
// fast); scripts without wildcards keep their precise skipping.
std::vector<size_t> closeDirtySet(const std::vector<StatementFacts> &facts,
                                  const std::vector<char> &marked)
{
    std::vector<char> dirty(marked);
    std::vector<size_t> work;
    // Rebuild downstream edges lazily? No — the caller passes the
    // graph's own closure; here only the wildcard-prefix rule runs.
    (void)facts;
    (void)dirty;
    (void)work;
    return {};
}

// Merge region-access maps (statement facts accumulate RHS + LHS).
void mergeAccess(ElemAccessMap &dst, const ElemAccessMap &src)
{
    for (const auto &kv : src) {
        VarRegions &d = dst[kv.first];
        if (kv.second.all)
            d.setAll();
        for (const auto &r : kv.second.rects)
            d.add(r);
    }
}

// ── Expression walker: collect every identifier evaluated ──────────
struct ReadCollector {
    std::set<std::string> reads;
    bool wildcard = false;   // eval/feval/assignin/clear or unresolved
    ElemAccessMap elemReads; // fine-grained rvalue regions (B1)

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
            // size(C)/numel(C)/... — merge-writes can GROW the
            // variable, so shape probes read ALL of it.
            if (depdetail::isSizeCallee(callee) && n->children.size() >= 2
                && n->children[1]->type == NodeType::IDENTIFIER)
                elemReads[n->children[1]->strValue].setAll();
            // Paren access vs call: the parser emits BOTH as CALL with
            // the head as an IDENTIFIER child (`C(7)` ≡ `foo(3)` at AST
            // level — the variable/function split is runtime). Record
            // REGIONS only for the unambiguous-index markers: an END_VAL
            // arg (valid only inside indexing) or a bare/stepped colon
            // arg. Everything else stays a whole-var read (conservative;
            // braces CELLIDX never passes through here).
            if (!n->children.empty()
                && n->children[0]->type == NodeType::IDENTIFIER) {
                bool endArg = false, colonArg = false;
                for (size_t k = 1; k < n->children.size(); ++k) {
                    NodeType t = n->children[k]->type;
                    if (t == NodeType::END_VAL)
                        endArg = true;
                    if (t == NodeType::COLON_EXPR)
                        colonArg = true;
                }
                if (endArg || colonArg)
                    depdetail::recordAccess(
                        elemReads, n->children[0]->strValue, n);
            }
            for (const auto &c : n->children)
                expr(c.get());
            return;
        }
        case NodeType::INDEX:
        case NodeType::CELL_INDEX:
            // Rvalue access A(i,j): plain read of A + a fine-grained
            // region when the base is a simple identifier and the
            // subscripts are literal. LHS merge-writes are handled by
            // factsFor, which calls recordAccess for the write side.
            if (!n->children.empty()
                && n->children[0]->type == NodeType::IDENTIFIER)
                depdetail::recordAccess(elemReads,
                                        n->children[0]->strValue, n);
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
            f.elemWrites[lhs->strValue].setAll();   // whole-var def
        } else if ((lhs->type == NodeType::INDEX
                    || lhs->type == NodeType::CELL_INDEX)
                   && !lhs->children.empty()
                   && lhs->children[0]->type == NodeType::IDENTIFIER) {
            // A(i,j) = v / C{r,c} = v — merge-write: the base variable
            // is BOTH read and written; the WRITE side carries the
            // literal region (the READ side is the merge of everything
            // ELSE — conservatively ALL via the coarse `reads`).
            std::string base = lhs->children[0]->strValue;
            f.reads.insert(base);          // coarse (portion-A compat)
            f.writes.insert(base);
            f.lhsMergeBases.insert(base);  // NOT an rvalue read (B1)
            depdetail::recordAccess(f.elemWrites, base, lhs);
        } else {
            // s.f = v and friends — merge-write without a tractable
            // region: coarse read+write of the walked names.
            ReadCollector lc;
            lc.expr(lhs);
            f.reads = lc.reads;
            f.writes = lc.reads;
            f.wildcardReads = lc.wildcard;
        }
        f.reads.insert(rc.reads.begin(), rc.reads.end());
        mergeAccess(f.elemReads, rc.elemReads);
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
        mergeAccess(f.elemReads, rc.elemReads);
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
        f.elemReads = std::move(rc.elemReads);
        f.wildcardReads = rc.wildcard;
        // A bare CALL statement (plot/title/grid on/legend/colormap…)
        // mutates shared FIGURE state invisible to variable facts.
        // Chain them through kFigureStateVar so a re-run plot re-runs
        // the decoration tail (equivalence on figure state; see hpp).
        // IDENT covers MATLAB's no-parens call form (`figure;`, `clf;`)
        // and the harmless bare-variable echo.
        const ASTNode *e = s->children.empty() ? nullptr : s->children[0].get();
        if (e && (e->type == NodeType::CALL
                  || e->type == NodeType::COMMAND_CALL
                  || e->type == NodeType::IDENTIFIER)) {
            f.reads.insert(kFigureStateVar);
            f.writes.insert(kFigureStateVar);
        }
        return f;
    }
    case NodeType::RETURN_STMT:
    case NodeType::BREAK_STMT:
    case NodeType::CONTINUE_STMT:
        return f;
    default: {
        // Bare expression at top level (rare — parser wraps in
        // EXPR_STMT) — treat as reads-only. Exception: a bare CALL /
        // COMMAND_CALL here is a graphics-shaped statement (figure;,
        // grid on, …) — chain it through the figure state like the
        // EXPR_STMT case above.
        ReadCollector rc;
        rc.expr(s);
        f.reads = rc.reads;
        f.elemReads = std::move(rc.elemReads);
        f.wildcardReads = rc.wildcard;
        if (s->type == NodeType::CALL
            || s->type == NodeType::COMMAND_CALL) {
            f.reads.insert(kFigureStateVar);
            f.writes.insert(kFigureStateVar);
        }
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
    // Two write semantics meet here:
    //   SCALAR REDEFINITION (`x = 1; x = 2`) KILLS — a reader of x
    //     depends on the NEAREST earlier writer only.
    //   MERGE-WRITE (`C{5,2} = v`, `C = ...` whole-var) ACCUMULATES —
    //     the current value is built from the init + every later
    //     merge-write, so a WHOLE-VARIABLE reader depends on EVERY
    //     earlier writer of that variable; a REGION reader depends on
    //     the nearest earlier writer whose write region intersects.
    for (size_t j = 0; j < facts_.size(); ++j) {
        if (facts_[j].wildcardReads) {
            for (size_t i = 0; i < j; ++i)
                downstream_[i].push_back(j);
            continue;
        }
        for (const auto &x : facts_[j].reads) {
            if (facts_[j].lhsMergeBases.count(x)
                && facts_[j].elemReads.find(x)
                       == facts_[j].elemReads.end()) {
                // Coarse read came from the LHS ONLY (the RHS does not
                // read the variable) — the assignment does not consume
                // the current value; no read edge. When the RHS reads
                // regions too, elemReads has an entry and the normal
                // region logic below applies.
                continue;
            }
            auto rj = facts_[j].elemReads.find(x);
            bool wholeReader =
                rj == facts_[j].elemReads.end() || rj->second.all;
            if (wholeReader) {
                // Accumulate: every earlier writer contributes.
                for (size_t i = j; i-- > 0;) {
                    if (facts_[i].writes.count(x)
                        || facts_[i].wildcardReads)
                        downstream_[i].push_back(j);
                }
                continue;
            }
            // Region reader: nearest intersecting writer per region.
            for (const auto &reg : rj->second.rects) {
                for (size_t i = j; i-- > 0;) {
                    if (facts_[i].wildcardReads) {
                        downstream_[i].push_back(j);
                        break;
                    }
                    if (!facts_[i].writes.count(x))
                        continue;
                    auto wi = facts_[i].elemWrites.find(x);
                    bool touches =
                        wi == facts_[i].elemWrites.end()
                        || wi->second.all || wi->second.touches(reg);
                    if (touches) {
                        downstream_[i].push_back(j);
                        break;   // this region's value source found
                    }
                }
            }
        }
    }
    // Dedup edges.
    for (auto &d : downstream_) {
        std::sort(d.begin(), d.end());
        d.erase(std::unique(d.begin(), d.end()), d.end());
    }
}

std::vector<size_t> DependencyGraph::dirtySetForElement(
    const std::string &var, size_t r, size_t c) const
{
    return dirtySetForElement(var, r, c, {});
}

std::vector<size_t> DependencyGraph::dirtySetForElement(
    const std::string &var, size_t r, size_t c,
    const std::set<std::string> &externalDefs) const
{
    const ElemRegion cell{r, r, c, c};
    std::vector<char> dirty(facts_.size(), 0);
    std::vector<size_t> work;
    for (size_t i = 0; i < facts_.size(); ++i) {
        bool affected = false;
        if (facts_[i].wildcardReads) {
            affected = true;
        } else if (facts_[i].reads.count(var)) {
            if (facts_[i].lhsMergeBases.count(var)
                && facts_[i].reads.count(var)
                       == facts_[i].lhsMergeBases.count(var)
                   && facts_[i].elemReads.find(var)
                          == facts_[i].elemReads.end()) {
                // The variable appears ONLY as a merge-write LHS:
                // a definition of some OTHER cell — its inputs did not
                // change, the external element write is independent.
                // (If the RHS also reads var, elemReads has an entry
                // and the checks below decide.)
                auto re = facts_[i].elemReads.find(var);
                if (re == facts_[i].elemReads.end()
                    || !(re->second.all || re->second.touches(cell)))
                    affected = false;
                else
                    affected = re->second.all
                               || re->second.touches(cell);
            } else {
                auto re = facts_[i].elemReads.find(var);
                affected = re == facts_[i].elemReads.end()   // bare id
                           || re->second.all
                           || re->second.touches(cell);
            }
        }
        if (affected) {
            dirty[i] = 1;
            work.push_back(i);
        }
    }
    // WILDCARD-PREFIX RULE (same as dirtySetFor): a wildcard re-run
    // may ACCUMULATE on earlier state — pull its prefix in.
    auto applyWildcardPrefix = [this, &dirty, &work, &var]() {
        bool again = true;
        while (again) {
            again = false;
            for (size_t i = 0; i < facts_.size(); ++i)
                if (dirty[i] && facts_[i].wildcardReads)
                    for (size_t k = 0; k < i; ++k) {
                        // The param's own pure definition stays
                        // skipped — re-running it would clobber the
                        // setParam write with the script default.
                        if (facts_[k].writes.count(var)
                            && !facts_[k].reads.count(var)
                            && !facts_[k].wildcardReads)
                            continue;
                        if (!dirty[k]) {
                            dirty[k] = 1;
                            work.push_back(k);
                            if (facts_[k].wildcardReads)
                                again = true;
                        }
                    }
        }
    };
    applyWildcardPrefix();
    while (!work.empty()) {
        size_t i = work.back();
        work.pop_back();
        for (size_t j : downstream_[i])
            if (!dirty[j]) {
                dirty[j] = 1;
                work.push_back(j);
            }
    }
    applyWildcardPrefix();
    std::vector<size_t> out;
    for (size_t i = 0; i < facts_.size(); ++i)
        if (dirty[i])
            out.push_back(i);
    return out;
}

std::vector<size_t> DependencyGraph::dirtySetFor(const std::string &var) const
{
    return dirtySetFor(var, {});
}

std::vector<size_t> DependencyGraph::dirtySetFor(
    const std::string &var,
    const std::set<std::string> &externalDefs) const
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
    // WILDCARD-PREFIX RULE: a wildcard re-run may ACCUMULATE on
    // earlier state (`AF = AF + …` inside a for over a previously
    // defined AF) — executing it without a fresh prefix double-counts.
    // Any wildcard in the dirty set pulls its whole earlier prefix in
    // (worst case = full re-run; the correct-over-fast contract).
    auto applyWildcardPrefix =
        [this, &dirty, &work, &var, &externalDefs]() {
        bool again = true;
        while (again) {
            again = false;
            for (size_t i = 0; i < facts_.size(); ++i)
                if (dirty[i] && facts_[i].wildcardReads)
                    for (size_t k = 0; k < i; ++k) {
                        // Pure defs of the changed var AND of every
                        // externally-set param stay skipped —
                        // re-running them would clobber param values
                        // (the current slider OR another one) back to
                        // script defaults.
                        bool pureOfParam =
                            facts_[k].writes.count(var)
                            && !facts_[k].reads.count(var)
                            && !facts_[k].wildcardReads;
                        if (!pureOfParam) {
                            for (const auto &pv : externalDefs) {
                                if (facts_[k].writes.count(pv)
                                    && !facts_[k].reads.count(pv)
                                    && !facts_[k].wildcardReads) {
                                    pureOfParam = true;
                                    break;
                                }
                            }
                        }
                        if (pureOfParam)
                            continue;
                        if (!dirty[k]) {
                            dirty[k] = 1;
                            work.push_back(k);
                            if (facts_[k].wildcardReads)
                                again = true;
                        }
                    }
        }
    };
    applyWildcardPrefix();
    while (!work.empty()) {
        size_t i = work.back();
        work.pop_back();
        for (size_t j : downstream_[i])
            if (!dirty[j]) {
                dirty[j] = 1;
                work.push_back(j);
            }
    }
    applyWildcardPrefix();   // closure may reach new wildcards
    std::vector<size_t> out;
    for (size_t i = 0; i < facts_.size(); ++i)
        if (dirty[i])
            out.push_back(i);
    return out;
}

}  // namespace nk
