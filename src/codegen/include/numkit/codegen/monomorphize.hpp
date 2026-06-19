// codegen/include/numkit/codegen/monomorphize.hpp
//
// Interprocedural inference (build plan §12, brick 1): the monomorphizing
// engine shared by user-function calls AND class methods — `f(args)` and
// `obj.method(args)` both type by specialising a callee body to its
// argument types.
//
// Mechanism: every user FUNCTION_DEF is registered into the TransferRegistry
// as a transfer that infers the callee's body under the call's argument
// types. Because inferExpr already routes a non-variable call through
// `reg.apply(name, args)`, no change to the inference driver is needed —
// user functions plug in exactly where builtins do, and (MATLAB path-shadow
// semantics) override a same-named builtin.
//
// Soundness: a call that re-enters a function already being inferred
// (recursion) returns Dynamic — the sound break that guarantees
// termination. A true Bottom-start fixpoint that types recursive returns
// precisely is a later refinement; Dynamic only loses precision, never
// soundness.

#pragma once

#include <numkit/codegen/inference.hpp>
#include <numkit/codegen/transfer.hpp>

#include <numkit/core/ast.hpp>

#include <string>
#include <unordered_map>

namespace numkit::codegen {

// Name -> FUNCTION_DEF node, collected from one or more parsed programs.
// The nodes are borrowed (owned by the caller's AST, which must outlive
// the table and any registry built from it).
class FunctionTable {
public:
    void           add(const ASTNode &funcDef);  // funcDef.type == FUNCTION_DEF
    const ASTNode *find(const std::string &name) const;
    bool           has(const std::string &name) const { return find(name) != nullptr; }
    std::size_t    size() const { return defs_.size(); }

    const std::unordered_map<std::string, const ASTNode *> &entries() const { return defs_; }

private:
    std::unordered_map<std::string, const ASTNode *> defs_;
};

// Collect every FUNCTION_DEF reachable in `root` (top-level and nested)
// into `table`. Later definitions win (last-on-path shadowing).
void collectFunctions(const ASTNode &root, FunctionTable &table);

// Infer the (first) return type of `funcDef` given argument infos, under
// `reg` (which should already carry the user functions so nested calls
// resolve). `classes` (optional) lets the body's field accesses type — it
// is what makes method-body inference work. Returns Dynamic when it cannot
// be typed: arity mismatch, not exactly one output (MVP), or the output is
// never assigned.
InferredType inferFunctionReturn(const ASTNode &funcDef,
                                 const std::vector<ArgInfo> &args,
                                 const TransferRegistry &reg,
                                 const ClassRegistry *classes = nullptr);

// Register each method of each class in `classes` under the transfer key
// "ClassName::methodName" (its first parameter is the object). A method
// call `obj.m(a)` resolves via reg.apply("Class::m", {self, a}). Recursion
// breaks to Dynamic. `reg`, `classes`, and the underlying AST must outlive
// the registry's use.
void registerClassMethods(TransferRegistry &reg, const ClassRegistry &classes);

// Register each function in `table` as a body-inferring transfer in `reg`
// (recursion -> Dynamic). `reg` and `table` must outlive the registry's
// use. Call after registerStandardTransfers so user functions shadow
// same-named builtins.
void registerUserFunctions(TransferRegistry &reg, const FunctionTable &table);

} // namespace numkit::codegen
