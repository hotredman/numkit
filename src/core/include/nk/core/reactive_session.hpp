/// @file reactive_session.hpp
/// @ingroup group_core
///
/// ReactiveSession — incremental re-evaluation of a linear script
/// (Play portion B2; also the engine core for the future reactive
/// table editor).
///
/// CONTRACTS (pinned in dev-docs/memory + the Play todo):
///   ONE semantics — dirty statements execute through the SAME
///     per-statement path (`runOneChunk`) the engine's VM script-split
///     uses; there is no second interpreter.
///   EQUIVALENCE PROPERTY — for any sequence of param writes,
///     evalDirty() leaves the workspace IDENTICAL to a full re-run of
///     the script with those params pre-set. This is the acceptance
///     gate (portion B3 tests assert it byte-for-byte).
///   Wildcard safety — statically unresolvable statements (eval, top
///     control flow, clear) dirty the tail via DependencyGraph; worst
///     case degrades to a full tail re-run, never a wrong result.
///
/// Param writes: the value lands in a scratch variable and a NATIVE
/// assignment statement (`var = __nk_reactive_param__;`,
/// `C{r,c} = __nk_reactive_param__;`) runs through eval — growth,
/// type checks and cell semantics are the engine's own, zero value
/// marshalling.
///
/// Cycles: DependencyGraph edges are strictly forward (reader →
/// earlier writer), so statement-level cycles are STRUCTURALLY
/// impossible; mutual cell references (`C{1,1}=C{2,2};
/// C{2,2}=C{1,1}+1`) resolve deterministically one-pass, like any
/// MATLAB script. detectCycles() runs a Tarjan SCC pass as a cheap
/// invariant guard (expected empty) for future cell-level graphs.

#pragma once

#include <nk/core/ast.hpp>
#include <nk/core/dependency_graph.hpp>
#include <nk/core/types.hpp>
#include <nk/figure/figure_manager.hpp>

#include <cstdint>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace nk {

class Engine;

class ReactiveSession
{
public:
    /// The engine is NOT owned; the session must outlive uses of it
    /// with that engine (workspace state is shared).
    explicit ReactiveSession(Engine &engine);

    /// Parse `source`, compute facts + DAG, run the initial FULL pass
    /// (whole-source eval — script/function semantics identical to a
    /// plain run). Returns false on parse or initial-run error
    /// (see lastError()).
    bool load(const std::string &source);

    /// Whole-variable parameter write (Play sliders). Marks the
    /// dependency-dirty set; the script's own pure definitions of the
    /// variable are skipped (the param replaces them).
    void setParam(const std::string &var, Value value);

    /// Single-cell write (the reactive table editor). Native cell
    /// assignment via the engine; marks the ELEMENT-dirty set — only
    /// formulas whose READ regions touch (r,c) re-run.
    void setParamElement(const std::string &var, size_t r, size_t c,
                         Value value);

    /// Execute the dirty statements in source order through the
    /// per-statement seam. Returns how many ran; on error, stops and
    /// keeps the remaining dirty set (see lastError()).
    size_t evalDirty();

    /// True when something is pending for evalDirty().
    bool hasDirty() const { return !dirty_.empty(); }

    /// Tarjan SCCs over the dependency edges (invariant guard —
    /// expected empty while edges stay forward-only).
    std::vector<std::vector<size_t>> detectCycles() const;

    const DependencyGraph &graph() const { return *graph_; }
    size_t statementCount() const { return statements_.size(); }
    const std::string &lastError() const { return lastError_; }

    /// Bumped on every committed write (param/element/evalDirty) — the
    /// buffer-generation source for zero-copy array views (portion C).
    uint64_t generation() const { return generation_; }

private:
    void markDirty(const std::vector<size_t> &indices);
    Value runStatement(size_t index);

    Engine &engine_;
    std::shared_ptr<const std::string> src_;
    std::vector<ASTNodePtr> statements_;   // program children (owned)
    std::unique_ptr<DependencyGraph> graph_;
    // Figure-state rollback points, one per statement (captured after
    // that statement ran). evalDirty restores the checkpoint of the
    // last CLEAN statement before the dirty set, so re-run graphics
    // statements reproduce the full-re-run figure (no `hold on`
    // line accumulation across ticks).
    std::vector<FigureManager::FigureCheckpoint> checkpoints_;
    std::set<size_t> dirty_;
    std::set<std::string> params_;   // externally-set variables
    std::string lastError_;
    uint64_t generation_ = 0;
};

}  // namespace nk
