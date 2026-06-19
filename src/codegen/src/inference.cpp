// codegen/src/inference.cpp — see inference.hpp.

#include <numkit/codegen/inference.hpp>

#include <numkit/codegen/classinfo.hpp>

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

// ── Lattice joins for control-flow merges ─────────────────────────────
AbstractValue join(const AbstractValue &a, const AbstractValue &b)
{
    return {join(a.type, b.type), join(a.constant, b.constant)};
}

TypeEnv joinEnv(const TypeEnv &a, const TypeEnv &b)
{
    TypeEnv out;
    for (const auto &[name, va] : a.entries()) {
        if (b.has(name)) out.set(name, join(va, b.get(name)));
        else             out.set(name, AbstractValue::dynamic());  // maybe-undef on b
    }
    for (const auto &[name, vb] : b.entries())
        if (!a.has(name)) out.set(name, AbstractValue::dynamic());  // maybe-undef on a
    return out;
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
                        const TransferRegistry &reg, const ClassRegistry *classes)
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

    case NodeType::COLON_EXPR:
        // a:b or a:s:b — a double row vector of (statically) unknown
        // length. (Integer-typed colons are a deferred gap.)
        return {InferredType::concrete(ValueType::DOUBLE, Shape::rowVector()),
                ConstVal::unknown()};

    case NodeType::BINARY_OP: {
        if (expr.children.size() != 2) return AbstractValue::dynamic();
        const std::string fn = opFuncName(expr.strValue, /*unary=*/false);
        if (fn.empty()) return AbstractValue::dynamic();
        const AbstractValue a = inferExpr(*expr.children[0], env, reg, classes);
        const AbstractValue b = inferExpr(*expr.children[1], env, reg, classes);
        return {reg.apply(fn, {a.asArg(), b.asArg()}), ConstVal::unknown()};
    }

    case NodeType::UNARY_OP: {
        if (expr.children.size() != 1) return AbstractValue::dynamic();
        const std::string fn = opFuncName(expr.strValue, /*unary=*/true);
        if (fn.empty()) return AbstractValue::dynamic();
        const AbstractValue a = inferExpr(*expr.children[0], env, reg, classes);
        return {reg.apply(fn, {a.asArg()}), ConstVal::unknown()};
    }

    case NodeType::FIELD_ACCESS: {
        // obj.field : strValue = field name, children[0] = object expr.
        // Typed only when a class registry is supplied and the base is a
        // known object; otherwise Dynamic (sound — non-class code, or a
        // struct/handle we cannot class).
        if (!classes || expr.children.empty()) return AbstractValue::dynamic();
        const AbstractValue base = inferExpr(*expr.children[0], env, reg, classes);
        if (!base.type.isObject()) return AbstractValue::dynamic();
        const ClassInfo *ci = classes->byId(base.type.classId);
        if (!ci) return AbstractValue::dynamic();
        const ClassField *f = ci->field(expr.strValue);
        if (!f) return AbstractValue::dynamic();
        return {f->type, ConstVal::unknown()};
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
            argVals.push_back(inferExpr(*expr.children[i], env, reg, classes));
            argInfos.push_back(argVals.back().asArg());
        }

        if (callee.type == NodeType::IDENTIFIER) {
            const std::string &name = callee.strValue;
            // MATLAB ambiguity: `name(...)` is an indexed read when `name`
            // is a variable in scope, otherwise a function call (or a class
            // constructor, registered as a transfer).
            if (env.has(name))
                return indexResult(env.get(name), argVals);
            return {reg.apply(name, argInfos), ConstVal::unknown()};
        }
        // Method call `obj.m(args)`: callee is FIELD_ACCESS. Resolve the
        // object's class, prepend the object as the implicit first argument,
        // and dispatch via the "Class::m" transfer (registerClassMethods).
        if (callee.type == NodeType::FIELD_ACCESS && classes && !callee.children.empty()) {
            const AbstractValue base = inferExpr(*callee.children[0], env, reg, classes);
            if (!base.type.isObject()) return AbstractValue::dynamic();
            const ClassInfo *ci = classes->byId(base.type.classId);
            if (!ci || !ci->method(callee.strValue)) return AbstractValue::dynamic();
            std::vector<ArgInfo> withSelf;
            withSelf.reserve(argInfos.size() + 1);
            withSelf.push_back(base.asArg());
            for (const auto &ai : argInfos) withSelf.push_back(ai);
            return {reg.apply(ci->name + "::" + callee.strValue, withSelf), ConstVal::unknown()};
        }
        // Other non-identifier callee (chained / expression callee) — deferred.
        return AbstractValue::dynamic();
    }

    default:
        return AbstractValue::dynamic();
    }
}

