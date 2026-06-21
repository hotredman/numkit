// codegen/src/emitter.cpp — see emitter.hpp.

#include <numkit/codegen/emitter.hpp>

#include <numkit/codegen/indexing.hpp>

#include <algorithm>
#include <charconv>
#include <cmath>
#include <iterator>
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
                             const TransferRegistry &reg, const ClassRegistry *classes)
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
    inferStmt(body, env, reg, &dt, classes);
    return dt;
}

// ── the stateful Emitter (3b-3e) ──────────────────────────────────────
namespace {

constexpr int kMaxFixpoint = 16;  // matches inference.cpp; lattice is shallow

// Per-array metadata for an array parameter or the single output array.
struct ArrayInfo {
    ValueType   dtype;
    std::string lenVar;            // 1-D: the length EXPRESSION — `<name>_len`
                                   // for a param/output, `<name>.size()` for a local
    bool        isOutput = false;  // the function's caller-allocated out-param
    bool        is2D     = false;  // a 2-D matrix (column-major storage)
    std::string rowsVar, colsVar;  // 2-D: dim companions (`<name>_rows/_cols`)
    bool        isLocal  = false;  // an owned `std::vector` local (not a buffer ptr)
    std::string dataExpr;          // the element-pointer EXPRESSION — `<name>` for a
                                   // param/output buffer, `<name>.data()` for a local
    bool        isND     = false;  // a rank-N (N>=3) array (column-major flat storage)
    std::vector<std::string> ndDims;  // when isND: per-dim size EXPRESSIONS (rank = size())
};

// A 2-D matrix type (KnownDims with both dims > 1): indexed A(i,j),
// column-major. A KnownDims(1,n)/(n,1) is treated as a 1-D vector.
bool is2DMatrixType(const InferredType &t)
{
    return t.isConcrete() && t.shape.kind == ShapeKind::KnownDims
           && t.shape.rows > 1 && t.shape.cols > 1
           && isBufferArray(AbstractValue{t, ConstVal::unknown()});
}

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

[[noreturn]] void unsupported(const std::string &what);  // fwd (defined below)

// The C++ variable type for an object: a value class is the plain struct
// (value semantics); a handle class is nk_rt::handle<T> (shared reference).
std::string cppObjectType(int classId, const ClassRegistry *classes)
{
    const ClassInfo *ci = classes ? classes->byId(classId) : nullptr;
    if (!ci) unsupported("object type with no class registry / unknown classId");
    return ci->isHandle ? "nk_rt::handle<" + ci->name + ">" : ci->name;
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

// numel-equality fact store: arrayLenVar[A] = v means numel(A) == value of
// scalar variable v. The store is maintained FLOW-SENSITIVELY by
// walkForFacts (forward over the top-level statement sequence) so a fact
// is only ever used where it provably holds.

// A change to variable `v` invalidates (a) any fact about an array named
// `v` (its contents/size changed) and (b) any fact whose length TOKEN is
// `v` (the token's value changed, so numel(A)==v no longer pinned).
void invalidateLen(std::unordered_map<std::string, std::string> &lenVar,
                   const std::string &v)
{
    lenVar.erase(v);
    for (auto it = lenVar.begin(); it != lenVar.end();)
        it = (it->second == v) ? lenVar.erase(it) : std::next(it);
}

// All variables assigned anywhere within `node` (identifier / indexed-base
// / for-var / multi-assign). Used to conservatively invalidate facts a
// compound statement (if/while/loop body) may have changed.
void collectAssignedVars(const ASTNode &node, std::set<std::string> &out)
{
    if (node.type == NodeType::ASSIGN && !node.children.empty()) {
        const ASTNode &lhs = *node.children[0];
        if (lhs.type == NodeType::IDENTIFIER)
            out.insert(lhs.strValue);
        else if (!lhs.children.empty() && lhs.children[0]->type == NodeType::IDENTIFIER)
            out.insert(lhs.children[0]->strValue);  // indexed / field base
    }
    if (node.type == NodeType::FOR_STMT) out.insert(node.strValue);
    if (node.type == NodeType::MULTI_ASSIGN)
        for (const auto &rn : node.returnNames)
            if (!rn.empty()) out.insert(rn);
    for (const auto &c : node.children) if (c) collectAssignedVars(*c, out);
    for (const auto &br : node.branches) {
        if (br.first)  collectAssignedVars(*br.first, out);
        if (br.second) collectAssignedVars(*br.second, out);
    }
    if (node.elseBranch) collectAssignedVars(*node.elseBranch, out);
}

// Establish a fact from a single assignment `lhs = rhs` (lhs already
// invalidated by the caller): `lhs = numel(A)` -> numel(A)==lhs;
// `lhs = zeros(1,v)` / `zeros(v,1)` -> numel(lhs)==v.
void establishLenFact(const std::string &lhs, const ASTNode &rhs,
                      const std::unordered_map<std::string, ArrayInfo> &arrays,
                      std::unordered_map<std::string, std::string> &lenVar)
{
    std::string arr;
    if (isNumelOfArray(rhs, arrays, arr)) { lenVar[arr] = lhs; return; }
    if (rhs.type == NodeType::CALL && rhs.children.size() == 3
        && rhs.children[0]->type == NodeType::IDENTIFIER
        && (rhs.children[0]->strValue == "zeros" || rhs.children[0]->strValue == "ones")
        && arrays.count(lhs)) {
        const ASTNode &d1 = *rhs.children[1], &d2 = *rhs.children[2];
        if (d1.type == NodeType::NUMBER_LITERAL && d1.numValue == 1.0
            && d2.type == NodeType::IDENTIFIER)
            lenVar[lhs] = d2.strValue;
        else if (d2.type == NodeType::NUMBER_LITERAL && d2.numValue == 1.0
                 && d1.type == NodeType::IDENTIFIER)
            lenVar[lhs] = d1.strValue;
    }
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

// Promote one top-level `for k = 1:H` loop iff (gate, all must hold with
// facts valid AT THE LOOP): every use of k is the sole index of a buffer
// array whose numel is provably H; and k is used nowhere outside the loop.
bool tryPromoteLoop(const ASTNode &f, const ASTNode &funcBody,
                    const std::unordered_map<std::string, ArrayInfo> &arrays,
                    const std::unordered_map<std::string, std::string> &lenVar,
                    OptFacts &facts)
{
    if (f.children.size() != 2) return false;
    const ASTNode     &range = *f.children[0];
    const std::string &k     = f.strValue;
    if (!(range.type == NodeType::COLON_EXPR && range.children.size() == 2
          && range.children[0]->type == NodeType::NUMBER_LITERAL
          && range.children[0]->numValue == 1.0
          && range.children[1]->type == NodeType::IDENTIFIER))
        return false;
    const std::string     boundVar = range.children[1]->strValue;
    const ASTNode        &lbody    = *f.children[1];
    std::set<std::string> idxArrays;
    bool safe = collectCleanIndexUses(lbody, k, arrays, idxArrays) && !idxArrays.empty();
    for (const auto &A : idxArrays) {
        auto it = lenVar.find(A);
        if (it == lenVar.end() || it->second != boundVar) { safe = false; break; }
    }
    if (!safe || countIdentUses(funcBody, k) != countIdentUses(lbody, k)) return false;

    std::string bound;  // prefer a param array's companion (== numel exactly)
    for (const auto &A : idxArrays)
        if (!arrays.at(A).isOutput) { bound = arrays.at(A).lenVar; break; }
    if (bound.empty()) bound = arrays.at(*idxArrays.begin()).lenVar;
    facts.promotedLoops.insert(&f);
    facts.loopBound[&f] = bound;
    return true;
}

// Flow-sensitive forward walk over a statement sequence, maintaining the
// numel-fact store (`lenVar`) with invalidation. A top-level `for` is
// promoted against the facts valid at its position; any compound statement
// conservatively invalidates every variable it could assign. (Only
// top-level loops are promoted in this MVP; a loop nested in a branch /
// another loop stays the always-correct checked form.)
void walkForFacts(const ASTNode &block, const ASTNode &funcBody,
                  const std::unordered_map<std::string, ArrayInfo> &arrays,
                  std::unordered_map<std::string, std::string> &lenVar, OptFacts &facts)
{
    for (const auto &sp : block.children) {
        if (!sp) continue;
        const ASTNode &s = *sp;
        if (s.type == NodeType::ASSIGN && !s.children.empty()) {
            const ASTNode &lhs = *s.children[0];
            if (lhs.type == NodeType::IDENTIFIER) {
                invalidateLen(lenVar, lhs.strValue);
                if (s.children.size() == 2)
                    establishLenFact(lhs.strValue, *s.children[1], arrays, lenVar);
            } else if (!lhs.children.empty()
                       && lhs.children[0]->type == NodeType::IDENTIFIER) {
                invalidateLen(lenVar, lhs.children[0]->strValue);  // indexed base changed
            }
        } else if (s.type == NodeType::FOR_STMT) {
            tryPromoteLoop(s, funcBody, arrays, lenVar, facts);
            std::set<std::string> assigned;  // body may rebind facts -> invalidate
            collectAssignedVars(s, assigned);
            for (const auto &v : assigned) invalidateLen(lenVar, v);
        } else {
            // if / while / switch / try / multi-assign / expr — conservative:
            // every variable it might assign becomes unknown.
            std::set<std::string> assigned;
            collectAssignedVars(s, assigned);
            for (const auto &v : assigned) invalidateLen(lenVar, v);
        }
    }
}

OptFacts analyzeOptimizations(const ASTNode &body,
                              const std::unordered_map<std::string, ArrayInfo> &arrays)
{
    std::unordered_map<std::string, std::string> lenVar;
    OptFacts                                     facts;
    walkForFacts(body, body, arrays, lenVar, facts);
    return facts;
}

// ── interprocedural call emission (§12 brick 1b) ──────────────────────
// A specialisation still to emit: callee body + the concrete arg types it
// is specialised to + its mangled C++ symbol.
struct CallSite {
    const ASTNode             *def = nullptr;
    std::vector<InferredType>  argTypes;
    std::string                mangled;
    // Pre-typed non-parameter locals to seed (a constructor's output object).
    std::vector<ParamSpec>     extraSeed;
};

// Shared across all functions emitted for one program: the function table,
// a worklist of specialisations still to emit, and the mangled names
// already queued (dedup).
struct ProgramEmitCtx {
    const FunctionTable   *funcs = nullptr;
    std::vector<CallSite>  pending;
    std::set<std::string>  seen;
};

struct OneFn {
    std::string signature;
    std::string definition;  // signature + " { ... }"
};

// A C++-identifier-safe code for a type so distinct specialisations get
// distinct symbols: dtype letter + shape tag (scalar = none).
std::string typeCode(const InferredType &t)
{
    if (!t.isConcrete()) return "X";
    if (t.dtype == ValueType::OBJECT) return "o" + std::to_string(t.classId);
    std::string c;
    switch (t.dtype) {
    case ValueType::DOUBLE:  c = "d";   break;
    case ValueType::SINGLE:  c = "f";   break;
    case ValueType::COMPLEX: c = "c";   break;
    case ValueType::LOGICAL: c = "b";   break;
    case ValueType::INT8:    c = "i8";  break;
    case ValueType::INT16:   c = "i16"; break;
    case ValueType::INT32:   c = "i32"; break;
    case ValueType::INT64:   c = "i64"; break;
    case ValueType::UINT8:   c = "u8";  break;
    case ValueType::UINT16:  c = "u16"; break;
    case ValueType::UINT32:  c = "u32"; break;
    case ValueType::UINT64:  c = "u64"; break;
    default:                 c = "o";   break;
    }
    switch (t.shape.kind) {
    case ShapeKind::Scalar:    break;
    case ShapeKind::RowVector: c += "r"; break;
    case ShapeKind::ColVector: c += "k"; break;
    case ShapeKind::KnownDims:
        c += "m" + std::to_string(t.shape.rows) + "x" + std::to_string(t.shape.cols);
        break;
    case ShapeKind::NDims:
        c += "n";  // ranked: rank + each dim (0 = unknown), distinct per shape
        for (std::size_t d : t.shape.nd) c += std::to_string(d) + "_";
        break;
    case ShapeKind::Unknown:   c += "u"; break;
    }
    return c;
}

std::string mangle(const std::string &base, const std::vector<InferredType> &args)
{
    if (args.empty()) return base + "__v";
    std::string m = base;
    for (const auto &a : args) m += "__" + typeCode(a);
    return m;
}

std::vector<ArgInfo> toArgInfos(const std::vector<InferredType> &types)
{
    std::vector<ArgInfo> out;
    out.reserve(types.size());
    for (const auto &t : types) out.push_back(ArgInfo(t, ConstVal::unknown()));
    return out;
}

class Emitter {
public:
    Emitter(TypeEnv types, const TransferRegistry &reg,
            std::unordered_map<std::string, ArrayInfo> arrays, OptFacts opt,
            ProgramEmitCtx *ctx = nullptr, const ClassRegistry *classes = nullptr,
            bool bridge = false)
        : types_(std::move(types)), reg_(reg), arrays_(std::move(arrays)),
          opt_(std::move(opt)), ctx_(ctx), classes_(classes), bridge_(bridge)
    {}

    // Hoist a local declaration at function entry (scalar or object).
    void hoistLocal(const std::string &name, const InferredType &t)
    {
        if (t.isObject()) {
            const ClassInfo *ci = classes_ ? classes_->byId(t.classId) : nullptr;
            if (!ci) unsupported("object local '" + name + "' with no class registry");
            if (ci->isHandle)
                line("nk_rt::handle<" + ci->name + "> " + name + ";");  // null until assigned
            else
                line(ci->name + " " + name + "{};");                    // default-constructed
            return;
        }
        line(cppScalarType(t.dtype) + " " + name + " = " + zeroLiteral(t.dtype) + ";");
    }

    // Declare every owned-vector array local (deterministic order). Their
    // ArrayInfo carries isLocal; storage is a runtime-sized std::vector filled
    // by a later assignment (zeros/ones or a bridged array call).
    void hoistArrayLocals()
    {
        std::map<std::string, ValueType> ordered;
        for (const auto &[name, ai] : arrays_)
            if (ai.isLocal) ordered.emplace(name, ai.dtype);
        for (const auto &[name, dtype] : ordered)
            line("std::vector<" + cppScalarType(dtype) + "> " + name + ";");
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
    std::string emitUserCall(const std::string &name, const ASTNode &call);
    std::string emitMethodCall(const ASTNode &call);
    std::string emitConstruct(const std::string &name, const ASTNode &call);
    void        appendCallArg(const ASTNode &arg, std::vector<InferredType> &argTypes,
                              std::string &argList);
    std::string emitIndexRead(const std::string &base, const ASTNode &call);
    // True if `e` is a pure ELEMENTWISE expression — literals, scalar vars,
    // whole arrays, and only elementwise ops (+ - .* ./ .\ .^, unary +/-).
    // Collects the distinct whole-array variable names into `arrays`. (Matrix
    // ops *,/,^ and calls/indexing make it false.)
    bool        collectElementwise(const ASTNode &e, std::set<std::string> &arrays) const;
    void        emitAssign(const ASTNode &s);
    void        emitMultiAssign(const ASTNode &s);
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
    // Non-empty inside an elementwise-array fill loop: the 0-based size_t
    // index, so a bare whole-array `x` emits the element `x[<idx>]`.
    std::string                                 elementCtx_;
    // Non-null when emitting as part of a multi-function program: routes
    // user-function calls and accumulates the specialisations to emit.
    ProgramEmitCtx                             *ctx_ = nullptr;
    // Non-null when classes are in play: resolves object field access,
    // value-vs-handle representation, and constructor calls.
    const ClassRegistry                        *classes_ = nullptr;
    // Bridged emission (DESIGN.md §6a): when true, a call the emitter cannot
    // lower but whose result inference proves scalar is emitted as a C-ABI
    // call (nk_rt::bridge_scalar) instead of throwing.
    bool                                        bridge_ = false;
};

// MATLAB unary-math name -> std:: name. Restricted to functions that BOTH
// have a registered transfer (so the result types to a concrete scalar and
// this lowering is actually reachable) AND have an exact std equivalent.
// Builtins outside this set (sqrt/log/asin/… not yet typed; sign no clean
// std form) hit the explicit boundary in emitBuiltinCall and throw — never
// silently wrong, never dead-but-unreachable speculation.
const char *unaryMathStd(const std::string &name)
{
    static const std::unordered_map<std::string, const char *> kMap = {
        {"sin", "sin"},     {"cos", "cos"},   {"tan", "tan"},   {"atan", "atan"},
        {"sinh", "sinh"},   {"cosh", "cosh"}, {"tanh", "tanh"}, {"exp", "exp"},
        {"floor", "floor"}, {"ceil", "ceil"}, {"round", "round"},
        {"abs", "abs"},     {"fix", "trunc"},
        // total-on-ℝ, real-only (transfer refuses complex) — see
        // realOnlyMathUnaryTransfer
        {"asinh", "asinh"}, {"erf", "erf"}, {"erfc", "erfc"}, {"expm1", "expm1"}};
    const auto it = kMap.find(name);
    return it == kMap.end() ? nullptr : it->second;
}

// MATLAB binary-math name -> std:: name. Total on ℝ², typed scalar->scalar by
// realBinaryMathTransfer (complex/array -> Dynamic, so this only fires for a
// real scalar result).
const char *binaryMathStd(const std::string &name)
{
    static const std::unordered_map<std::string, const char *> kMap = {
        {"atan2", "atan2"}, {"hypot", "hypot"}};
    const auto it = kMap.find(name);
    return it == kMap.end() ? nullptr : it->second;
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
        if (isArrayVar(e.strValue)) {
            // Inside an elementwise-array fill loop a whole array means its
            // current element; in any other (scalar) context it is an error.
            if (!elementCtx_.empty())
                return arrays_.at(e.strValue).dataExpr + "[" + elementCtx_ + "]";
            unsupported("bare array identifier '" + e.strValue + "' in scalar context");
        }
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
        if (callee.type == NodeType::FIELD_ACCESS && classes_)
            return emitMethodCall(e);  // obj.method(args)
        if (callee.type != NodeType::IDENTIFIER)
            unsupported("non-identifier callee");
        if (isArrayVar(callee.strValue))
            return emitIndexRead(callee.strValue, e);
        // A class name (not a variable in scope) -> construction.
        if (classes_ && !types_.has(callee.strValue) && classes_->has(callee.strValue))
            return emitConstruct(callee.strValue, e);
        // A user function (not a variable in scope) takes priority over a
        // same-named builtin (MATLAB path shadowing) when emitting a program.
        if (ctx_ && ctx_->funcs && !types_.has(callee.strValue)
            && ctx_->funcs->has(callee.strValue))
            return emitUserCall(callee.strValue, e);
        return emitBuiltinCall(callee.strValue, e);
    }
    case NodeType::FIELD_ACCESS: {  // obj.field read -> obj.field / obj->field
        if (e.children.empty()) unsupported("field access arity");
        const AbstractValue base = inferExpr(*e.children[0], types_, reg_, classes_);
        if (!base.type.isObject() || !classes_)
            unsupported("field access on a non-object value");
        const ClassInfo *ci = classes_->byId(base.type.classId);
        if (!ci || !ci->field(e.strValue))
            unsupported("unknown field '" + e.strValue + "'");
        return emitExpr(*e.children[0]) + (ci->isHandle ? "->" : ".") + e.strValue;
    }
    default:
        unsupported("expression node kind");
    }
}

std::string Emitter::emitBuiltinCall(const std::string &name, const ASTNode &call)
{
    const std::size_t nargs = call.children.size() - 1;

    // numel / length / ndims of an array variable. 1-D: the length companion.
    // 2-D: numel = rows*cols; length = max(rows,cols). N-D: product / max of dims.
    if ((name == "numel" || name == "length" || name == "ndims") && nargs == 1
        && call.children[1]->type == NodeType::IDENTIFIER
        && isArrayVar(call.children[1]->strValue)) {
        const ArrayInfo &ai = arrays_.at(call.children[1]->strValue);
        if (name == "ndims")  // MATLAB: vectors/matrices are 2-D; N-D reports its rank
            return "static_cast<double>(" + std::to_string(ai.isND ? ai.ndDims.size() : std::size_t{2})
                   + ")";
        if (ai.isND) {
            std::string e = ai.ndDims[0];
            for (std::size_t i = 1; i < ai.ndDims.size(); ++i)
                e = name == "numel" ? (e + " * " + ai.ndDims[i])
                                    : ("(" + e + " > " + ai.ndDims[i] + " ? " + e + " : "
                                       + ai.ndDims[i] + ")");  // length = max dim
            return "static_cast<double>(" + e + ")";
        }
        if (!ai.is2D)
            return "static_cast<double>(" + ai.lenVar + ")";
        if (name == "numel")
            return "static_cast<double>(" + ai.rowsVar + " * " + ai.colsVar + ")";
        return "static_cast<double>(" + ai.rowsVar + " > " + ai.colsVar + " ? " + ai.rowsVar
               + " : " + ai.colsVar + ")";  // length = max(rows, cols)
    }

    // size(A, dim) with a compile-time literal dim: the dim's size (2-D
    // rows/cols, N-D the dim, out-of-range -> 1). A 1-D buffer's orientation
    // is untracked in the RawBuffer ABI, so size(vec,dim) falls through (to a
    // bridged call or the explicit boundary), not handled here.
    if (name == "size" && nargs == 2 && call.children[1]->type == NodeType::IDENTIFIER
        && isArrayVar(call.children[1]->strValue)
        && call.children[2]->type == NodeType::NUMBER_LITERAL) {
        const ArrayInfo &ai = arrays_.at(call.children[1]->strValue);
        const double     kd = call.children[2]->numValue;
        const auto       k  = static_cast<std::size_t>(kd);
        const bool       kok = kd >= 1.0 && static_cast<double>(k) == kd;
        if (kok && ai.isND)
            return "static_cast<double>("
                   + (k <= ai.ndDims.size() ? ai.ndDims[k - 1] : std::string("1")) + ")";
        if (kok && ai.is2D)
            return "static_cast<double>("
                   + (k == 1 ? ai.rowsVar : k == 2 ? ai.colsVar : std::string("1")) + ")";
    }

    if (nargs == 1)
        if (const char *fn = unaryMathStd(name))
            return std::string("std::") + fn + "(" + emitExpr(*call.children[1]) + ")";

    if (nargs == 2)
        if (const char *fn = binaryMathStd(name))
            return std::string("std::") + fn + "(" + emitExpr(*call.children[1]) + ", "
                   + emitExpr(*call.children[2]) + ")";

    // Bridged fallback (opt-in, DESIGN.md §6a): a builtin the emitter cannot
    // lower goes through the C-ABI to the runtime — but ONLY when inference
    // proves the result is an unboxed real scalar (Contract 2: the fast form
    // is emitted under a proven precondition; otherwise we still throw). Each
    // argument is emitted as a scalar C++ expression, so an array/complex arg
    // hits the existing scalar-context boundary rather than miscompiling.
    if (bridge_) {
        const AbstractValue res = inferExpr(call, types_, reg_, classes_);
        if (res.type.isUnboxableScalar() && res.type.dtype != ValueType::COMPLEX) {
            std::string out = "nk_rt::bridge_scalar(\"" + name + "\", {";
            for (std::size_t i = 1; i < call.children.size(); ++i)
                out += (i > 1 ? ", " : "") + emitExpr(*call.children[i]);
            out += "})";
            return out;
        }
    }

    unsupported("builtin call '" + name + "' (arity " + std::to_string(nargs) + ")");
}

// Append one interprocedural call argument: a scalar or object is passed by
// value (one C++ arg); an array VARIABLE is passed as `ptr, len` (two C++
// args, reusing its length companion). An array expression (no companion)
// or a Dynamic arg is refused (v1). Its inferred type drives the callee
// specialisation's signature.
void Emitter::appendCallArg(const ASTNode &arg, std::vector<InferredType> &argTypes,
                            std::string &argList)
{
    const AbstractValue av = inferExpr(arg, types_, reg_, classes_);
    if (!argList.empty()) argList += ", ";
    if (isUnboxableScalarType(av.type) || av.type.isObject()) {
        argList += emitExpr(arg);
    } else if (isBufferArrayType(av.type) && arg.type == NodeType::IDENTIFIER
               && isArrayVar(arg.strValue)) {
        const ArrayInfo &ai = arrays_.at(arg.strValue);
        argList += ai.dataExpr + ", " + ai.lenVar;  // ptr + len (local: .data()/.size())
    } else {
        unsupported("call argument must be a scalar, object, or array variable (v1)");
    }
    argTypes.push_back(av.type);
}

// A call to a user-defined function within a program (ctx_ set). Arguments
// may be scalars, objects, or array variables; the result is scalar/object
// or void. Emits a direct call to the monomorphised specialisation and
// queues it for emission.
std::string Emitter::emitUserCall(const std::string &name, const ASTNode &call)
{
    const ASTNode *def = ctx_->funcs->find(name);
    if (!def) unsupported("user call to unknown function '" + name + "'");

    std::vector<InferredType> argTypes;
    std::string               argList;
    for (std::size_t i = 1; i < call.children.size(); ++i)
        appendCallArg(*call.children[i], argTypes, argList);
    if (argTypes.size() != def->paramNames.size())
        unsupported("arity mismatch calling '" + name + "'");

    const std::size_t nout = def->returnNames.size();
    if (nout >= 2)
        unsupported("multi-output call not yet supported (2b): '" + name + "'");
    if (nout == 1) {  // 0 -> void function, result discarded
        const InferredType ret = reg_.apply(name, toArgInfos(argTypes));
        if (!isUnboxableScalarType(ret))
            unsupported("interprocedural call result must be an unboxed scalar (v1): '"
                        + name + "'");
    }

    const std::string mangled = mangle(name, argTypes);
    if (ctx_->seen.insert(mangled).second)
        ctx_->pending.push_back({def, argTypes, mangled});
    return mangled + "(" + argList + ")";
}

// A method call `obj.m(args)` (ctx_ set). The object is the implicit first
// argument; dispatch is monomorphic (the exact class is known). Emits a
// direct call to the specialisation `Class__m(self, args)` and queues it.
// v1: extra args + result are unboxed scalars (the result may also be an
// object — value-in/out methods return the modified object).
std::string Emitter::emitMethodCall(const ASTNode &call)
{
    const ASTNode &callee = *call.children[0];  // FIELD_ACCESS: method on object
    if (callee.children.empty()) unsupported("method call arity");
    if (!ctx_) unsupported("method call requires program emission (emitProgram)");

    const AbstractValue base = inferExpr(*callee.children[0], types_, reg_, classes_);
    if (!base.type.isObject() || !classes_) unsupported("method call on a non-object");
    const ClassInfo *ci = classes_->byId(base.type.classId);
    const ASTNode   *md = ci ? ci->method(callee.strValue) : nullptr;
    if (!md) unsupported("unknown method '" + callee.strValue + "'");

    std::vector<InferredType> argTypes;
    argTypes.push_back(base.type);                       // self is arg 0
    std::string argList = emitExpr(*callee.children[0]);  // the object expr
    for (std::size_t i = 1; i < call.children.size(); ++i)
        appendCallArg(*call.children[i], argTypes, argList);

    const std::size_t nout = md->returnNames.size();
    if (nout >= 2)
        unsupported("multi-output method not yet supported (2b): '" + callee.strValue + "'");
    if (nout == 1) {  // 0 -> void method (in-place mutator), result discarded
        const InferredType ret =
            reg_.apply(ci->name + "::" + callee.strValue, toArgInfos(argTypes));
        if (!isUnboxableScalarType(ret) && !ret.isObject())
            unsupported("method result must be an unboxed scalar or object (v1): '"
                        + callee.strValue + "'");
    }

    const std::string mangled = mangle(ci->name + "__" + callee.strValue, argTypes);
    if (ctx_->seen.insert(mangled).second)
        ctx_->pending.push_back({md, argTypes, mangled});
    return mangled + "(" + argList + ")";
}

// Object construction `Name(args)`. 1a: default construction only — a
// value class -> Name{} (fields default-initialised), a handle class ->
// nk_rt::handle<Name>::make(). An explicit constructor (1b) and
// argument-taking construction are refused for now.
std::string Emitter::emitConstruct(const std::string &name, const ASTNode &call)
{
    const ClassInfo *ci = classes_->byName(name);
    if (!ci) unsupported("construction of unknown class '" + name + "'");
    const std::size_t nargs = call.children.size() - 1;
    const ASTNode    *ctor  = ci->method(name);

    if (ctor) {
        // Explicit constructor: emit a call to its specialisation, whose
        // OUTPUT object is seeded as Object(this class) so its field writes
        // type (the object can't be inferred from field writes alone).
        if (!ctx_) unsupported("constructor call requires program emission (emitProgram)");
        if (ctor->returnNames.size() != 1)
            unsupported("constructor of '" + name + "' must have one output");
        if (ctor->paramNames.size() != nargs)
            unsupported("constructor of '" + name + "' arity mismatch");
        std::vector<InferredType> argTypes;
        std::string               argList;
        for (std::size_t i = 1; i < call.children.size(); ++i) {
            const AbstractValue av = inferExpr(*call.children[i], types_, reg_, classes_);
            if (!isUnboxableScalarType(av.type))
                unsupported("constructor arg must be an unboxed scalar (v1): '" + name + "'");
            argTypes.push_back(av.type);
            argList += (i > 1 ? ", " : "") + emitExpr(*call.children[i]);
        }
        const std::string mangled = mangle(ci->name + "__ctor", argTypes);
        if (ctx_->seen.insert(mangled).second) {
            CallSite cs;
            cs.def       = ctor;
            cs.argTypes  = argTypes;
            cs.mangled   = mangled;
            cs.extraSeed = {{ctor->returnNames[0], InferredType::object(ci->id)}};
            ctx_->pending.push_back(cs);
        }
        return mangled + "(" + argList + ")";
    }

    // No explicit constructor: default construction.
    if (nargs != 0)
        unsupported("class '" + name + "' has no constructor accepting arguments");
    return ci->isHandle ? "nk_rt::handle<" + ci->name + ">::make()" : ci->name + "{}";
}

std::string Emitter::emitIndexRead(const std::string &base, const ASTNode &call)
{
    const ArrayInfo  &ai  = arrays_.at(base);
    const std::string ptr = ai.dataExpr;  // `base` (buffer) or `base.data()` (local vec)

    // brick 6: inside a promoted clean-index loop, A(counter) is provably
    // in-bounds -> direct 0-based access, no check, no -1.
    if (promotedCounter_ && call.children.size() == 2
        && call.children[1]->type == NodeType::IDENTIFIER
        && call.children[1]->strValue == *promotedCounter_)
        return ptr + "[" + *promotedCounter_ + "]";

    // Rank-N (N>=3) read A(i,j,k,...) -> column-major nk_rt::indexN.
    if (ai.isND) {
        if (call.children.size() - 1 != ai.ndDims.size())
            unsupported("N-D index arity for '" + base + "' (expected "
                        + std::to_string(ai.ndDims.size()) + " subscripts)");
        std::string dims, subs;
        for (std::size_t k = 0; k < ai.ndDims.size(); ++k) dims += (k ? ", " : "") + ai.ndDims[k];
        for (std::size_t i = 1; i < call.children.size(); ++i)
            subs += (i > 1 ? ", " : "") + emitExpr(*call.children[i]);
        return "nk_rt::indexN(" + ptr + ", {" + dims + "}, {" + subs + "})";
    }

    std::vector<AbstractValue> idx;
    for (std::size_t i = 1; i < call.children.size(); ++i)
        idx.push_back(inferExpr(*call.children[i], types_, reg_, classes_));

    const IndexPlan plan = planIndexRead(arrayValue(base), idx);
    if (plan.form == IndexForm::Subscript2D) {
        if (!ai.is2D) unsupported("A(i,j) on a non-matrix '" + base + "'");
        return "nk_rt::index2(" + ptr + ", " + ai.rowsVar + ", " + ai.colsVar + ", "
               + emitExpr(*call.children[1]) + ", " + emitExpr(*call.children[2]) + ")";
    }
    if (plan.form != IndexForm::LinearScalar || ai.is2D)
        unsupported("index read form for '" + base
                    + "' (a 1-D scalar index, or A(i,j) on a matrix)");

    const std::string idxExpr = emitExpr(*call.children[1]);
    if (plan.boundsChecked)
        return "nk_rt::index(" + ptr + ", " + ai.lenVar + ", " + idxExpr + ")";
    return ptr + "[static_cast<std::size_t>(" + idxExpr + ") - 1]";  // proven in-bounds
}

void Emitter::emitIndexWrite(const ASTNode &lhsCall, const ASTNode &rhs)
{
    const std::string &base = lhsCall.children[0]->strValue;
    if (!isArrayVar(base)) unsupported("indexed write to non-array '" + base + "'");
    const ArrayInfo  &ai  = arrays_.at(base);
    const std::string ptr = ai.dataExpr;  // `base` (buffer) or `base.data()` (local vec)

    // brick 6: inside a promoted clean-index loop, A(counter) is provably
    // in-bounds -> direct 0-based store, no check.
    if (promotedCounter_ && lhsCall.children.size() == 2
        && lhsCall.children[1]->type == NodeType::IDENTIFIER
        && lhsCall.children[1]->strValue == *promotedCounter_) {
        line(ptr + "[" + *promotedCounter_ + "] = " + emitExpr(rhs) + ";");
        return;
    }

    // Rank-N (N>=3) write A(i,j,k,...) = v -> column-major nk_rt::indexN_set.
    if (ai.isND) {
        if (!ai.isLocal && !ai.isOutput)
            unsupported("N-D write to a read-only matrix parameter '" + base + "'");
        if (lhsCall.children.size() - 1 != ai.ndDims.size())
            unsupported("N-D index arity for '" + base + "' (expected "
                        + std::to_string(ai.ndDims.size()) + " subscripts)");
        std::string dims, subs;
        for (std::size_t k = 0; k < ai.ndDims.size(); ++k) dims += (k ? ", " : "") + ai.ndDims[k];
        for (std::size_t i = 1; i < lhsCall.children.size(); ++i)
            subs += (i > 1 ? ", " : "") + emitExpr(*lhsCall.children[i]);
        line("nk_rt::indexN_set(" + ptr + ", {" + dims + "}, {" + subs + "}, " + emitExpr(rhs)
             + ");");
        return;
    }

    std::vector<AbstractValue> idx;
    for (std::size_t i = 1; i < lhsCall.children.size(); ++i)
        idx.push_back(inferExpr(*lhsCall.children[i], types_, reg_, classes_));
    const AbstractValue rhsAV = inferExpr(rhs, types_, reg_, classes_);

    const IndexPlan plan = planIndexWrite(arrayValue(base), idx, rhsAV);
    if (plan.form == IndexForm::Subscript2D) {
        // A 2-D PARAM is read-only (const T*); a mutable 2-D (a local or the
        // output) is writable via column-major index2_set.
        if (!ai.is2D) unsupported("A(i,j) on a non-matrix '" + base + "'");
        if (!ai.isLocal && !ai.isOutput)
            unsupported("2-D write to a read-only matrix parameter '" + base + "'");
        line("nk_rt::index2_set(" + ptr + ", " + ai.rowsVar + ", " + ai.colsVar + ", "
             + emitExpr(*lhsCall.children[1]) + ", " + emitExpr(*lhsCall.children[2]) + ", "
             + emitExpr(rhs) + ");");
        return;
    }
    if (plan.form != IndexForm::LinearScalar || ai.is2D)
        unsupported("index write form for '" + base
                    + "' (a 1-D scalar store, or A(i,j) on a matrix)");

    const std::string idxExpr = emitExpr(*lhsCall.children[1]);
    const std::string rhsExpr = emitExpr(rhs);
    if (plan.boundsChecked)
        line("nk_rt::index_set(" + ptr + ", " + ai.lenVar + ", " + idxExpr + ", " + rhsExpr + ");");
    else
        line(ptr + "[static_cast<std::size_t>(" + idxExpr + ") - 1] = " + rhsExpr + ";");
}

bool Emitter::collectElementwise(const ASTNode &e, std::set<std::string> &arrays) const
{
    switch (e.type) {
    case NodeType::NUMBER_LITERAL:
    case NodeType::BOOL_LITERAL:
    case NodeType::IMAG_LITERAL:
        return true;
    case NodeType::IDENTIFIER:
        if (isArrayVar(e.strValue)) arrays.insert(e.strValue);
        return true;  // a scalar var, or a whole array (recorded)
    case NodeType::BINARY_OP: {
        // Operator SYMBOLS (the AST stores the symbol; inference maps it to the
        // transfer name). Elementwise only: + - .* ./ .\ .^  — NOT the matrix
        // forms * / \ ^ (those aren't elementwise except for scalars).
        static const std::set<std::string> kElementwise = {"+", "-", ".*", "./", ".\\", ".^"};
        return kElementwise.count(e.strValue) != 0 && e.children.size() == 2
               && collectElementwise(*e.children[0], arrays)
               && collectElementwise(*e.children[1], arrays);
    }
    case NodeType::UNARY_OP:
        return (e.strValue == "-" || e.strValue == "+") && e.children.size() == 1
               && collectElementwise(*e.children[0], arrays);
    case NodeType::CALL: {
        // An ELEMENTWISE-math call (sin/cos/erf/atan2/…) over elementwise args
        // is itself elementwise — sin(x)[i] == std::sin(x[i]). An index `x(k)`
        // (callee is an array var) or any other call is not.
        if (e.children.empty() || e.children[0]->type != NodeType::IDENTIFIER) return false;
        const std::string &callee = e.children[0]->strValue;
        if (isArrayVar(callee)) return false;  // x(k) indexing -> not pure elementwise
        const std::size_t nargs = e.children.size() - 1;
        const bool isMath = (nargs == 1 && unaryMathStd(callee) != nullptr)
                            || (nargs == 2 && binaryMathStd(callee) != nullptr);
        if (!isMath) return false;
        for (std::size_t i = 1; i < e.children.size(); ++i)
            if (!collectElementwise(*e.children[i], arrays)) return false;
        return true;
    }
    default:
        return false;  // indexing / fields / non-math calls -> not pure elementwise
    }
}

void Emitter::emitAssign(const ASTNode &s)
{
    if (s.children.size() != 2) unsupported("assign arity");
    const ASTNode &lhs = *s.children[0];
    const ASTNode &rhs = *s.children[1];

    if (lhs.type == NodeType::IDENTIFIER) {
        const std::string &name = lhs.strValue;

        // Array initialised by a size constructor: `a = zeros(1,n)` / `ones`.
        // The OUTPUT out-param (caller-sized) becomes a fill loop over its
        // length; an owned-vector LOCAL is `a.assign(numel, fill)` (numel =
        // product of the size args).
        if (isArrayVar(name)
            && (arrays_.at(name).isLocal || !arrays_.at(name).is2D)  // a local may be 2-D
            && (arrays_.at(name).isOutput || arrays_.at(name).isLocal)
            && rhs.type == NodeType::CALL && !rhs.children.empty()
            && rhs.children[0]->type == NodeType::IDENTIFIER
            && (rhs.children[0]->strValue == "zeros"
                || rhs.children[0]->strValue == "ones")) {
            const ArrayInfo  &ai   = arrays_.at(name);
            const std::string fill = rhs.children[0]->strValue == "zeros"
                                         ? zeroLiteral(ai.dtype)
                                         : "1";
            if (ai.isLocal) {
                std::string numel;  // product of the size args (zeros(1,n) -> 1*n)
                for (std::size_t i = 1; i < rhs.children.size(); ++i)
                    numel += (i > 1 ? " * " : "")
                             + ("static_cast<std::size_t>(" + emitExpr(*rhs.children[i]) + ")");
                if (numel.empty()) numel = "0";
                line(name + ".assign(" + numel + ", " + fill + ");");
            } else {
                open("for (std::size_t __i = 0; __i < " + ai.lenVar + "; ++__i)");
                line(ai.dataExpr + "[__i] = " + fill + ";");
                close();
            }
            // Record the array type so a later `name(k)` infers element-access
            // (the env, not arrays_, drives inferExpr).
            types_.set(name, {InferredType::concrete(ai.dtype, Shape::rowVector()),
                              ConstVal::unknown()});
            return;
        }
        // Output array from a BRIDGED call (opt-in): y = sin(x). Sound ONLY
        // when inference proves the RHS is a concrete array (Contract 2); box
        // the (array-var / scalar) args, call the runtime (1 output), and
        // unbox into the caller-allocated out-param. v1: a DOUBLE output.
        if (bridge_ && isArrayVar(name) && !arrays_.at(name).is2D
            && (arrays_.at(name).isOutput || arrays_.at(name).isLocal)
            && arrays_.at(name).dtype == ValueType::DOUBLE
            && rhs.type == NodeType::CALL && !rhs.children.empty()
            && rhs.children[0]->type == NodeType::IDENTIFIER
            // An elementwise-math call (sin/erf/…) lowers NATIVELY below — only
            // bridge a call the emitter cannot lower (sort, fft, …).
            && unaryMathStd(rhs.children[0]->strValue) == nullptr
            && binaryMathStd(rhs.children[0]->strValue) == nullptr) {
            const AbstractValue res = inferExpr(rhs, types_, reg_, classes_);
            if (res.type.isConcrete() && !res.type.shape.isScalar()
                && res.type.dtype == ValueType::DOUBLE) {
                const ArrayInfo  &ai     = arrays_.at(name);
                const std::string callee = rhs.children[0]->strValue;
                const std::size_t nargs  = rhs.children.size() - 1;
                std::string       boxed;
                for (std::size_t i = 1; i < rhs.children.size(); ++i) {
                    const ASTNode &arg = *rhs.children[i];
                    if (i > 1) boxed += ", ";
                    if (arg.type == NodeType::IDENTIFIER && isArrayVar(arg.strValue)) {
                        const ArrayInfo &aai = arrays_.at(arg.strValue);
                        if (aai.is2D) unsupported("bridged call: 2-D array argument (v1)");
                        boxed += "nk_box_array(" + aai.dataExpr + ", " + aai.lenVar + ")";
                    } else {
                        boxed += "nk_box_scalar(" + emitExpr(arg) + ")";
                    }
                }
                // A LOCAL resizes its owned vector (bridge_to_vec); the OUTPUT
                // fills its fixed, caller-sized out-param (bridge_into).
                const std::string fn   = ai.isLocal ? "nk_rt::bridge_to_vec(\""
                                                     : "nk_rt::bridge_into(\"";
                const std::string dest = ai.isLocal ? (", " + name + ");")
                                                    : (", " + name + ", " + ai.lenVar + ");");
                if (nargs == 0) {
                    line(fn + callee + "\", nullptr, 0" + dest);
                } else {
                    open("");  // a fresh scope for the temporary arg array
                    line("nk_val __nk_args[] = { " + boxed + " };");
                    line(fn + callee + "\", __nk_args, " + std::to_string(nargs) + dest);
                    close();
                }
                types_.set(name, {InferredType::concrete(ai.dtype, Shape::rowVector()),
                                  ConstVal::unknown()});
                return;
            }
        }
        // Elementwise array ARITHMETIC: dest = <expr over ONE whole array +
        // scalars> (self-contained). Sound only with a single distinct array
        // operand (no length-mismatch, no matrix semantics) and an
        // inference-proven array result. Emit a fill loop; a LOCAL is resized
        // to the operand's length, the OUTPUT uses its caller-sized length.
        if (isArrayVar(name) && !arrays_.at(name).is2D
            && arrays_.at(name).dtype == ValueType::DOUBLE) {
            std::set<std::string> srcArrays;
            if (collectElementwise(rhs, srcArrays) && !srcArrays.empty()) {
                const AbstractValue res = inferExpr(rhs, types_, reg_, classes_);
                if (res.type.isConcrete() && !res.type.shape.isScalar()
                    && res.type.dtype == ValueType::DOUBLE) {
                    const ArrayInfo  &ai    = arrays_.at(name);
                    // Loop length: the OUTPUT's caller-sized length, else (a
                    // local) the first operand's length.
                    const std::string bound =
                        ai.isLocal ? arrays_.at(*srcArrays.begin()).lenVar : ai.lenVar;
                    // SOUNDNESS: every array operand must have that length, or
                    // the per-element loop would read out of bounds. Guard at
                    // runtime (MATLAB errors on a size mismatch too).
                    std::string guard;
                    for (const std::string &an : srcArrays) {
                        const std::string &alen = arrays_.at(an).lenVar;
                        if (alen == bound) continue;  // trivially equal
                        guard += (guard.empty() ? "" : " || ") + (alen + " != " + bound);
                    }
                    if (!guard.empty())
                        line("if (" + guard
                             + ") throw std::out_of_range(\"numkit: array dimensions must match\");");
                    if (ai.isLocal) line(name + ".resize(" + bound + ");");
                    elementCtx_               = "__i";
                    const std::string rhsExpr = emitExpr(rhs);  // whole arrays -> [__i]
                    elementCtx_.clear();
                    open("for (std::size_t __i = 0; __i < " + bound + "; ++__i)");
                    line(ai.dataExpr + "[__i] = " + rhsExpr + ";");
                    close();
                    types_.set(name, {InferredType::concrete(ai.dtype, Shape::rowVector()),
                                      ConstVal::unknown()});
                    return;
                }
            }
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
        types_.set(name, inferExpr(rhs, types_, reg_, classes_));
        return;
    }

    if (lhs.type == NodeType::FIELD_ACCESS) {  // obj.field = rhs
        if (lhs.children.empty()) unsupported("field write arity");
        const AbstractValue base = inferExpr(*lhs.children[0], types_, reg_, classes_);
        if (!base.type.isObject() || !classes_)
            unsupported("field write on a non-object value");
        const ClassInfo *ci = classes_->byId(base.type.classId);
        if (!ci || !ci->field(lhs.strValue))
            unsupported("unknown field '" + lhs.strValue + "'");
        line(emitExpr(*lhs.children[0]) + (ci->isHandle ? "->" : ".") + lhs.strValue + " = "
             + emitExpr(rhs) + ";");
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

// `[a, b, ...] = f(args)` -> a void call to f's specialisation with the
// targets appended as reference out-args. v1: simple-identifier targets, a
// bare-name user-function RHS, and all of f's outputs requested.
void Emitter::emitMultiAssign(const ASTNode &s)
{
    if (!s.lhsTargets.empty()) unsupported("complex multi-assign targets (v1)");
    if (s.children.empty()) unsupported("multi-assign arity");
    const ASTNode &rhs = *s.children[0];
    if (rhs.type != NodeType::CALL || rhs.children.empty()
        || rhs.children[0]->type != NodeType::IDENTIFIER)
        unsupported("multi-assign RHS must be a user-function call (v1)");
    const std::string &name = rhs.children[0]->strValue;
    if (!ctx_ || !ctx_->funcs || types_.has(name) || !ctx_->funcs->has(name))
        unsupported("multi-assign of a non-user-function '" + name + "'");
    const ASTNode *def = ctx_->funcs->find(name);

    std::vector<InferredType> argTypes;
    std::string               argList;
    for (std::size_t i = 1; i < rhs.children.size(); ++i)
        appendCallArg(*rhs.children[i], argTypes, argList);
    if (argTypes.size() != def->paramNames.size())
        unsupported("arity mismatch calling '" + name + "'");
    if (s.returnNames.size() != def->returnNames.size())
        unsupported("multi-assign must request all of '" + name + "'s outputs (v1)");

    const std::vector<InferredType> outs = reg_.applyMulti(name, toArgInfos(argTypes));
    for (std::size_t i = 0; i < s.returnNames.size(); ++i) {
        const std::string &rn = s.returnNames[i];
        if (rn.empty() || rn == "~")
            unsupported("ignored (~) multi-output target not yet supported (v1)");
        if (!argList.empty()) argList += ", ";
        argList += rn;  // out-arg, bound to the callee's reference out-param
        types_.set(rn, {i < outs.size() ? outs[i] : InferredType::dynamic(), ConstVal::unknown()});
    }

    const std::string mangled = mangle(name, argTypes);
    if (ctx_->seen.insert(mangled).second)
        ctx_->pending.push_back({def, argTypes, mangled});
    line(mangled + "(" + argList + ");");
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
        inferStmt(body, bodyEnv, reg_, nullptr, classes_);
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
        const ConstVal sc = inferExpr(*range.children[1], types_, reg_, classes_).constant;
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
        inferStmt(body, bodyEnv, reg_, nullptr, classes_);
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
    case NodeType::ASSIGN:        emitAssign(s);      return;
    case NodeType::MULTI_ASSIGN:  emitMultiAssign(s); return;
    case NodeType::FOR_STMT:   emitFor(s);    return;
    case NodeType::WHILE_STMT: emitWhile(s);  return;
    case NodeType::IF_STMT:    emitIf(s);     return;
    case NodeType::EXPR_STMT:
        // A call evaluated for effect — a void method/function (e.g. a
        // handle class's in-place mutator), result discarded.
        if (!s.children.empty() && s.children[0]->type == NodeType::CALL) {
            line(emitExpr(*s.children[0]) + ";");
            return;
        }
        unsupported("expression statement (only a void call is supported)");
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
    "#include <initializer_list>\n"
    "#include <limits>\n"
    "#include <memory>\n"
    "#include <stdexcept>\n"
    "#include <vector>\n"
    "\n"
    "namespace nk_rt {\n"
    "// Reference wrapper for a handle class: shared identity + lifetime; the\n"
    "// wrapped object stays a plain struct (no inheritance from shared_ptr).\n"
    "template <class T> class handle {\n"
    "    std::shared_ptr<T> p_;\n"
    "public:\n"
    "    handle() = default;\n"
    "    explicit handle(std::shared_ptr<T> p) : p_(static_cast<std::shared_ptr<T>&&>(p)) {}\n"
    "    T* operator->() const { return p_.get(); }\n"
    "    T& operator*()  const { return *p_; }\n"
    "    bool operator==(const handle& o) const { return p_ == o.p_; }\n"
    "    bool operator!=(const handle& o) const { return p_ != o.p_; }\n"
    "    bool isvalid() const { return static_cast<bool>(p_); }\n"
    "    template <class U> handle(const handle<U>& o) : p_(o.shared()) {}\n"
    "    const std::shared_ptr<T>& shared() const { return p_; }\n"
    "    template <class... A> static handle make(A&&... a)\n"
    "        { return handle(std::make_shared<T>(static_cast<A&&>(a)...)); }\n"
    "};\n"
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
    "template <class T>\n"  // A(i,j) read, column-major; 2-D writes are a later step
    "inline T index2(const T* a, std::size_t rows, std::size_t cols, double i1, double j1) {\n"
    "    const std::size_t i = static_cast<std::size_t>(i1), j = static_cast<std::size_t>(j1);\n"
    "    if (i1 < 1.0 || i > rows || j1 < 1.0 || j > cols)\n"
    "        throw std::out_of_range(\"numkit: 2-D index out of bounds\");\n"
    "    return a[(j - 1) * rows + (i - 1)];\n"
    "}\n"
    "template <class T>\n"  // A(i,j) = v write, column-major (mutable 2-D: local/output)
    "inline void index2_set(T* a, std::size_t rows, std::size_t cols, double i1, double j1, T v) {\n"
    "    const std::size_t i = static_cast<std::size_t>(i1), j = static_cast<std::size_t>(j1);\n"
    "    if (i1 < 1.0 || i > rows || j1 < 1.0 || j > cols)\n"
    "        throw std::out_of_range(\"numkit: 2-D index out of bounds\");\n"
    "    a[(j - 1) * rows + (i - 1)] = v;\n"
    "}\n"
    "// N-D (rank>=3) column-major linear offset from 1-based subscripts + dims;\n"
    "// bounds-checked per axis. dims and subs are parallel (same rank).\n"
    "inline std::size_t nd_off(std::initializer_list<std::size_t> dims,\n"
    "                          std::initializer_list<double> subs) {\n"
    "    const std::size_t* d = dims.begin(); const double* s = subs.begin();\n"
    "    std::size_t off = 0, stride = 1;\n"
    "    for (std::size_t __a = 0; __a < dims.size(); ++__a) {\n"
    "        const std::size_t ik = static_cast<std::size_t>(s[__a]);\n"
    "        if (s[__a] < 1.0 || ik > d[__a]) throw std::out_of_range(\"numkit: N-D index out of bounds\");\n"
    "        off += (ik - 1) * stride; stride *= d[__a];\n"
    "    }\n"
    "    return off;\n"
    "}\n"
    "template <class T>\n"
    "inline T indexN(const T* a, std::initializer_list<std::size_t> dims,\n"
    "                std::initializer_list<double> subs) { return a[nd_off(dims, subs)]; }\n"
    "template <class T>\n"
    "inline void indexN_set(T* a, std::initializer_list<std::size_t> dims,\n"
    "                       std::initializer_list<double> subs, T v) { a[nd_off(dims, subs)] = v; }\n"
    "} // namespace nk_rt\n";

// The bridged-emission addendum (DESIGN.md §6a): the runtime C-ABI header +
// a tiny scalar-call helper. Appended ONLY when bridging is enabled, so a
// non-bridged TU stays self-contained / stdlib-only (the no-kludge litmus).
std::string bridgePrelude(const std::string &runtimeHeader)
{
    return "#include \"" + runtimeHeader + "\"\n"
           "#include <initializer_list>\n"
           "#include <vector>\n"
           "namespace nk_rt {\n"
           "// Box scalar args, call the runtime, unbox a scalar result; leak-free,\n"
           "// errors propagate as a C++ exception (never across the C ABI).\n"
           "inline double bridge_scalar(const char* name, std::initializer_list<double> args) {\n"
           "    std::vector<nk_val> argv; argv.reserve(args.size());\n"
           "    for (double x : args) argv.push_back(nk_box_scalar(x));\n"
           "    nk_error err; err.code = 0;\n"
           "    nk_val r = nk_call(name, argv.data(), argv.size(), 1, nullptr, &err);\n"
           "    for (nk_val h : argv) nk_release(h);\n"
           "    if (!r || err.code) { nk_release(r);\n"
           "        throw std::runtime_error(err.code ? err.message : \"numkit bridged call failed\"); }\n"
           "    const double v = nk_unbox_scalar(r); nk_release(r); return v;\n"
           "}\n"
           "// Box-args -> call (1 output) -> unbox the array result into the\n"
           "// caller-allocated out buffer; releases args + result; errors throw.\n"
           "inline void bridge_into(const char* name, nk_val* args, std::size_t nargs,\n"
           "                        double* out, std::size_t out_len) {\n"
           "    nk_error err; err.code = 0;\n"
           "    nk_val r = nk_call(name, args, nargs, 1, nullptr, &err);\n"
           "    for (std::size_t i = 0; i < nargs; ++i) nk_release(args[i]);\n"
           "    if (!r || err.code) { nk_release(r);\n"
           "        throw std::runtime_error(err.code ? err.message : \"numkit bridged call failed\"); }\n"
           "    nk_unbox_array(r, out, out_len); nk_release(r);\n"
           "}\n"
           "// Same, but the result fills an owned vector (resized to its numel) —\n"
           "// for an array LOCAL whose size is only known at the call.\n"
           "inline void bridge_to_vec(const char* name, nk_val* args, std::size_t nargs,\n"
           "                          std::vector<double>& out) {\n"
           "    nk_error err; err.code = 0;\n"
           "    nk_val r = nk_call(name, args, nargs, 1, nullptr, &err);\n"
           "    for (std::size_t i = 0; i < nargs; ++i) nk_release(args[i]);\n"
           "    if (!r || err.code) { nk_release(r);\n"
           "        throw std::runtime_error(err.code ? err.message : \"numkit bridged call failed\"); }\n"
           "    out.resize(nk_numel(r)); nk_unbox_array(r, out.data(), out.size()); nk_release(r);\n"
           "}\n"
           "} // namespace nk_rt\n";
}

// Emit ONE function (no prelude) under the RawBuffer ABI. `cppName`
// overrides the emitted symbol (for a mangled specialisation; empty -> the
// source name). `ctx` (when set) routes user-function calls and collects
// further specialisations. Returns {signature, definition}.
OneFn emitOneFunction(const ASTNode &funcDef, const std::vector<ParamSpec> &params,
                      const TransferRegistry &reg, ProgramEmitCtx *ctx,
                      const std::string &cppName, const ClassRegistry *classes,
                      const std::vector<ParamSpec> &extraSeed = {}, bool bridge = false)
{
    if (funcDef.type != NodeType::FUNCTION_DEF || funcDef.children.empty())
        unsupported("emitOneFunction expects a FUNCTION_DEF with a body");
    const std::size_t nout = funcDef.returnNames.size();

    const ASTNode &body = *funcDef.children[0];

    // The typing seed (entry) and the signature's parameters are distinct:
    // params drive the signature + the hoist-skip; `entry` is the typing
    // env (params PLUS extraSeed — pre-typed non-parameter locals such as a
    // constructor's output object, which are hoisted as locals, not params).
    TypeEnv                                     entry;
    std::unordered_map<std::string, ArrayInfo>  arrays;
    std::vector<std::string>                    sigParams;
    std::set<std::string>                       paramSet;
    for (const ParamSpec &p : params) {
        entry.set(p.name, {p.type, ConstVal::unknown()});
        paramSet.insert(p.name);
        if (isUnboxableScalarType(p.type)) {
            sigParams.push_back(cppScalarType(p.type.dtype) + " " + p.name);
        } else if (is2DMatrixType(p.type)) {
            // 2-D matrix -> pointer + dim companions (column-major).
            ArrayInfo ai;
            ai.dtype    = p.type.dtype;
            ai.is2D     = true;
            ai.rowsVar  = p.name + "_rows";
            ai.colsVar  = p.name + "_cols";
            ai.dataExpr = p.name;
            arrays[p.name] = ai;
            sigParams.push_back("const " + cppScalarType(p.type.dtype) + "* " + p.name
                                + ", std::size_t " + ai.rowsVar + ", std::size_t " + ai.colsVar);
        } else if (p.type.isConcrete() && p.type.shape.isNDims()) {
            // N-D (rank>=3) param -> pointer + one size_t companion per dim
            // (column-major). Checked BEFORE isBufferArrayType, which also
            // matches an N-D shape. Companions carry the dims, so a runtime-dim
            // N-D param works too (the caller passes the sizes). Read-only.
            ArrayInfo ai;
            ai.dtype    = p.type.dtype;
            ai.isND     = true;
            ai.dataExpr = p.name;
            std::string sig = "const " + cppScalarType(p.type.dtype) + "* " + p.name;
            for (std::size_t d = 0; d < p.type.shape.nd.size(); ++d) {
                const std::string dv = p.name + "_d" + std::to_string(d);
                ai.ndDims.push_back(dv);
                sig += ", std::size_t " + dv;
            }
            arrays[p.name] = ai;
            sigParams.push_back(sig);
        } else if (isBufferArrayType(p.type)) {
            ArrayInfo ai;
            ai.dtype       = p.type.dtype;
            ai.lenVar      = p.name + "_len";
            ai.dataExpr    = p.name;
            arrays[p.name] = ai;
            sigParams.push_back("const " + cppScalarType(p.type.dtype) + "* " + p.name
                                + ", std::size_t " + ai.lenVar);
        } else if (p.type.isObject()) {
            // value class -> by value (value semantics); handle -> wrapper.
            sigParams.push_back(cppObjectType(p.type.classId, classes) + " " + p.name);
        } else {
            unsupported("parameter '" + p.name + "' has an unsupported type for RawBuffer ABI");
        }
    }
    for (const ParamSpec &es : extraSeed)  // pre-typed locals (e.g. ctor output)
        entry.set(es.name, {es.type, ConstVal::unknown()});

    const DeclTypeMap decls = computeDeclTypes(body, entry, reg, classes);

    // Return classification:
    //   0 outputs -> void (e.g. a handle class's in-place mutator);
    //   1 output  -> scalar (by value) / array (out-param) / object (by value);
    //   N outputs -> void + a reference out-param per (scalar) output.
    std::string retCpp      = "void";
    std::string retName;
    bool        arrayReturn = false;
    if (nout == 1) {
        retName             = funcDef.returnNames[0];
        const auto retIt    = decls.find(retName);
        if (retIt == decls.end())
            unsupported("output '" + retName + "' is never assigned");
        const InferredType retType = retIt->second;
        if (isUnboxableScalarType(retType)) {
            retCpp = cppScalarType(retType.dtype);
        } else if (isBufferArrayType(retType)) {
            arrayReturn = true;
            retCpp      = "void";
            ArrayInfo ai;
            ai.dtype        = retType.dtype;
            ai.lenVar       = retName + "_len";
            ai.isOutput     = true;
            ai.dataExpr     = retName;
            arrays[retName] = ai;
            sigParams.push_back(cppScalarType(retType.dtype) + "* " + retName
                                + ", std::size_t " + ai.lenVar);
        } else if (retType.isObject()) {
            // returned BY VALUE (value class) / handle wrapper — not an
            // out-param; the scalar-return path (return retName;) applies.
            retCpp = cppObjectType(retType.classId, classes);
        } else {
            unsupported("output '" + retName + "' has an unsupported type");
        }
    } else if (nout >= 2) {
        // Each output is a reference out-param the body writes directly.
        // v1: scalar outputs only (array/object multi-outputs deferred).
        for (const std::string &rn : funcDef.returnNames) {
            if (rn.empty()) unsupported("multi-output with an unnamed output");
            const auto         it = decls.find(rn);
            const InferredType t  = (it != decls.end()) ? it->second : InferredType::dynamic();
            if (!isUnboxableScalarType(t))
                unsupported("multi-output '" + rn + "' must be an unboxed scalar (v1)");
            sigParams.push_back(cppScalarType(t.dtype) + "& " + rn);
            paramSet.insert(rn);  // a reference param, not a hoisted local
        }
    }

    // Array LOCALS: a 1-D buffer-array-typed variable that is neither a
    // parameter nor the output is an owned, runtime-sized std::vector. Register
    // it so index / numel / call-arg resolve to `<name>.data()` / `.size()`.
    // (A 2-D array local is not yet supported — it falls to the hoist below.)
    for (const auto &[name, t] : decls) {
        if (paramSet.count(name) || arrays.count(name)) continue;  // params + the output
        // 2-D first: a matrix also satisfies isBufferArrayType (as in the
        // parameter loop), so it must be classified before the 1-D case.
        if (is2DMatrixType(t)) {
            // A 2-D matrix local — flat owned vector + compile-time KnownDims.
            // (Runtime-dim 2-D locals need size()/2-D-zeros-with-vars — later.)
            ArrayInfo ai;
            ai.dtype     = t.dtype;
            ai.isLocal   = true;
            ai.is2D      = true;
            ai.dataExpr  = name + ".data()";
            ai.lenVar    = name + ".size()";
            ai.rowsVar   = std::to_string(t.shape.rows);
            ai.colsVar   = std::to_string(t.shape.cols);
            arrays[name] = ai;
        } else if (t.isConcrete() && t.shape.isNDims()) {
            // A rank-N (N>=3) local — checked BEFORE isBufferArrayType (which
            // also matches an N-D shape). Flat owned vector + compile-time
            // dims. (Only fully-known dims for now; a runtime dim needs vars.)
            bool allKnown = true;
            for (std::size_t d : t.shape.nd)
                if (d == 0) { allKnown = false; break; }
            if (allKnown) {
                ArrayInfo ai;
                ai.dtype    = t.dtype;
                ai.isLocal  = true;
                ai.isND     = true;
                ai.dataExpr = name + ".data()";
                ai.lenVar   = name + ".size()";
                for (std::size_t d : t.shape.nd) ai.ndDims.push_back(std::to_string(d));
                arrays[name] = ai;
            }
        } else if (isBufferArrayType(t)) {
            ArrayInfo ai;
            ai.dtype     = t.dtype;
            ai.isLocal   = true;
            ai.dataExpr  = name + ".data()";
            ai.lenVar    = name + ".size()";
            arrays[name] = ai;
        }
    }

    const std::string symbol = cppName.empty() ? funcDef.strValue : cppName;
    std::string       sig    = retCpp + " " + symbol + "(";
    for (std::size_t i = 0; i < sigParams.size(); ++i)
        sig += (i ? ", " : "") + sigParams[i];
    sig += ")";

    // Optimisation facts (brick 6). Empty facts => the always-correct
    // checked form; the analysis only enables faster lowering.
    const OptFacts opt = analyzeOptimizations(body, arrays);
    std::set<std::string> promotedVars;  // their counter is declared in the for, not hoisted
    for (const ASTNode *f : opt.promotedLoops) promotedVars.insert(f->strValue);

    // Emit hoisted local declarations (deterministic order) + the body.
    Emitter em(entry, reg, arrays, opt, ctx, classes, bridge);
    em.hoistArrayLocals();  // owned-vector array locals first
    std::map<std::string, InferredType> ordered(decls.begin(), decls.end());
    for (const auto &[name, t] : ordered) {
        if (paramSet.count(name) || arrays.count(name) || promotedVars.count(name))
            continue;  // signature params / arrays / promoted loop counters
        if (!isUnboxableScalarType(t) && !t.isObject())
            unsupported("local '" + name + "' is not an unboxable scalar or object (type "
                        + t.str() + ") — unsupported in RawBuffer ABI");
        em.hoistLocal(name, t);
    }
    em.emitStmt(body);
    if (nout == 1 && !arrayReturn) em.emitReturnScalar(retName);

    std::string definition = sig + " {\n";
    definition += em.out();
    definition += "}\n";
    return {sig, definition};
}

} // namespace

// Emit the C++ struct for every class in `classes` (empty when null).
static std::string emitAllStructs(const ClassRegistry *classes)
{
    std::string s;
    if (classes)
        for (std::size_t i = 0; i < classes->size(); ++i)
            s += emitClassStruct(*classes->byId(static_cast<int>(i)));
    return s;
}

// ── whole-function emission (3f) ──────────────────────────────────────
EmittedFunction emitFunction(const ASTNode &funcDef,
                             const std::vector<ParamSpec> &params,
                             const TransferRegistry &reg, const ClassRegistry *classes,
                             const BridgeOptions &bridge)
{
    const OneFn f = emitOneFunction(funcDef, params, reg, /*ctx=*/nullptr, /*cppName=*/"", classes,
                                    /*extraSeed=*/{}, bridge.enabled);
    std::string source = kPrelude;
    source += "\n";
    if (bridge.enabled) source += bridgePrelude(bridge.runtimeHeader);
    source += emitAllStructs(classes);
    source += f.definition;
    return {source, funcDef.strValue, f.signature};
}

// ── whole-program emission (§12 brick 1b) ─────────────────────────────
EmittedFunction emitProgram(const ASTNode &entryDef,
                            const std::vector<ParamSpec> &params,
                            const FunctionTable &table, const TransferRegistry &reg,
                            const ClassRegistry *classes, const BridgeOptions &bridge)
{
    ProgramEmitCtx ctx;
    ctx.funcs = &table;

    std::vector<InferredType> entryArgTypes;
    entryArgTypes.reserve(params.size());
    for (const auto &p : params) entryArgTypes.push_back(p.type);
    const std::string entryMangled = mangle(entryDef.strValue, entryArgTypes);
    ctx.seen.insert(entryMangled);

    std::vector<std::string> sigs, defs;
    const OneFn ef =
        emitOneFunction(entryDef, params, reg, &ctx, entryMangled, classes, {}, bridge.enabled);
    const std::string entrySig = ef.signature;
    sigs.push_back(ef.signature);
    defs.push_back(ef.definition);

    // Drain the worklist: each specialisation may discover more calls.
    while (!ctx.pending.empty()) {
        const CallSite cs = ctx.pending.back();
        ctx.pending.pop_back();
        if (cs.argTypes.size() != cs.def->paramNames.size())
            unsupported("arity mismatch emitting '" + cs.def->strValue + "'");
        std::vector<ParamSpec> ps;
        ps.reserve(cs.argTypes.size());
        for (std::size_t i = 0; i < cs.argTypes.size(); ++i)
            ps.push_back({cs.def->paramNames[i], cs.argTypes[i]});
        const OneFn cf =
            emitOneFunction(*cs.def, ps, reg, &ctx, cs.mangled, classes, cs.extraSeed, bridge.enabled);
        sigs.push_back(cf.signature);
        defs.push_back(cf.definition);
    }

    std::string source = kPrelude;
    source += "\n";
    if (bridge.enabled) source += bridgePrelude(bridge.runtimeHeader);
    source += emitAllStructs(classes);              // class structs precede all functions
    source += "\n";
    for (const auto &s : sigs) source += s + ";\n";  // forward declarations
    source += "\n";
    for (const auto &d : defs) source += d;
    return {source, entryMangled, entrySig};
}

// ── codegen-as-plugin (tiered acceleration, §6b) ──────────────────────
std::string emitScalarPlugin(const ASTNode &funcDef,
                             const std::vector<ParamSpec> &params,
                             const TransferRegistry &reg, const std::string &exportName,
                             const std::string &abiHeaderPath, const ClassRegistry *classes)
{
    // Preconditions (the sound boundary): a SCALAR single output (the
    // output-size protocol for an array result is a later layer), and each
    // parameter is a scalar OR a double row vector. inferFunctionReturn yields
    // Dynamic for a multi-output / untypeable function, so the
    // isUnboxableScalar() return check covers those too.
    std::vector<ArgInfo> args;
    args.reserve(params.size());
    for (const auto &p : params) {
        const bool scalar   = p.type.isUnboxableScalar();
        const bool dblVector = isBufferArrayType(p.type) && p.type.dtype == ValueType::DOUBLE;
        if (!scalar && !dblVector)
            unsupported("plugin export: parameter '" + p.name
                        + "' must be a scalar or a double vector (v1)");
        args.push_back(ArgInfo::of(p.type));
    }
    const InferredType ret = inferFunctionReturn(funcDef, args, reg, classes);
    if (!ret.isUnboxableScalar())
        unsupported("plugin export: '" + funcDef.strValue
                    + "' must have a single SCALAR output (v1; array output needs the "
                      "size protocol)");

    // The compiled function (throws if its body is outside the subset).
    const EmittedFunction ef = emitFunction(funcDef, params, reg, classes);
    const std::size_t     n  = params.size();

    // Per-parameter marshalling: a scalar unboxes inline; a double vector
    // unboxes into a buffer (sized by the runtime value) passed as ptr+len.
    std::string preDecls, callArgs;
    for (std::size_t i = 0; i < n; ++i) {
        const std::string ix = std::to_string(i);
        if (i) callArgs += ", ";
        if (params[i].type.isUnboxableScalar()) {
            callArgs += "static_cast<" + cppScalarType(params[i].type.dtype)
                        + ">(nk__host->unbox_scalar(a[" + ix + "]))";
        } else {  // double vector
            const std::string b = "__b" + ix;
            preDecls += "    std::vector<double> " + b + "(nk__host->numel(a[" + ix + "]));\n";
            preDecls += "    nk__host->unbox_array(a[" + ix + "], " + b + ".data(), " + b
                        + ".size());\n";
            callArgs += b + ".data(), " + b + ".size()";
        }
    }

    std::string s = ef.source;
    s += "\n#include \"" + abiHeaderPath + "\"\n";
    s += "#include <cstdio>\n";
    s += "#include <vector>\n";
    s += "namespace {\n";
    s += "const nk_host_api *nk__host = nullptr;\n";
    s += "nk_val nk__wrap(const nk_val *a, size_t nargs, size_t nargout,\n";
    s += "                nk_val *extra_outs, nk_error *err) {\n";
    s += "    (void)nargout; (void)extra_outs;\n";
    s += "    if (nargs < " + std::to_string(n) + ") {\n";
    s += "        if (err) { err->code = 1; std::snprintf(err->message, sizeof(err->message),\n";
    s += "            \"" + exportName + ": expected " + std::to_string(n) + " argument(s)\"); }\n";
    s += "        return nullptr;\n";
    s += "    }\n";
    s += preDecls;
    s += "    return nk__host->box_scalar(static_cast<double>(" + ef.name + "(" + callArgs + ")));\n";
    s += "}\n";
    s += "}  // namespace\n";
    s += "extern \"C\" {\n";
    s += "NK_PLUGIN_EXPORT int nk_plugin_abi_version(void) { return NK_PLUGIN_ABI_VERSION; }\n";
    s += "NK_PLUGIN_EXPORT int nk_plugin_register(const nk_host_api *host) {\n";
    s += "    nk__host = host;\n";
    s += "    return host->register_fn(\"" + exportName + "\", &nk__wrap);\n";
    s += "}\n";
    s += "}  // extern \"C\"\n";
    return s;
}

// ── class struct emission (§12 brick 5) ───────────────────────────────
std::string emitClassStruct(const ClassInfo &ci)
{
    std::string s = "struct " + ci.name + " {\n";
    for (const auto &f : ci.fields) {
        if (!f.type.isUnboxableScalar())
            unsupported("class '" + ci.name + "' field '" + f.name
                        + "' is not an unboxed scalar (v1)");
        const std::string init =
            f.defaultExpr ? emitScalarExpr(*f.defaultExpr) : zeroLiteral(f.type.dtype);
        s += "    " + cppScalarType(f.type.dtype) + " " + f.name + " = " + init + ";\n";
    }
    s += "};\n";
    return s;
}

} // namespace numkit::codegen
