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

// The flattened field-local name for a plain-struct field chain (field-flattening).
// `s.a.b` (a FIELD_ACCESS) -> "_nk_fld_s_a_b"; "" if the chain is not rooted at a
// plain identifier (a call/expression base) -> not a flattenable struct field. A
// single-level `s.a` gives "_nk_fld_s_a" (unchanged), so this generalises the old
// hardcoded name to nested access. MUST match emitter.cpp's copy.
std::string structFieldLocal(const ASTNode &fa)
{
    std::string    suffix;
    const ASTNode *n = &fa;
    while (n->type == NodeType::FIELD_ACCESS) {
        if (n->children.empty() || !n->children[0]) return "";
        suffix = "_" + n->strValue + suffix;
        n      = n->children[0].get();
    }
    if (n->type != NodeType::IDENTIFIER) return "";
    return "_nk_fld_" + n->strValue + suffix;
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

    case NodeType::END_VAL:
        // `end` inside an index is a scalar double (the indexed extent). Its VALUE
        // is supplied by the emitter's end-context; here we only fix the TYPE so the
        // index planner classifies e.g. x(end), x(end-1) as a scalar (LinearScalar).
        return {InferredType::scalar(ValueType::DOUBLE), ConstVal::unknown()};

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
        if (expr.children.empty()) return AbstractValue::dynamic();
        const AbstractValue base = inferExpr(*expr.children[0], env, reg, classes);
        // Plain struct: a synthesized field-local from a prior `s.f = ...` (field-
        // flattening; no struct type), generalised to a NESTED chain s.a.b via the
        // chain helper. The immediate base being non-object gates struct-vs-object at
        // every level (a sub-struct s.a is itself Dynamic, not a value). Before the
        // classdef path.
        if (!base.type.isObject()) {
            const std::string fld = structFieldLocal(expr);
            if (!fld.empty()) return env.has(fld) ? env.get(fld) : AbstractValue::dynamic();
        }
        // Object field (classdef): typed only with a class registry + known field;
        // otherwise Dynamic (sound — non-class code, or a handle we cannot class).
        if (!classes || !base.type.isObject()) return AbstractValue::dynamic();
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
            if (env.has(name)) {
                // PAGE-SLICE read A(:,:,k): leading bare colons + a trailing SCALAR on an NDims
                // rank-r base -> drop the last dim -> rank (r-1) (for r=3 a 2-D m x n page). The
                // dims are runtime (the emit copies the contiguous page); typed here so the dest
                // hoists as a (r-1)-D local. r >= 3 only (r=2 A(:,k) is the column slice).
                const AbstractValue base = env.get(name);
                const std::size_t   nsub = expr.children.size() - 1;
                if (base.type.shape.kind == ShapeKind::NDims && base.type.shape.nd.size() >= 3
                    && nsub == base.type.shape.nd.size()) {
                    const std::size_t r = base.type.shape.nd.size();
                    bool              pageSlice = true;
                    for (std::size_t i = 1; i < r; ++i)  // children[1..r-1] = bare colons
                        if (expr.children[i]->type != NodeType::COLON_EXPR
                            || !expr.children[i]->children.empty()) {
                            pageSlice = false;
                            break;
                        }
                    if (pageSlice && expr.children[r]->type != NodeType::COLON_EXPR
                        && isScalarValue(argVals[r - 1]))  // trailing subscript is a scalar
                        return {InferredType::concrete(
                                    base.type.dtype,
                                    Shape::ndShape(std::vector<std::size_t>(r - 1, 0))),
                                ConstVal::unknown()};
                }
                return indexResult(base, argVals);
            }
            return {reg.apply(name, argInfos), ConstVal::unknown()};
        }
        // s.v(k): indexing a struct array FIELD (field-flattening). The callee is
        // FIELD_ACCESS on a non-object base var -> index the field-local array.
        // (An object method obj.m(args) falls through to the method path below.)
        if (callee.type == NodeType::FIELD_ACCESS && !callee.children.empty()
            && callee.children[0]->type == NodeType::IDENTIFIER
            && !inferExpr(*callee.children[0], env, reg, classes).type.isObject()) {
            const std::string fld =
                "_nk_fld_" + callee.children[0]->strValue + "_" + callee.strValue;
            return env.has(fld) ? indexResult(env.get(fld), argVals) : AbstractValue::dynamic();
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

    case NodeType::MATRIX_LITERAL: {
        // Multi-row, each row a SINGLE SCALAR -> vertcat of scalars = a 1-D COLUMN of
        // the common dtype (runtime length). [a; b; c]. (Multi-row with array/row
        // operands = a true 2-D stack, deferred -> Dynamic via the fall-through.)
        if (expr.children.size() > 1) {
            ValueType dt = ValueType::EMPTY;
            bool      ok = true;
            for (const auto &rowN : expr.children) {
                if (!rowN || rowN->children.size() != 1) { ok = false; break; }
                const AbstractValue ev = inferExpr(*rowN->children[0], env, reg, classes);
                if (!ev.type.isConcrete() || !ev.type.shape.isScalar()) { ok = false; break; }
                if (dt == ValueType::EMPTY) dt = ev.type.dtype;
                else if (dt != ev.type.dtype) { ok = false; break; }  // no dtype mix
            }
            if (ok && dt != ValueType::EMPTY)
                return {InferredType::concrete(dt, Shape::unknown()), ConstVal::unknown()};
            // Each row a single ROW VECTOR of a common dtype -> a k x n 2-D matrix
            // (vertcat of rows): rows = k (known), cols = n (the rows' length, taken as
            // runtime -> NDims rank-2 ndShape({k, 0}); a runtime-dim 2-D the emitter
            // materialises as an ndRuntimeLocal). All rows must share length n
            // (precondition; MATLAB errors on a mismatch).
            ValueType rdt     = ValueType::EMPTY;
            bool      rowsOk  = true;
            for (const auto &rowN : expr.children) {
                if (!rowN || rowN->children.size() != 1) { rowsOk = false; break; }
                const AbstractValue ev    = inferExpr(*rowN->children[0], env, reg, classes);
                const bool          isRow =
                    ev.type.isConcrete()
                    && (ev.type.shape.kind == ShapeKind::RowVector
                        || (ev.type.shape.kind == ShapeKind::KnownDims
                            && ev.type.shape.rows == 1));
                if (!isRow) { rowsOk = false; break; }
                if (rdt == ValueType::EMPTY) rdt = ev.type.dtype;
                else if (rdt != ev.type.dtype) { rowsOk = false; break; }  // no dtype mix
            }
            if (rowsOk && rdt != ValueType::EMPTY)
                return {InferredType::concrete(rdt, Shape::ndShape({expr.children.size(), 0})),
                        ConstVal::unknown()};
            // Each row a single 2-D MATRIX or a ROW VECTOR of a common dtype -> vertcat
            // [A; B; ...] / [A; r] (appending a row) = a (sum-of-rows) x n matrix (all share
            // the column count; a row vector contributing one row). A runtime-dim 2-D (NDims
            // rank-2; the emitter sets the dims). A block is a matrix (KnownDims rows>1 &
            // cols>1, or an NDims rank-2) or a 1 x n row vector. (All-row-vector is the
            // vertcat-of-rows case above; this reaches the matrix / mixed cases.)
            ValueType vdt     = ValueType::EMPTY;
            bool      matsOk  = true;
            for (const auto &rowN : expr.children) {
                if (!rowN || rowN->children.size() != 1) { matsOk = false; break; }
                const AbstractValue ev      = inferExpr(*rowN->children[0], env, reg, classes);
                const bool          isBlock =
                    ev.type.isConcrete()
                    && ((ev.type.shape.kind == ShapeKind::KnownDims && ev.type.shape.rows > 1
                         && ev.type.shape.cols > 1)
                        || (ev.type.shape.kind == ShapeKind::NDims
                            && ev.type.shape.nd.size() == 2)
                        || ev.type.shape.kind == ShapeKind::RowVector);  // a 1 x n block
                if (!isBlock) { matsOk = false; break; }
                if (vdt == ValueType::EMPTY) vdt = ev.type.dtype;
                else if (vdt != ev.type.dtype) { matsOk = false; break; }  // no dtype mix
            }
            if (matsOk && vdt != ValueType::EMPTY)
                return {InferredType::concrete(vdt, Shape::ndShape({0, 0})), ConstVal::unknown()};
            // BLOCK-matrix literal [A B; C D]: a multi-row literal whose every row is a
            // HORZCAT of >=1 matrices (the >1-block rows the single-matrix vertcat above
            // does not match). Every element of every row is a 2-D matrix of a common
            // dtype -> a (sum-of-row-heights) x (common-total-cols) runtime-dim 2-D.
            ValueType blkdt = ValueType::EMPTY;
            bool      blkOk = true;
            for (const auto &rowN : expr.children) {
                if (!rowN || rowN->children.empty()) { blkOk = false; break; }
                for (const auto &el : rowN->children) {
                    if (!el) { blkOk = false; break; }
                    const AbstractValue ev    = inferExpr(*el, env, reg, classes);
                    const bool          isMat =
                        ev.type.isConcrete()
                        && ((ev.type.shape.kind == ShapeKind::KnownDims && ev.type.shape.rows > 1
                             && ev.type.shape.cols > 1)
                            || (ev.type.shape.kind == ShapeKind::NDims
                                && ev.type.shape.nd.size() == 2));
                    if (!isMat) { blkOk = false; break; }
                    if (blkdt == ValueType::EMPTY) blkdt = ev.type.dtype;
                    else if (blkdt != ev.type.dtype) { blkOk = false; break; }  // no dtype mix
                }
                if (!blkOk) break;
            }
            if (blkOk && blkdt != ValueType::EMPTY)
                return {InferredType::concrete(blkdt, Shape::ndShape({0, 0})), ConstVal::unknown()};
        }
        // v1: a single-row horzcat [a b ...] -> a 1-D array of the common dtype
        // (runtime length). Each element may be a 1-D array, a char/string literal,
        // or a SCALAR (each contributes its elements). Other multi-row, a 2-D/N-D
        // element, a mixed char/numeric dtype, or empty -> Dynamic.
        if (expr.children.size() != 1 || !expr.children[0]
            || expr.children[0]->children.empty())
            return AbstractValue::dynamic();
        // Single-row literal whose every element is a COLUMN VECTOR (>=2 of them) ->
        // horzcat of columns = an n x k 2-D matrix: cols = k (known), rows = n (the
        // columns' length, taken as runtime -> NDims rank-2 ndShape({0, k}); a runtime-
        // dim 2-D the emitter materialises as an ndRuntimeLocal). All columns must share
        // length n (precondition). Checked before the 1-D horzcat fall-through, which
        // would otherwise flatten columns into a wrong 1-D concatenation.
        if (expr.children[0]->children.size() >= 2) {
            ValueType cdt    = ValueType::EMPTY;
            bool      colsOk = true;
            for (const auto &el : expr.children[0]->children) {
                if (!el) { colsOk = false; break; }
                const AbstractValue ev = inferExpr(*el, env, reg, classes);
                if (!ev.type.isConcrete() || ev.type.shape.kind != ShapeKind::ColVector) {
                    colsOk = false;
                    break;
                }
                if (cdt == ValueType::EMPTY) cdt = ev.type.dtype;
                else if (cdt != ev.type.dtype) { colsOk = false; break; }  // no dtype mix
            }
            if (colsOk && cdt != ValueType::EMPTY)
                return {InferredType::concrete(
                            cdt, Shape::ndShape({0, expr.children[0]->children.size()})),
                        ConstVal::unknown()};
        }
        // Single-row literal whose every element is a 2-D MATRIX or a COLUMN VECTOR (>=2
        // of them, at least one a matrix) -> horizontal concatenation [A B ...] / [A b]
        // (augmented matrix) = a wider matrix (all share the row count; result rows x
        // sum-of-cols, a column vector contributing one column). A runtime-dim 2-D (NDims
        // rank-2; the emitter sets the dims). A block is a matrix (KnownDims rows>1 & cols>1,
        // or an NDims rank-2) or an n x 1 column vector. (All-column-vector is handled by
        // the case above; this reaches the matrix / mixed cases the 1-D horzcat rejects.)
        if (expr.children[0]->children.size() >= 2) {
            ValueType bdt      = ValueType::EMPTY;
            bool      blocksOk = true;
            for (const auto &el : expr.children[0]->children) {
                if (!el) { blocksOk = false; break; }
                const AbstractValue ev = inferExpr(*el, env, reg, classes);
                const bool isBlock =
                    ev.type.isConcrete()
                    && ((ev.type.shape.kind == ShapeKind::KnownDims && ev.type.shape.rows > 1
                         && ev.type.shape.cols > 1)
                        || (ev.type.shape.kind == ShapeKind::NDims
                            && ev.type.shape.nd.size() == 2)
                        || ev.type.shape.kind == ShapeKind::ColVector);  // an n x 1 block
                if (!isBlock) { blocksOk = false; break; }
                if (bdt == ValueType::EMPTY) bdt = ev.type.dtype;
                else if (bdt != ev.type.dtype) { blocksOk = false; break; }  // no dtype mix
            }
            if (blocksOk && bdt != ValueType::EMPTY)
                return {InferredType::concrete(bdt, Shape::ndShape({0, 0})), ConstVal::unknown()};
        }
        ValueType dt = ValueType::EMPTY;
        for (const auto &el : expr.children[0]->children) {
            if (!el) return AbstractValue::dynamic();
            const AbstractValue ev      = inferExpr(*el, env, reg, classes);
            const bool          isMatrix =
                (ev.type.shape.kind == ShapeKind::KnownDims && ev.type.shape.rows > 1
                 && ev.type.shape.cols > 1)
                || ev.type.shape.isNDims();
            if (!ev.type.isConcrete() || isMatrix) return AbstractValue::dynamic();
            if (dt == ValueType::EMPTY) dt = ev.type.dtype;
            else if (dt != ev.type.dtype) return AbstractValue::dynamic();  // no dtype mix
        }
        return {InferredType::concrete(dt, Shape::unknown()), ConstVal::unknown()};
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
            } else if (lhs.type == NodeType::FIELD_ACCESS) {
                // Plain struct: flatten `s.f = rhs` to a synthesized scalar
                // field-local (no struct type; field-flattening). The base var is
                // not a value itself — only its fields are read/written.
                const std::string fld = "_nk_fld_" + base + "_" + lhs.strValue;
                env.set(fld, rhs);
                recordDecl(declOut, fld, rhs.type);
            } else if (cur.type.isConcrete() && rhs.type.isConcrete()
                       && cur.type.dtype == rhs.type.dtype) {
                // Indexed assign x(i)=rhs: base keeps its dtype. A 1-D x(i)=v
                // may grow the vector -> Unknown shape. A 2-D subscript
                // A(i,j)=v on a KnownDims matrix does NOT change its dims (the
                // codegen is fixed-size — an out-of-range write throws, never
                // grows), so its 2-D shape is preserved.
                const std::size_t nidx   = lhs.children.size() - 1;
                const bool        keep2D = nidx == 2
                                    && cur.type.shape.kind == ShapeKind::KnownDims;
                // A rank-N subscript write on a ranked matrix likewise keeps
                // its dims (fixed-size; out-of-range throws, never grows).
                const bool keepND = cur.type.shape.isNDims() && nidx == cur.type.shape.ndRank();
                // A bare-colon whole-array write `A(:) = rhs` fills in place -- it never
                // changes the shape (1-D length / 2-D / N-D dims preserved), so keep it.
                const bool keepColon = nidx == 1 && lhs.children[1]
                                       && lhs.children[1]->type == NodeType::COLON_EXPR
                                       && lhs.children[1]->children.empty();
                // A LOGICAL-array mask write `A(A>0) = c` scatters into EXISTING elements --
                // it cannot grow A (unlike a numeric linear index, which can), so it preserves
                // shape. Detect via the subscript inferring a concrete non-scalar LOGICAL array.
                bool keepMask = false;
                if (nidx == 1 && lhs.children[1]) {
                    const InferredType st =
                        inferExpr(*lhs.children[1], env, reg, classes).type;
                    keepMask = st.isConcrete() && st.dtype == ValueType::LOGICAL
                               && !st.shape.isScalar();
                }
                const Shape sh = (keep2D || keepND || keepColon || keepMask)
                                     ? cur.type.shape
                                     : Shape::unknown();
                env.set(base, {InferredType::concrete(cur.type.dtype, sh), ConstVal::unknown()});
                recordDecl(declOut, base, env.get(base).type);
            } else {
                // Differing dtype / unknown base or rhs -> conservative.
                env.set(base, AbstractValue::dynamic());
                recordDecl(declOut, base, env.get(base).type);
            }
        } else if (lhs.type == NodeType::FIELD_ACCESS && !lhs.children.empty()) {
            // Nested plain-struct field write s.a.b = rhs: children[0] is itself a
            // FIELD_ACCESS, so the single-level branch above (children[0]==IDENTIFIER)
            // missed it. Flatten to the leaf field-local via the chain helper, unless
            // the chain is rooted at an object.
            const AbstractValue ibase = inferExpr(*lhs.children[0], env, reg, classes);
            if (!ibase.type.isObject()) {
                const std::string fld = structFieldLocal(lhs);
                if (!fld.empty()) {
                    env.set(fld, rhs);
                    recordDecl(declOut, fld, rhs.type);
                }
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

    case NodeType::MULTI_ASSIGN: {
        // [a, b, ...] = f(args). Type each simple-identifier target from the
        // call's corresponding output. Complex lvalue targets (lhsTargets),
        // or a RHS that is not a bare-name user-function call, fall back to
        // conservative Dynamic.
        if (stmt.children.empty() || !stmt.lhsTargets.empty()) {
            markAssignedDynamic(stmt, env, declOut);
            return;
        }
        const ASTNode           &rhs = *stmt.children[0];
        std::vector<InferredType> outs;
        if (rhs.type == NodeType::CALL && !rhs.children.empty()
            && rhs.children[0]->type == NodeType::IDENTIFIER
            && !env.has(rhs.children[0]->strValue)) {
            std::vector<ArgInfo> argInfos;
            for (std::size_t i = 1; i < rhs.children.size(); ++i)
                argInfos.push_back(inferExpr(*rhs.children[i], env, reg, classes).asArg());
            outs = reg.applyMulti(rhs.children[0]->strValue, argInfos);
        }
        for (std::size_t i = 0; i < stmt.returnNames.size(); ++i) {
            const std::string &rn = stmt.returnNames[i];
            if (rn.empty() || rn == "~") continue;  // ignored output
            const InferredType t = (i < outs.size()) ? outs[i] : InferredType::dynamic();
            env.set(rn, {t, ConstVal::unknown()});
            recordDecl(declOut, rn, t);
        }
        return;
    }

    default:
        // try/catch, function defs, etc. — not modelled precisely. Sound
        // fallback: anything they assign -> Dynamic.
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
