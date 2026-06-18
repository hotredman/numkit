// codegen/src/emitter.cpp — see emitter.hpp.

#include <numkit/codegen/emitter.hpp>

#include <numkit/codegen/indexing.hpp>

#include <algorithm>
#include <charconv>
#include <cmath>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace numkit::codegen {

// ── scalar dtype / literal helpers (3a) ───────────────────────────────
std::string cppScalarType(ValueType dtype)
{
    switch (dtype) {
    case ValueType::DOUBLE:  return "double";
    case ValueType::SINGLE:  return "float";
    case ValueType::COMPLEX: return "std::complex<double>";
    case ValueType::LOGICAL: return "bool";
    case ValueType::INT8:    return "std::int8_t";
    case ValueType::INT16:   return "std::int16_t";
    case ValueType::INT32:   return "std::int32_t";
    case ValueType::INT64:   return "std::int64_t";
    case ValueType::UINT8:   return "std::uint8_t";
    case ValueType::UINT16:  return "std::uint16_t";
    case ValueType::UINT32:  return "std::uint32_t";
    case ValueType::UINT64:  return "std::uint64_t";
    default:
        throw std::runtime_error("cppScalarType: no scalar C++ mapping for this dtype");
    }
}

std::string formatDoubleLiteral(double v)
{
    if (std::isnan(v)) return "std::numeric_limits<double>::quiet_NaN()";
    if (std::isinf(v))
        return v < 0 ? "-std::numeric_limits<double>::infinity()"
                     : "std::numeric_limits<double>::infinity()";

    char buf[32];
    auto [ptr, ec] = std::to_chars(buf, buf + sizeof(buf), v);
    std::string s(buf, ptr);
    // Ensure it reads as a double in C++ (e.g. "2" -> "2.0", not an int).
    if (s.find('.') == std::string::npos && s.find('e') == std::string::npos &&
        s.find('E') == std::string::npos)
        s += ".0";
    return s;
}

namespace {

// The zero / default-init literal for a hoisted local of `dtype`.
std::string zeroLiteral(ValueType dtype)
{
    switch (dtype) {
    case ValueType::DOUBLE:
    case ValueType::SINGLE:  return "0.0";
    case ValueType::COMPLEX: return "std::complex<double>(0.0, 0.0)";
    case ValueType::LOGICAL: return "false";
    default:                 return "0";  // integer dtypes
    }
}

// MATLAB binary operator -> C++ operator for the SCALAR case. Returns
// nullptr for power (handled via std::pow) or an unhandled operator.
const char *cppScalarBinOp(const std::string &op)
{
    if (op == "+")  return "+";
    if (op == "-")  return "-";
    if (op == "*" || op == ".*") return "*";
    if (op == "/" || op == "./") return "/";
    if (op == "==") return "==";
    if (op == "~=" || op == "!=") return "!=";
    if (op == "<")  return "<";
    if (op == ">")  return ">";
    if (op == "<=") return "<=";
    if (op == ">=") return ">=";
    if (op == "&&" || op == "&") return "&&";  // scalar logical
    if (op == "||" || op == "|") return "||";
    return nullptr;  // ^, .^ -> std::pow; anything else -> unsupported
}

[[noreturn]] void unsupported(const std::string &what)
{
    throw std::runtime_error("codegen emitter: unsupported — " + what);
}

// Operator joiners shared by emitScalarExpr (free) and Emitter::emitExpr.
std::string emitBinOpJoin(const std::string &op, const std::string &l,
                          const std::string &r)
{
    if (op == "^" || op == ".^")
        return "std::pow(" + l + ", " + r + ")";
    const char *o = cppScalarBinOp(op);
    if (!o) unsupported("operator " + op);
    return "(" + l + " " + o + " " + r + ")";
}

std::string emitUnOpJoin(const std::string &op, const std::string &a)
{
    if (op == "-") return "(-" + a + ")";
    if (op == "+") return "(+" + a + ")";
    if (op == "~" || op == "!") return "(!" + a + ")";
    unsupported("unary operator " + op);
}

} // namespace

