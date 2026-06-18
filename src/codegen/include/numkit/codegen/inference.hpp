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
};

// Variable name -> abstract value. A name not present reads back as
// Dynamic (an externally-supplied or use-before-def variable is
// conservatively unknown).
class TypeEnv {
public:
    void          set(const std::string &name, AbstractValue v);
    AbstractValue get(const std::string &name) const;
    bool          has(const std::string &name) const;
    std::size_t   size() const { return vars_.size(); }

private:
    std::unordered_map<std::string, AbstractValue> vars_;
};

// Infer the abstract value of an expression node. `env` is consulted for
// identifiers and to disambiguate `x(i)` (indexed read of a variable)
// from `f(x)` (a call), exactly as the interpreter does.
AbstractValue inferExpr(const ASTNode &expr, const TypeEnv &env,
                        const TransferRegistry &reg);

// Apply one statement to the environment (straight-line). Unmodelled
// statement kinds set every variable they assign to Dynamic (sound).
void inferStmt(const ASTNode &stmt, TypeEnv &env, const TransferRegistry &reg);

// Infer a whole parsed program (the BLOCK Parser::parse() returns),
// returning the final environment.
TypeEnv inferProgram(const ASTNode &root, const TransferRegistry &reg);

} // namespace numkit::codegen