namespace {

// Per-iteration value of a for-loop variable `for v = range`. MATLAB
// iterates the COLUMNS of `range`: a scalar / row / range yields scalars;
// an N-row matrix yields N x 1 columns. A shape we can't pin down yields
// Dynamic (sound).
AbstractValue forIterValue(const InferredType &range)
{
    if (!range.isConcrete()) return AbstractValue::dynamic();
    switch (range.shape.kind) {
    case ShapeKind::Scalar:
    case ShapeKind::RowVector:
        return {InferredType::scalar(range.dtype), ConstVal::unknown()};
    case ShapeKind::KnownDims:
        if (range.shape.rows == 1)  // a row -> scalar per iteration
            return {InferredType::scalar(range.dtype), ConstVal::unknown()};
        return {InferredType::concrete(range.dtype, Shape::dims(range.shape.rows, 1)),
                ConstVal::unknown()};  // columns
    case ShapeKind::ColVector:
        return {InferredType::concrete(range.dtype, Shape::colVector()),
                ConstVal::unknown()};  // single column iteration
    case ShapeKind::Unknown:
    default:
        return AbstractValue::dynamic();
    }
}

// Fixpoint iteration cap. The lattice has tiny height (a variable can
// only widen Concrete -> Concrete' -> Dynamic), so the monotone sequence
// converges in a handful of steps; this is a defensive backstop.
constexpr int kMaxFixpoint = 16;

// Join `t` into a decl-type recorder at variable `name` (Bottom identity
// for a not-yet-present variable). No-op when `declOut` is null.
void recordDecl(DeclTypeRecorder *declOut, const std::string &name,
                const InferredType &t)
{
    if (!declOut) return;
    auto it = declOut->find(name);
    (*declOut)[name] = (it == declOut->end()) ? t : join(it->second, t);
}

// Conservatively mark every variable assigned anywhere within `node` as
// Dynamic (used for statement kinds the straight-line driver does not
// model precisely yet — try/catch, function defs, etc.). Sound.
void markAssignedDynamic(const ASTNode &node, TypeEnv &env,
                         DeclTypeRecorder *declOut = nullptr)
{
    if (node.type == NodeType::ASSIGN && !node.children.empty()
        && node.children[0]->type == NodeType::IDENTIFIER) {
        env.set(node.children[0]->strValue, AbstractValue::dynamic());
        recordDecl(declOut, node.children[0]->strValue, InferredType::dynamic());
    }
    if (node.type == NodeType::MULTI_ASSIGN)
        for (const auto &rn : node.returnNames)
            if (!rn.empty()) {
                env.set(rn, AbstractValue::dynamic());
                recordDecl(declOut, rn, InferredType::dynamic());
            }

    for (const auto &c : node.children)
        if (c) markAssignedDynamic(*c, env, declOut);
    for (const auto &br : node.branches) {
        if (br.first)  markAssignedDynamic(*br.first, env, declOut);
        if (br.second) markAssignedDynamic(*br.second, env, declOut);
    }
    if (node.elseBranch) markAssignedDynamic(*node.elseBranch, env, declOut);
}

} // namespace