std::string emitScalarExpr(const ASTNode &expr)
{
    switch (expr.type) {
    case NodeType::NUMBER_LITERAL:
        return formatDoubleLiteral(expr.numValue);
    case NodeType::IMAG_LITERAL:
        return "std::complex<double>(0.0, " + formatDoubleLiteral(expr.numValue) + ")";
    case NodeType::BOOL_LITERAL:
        return expr.boolValue ? "true" : "false";
    case NodeType::IDENTIFIER:
        return expr.strValue;
    case NodeType::BINARY_OP: {
        if (expr.children.size() != 2) unsupported("binary op arity");
        return emitBinOpJoin(expr.strValue, emitScalarExpr(*expr.children[0]),
                             emitScalarExpr(*expr.children[1]));
    }
    case NodeType::UNARY_OP: {
        if (expr.children.size() != 1) unsupported("unary op arity");
        return emitUnOpJoin(expr.strValue, emitScalarExpr(*expr.children[0]));
    }
    default:
        unsupported("node kind (calls / indexing / matrices) in emitScalarExpr");
    }
}

// ── declaration-type prepass (3b) ─────────────────────────────────────
DeclTypeMap computeDeclTypes(const ASTNode &body, const TypeEnv &entryEnv,
                             const TransferRegistry &reg)
{
    // One inference pass records, at every definition site (including
    // loop-body temporaries and loop variables), the type the variable is
    // assigned THERE. That program-point type — not the post-loop env,
    // where a loop temporary is conservatively Dynamic ("maybe undefined
    // after 0 iterations") — is the right basis for one C++ declaration.
    DeclTypeMap dt;
    for (const auto &[name, av] : entryEnv.entries())  // seed parameters
        dt[name] = av.type;

    TypeEnv env = entryEnv;
    inferStmt(body, env, reg, &dt);
    return dt;
}

