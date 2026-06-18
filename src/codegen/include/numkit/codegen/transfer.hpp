// codegen/include/numkit/codegen/transfer.hpp
//
// Type transfer functions — the per-builtin rules the inference dataflow
// applies at call nodes. A transfer function maps the abstract argument
// info at a call site to the inferred type of the builtin's result.
//
// These live in src/codegen (NOT next to the builtin implementations):
// codegen is an L2 pass above the toolboxes, so a toolbox cannot depend
// on the lattice types. The registry describes the builtins from the
// outside and is validated against them by the differential test
// (tests/transfer_diff_test.cpp) — drift is caught automatically rather
// than prevented by co-location. See src/codegen/DESIGN.md §4.
//
// Organised by family, one .cpp per family under src/transfer/ (mirrors
// the src/math, src/lang category split): constructors, elementwise,
// reductions, shape, cast, bespoke.

#pragma once

#include <numkit/codegen/type_lattice.hpp>

#include <cstddef>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace numkit::codegen {

// Abstract description of one argument at a call site: its inferred type
// plus its known-constant facet. The constant drives shape-from-value
// (the `n` in linspace(_,_,n) / zeros(n)); see DESIGN.md §4.
struct ArgInfo {
    InferredType type;
    ConstVal     constant;

    ArgInfo() = default;
    ArgInfo(InferredType t, ConstVal c) : type(t), constant(c) {}

    static ArgInfo of(InferredType t, ConstVal c = ConstVal::unknown())
    {
        return ArgInfo(t, c);
    }
    // Convenience: a scalar of dtype `dt` with a known constant value `v`
    // (the common literal-argument case, e.g. the `64` in linspace).
    static ArgInfo scalarConst(ValueType dt, double v)
    {
        return ArgInfo(InferredType::scalar(dt), ConstVal::known(v));
    }
};

// A transfer function: abstract args -> inferred type of the (first)
// result. Multi-output (`[a,b] = ...`) is a later extension.
using TransferFn = std::function<InferredType(const std::vector<ArgInfo> &)>;

// builtin name -> transfer function. A name with no entry yields Dynamic
// (the sound fallback: unknown builtin -> boxed Value).
class TransferRegistry {
public:
    void add(std::string name, TransferFn fn);

    bool         has(const std::string &name) const;
    std::size_t  size() const;

    // Result type for a call to `name` with `args`; Dynamic if `name`
    // has no registered transfer.
    InferredType apply(const std::string &name,
                       const std::vector<ArgInfo> &args) const;

private:
    std::unordered_map<std::string, TransferFn> table_;
};

// Populate a registry with every standard transfer (dispatches to the
// per-family registrars below).
void registerStandardTransfers(TransferRegistry &reg);

// ── Per-family registrars (one .cpp each under src/transfer/) ─────────
void registerConstructorTransfers(TransferRegistry &reg);
// future: registerElementwiseTransfers / registerReductionTransfers / ...

} // namespace numkit::codegen