void inferStmt(const ASTNode &stmt, TypeEnv &env, const TransferRegistry &reg,
               DeclTypeRecorder *declOut, const ClassRegistry *classes)
{
    switch (stmt.type) {
    case NodeType::BLOCK:
        for (const auto &c : stmt.children)
            if (c) inferStmt(*c, env, reg, declOut, classes);
        return;

    case NodeType::ASSIGN: {
        if (stmt.children.size() != 2) return;
        const ASTNode &lhs = *stmt.children[0];
        const AbstractValue rhs = inferExpr(*stmt.children[1], env, reg, classes);
        if (lhs.type == NodeType::IDENTIFIER) {
            env.set(lhs.strValue, rhs);
            recordDecl(declOut, lhs.strValue, rhs.type);
        } else if (!lhs.children.empty()
                   && lhs.children[0]->type == NodeType::IDENTIFIER) {
            const std::string  &base = lhs.children[0]->strValue;
            const AbstractValue cur  = env.get(base);
            if (lhs.type == NodeType::FIELD_ACCESS && cur.type.isObject()) {
                // Writing a field (`obj.f = rhs`) leaves the object's class
                // unchanged — do NOT clobber the base to Dynamic.
                recordDecl(declOut, base, cur.type);
            } else if (cur.type.isConcrete() && rhs.type.isConcrete()
                       && cur.type.dtype == rhs.type.dtype) {
                // Indexed assign x(i)=rhs: base keeps its dtype; shape may
                // grow -> Unknown.
                env.set(base, {InferredType::concrete(cur.type.dtype, Shape::unknown()),
                               ConstVal::unknown()});
                recordDecl(declOut, base, env.get(base).type);
            } else {
                // Differing dtype / unknown base or rhs -> conservative.
                env.set(base, AbstractValue::dynamic());
                recordDecl(declOut, base, env.get(base).type);
            }
        }
        return;
    }

    case NodeType::EXPR_STMT:
        // Pure type inference: an expression statement has no binding
        // effect we model (side effects are out of scope).
        return;

    case NodeType::IF_STMT: {
        // Each branch body runs on a copy of the incoming env; the merge
        // joins all branch out-envs, plus the fall-through env when the
        // if is not exhaustive (no else).
        TypeEnv merged;
        bool    have = false;
        auto    mergeIn = [&](const TypeEnv &e) {
            merged = have ? joinEnv(merged, e) : e;
            have = true;
        };
        for (const auto &br : stmt.branches) {
            TypeEnv branchEnv = env;
            if (br.second) inferStmt(*br.second, branchEnv, reg, declOut, classes);
            mergeIn(branchEnv);
        }
        if (stmt.elseBranch) {
            TypeEnv elseEnv = env;
            inferStmt(*stmt.elseBranch, elseEnv, reg, declOut, classes);
            mergeIn(elseEnv);
        } else {
            mergeIn(env);  // no branch taken — fall-through
        }
        if (have) env = merged;
        return;
    }

    case NodeType::SWITCH_STMT: {
        // Like IF: join every case body (+ otherwise / fall-through).
        TypeEnv merged;
        bool    have = false;
        auto    mergeIn = [&](const TypeEnv &e) {
            merged = have ? joinEnv(merged, e) : e;
            have = true;
        };
        for (const auto &br : stmt.branches) {
            TypeEnv caseEnv = env;
            if (br.second) inferStmt(*br.second, caseEnv, reg, declOut, classes);
            mergeIn(caseEnv);
        }
        if (stmt.elseBranch) {
            TypeEnv otherEnv = env;
            inferStmt(*stmt.elseBranch, otherEnv, reg, declOut, classes);
            mergeIn(otherEnv);
        } else {
            mergeIn(env);
        }
        if (have) env = merged;
        return;
    }

    case NodeType::FOR_STMT: {
        // for <strValue> = children[0]; children[1]; end
        if (stmt.children.size() != 2) { markAssignedDynamic(stmt, env, declOut); return; }
        const std::string &loopVar = stmt.strValue;
        const AbstractValue iterVal = forIterValue(
            inferExpr(*stmt.children[0], env, reg, classes).type);
        const ASTNode &body = *stmt.children[1];

        // Fixpoint: the body may run 0..n times and carry values across
        // iterations. Start from the entry env; repeatedly run the body
        // (loop var rebound) and join back until stable. The env only
        // widens between iterations, so a def-site type recorded into
        // declOut is monotone — recording on every pass is exact.
        TypeEnv cur = env;
        for (int i = 0; i < kMaxFixpoint; ++i) {
            TypeEnv bodyEnv = cur;
            bodyEnv.set(loopVar, iterVal);
            inferStmt(body, bodyEnv, reg, declOut, classes);
            TypeEnv next = joinEnv(cur, bodyEnv);
            if (next == cur) break;
            cur = next;
        }
        cur.set(loopVar, iterVal);  // loop var keeps its (last) iteration type
        recordDecl(declOut, loopVar, iterVal.type);
        env = cur;
        return;
    }

    case NodeType::WHILE_STMT: {
        // while children[0]; children[1]; end — fixpoint, no loop var.
        if (stmt.children.size() != 2) { markAssignedDynamic(stmt, env, declOut); return; }
        const ASTNode &body = *stmt.children[1];
        TypeEnv cur = env;
        for (int i = 0; i < kMaxFixpoint; ++i) {
            TypeEnv bodyEnv = cur;
            inferStmt(body, bodyEnv, reg, declOut, classes);
            TypeEnv next = joinEnv(cur, bodyEnv);
            if (next == cur) break;
            cur = next;
        }
        env = cur;
        return;
    }

    default:
        // try/catch, multi-assign, function defs, etc. — not modelled
        // precisely. Sound fallback: anything they assign -> Dynamic.
        markAssignedDynamic(stmt, env, declOut);
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