// ── the stateful Emitter (3b-3e) ──────────────────────────────────────
namespace {

constexpr int kMaxFixpoint = 16;  // matches inference.cpp; lattice is shallow

// Per-array metadata for an array parameter or the single output array.
struct ArrayInfo {
    ValueType   dtype;
    std::string lenVar;    // companion length variable (`<name>_len`)
    bool        isOutput;  // the function's caller-allocated out-param
};

bool isUnboxableScalarType(const InferredType &t)
{
    return t.isConcrete() && t.shape.isScalar() && t.isUnboxableScalar();
}

// A concrete numeric/logical buffer of non-scalar shape (raw-buffer array).
bool isBufferArrayType(const InferredType &t)
{
    if (!t.isConcrete() || t.shape.isScalar()) return false;
    return isBufferArray(AbstractValue{t, ConstVal::unknown()});
}

// ── brick 6: optimisation facts (gated, deletable) ────────────────────
// A fact only ENABLES an optimisation; its absence is the safe default
// (every index bounds-checked, the loop variable a double). Deleting
// analyzeOptimizations() — returning empty facts — leaves correct, slower
// code (the no-kludge litmus, DESIGN.md §10). The one optimisation here is
// clean-index loop promotion: a `for k = 1:H` whose body uses `k` ONLY as
// the sole 1-based index of buffer arrays whose element count is provably
// H becomes a 0-based `std::size_t` counter with unchecked `A[k]` access —
// no double loop var, no `(size_t)k - 1`, no bounds check.
struct OptFacts {
    std::set<const ASTNode *>              promotedLoops;  // FOR nodes -> 0-based counter
    std::map<const ASTNode *, std::string> loopBound;      // FOR node -> C++ size_t bound
};

// numel(A) / length(A) of a tracked array variable A?
bool isNumelOfArray(const ASTNode &e,
                    const std::unordered_map<std::string, ArrayInfo> &arrays,
                    std::string &arrayOut)
{
    if (e.type != NodeType::CALL || e.children.size() != 2) return false;
    if (e.children[0]->type != NodeType::IDENTIFIER) return false;
    const std::string &fn = e.children[0]->strValue;
    if (fn != "numel" && fn != "length") return false;
    if (e.children[1]->type != NodeType::IDENTIFIER) return false;
    if (!arrays.count(e.children[1]->strValue)) return false;
    arrayOut = e.children[1]->strValue;
    return true;
}

// Record numel-equality facts (forward walk): arrayLenVar[A] = v means
// numel(A) == value of scalar variable v. Sources: `v = numel(A)` and a
// vector size-constructor `B = zeros(1,v)` / `zeros(v,1)`.
void recordLengthFacts(const ASTNode &node,
                       const std::unordered_map<std::string, ArrayInfo> &arrays,
                       std::unordered_map<std::string, std::string> &arrayLenVar)
{
    if (node.type == NodeType::ASSIGN && node.children.size() == 2
        && node.children[0]->type == NodeType::IDENTIFIER) {
        const std::string &lhs = node.children[0]->strValue;
        const ASTNode     &rhs = *node.children[1];
        std::string        arr;
        if (isNumelOfArray(rhs, arrays, arr))
            arrayLenVar[arr] = lhs;  // numel(arr) == lhs
        else if (rhs.type == NodeType::CALL && rhs.children.size() == 3
                 && rhs.children[0]->type == NodeType::IDENTIFIER
                 && (rhs.children[0]->strValue == "zeros"
                     || rhs.children[0]->strValue == "ones")
                 && arrays.count(lhs)) {
            const ASTNode &d1 = *rhs.children[1], &d2 = *rhs.children[2];
            if (d1.type == NodeType::NUMBER_LITERAL && d1.numValue == 1.0
                && d2.type == NodeType::IDENTIFIER)
                arrayLenVar[lhs] = d2.strValue;  // zeros(1, v) -> numel == v
            else if (d2.type == NodeType::NUMBER_LITERAL && d2.numValue == 1.0
                     && d1.type == NodeType::IDENTIFIER)
                arrayLenVar[lhs] = d1.strValue;  // zeros(v, 1) -> numel == v
        }
    }
    for (const auto &c : node.children)
        if (c) recordLengthFacts(*c, arrays, arrayLenVar);
    for (const auto &br : node.branches) {
        if (br.first)  recordLengthFacts(*br.first, arrays, arrayLenVar);
        if (br.second) recordLengthFacts(*br.second, arrays, arrayLenVar);
    }
    if (node.elseBranch) recordLengthFacts(*node.elseBranch, arrays, arrayLenVar);
}

// Every use of `k` in `node` is the sole index of a buffer-array access
// `A(k)` (collected into idxArrays). Any other occurrence of `k`
// (arithmetic, A(k,j), non-array call) returns false (not clean).
bool collectCleanIndexUses(const ASTNode &node, const std::string &k,
                           const std::unordered_map<std::string, ArrayInfo> &arrays,
                           std::set<std::string> &idxArrays)
{
    if (node.type == NodeType::CALL && node.children.size() == 2
        && node.children[0]->type == NodeType::IDENTIFIER
        && arrays.count(node.children[0]->strValue)
        && node.children[1]->type == NodeType::IDENTIFIER
        && node.children[1]->strValue == k) {
        idxArrays.insert(node.children[0]->strValue);
        return true;  // clean index — do not descend into the index arg
    }
    if (node.type == NodeType::IDENTIFIER && node.strValue == k)
        return false;  // a bare / non-index use of k
    for (const auto &c : node.children)
        if (c && !collectCleanIndexUses(*c, k, arrays, idxArrays)) return false;
    for (const auto &br : node.branches) {
        if (br.first && !collectCleanIndexUses(*br.first, k, arrays, idxArrays)) return false;
        if (br.second && !collectCleanIndexUses(*br.second, k, arrays, idxArrays)) return false;
    }
    if (node.elseBranch && !collectCleanIndexUses(*node.elseBranch, k, arrays, idxArrays))
        return false;
    return true;
}

int countIdentUses(const ASTNode &node, const std::string &name)
{
    int n = (node.type == NodeType::IDENTIFIER && node.strValue == name) ? 1 : 0;
    for (const auto &c : node.children) if (c) n += countIdentUses(*c, name);
    for (const auto &br : node.branches) {
        if (br.first)  n += countIdentUses(*br.first, name);
        if (br.second) n += countIdentUses(*br.second, name);
    }
    if (node.elseBranch) n += countIdentUses(*node.elseBranch, name);
    return n;
}

void analyzeLoops(const ASTNode &node, const ASTNode &funcBody,
                  const std::unordered_map<std::string, ArrayInfo> &arrays,
                  const std::unordered_map<std::string, std::string> &arrayLenVar,
                  OptFacts &facts)
{
    if (node.type == NodeType::FOR_STMT && node.children.size() == 2) {
        const ASTNode     &range = *node.children[0];
        const std::string &k     = node.strValue;
        if (range.type == NodeType::COLON_EXPR && range.children.size() == 2
            && range.children[0]->type == NodeType::NUMBER_LITERAL
            && range.children[0]->numValue == 1.0
            && range.children[1]->type == NodeType::IDENTIFIER) {
            const std::string     boundVar = range.children[1]->strValue;
            const ASTNode        &lbody    = *node.children[1];
            std::set<std::string> idxArrays;
            bool safe = collectCleanIndexUses(lbody, k, arrays, idxArrays)
                        && !idxArrays.empty();
            for (const auto &A : idxArrays) {
                auto it = arrayLenVar.find(A);
                if (it == arrayLenVar.end() || it->second != boundVar) { safe = false; break; }
            }
            // k must appear ONLY inside this loop body (no post-loop use).
            if (safe && countIdentUses(funcBody, k) == countIdentUses(lbody, k)) {
                std::string bound;  // prefer a param array's companion (== numel exactly)
                for (const auto &A : idxArrays)
                    if (!arrays.at(A).isOutput) { bound = arrays.at(A).lenVar; break; }
                if (bound.empty()) bound = arrays.at(*idxArrays.begin()).lenVar;
                facts.promotedLoops.insert(&node);
                facts.loopBound[&node] = bound;
            }
        }
    }
    for (const auto &c : node.children)
        if (c) analyzeLoops(*c, funcBody, arrays, arrayLenVar, facts);
    for (const auto &br : node.branches) {
        if (br.first)  analyzeLoops(*br.first, funcBody, arrays, arrayLenVar, facts);
        if (br.second) analyzeLoops(*br.second, funcBody, arrays, arrayLenVar, facts);
    }
    if (node.elseBranch) analyzeLoops(*node.elseBranch, funcBody, arrays, arrayLenVar, facts);
}

OptFacts analyzeOptimizations(const ASTNode &body,
                              const std::unordered_map<std::string, ArrayInfo> &arrays)
{
    std::unordered_map<std::string, std::string> arrayLenVar;
    recordLengthFacts(body, arrays, arrayLenVar);
    OptFacts facts;
    analyzeLoops(body, body, arrays, arrayLenVar, facts);
    return facts;
}

class Emitter {
public:
    Emitter(TypeEnv types, const TransferRegistry &reg,
            std::unordered_map<std::string, ArrayInfo> arrays, OptFacts opt)
        : types_(std::move(types)), reg_(reg), arrays_(std::move(arrays)),
          opt_(std::move(opt))
    {}

