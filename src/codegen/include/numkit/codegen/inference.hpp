// codegen/include/numkit/codegen/inference.hpp
//
// The inference driver — the engine middle that turns the transfer
// rules into actual type inference over parsed code. It walks the AST
// maintaining an environment of variable -> abstract value, evaluating
// each expression through the transfer registry and propagating
// compile-time constants (so a literal `n` flows into a size
// constructor's shape).
//
// MVP scope: straight-line code (a block of assignments / expression
// statements). It is SOUND on any input — statement kinds not yet
// modelled (control flow, multi-assign, indexed assign) conservatively
// set every variable they could touch to Dynamic. A control-flow graph
// with join/fixpoint that infers PRECISELY through if/for/while is the
// next brick (cfg.hpp); this driver is the expression/statement core it
// will build on.

#pragma once

#include <numkit/codegen/transfer.hpp>
#include <numkit/codegen/type_lattice.hpp>

#include <numkit/core/ast.hpp>

#include <string>
#include <unordered_map>

namespace numkit::codegen {

// Abstract value at a program point: the inferred type plus the
// constant facet (a known literal value drives shape-from-value).
struct AbstractValue {
    InferredType type;
    ConstVal     constant = ConstVal::unknown();

    static AbstractValue dynamic() { return {InferredType::dynamic(), ConstVal::unknown()}; }

    // The ArgInfo a transfer function consumes for this value.
    ArgInfo asArg() const { return ArgInfo(type, constant); }

    bool operator==(const AbstractValue &o) const
    {
        return type == o.type && constant == o.constant;
    }
    bool operator!=(const AbstractValue &o) const { return !(*this == o); }
};

// Lattice join of two abstract values (type join + constant join) — used
// at control-flow merge points.
AbstractValue join(const AbstractValue &a, const AbstractValue &b);

// Variable name -> abstract value. A name not present reads back as
// Dynamic (an externally-supplied or use-before-def variable is
// conservatively unknown).
class TypeEnv {
public:
    void          set(const std::string &name, AbstractValue v);
    AbstractValue get(const std::string &name) const;
    bool          has(const std::string &name) const;
    std::size_t   size() const { return vars_.size(); }

    const std::unordered_map<std::string, AbstractValue> &entries() const { return vars_; }
    bool operator==(const TypeEnv &o) const { return vars_ == o.vars_; }
    bool operator!=(const TypeEnv &o) const { return !(*this == o); }

private:
    std::unordered_map<std::string, AbstractValue> vars_;
};

// Join two environments at a control-flow merge: a variable in both is
// joined value-wise; a variable in only one path is conservatively
// Dynamic (it may be undefined on the other path).
TypeEnv joinEnv(const TypeEnv &a, const TypeEnv &b);

// Infer the abstract value of an expression node. `env` is consulted for
// identifiers and to disambiguate `x(i)` (indexed read of a variable)
// from `f(x)` (a call), exactly as the interpreter does.
AbstractValue inferExpr(const ASTNode &expr, const TypeEnv &env,
                        const TransferRegistry &reg);

// Optional decl-type recorder: a map joined at every definition site with
// the type the variable is assigned THERE (its program-point type, not the
// post-merge value). The emitter consumes this to choose one C++ type per
// local that is valid at every point. nullptr disables recording.
using DeclTypeRecorder = std::unordered_map<std::string, InferredType>;

// Apply one statement to the environment (straight-line). Unmodelled
// statement kinds set every variable they assign to Dynamic (sound). When
// `declOut` is non-null, every definition (including loop variables and
// conservatively-Dynamic assignments) is joined into it at its actual
// program point — so a loop-body temporary records its in-loop type, not
// its (maybe-undefined) post-loop type.
void inferStmt(const ASTNode &stmt, TypeEnv &env, const TransferRegistry &reg,
               DeclTypeRecorder *declOut = nullptr);

// Infer a whole parsed program (the BLOCK Parser::parse() returns),
// returning the final environment.
TypeEnv inferProgram(const ASTNode &root, const TransferRegistry &reg);

} // namespace numkit::codegen
