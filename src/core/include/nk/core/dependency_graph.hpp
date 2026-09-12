/// @file dependency_graph.hpp
/// @ingroup group_core
///
/// Statement-level variable dependency analysis (NK Play, portion A).
///
/// Facts per TOP-LEVEL statement of a script:
///   Writes — variables the statement (re)defines: `x = …`,
///            `[a, b] = …`, `A(i) = …` (also READS A — merge-write).
///   Reads  — identifiers the statement evaluates: RHS expressions,
///            call arguments, index expressions.
///
/// The DAG edges follow NEAREST-DEFINITION wins: a read of `x` in
/// statement j depends on the LAST i < j that writes x (linear script
/// semantics — earlier defs are dead at j).
///
/// WILDCARD CONTRACT (correctness over speed): a statement the visitor
/// cannot statically resolve — top-level `if`/`for`/`while`/`switch`/
/// `try`, calls to `eval`/`feval`/`assignin`, `clear`, `global` — gets
/// Reads = * : it depends on EVERY earlier write, and everything after
/// it depends transitively. Worst case degenerates to re-running the
/// tail from that statement — always CORRECT, never wrong-fast. The
/// acceptance property (portion B): evalDirty() output == full re-run,
/// byte-for-byte.
///
/// Script-local FUNCTION DEFINITIONS: a def statement writes the
/// function name; a call to a known local function reads its argument
/// variables (script-local functions cannot touch the script workspace
/// in MATLAB — arguments only), so facts stay exact rather than
/// wildcard. Function BODIES are not analysed in v1: if a body itself
/// contains eval/assignin the equivalence property still holds via the
/// rarer full-tail re-run (the call statement is re-executed whenever
/// any argument changed; a body reading only its parameters cannot see
/// stale globals).

#pragma once

#include <nk/core/ast.hpp>
#include <nk/core/elem_regions.hpp>

#include <set>
#include <string>
#include <vector>

namespace nk {

struct StatementFacts
{
    bool wildcardReads = false;      // Reads = * (opaque statement)
    std::set<std::string> reads;
    std::set<std::string> writes;

    // Fine-grained element access (portion B1): variable → regions
    // touched on the READ side / WRITE side. All accesses that are not
    // statically literal collapse to VarRegions::all (conservative).
    // Coarse `reads`/`writes` stay authoritative; the maps refine them.
    ElemAccessMap elemReads;
    ElemAccessMap elemWrites;

    // Variables whose coarse `reads` entry came ONLY from a merge-write
    // LHS (`C{5,2} = v` — the LHS does NOT read the old value; the
    // coarse read exists for portion-A dirtySetFor compatibility).
    // Element-level dirty sets use this to skip cell definitions.
    std::set<std::string> lhsMergeBases;
};

/// Compute facts for every top-level statement of a parsed program
/// (Parser::parse() result — node children are the statements).
std::vector<StatementFacts> computeStatementFacts(const ASTNode *program);

/// Dependency DAG over statement indices. Edge i → j means "j must
/// re-run when i re-runs" (j reads the nearest def i provides, or j is
/// a wildcard reading everything, or i is a wildcard and j follows it).
class DependencyGraph
{
public:
    /// Build from facts. Nearest-definition edges for reads; wildcard
    /// statements depend on all prior statements AND every later
    /// statement depends on them.
    explicit DependencyGraph(std::vector<StatementFacts> facts);

    /// Indices that must re-run when `var` is assigned externally.
    /// A PURE DEFINITION of var (`var = literal` — writes, never
    /// reads) is SKIPPED: setParam replaces it (re-running would reset
    /// the slider). Affected = readers + merge-writers + wildcard
    /// statements that may invisibly write var + transitive closure.
    std::vector<size_t> dirtySetFor(const std::string &var) const;

    /// With the set of EXTERNALLY-DEFINED params (ReactiveSession):
    /// the wildcard-prefix rule additionally skips the pure defs of
    /// EVERY param in the set (a prefix re-run must not clobber other
    /// sliders' values back to script defaults).
    std::vector<size_t> dirtySetFor(
        const std::string &var,
        const std::set<std::string> &externalDefs) const;

    /// Element-level variant: external write of ONE CELL (the table
    /// editor's setParamElement). Affected = statements whose READ
    /// regions touch (r,c) on var + wildcards + statements that
    /// merge-write var while also reading it + transitive closure.
    /// Statements that only WRITE regions of var (definitions of other
    /// cells) stay clean — their inputs did not change.
    std::vector<size_t> dirtySetForElement(const std::string &var,
                                           size_t r, size_t c) const;
    std::vector<size_t> dirtySetForElement(
        const std::string &var, size_t r, size_t c,
        const std::set<std::string> &externalDefs) const;

    const std::vector<StatementFacts> &facts() const { return facts_; }
    size_t size() const { return facts_.size(); }
    /// Direct dependents of statement i (edges i → j). Const accessor
    /// for cycle detection / debug tooling.
    const std::vector<size_t> &downstreamOf(size_t i) const
    {
        return downstream_[i];
    }

private:
    std::vector<StatementFacts> facts_;
    std::vector<std::vector<size_t>> downstream_;   // i → dependents
};

}  // namespace nk