    // Hoist a local scalar declaration at function entry.
    void hoistLocal(const std::string &name, const InferredType &t)
    {
        line(cppScalarType(t.dtype) + " " + name + " = " + zeroLiteral(t.dtype) + ";");
    }

    void emitStmt(const ASTNode &s);
    void emitReturnScalar(const std::string &name) { line("return " + name + ";"); }

    const std::string &out() const { return out_; }

private:
    void line(const std::string &s) { out_ += std::string(indent_ * 4, ' ') + s + "\n"; }
    void open(const std::string &head) { line(head + " {"); ++indent_; }
    void close() { --indent_; line("}"); }

    std::string emitExpr(const ASTNode &e);
    std::string emitBuiltinCall(const std::string &name, const ASTNode &call);
    std::string emitIndexRead(const std::string &base, const ASTNode &call);
    void        emitAssign(const ASTNode &s);
    void        emitIndexWrite(const ASTNode &lhsCall, const ASTNode &rhs);
    void        emitFor(const ASTNode &s);
    void        emitWhile(const ASTNode &s);
    void        emitIf(const ASTNode &s);

    bool isArrayVar(const std::string &n) const { return arrays_.count(n) != 0; }

    // The AbstractValue for an array variable (non-scalar -> buffer).
    AbstractValue arrayValue(const std::string &n) const
    {
        return {InferredType::concrete(arrays_.at(n).dtype, Shape::rowVector()),
                ConstVal::unknown()};
    }

