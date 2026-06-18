// codegen/src/inference.cpp — see inference.hpp.

#include <numkit/codegen/inference.hpp>

#include <vector>

namespace numkit::codegen {

// ── TypeEnv ───────────────────────────────────────────────────────────
void TypeEnv::set(const std::string &name, AbstractValue v)
{
    vars_.insert_or_assign(name, v);
}

AbstractValue TypeEnv::get(const std::string &name) const
{
    const auto it = vars_.find(name);
    return it == vars_.end() ? AbstractValue::dynamic() : it->second;
}

bool TypeEnv::has(const std::string &name) const
{
    return vars_.find(name) != vars_.end();
}

namespace {

// MATLAB operator -> its function name (the key transfer rules register
// under). Returns empty for an unhandled operator (-> Dynamic). The
// short-circuit operators && / || are intentionally left unmapped.
std::string opFuncName(const std::string &op, bool unary)
{
    if (unary) {
        if (op == "-")  return "uminus";
        if (op == "+")  return "uplus";
        if (op == "~" || op == "!") return "not";
        if (op == "'")  return "ctranspose";
        if (op == ".'") return "transpose";
        return {};
    }
    if (op == "+")  return "plus";
    if (op == "-")  return "minus";
    if (op == "*")  return "mtimes";
    if (op == ".*") return "times";
    if (op == "/")  return "mrdivide";
    if (op == "./") return "rdivide";
    if (op == "\\") return "mldivide";
    if (op == ".\\") return "ldivide";
    if (op == "^")  return "mpower";
    if (op == ".^") return "power";
    if (op == "==") return "eq";
    if (op == "~=" || op == "!=") return "ne";
    if (op == "<")  return "lt";
    if (op == ">")  return "gt";
    if (op == "<=") return "le";
    if (op == ">=") return "ge";
    if (op == "&")  return "and";
    if (op == "|")  return "or";
    return {};  // && / || and anything else -> Dynamic
}

// A scalar-valued abstract value? (used to decide x(i) element access)
bool isScalarValue(const AbstractValue &v)
{
    return v.type.isConcrete() && v.type.shape.isScalar();
}

// Result of indexing a variable `var` with the given argument values:
//   * all-scalar indices on a concrete array -> element access -> scalar
//     of the array's dtype (the hot x(n)-in-a-loop case);
//   * otherwise (sub-array / non-concrete) -> concrete dtype, Unknown
//     shape, or Dynamic if the variable's type isn't known.
AbstractValue indexResult(const AbstractValue &var,
                          const std::vector<AbstractValue> &args)
{
    if (!var.type.isConcrete())
        return AbstractValue::dynamic();
    bool allScalar = true;
    for (const auto &a : args)
        if (!isScalarValue(a)) { allScalar = false; break; }
    const Shape sh = (allScalar && !args.empty()) ? Shape::scalar() : Shape::unknown();
    return {InferredType::concrete(var.type.dtype, sh), ConstVal::unknown()};
}

} // namespace

AbstractValue inferExpr(const ASTNode &expr, const TypeEnv &env,
                        const TransferRegistry &reg)
{
    switch (expr.type) {
    case NodeType::NUMBER_LITERAL:
        return {InferredType::scalar(ValueType::DOUBLE), ConstVal::known(expr.numValue)};

    case NodeType::IMAG_LITERAL:
        return {InferredType::scalar(ValueType::COMPLEX), ConstVal::unknown()};

    case NodeType::BOOL_LITERAL:
        return {InferredType::scalar(ValueType::LOGICAL),
                ConstVal::known(expr.boolValue ? 1.0 : 0.0)};

    case NodeType::STRING_LITERAL:
        // single-quoted char row 1 x len
        return {InferredType::concrete(ValueType::CHAR,
                                       Shape::dims(1, expr.strValue.size())),
                ConstVal::unknown()};

    case NodeType::DQSTRING_LITERAL:
        // double-quoted string scalar
        return {InferredType::scalar(ValueType::STRING), ConstVal::unknown()};

    case NodeType::IDENTIFIER:
        return env.get(expr.strValue);

    case NodeType::BINARY_OP: {
        if (expr.children.size() != 2) return AbstractValue::dynamic();
        const std::string fn = opFuncName(expr.strValue, /*unary=*/false);
        if (fn.empty()) return AbstractValue::dynamic();
        const AbstractValue a = inferExpr(*expr.children[0], env, reg);
        const AbstractValue b = inferExpr(*expr.children[1], env, reg);
        return {reg.apply(fn, {a.asArg(), b.asArg()}), ConstVal::unknown()};
    }

    case NodeType::UNARY_OP: {
        if (expr.children.size() != 1) return AbstractValue::dynamic();
        const std::string fn = opFuncName(expr.strValue, /*unary=*/true);
        if (fn.empty()) return AbstractValue::dynamic();
        const AbstractValue a = inferExpr(*expr.children[0], env, reg);
        return {reg.apply(fn, {a.asArg()}), ConstVal::unknown()};
    }

    case NodeType::CALL: {
        if (expr.children.empty()) return AbstractValue::dynamic();
        const ASTNode &callee = *expr.children[0];

        // Evaluate the arguments (children[1..]).
        std::vector<AbstractValue> argVals;
        std::vector<ArgInfo>       argInfos;
        argVals.reserve(expr.children.size() - 1);
        argInfos.reserve(expr.children.size() - 1);
        for (std::size_t i = 1; i < expr.children.size(); ++i) {
            argVals.push_back(inferExpr(*expr.children[i], env, reg));
            argInfos.push_back(argVals.back().asArg());
        }

        if (callee.type == NodeType::IDENTIFIER) {
            const std::string &name = callee.strValue;
            // MATLAB ambiguity: `name(...)` is an indexed read when `name`
            // is a variable in scope, otherwise a function call.
            if (env.has(name))
                return indexResult(env.get(name), argVals);
            return {reg.apply(name, argInfos), ConstVal::unknown()};
        }
        // Non-identifier callee (chained / expression callee) — deferred.
        return AbstractValue::dynamic();
    }

    default:
        return AbstractValue::dynamic();
    }
}

namespace {

// Conservatively mark every variable assigned anywhere within `node` as
// Dynamic (used for statement kinds the straight-line driver does not
// model precisely yet — control flow, etc.). Sound over-approximation.
void markAssignedDynamic(const ASTNode &node, TypeEnv &env)
{
    if (node.type == NodeType::ASSIGN && !node.children.empty()
        && node.children[0]->type == NodeType::IDENTIFIER) {
        env.set(node.children[0]->strValue, AbstractValue::dynamic());
    }
    if (node.type == NodeType::MULTI_ASSIGN)
        for (const auto &rn : node.returnNames)
            if (!rn.empty()) env.set(rn, AbstractValue::dynamic());

    for (const auto &c : node.children)
        if (c) markAssignedDynamic(*c, env);
    for (const auto &br : node.branches) {
        if (br.first)  markAssignedDynamic(*br.first, env);
        if (br.second) markAssignedDynamic(*br.second, env);
    }
    if (node.elseBranch) markAssignedDynamic(*node.elseBranch, env);
}

} // namespace

void inferStmt(const ASTNode &stmt, TypeEnv &env, const TransferRegistry &reg)
{
    switch (stmt.type) {
    case NodeType::BLOCK:
        for (const auto &c : stmt.children)
            if (c) inferStmt(*c, env, reg);
        return;

    case NodeType::ASSIGN: {
        if (stmt.children.size() != 2) return;
        const ASTNode &lhs = *stmt.children[0];
        if (lhs.type == NodeType::IDENTIFIER) {
            env.set(lhs.strValue, inferExpr(*stmt.children[1], env, reg));
        } else if (!lhs.children.empty()
                   && lhs.children[0]->type == NodeType::IDENTIFIER) {
            // Indexed / field assign (x(i)=..., s.f=...): the base
            // variable's type/shape may change -> conservatively Dynamic.
            env.set(lhs.children[0]->strValue, AbstractValue::dynamic());
        }
        return;
    }

    case NodeType::EXPR_STMT:
        // Pure type inference: an expression statement has no binding
        // effect we model (side effects are out of scope).
        return;

    default:
        // Control flow, multi-assign, function defs, etc. — not modelled
        // precisely yet. Sound fallback: anything they assign -> Dynamic.
        markAssignedDynamic(stmt, env);
        return;
    }
}

TypeEnv inferProgram(const ASTNode &root, const TransferRegistry &reg)
{
    TypeEnv env;
    inferStmt(root, env, reg);
    return env;
}

} // namespace numkit::codegen
