// codegen/include/numkit/codegen/emitter.hpp
//
// The C++ emitter — turns a typed AST into C++ source text.
//
// Sub-brick 3a: the unboxed-scalar expression core (literals, identifiers,
// scalar arithmetic / comparison / logical / power) -> a plain C++
// expression. Sub-bricks 3b-3f add statements, control flow, builtin
// calls, indexing and whole-function emission, wired together by the
// stateful Emitter (in emitter.cpp) and exposed here via emitFunction().
//
// ABI (DESIGN.md §6, RawBuffer variant): the whole-function emitter
// produces a SELF-CONTAINED translation unit depending only on the C++
// standard library — scalars are unboxed primitives, an array parameter
// `a` becomes `const T* a, std::size_t a_len`, and a single output array
// becomes a trailing caller-allocated out-param `T* ret, std::size_t
// ret_len` (void return). This makes the end-to-end differential gate
// (compile with an external compiler + run + diff) robust without linking
// the numkit runtime. The Value-ABI variant (array args/returns as
// numkit::Value) is a thin wrapper around the identical numeric core.
//
// Contract 2 (DESIGN.md §10): the emitter emits a fast unboxed form ONLY
// for a construct it can prove correct; anything outside the supported
// subset throws std::runtime_error (the explicit boundary) rather than
// emitting code that might be wrong.

#pragma once

#include <numkit/codegen/classinfo.hpp>
#include <numkit/codegen/inference.hpp>
#include <numkit/codegen/monomorphize.hpp>
#include <numkit/codegen/transfer.hpp>
#include <numkit/codegen/type_lattice.hpp>

#include <numkit/core/ast.hpp>

#include <string>
#include <unordered_map>
#include <vector>

namespace numkit::codegen {

// The C++ type name for an unboxable scalar dtype (double / float /
// std::complex<double> / bool / intN_t). Throws std::runtime_error for a
// dtype with no scalar C++ mapping (CELL/STRUCT/STRING/…).
std::string cppScalarType(ValueType dtype);

// A round-trip C++ double literal for `v` (shortest form; integers get a
// trailing .0; Inf/NaN map to std::numeric_limits forms).
std::string formatDoubleLiteral(double v);

// Emit a C++ expression string for an unboxed-scalar AST expression
// (NUMBER/IMAG/BOOL literal, IDENTIFIER, scalar BINARY_OP / UNARY_OP).
// `^`/`.^` lower to std::pow. Throws std::runtime_error on a node kind
// outside this pure-scalar subset (calls, indexing — handled by Emitter).
std::string emitScalarExpr(const ASTNode &expr);

// Declaration type per variable = the lattice JOIN of every type it is
// ever assigned in `body` (so one hoisted C++ declaration is valid at
// every program point). RHS types are evaluated under the inference
// fixpoint env, making the result a sound over-approximation. `entryEnv`
// seeds parameter types, which are also recorded.
using DeclTypeMap = std::unordered_map<std::string, InferredType>;
DeclTypeMap computeDeclTypes(const ASTNode &body, const TypeEnv &entryEnv,
                             const TransferRegistry &reg,
                             const ClassRegistry *classes = nullptr);

// Entry-point parameter type annotation (the inference seed for one
// function). The order must match the FUNCTION_DEF's parameter list.
struct ParamSpec {
    std::string  name;
    InferredType type;
};

// A complete emitted function.
struct EmittedFunction {
    std::string source;     // a self-contained C++ translation unit
    std::string name;       // the emitted C++ symbol
    std::string signature;  // the function's declaration (for a header)
};

// Bridged emission (DESIGN.md §6a). OPT-IN: when disabled (the default), a
// call the emitter cannot lower throws (the TU stays stdlib-only,
// self-contained — the no-kludge litmus). When enabled, such a call — IF
// inference proves its result is an unboxed scalar — is emitted as a C-ABI
// call into the numkit runtime (nk_rt::bridge_scalar), and the TU #includes
// `runtimeHeader` (nk_codegen_rt.h) + links the runtime. Correctness is
// unchanged either way; bridging only widens what compiles.
struct BridgeOptions {
    bool        enabled = false;
    std::string runtimeHeader;  // include path to nk_codegen_rt.h (when enabled)
};

// Emit one FUNCTION_DEF as a self-contained C++ TU (RawBuffer ABI, above).
// Throws std::runtime_error on any construct outside the supported subset.
EmittedFunction emitFunction(const ASTNode &funcDef,
                             const std::vector<ParamSpec> &params,
                             const TransferRegistry &reg,
                             const ClassRegistry *classes = nullptr,
                             const BridgeOptions &bridge = {});

// Emit a whole program (build plan §12, brick 1b): the entry function plus
// every user-function specialisation it transitively calls, monomorphised
// by argument types, into one self-contained C++ TU. `reg` must already
// carry the user functions from `table` (registerUserFunctions) so calls
// type. `EmittedFunction::name` is the mangled entry symbol to call.
//
// v1b scope: interprocedural calls pass and return UNBOXED SCALARS (a call
// whose arg or result is an array/Dynamic is refused — array-valued
// interprocedural calls are a later brick). The entry itself may still take
// /return arrays (RawBuffer ABI) as in emitFunction.
EmittedFunction emitProgram(const ASTNode &entryDef,
                            const std::vector<ParamSpec> &params,
                            const FunctionTable &table, const TransferRegistry &reg,
                            const ClassRegistry *classes = nullptr,
                            const BridgeOptions &bridge = {});

// Emit the C++ struct for a class (build plan §12, brick 5). The object's
// storage is the same for value and handle classes — handle-ness changes
// only the VARIABLE type (`nk_rt::handle<Foo>` vs `Foo`), not this struct.
// v1: stored scalar properties only (a non-scalar field is refused).
std::string emitClassStruct(const ClassInfo &ci);

// Emit a SCALAR function wrapped as a loadable numkit plugin (tiered
// acceleration / codegen-as-plugin, DESIGN.md §6b): the compiled function
// (emitFunction, RawBuffer ABI) + the plugin ABI hooks
// (nk_plugin_abi_version / nk_plugin_register) + an nk_fn that marshals
// nk_val<->scalar through the host table and calls it. Compiled to a shared
// library and loaded with nk_load_plugin, the result makes `exportName` run
// native inside a live session.
//
// v1 scope: every parameter AND the single output must be an unboxed scalar
// (verified by inference); a non-scalar / multi-output / Dynamic function is
// refused via the explicit boundary (array-valued tiering needs an
// output-size protocol — a later brick). `abiHeaderPath` is the include path
// to nk_plugin.h that the generated TU will #include.
std::string emitScalarPlugin(const ASTNode &funcDef,
                             const std::vector<ParamSpec> &params,
                             const TransferRegistry &reg,
                             const std::string &exportName,
                             const std::string &abiHeaderPath,
                             const ClassRegistry *classes = nullptr);

} // namespace numkit::codegen