    std::string                                 out_;
    int                                         indent_ = 1;
    TypeEnv                                     types_;
    const TransferRegistry                     &reg_;
    std::unordered_map<std::string, ArrayInfo>  arrays_;
    OptFacts                                    opt_;
    // Non-null inside a promoted clean-index loop: the 0-based size_t
    // counter that replaced the 1-based double loop variable.
    const std::string                          *promotedCounter_ = nullptr;
};

// std::<name> for a 1-arg real math builtin with an exact std equivalent.
const char *unaryMathStd(const std::string &name)
{
    static const std::unordered_set<std::string> kSet = {
        "sin", "cos", "tan", "asin", "acos", "atan", "sinh", "cosh", "tanh",
        "asinh", "acosh", "atanh", "exp", "expm1", "log", "log1p", "log2",
        "log10", "sqrt", "cbrt", "abs", "floor", "ceil", "round", "trunc"};
    return kSet.count(name) ? name.c_str() : nullptr;
}

const char *binaryMathStd(const std::string &name)
{
    if (name == "atan2") return "atan2";
    if (name == "hypot") return "hypot";
    if (name == "power" || name == "mpower") return "pow";  // function-call form
    if (name == "min")   return "fmin";  // MATLAB min/max ignore NaN, like fmin/fmax
    if (name == "max")   return "fmax";
    return nullptr;
}

std::string Emitter::emitExpr(const ASTNode &e)
{
    switch (e.type) {
    case NodeType::NUMBER_LITERAL:
        return formatDoubleLiteral(e.numValue);
    case NodeType::IMAG_LITERAL:
        return "std::complex<double>(0.0, " + formatDoubleLiteral(e.numValue) + ")";
    case NodeType::BOOL_LITERAL:
        return e.boolValue ? "true" : "false";
    case NodeType::IDENTIFIER:
        if (isArrayVar(e.strValue))
            unsupported("bare array identifier '" + e.strValue + "' in scalar context");
        return e.strValue;
    case NodeType::BINARY_OP:
        if (e.children.size() != 2) unsupported("binary op arity");
        return emitBinOpJoin(e.strValue, emitExpr(*e.children[0]),
                             emitExpr(*e.children[1]));
    case NodeType::UNARY_OP:
        if (e.children.size() != 1) unsupported("unary op arity");
        return emitUnOpJoin(e.strValue, emitExpr(*e.children[0]));
    case NodeType::CALL: {
        if (e.children.empty()) unsupported("empty call");
        const ASTNode &callee = *e.children[0];
        if (callee.type != NodeType::IDENTIFIER)
            unsupported("non-identifier callee");
        if (isArrayVar(callee.strValue))
            return emitIndexRead(callee.strValue, e);
        return emitBuiltinCall(callee.strValue, e);
    }
    default:
        unsupported("expression node kind");
    }
}

std::string Emitter::emitBuiltinCall(const std::string &name, const ASTNode &call)
{
    const std::size_t nargs = call.children.size() - 1;

    // numel / length of an array variable -> its length companion.
    if ((name == "numel" || name == "length") && nargs == 1
        && call.children[1]->type == NodeType::IDENTIFIER
        && isArrayVar(call.children[1]->strValue)) {
        return "static_cast<double>(" + arrays_.at(call.children[1]->strValue).lenVar + ")";
    }

    if (nargs == 1) {
        if (const char *fn = unaryMathStd(name))
            return std::string("std::") + fn + "(" + emitExpr(*call.children[1]) + ")";
    }
    if (nargs == 2) {
        if (const char *fn = binaryMathStd(name))
            return std::string("std::") + fn + "(" + emitExpr(*call.children[1]) + ", "
                   + emitExpr(*call.children[2]) + ")";
        if (name == "mod")
            return "nk_rt::mod(" + emitExpr(*call.children[1]) + ", "
                   + emitExpr(*call.children[2]) + ")";
        if (name == "rem")
            return "nk_rt::rem(" + emitExpr(*call.children[1]) + ", "
                   + emitExpr(*call.children[2]) + ")";
    }
    unsupported("builtin call '" + name + "' (arity " + std::to_string(nargs) + ")");
}

std::string Emitter::emitIndexRead(const std::string &base, const ASTNode &call)
{
    // brick 6: inside a promoted clean-index loop, A(counter) is provably
    // in-bounds -> direct 0-based access, no check, no -1.
    if (promotedCounter_ && call.children.size() == 2
        && call.children[1]->type == NodeType::IDENTIFIER
        && call.children[1]->strValue == *promotedCounter_)
        return base + "[" + *promotedCounter_ + "]";

    std::vector<AbstractValue> idx;
    for (std::size_t i = 1; i < call.children.size(); ++i)
        idx.push_back(inferExpr(*call.children[i], types_, reg_));

    const IndexPlan plan = planIndexRead(arrayValue(base), idx);
    if (plan.form != IndexForm::LinearScalar)
        unsupported("index read form for '" + base
                    + "' (only 1-D scalar index in RawBuffer ABI MVP)");

    const std::string idxExpr = emitExpr(*call.children[1]);
    const std::string lenVar  = arrays_.at(base).lenVar;
    if (plan.boundsChecked)
        return "nk_rt::index(" + base + ", " + lenVar + ", " + idxExpr + ")";
    return base + "[static_cast<std::size_t>(" + idxExpr + ") - 1]";  // proven in-bounds
}

void Emitter::emitIndexWrite(const ASTNode &lhsCall, const ASTNode &rhs)
{
    const std::string &base = lhsCall.children[0]->strValue;
    if (!isArrayVar(base)) unsupported("indexed write to non-array '" + base + "'");

    // brick 6: inside a promoted clean-index loop, A(counter) is provably
    // in-bounds -> direct 0-based store, no check.
    if (promotedCounter_ && lhsCall.children.size() == 2
        && lhsCall.children[1]->type == NodeType::IDENTIFIER
        && lhsCall.children[1]->strValue == *promotedCounter_) {
        line(base + "[" + *promotedCounter_ + "] = " + emitExpr(rhs) + ";");
        return;
    }

    std::vector<AbstractValue> idx;
    for (std::size_t i = 1; i < lhsCall.children.size(); ++i)
        idx.push_back(inferExpr(*lhsCall.children[i], types_, reg_));
    const AbstractValue rhsAV = inferExpr(rhs, types_, reg_);

    const IndexPlan plan = planIndexWrite(arrayValue(base), idx, rhsAV);
    if (plan.form != IndexForm::LinearScalar)
        unsupported("index write form for '" + base
                    + "' (only 1-D scalar store of matching scalar dtype in MVP)");

    const std::string idxExpr = emitExpr(*lhsCall.children[1]);
    const std::string rhsExpr = emitExpr(rhs);
    const std::string lenVar  = arrays_.at(base).lenVar;
    if (plan.boundsChecked)
        line("nk_rt::index_set(" + base + ", " + lenVar + ", " + idxExpr + ", "
             + rhsExpr + ");");
    else
        line(base + "[static_cast<std::size_t>(" + idxExpr + ") - 1] = " + rhsExpr + ";");
}

void Emitter::emitAssign(const ASTNode &s)
{
    if (s.children.size() != 2) unsupported("assign arity");
    const ASTNode &lhs = *s.children[0];
    const ASTNode &rhs = *s.children[1];

    if (lhs.type == NodeType::IDENTIFIER) {
        const std::string &name = lhs.strValue;

        // Output array initialised by a size constructor: `y = zeros(1,n)`
        // becomes a fill loop over the caller-allocated out-param.
        if (isArrayVar(name) && arrays_.at(name).isOutput
            && rhs.type == NodeType::CALL && !rhs.children.empty()
            && rhs.children[0]->type == NodeType::IDENTIFIER
            && (rhs.children[0]->strValue == "zeros"
                || rhs.children[0]->strValue == "ones")) {
            const std::string fill = rhs.children[0]->strValue == "zeros"
                                         ? zeroLiteral(arrays_.at(name).dtype)
                                         : "1";
            const std::string lenVar = arrays_.at(name).lenVar;
            open("for (std::size_t __i = 0; __i < " + lenVar + "; ++__i)");
            line(name + "[__i] = " + fill + ";");
            close();
            return;
        }
        if (isArrayVar(name))
            unsupported("array-valued assignment to '" + name
                        + "' (only size-constructor init of the output in RawBuffer ABI)");

        // Scalar assignment (the local was hoisted at function entry).
        const std::string rhsExpr = emitExpr(rhs);
        // numel/length return a count; assigning into a double local needs
        // no cast (the builtin emitter already produced a double). But the
        // companion length is size_t, so the cast is inside emitBuiltinCall.
        line(name + " = " + rhsExpr + ";");
        types_.set(name, inferExpr(rhs, types_, reg_));
        return;
    }

    if ((lhs.type == NodeType::CALL || lhs.type == NodeType::INDEX)
        && !lhs.children.empty()
        && lhs.children[0]->type == NodeType::IDENTIFIER) {
        emitIndexWrite(lhs, rhs);
        return;
    }
    unsupported("assignment lhs kind");
}

void Emitter::emitFor(const ASTNode &s)
{
    if (s.children.size() != 2) unsupported("for arity");
    const ASTNode &range = *s.children[0];
    if (range.type != NodeType::COLON_EXPR)
        unsupported("for range must be a colon expression a:b / a:s:b");

    const std::string &loopVar = s.strValue;
    const ASTNode     &body    = *s.children[1];

    // Settle body types (fixpoint) so indexing / disambiguation inside the
    // loop sees loop-carried types. The loop var takes the range element
    // type (a colon range is always double scalar). The env only widens
    // between iterations, so this terminates.
    const AbstractValue iterVal{InferredType::scalar(ValueType::DOUBLE),
                                ConstVal::unknown()};
    TypeEnv cur = types_;
    for (int i = 0; i < kMaxFixpoint; ++i) {
        TypeEnv bodyEnv = cur;
        bodyEnv.set(loopVar, iterVal);
        inferStmt(body, bodyEnv, reg_);
        TypeEnv next = joinEnv(cur, bodyEnv);
        if (next == cur) break;
        cur = next;
    }
    cur.set(loopVar, iterVal);
    types_ = cur;

    // brick 6: clean-index promotion -> 0-based size_t counter (no double
    // loop var, no bounds check). Gated on analyzeOptimizations(); absent
    // the fact this falls through to the always-correct checked form.
    if (opt_.promotedLoops.count(&s)) {
        const std::string         *saved = promotedCounter_;
        promotedCounter_                 = &loopVar;
        open("for (std::size_t " + loopVar + " = 0; " + loopVar + " < "
             + opt_.loopBound.at(&s) + "; ++" + loopVar + ")");
        emitStmt(body);
        close();
        promotedCounter_ = saved;
        types_           = cur;
        return;
    }

    // Default (always correct): a double loop variable counting the colon
    // range; index sites stay bounds-checked.
    const std::string startE = emitExpr(*range.children[0]);
    std::string       stopE, stepE, cond;
    if (range.children.size() == 2) {
        stopE = emitExpr(*range.children[1]);
        stepE = "1.0";
        cond  = "<=";
    } else if (range.children.size() == 3) {
        const ConstVal sc = inferExpr(*range.children[1], types_, reg_).constant;
        if (!sc.isKnown() || sc.value == 0.0)
            unsupported("for range step must be a known non-zero constant");
        stepE = emitExpr(*range.children[1]);
        stopE = emitExpr(*range.children[2]);
        cond  = sc.value > 0.0 ? "<=" : ">=";
    } else {
        unsupported("malformed colon range");
    }

    open("for (" + loopVar + " = " + startE + "; " + loopVar + " " + cond + " " + stopE
         + "; " + loopVar + " += " + stepE + ")");
    emitStmt(body);
    close();
    types_ = cur;
}

void Emitter::emitWhile(const ASTNode &s)
{
    if (s.children.size() != 2) unsupported("while arity");
    const ASTNode &body = *s.children[1];

    TypeEnv cur = types_;
    for (int i = 0; i < kMaxFixpoint; ++i) {
        TypeEnv bodyEnv = cur;
        inferStmt(body, bodyEnv, reg_);
        TypeEnv next = joinEnv(cur, bodyEnv);
        if (next == cur) break;
        cur = next;
    }
    types_ = cur;

    open("while (" + emitExpr(*s.children[0]) + ")");
    emitStmt(body);
    close();
    types_ = cur;
}

void Emitter::emitIf(const ASTNode &s)
{
    const TypeEnv entry = types_;
    TypeEnv merged;
    bool    have = false;
    auto    mergeIn = [&](const TypeEnv &e) {
        merged = have ? joinEnv(merged, e) : e;
        have   = true;
    };

    for (std::size_t i = 0; i < s.branches.size(); ++i) {
        types_ = entry;  // condition + body typed from the incoming env
        const std::string cond = emitExpr(*s.branches[i].first);
        open((i == 0 ? "if (" : "else if (") + cond + ")");
        if (s.branches[i].second) emitStmt(*s.branches[i].second);
        close();
        mergeIn(types_);
    }
    if (s.elseBranch) {
        types_ = entry;
        open("else");
        emitStmt(*s.elseBranch);
        close();
        mergeIn(types_);
    } else {
        mergeIn(entry);  // no branch taken — fall-through
    }
    types_ = have ? merged : entry;
}

void Emitter::emitStmt(const ASTNode &s)
{
    switch (s.type) {
    case NodeType::BLOCK:
        for (const auto &c : s.children)
            if (c) emitStmt(*c);
        return;
    case NodeType::ASSIGN:     emitAssign(s); return;
    case NodeType::FOR_STMT:   emitFor(s);    return;
    case NodeType::WHILE_STMT: emitWhile(s);  return;
    case NodeType::IF_STMT:    emitIf(s);     return;
    case NodeType::EXPR_STMT:
        unsupported("expression statement (no value binding modelled)");
    default:
        unsupported("statement node kind");
    }
}

// The self-contained runtime prelude every emitted TU carries.
const char *kPrelude =
    "// Generated by numkit codegen (RawBuffer ABI). Do not edit.\n"
    "#include <cmath>\n"
    "#include <complex>\n"
    "#include <cstddef>\n"
    "#include <cstdint>\n"
    "#include <limits>\n"
    "#include <stdexcept>\n"
    "\n"
    "namespace nk_rt {\n"
    "template <class T>\n"
    "inline T index(const T* a, std::size_t len, double idx1) {\n"
    "    const std::size_t i = static_cast<std::size_t>(idx1);\n"
    "    if (idx1 < 1.0 || i > len) throw std::out_of_range(\"numkit: index out of bounds\");\n"
    "    return a[i - 1];\n"
    "}\n"
    "template <class T>\n"
    "inline void index_set(T* a, std::size_t len, double idx1, T v) {\n"
    "    const std::size_t i = static_cast<std::size_t>(idx1);\n"
    "    if (idx1 < 1.0 || i > len)\n"
    "        throw std::out_of_range(\"numkit: index out of bounds (RawBuffer ABI cannot grow)\");\n"
    "    a[i - 1] = v;\n"
    "}\n"
    "inline double mod(double a, double b) { return b == 0.0 ? a : a - std::floor(a / b) * b; }\n"
    "inline double rem(double a, double b) {\n"
    "    return b == 0.0 ? std::numeric_limits<double>::quiet_NaN() : std::fmod(a, b);\n"
    "}\n"
    "} // namespace nk_rt\n";

} // namespace

// ── whole-function emission (3f) ──────────────────────────────────────
EmittedFunction emitFunction(const ASTNode &funcDef,
                             const std::vector<ParamSpec> &params,
                             const TransferRegistry &reg)
{
    if (funcDef.type != NodeType::FUNCTION_DEF || funcDef.children.empty())
        unsupported("emitFunction expects a FUNCTION_DEF with a body");
    if (funcDef.returnNames.size() != 1)
        unsupported("only single-output functions are supported (MVP)");

    const ASTNode &body = *funcDef.children[0];

    // Entry env (parameter types) + array metadata + the signature's
    // parameter list, in source order.
    TypeEnv                                     entry;
    std::unordered_map<std::string, ArrayInfo>  arrays;
    std::vector<std::string>                    sigParams;
    for (const ParamSpec &p : params) {
        entry.set(p.name, {p.type, ConstVal::unknown()});
        if (isUnboxableScalarType(p.type)) {
            sigParams.push_back(cppScalarType(p.type.dtype) + " " + p.name);
        } else if (isBufferArrayType(p.type)) {
            const std::string lenVar = p.name + "_len";
            arrays[p.name] = {p.type.dtype, lenVar, /*isOutput=*/false};
            sigParams.push_back("const " + cppScalarType(p.type.dtype) + "* " + p.name
                                + ", std::size_t " + lenVar);
        } else {
            unsupported("parameter '" + p.name + "' has an unsupported type for RawBuffer ABI");
        }
    }

    const DeclTypeMap decls   = computeDeclTypes(body, entry, reg);
    const std::string retName = funcDef.returnNames[0];
    const auto        retIt   = decls.find(retName);
    if (retIt == decls.end())
        unsupported("output '" + retName + "' is never assigned");
    const InferredType retType = retIt->second;

    std::string retCpp;
    bool        arrayReturn = false;
    if (isUnboxableScalarType(retType)) {
        retCpp = cppScalarType(retType.dtype);
    } else if (isBufferArrayType(retType)) {
        arrayReturn = true;
        retCpp      = "void";
        const std::string lenVar = retName + "_len";
        arrays[retName] = {retType.dtype, lenVar, /*isOutput=*/true};
        sigParams.push_back(cppScalarType(retType.dtype) + "* " + retName
                            + ", std::size_t " + lenVar);
    } else {
        unsupported("output '" + retName + "' has an unsupported type for RawBuffer ABI");
    }

    std::string sig = retCpp + " " + funcDef.strValue + "(";
    for (std::size_t i = 0; i < sigParams.size(); ++i)
        sig += (i ? ", " : "") + sigParams[i];
    sig += ")";

    // Optimisation facts (brick 6). Empty facts => the always-correct
    // checked form; the analysis only enables faster lowering.
    const OptFacts opt = analyzeOptimizations(body, arrays);
    std::set<std::string> promotedVars;  // their counter is declared in the for, not hoisted
    for (const ASTNode *f : opt.promotedLoops) promotedVars.insert(f->strValue);

    // Emit hoisted local declarations (deterministic order) + the body.
    Emitter em(entry, reg, arrays, opt);
    std::map<std::string, InferredType> ordered(decls.begin(), decls.end());
    for (const auto &[name, t] : ordered) {
        if (entry.has(name) || arrays.count(name) || promotedVars.count(name))
            continue;  // params / arrays / promoted loop counters
        if (!isUnboxableScalarType(t))
            unsupported("local '" + name + "' is not an unboxable scalar (type "
                        + t.str() + ") — unsupported in RawBuffer ABI");
        em.hoistLocal(name, t);
    }
    em.emitStmt(body);
    if (!arrayReturn) em.emitReturnScalar(retName);

    std::string source = kPrelude;
    source += "\n";
    source += sig + " {\n";
    source += em.out();
    source += "}\n";

    return {source, funcDef.strValue, sig};
}

} // namespace numkit::codegen
