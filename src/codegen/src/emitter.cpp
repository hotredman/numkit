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
    case ValueType::CHAR:    return "std::uint16_t";  // MATLAB char is a UTF-16 code unit
    default:
        throw std::runtime_error("cppScalarType: no scalar C++ mapping for this dtype");
    }
}

// The C++ ELEMENT type for an owned array buffer. Identical to cppScalarType
// except LOGICAL uses std::uint8_t, not bool: std::vector<bool> is bit-packed and
// exposes no .data(), which the buffer ABI (name.data()) relies on. A bool value
// assigns to / reads from a uint8 element as 0/1 (MATLAB logical 1-byte storage).
std::string cppArrayElemType(ValueType dtype)
{
    return dtype == ValueType::LOGICAL ? "std::uint8_t" : cppScalarType(dtype);
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
// 1-D vector orientation. The RawBuffer ABI erases it (a vector is just
// ptr+len), but the compile-time type knows it — used to fold size(vec, dim).
enum class VecOrient { Unknown, Row, Col };

struct ArrayInfo {
    ValueType   dtype;
    std::string lenVar;            // 1-D: the length EXPRESSION — `<name>_len`
                                   // for a param/output, `<name>.size()` for a local
    bool        isOutput = false;  // the function's caller-allocated out-param
    bool        is2D     = false;  // a 2-D matrix (column-major storage)
    VecOrient   orient   = VecOrient::Unknown;  // 1-D only (!is2D && !isND)
    std::string rowsVar, colsVar;  // 2-D: dim companions (`<name>_rows/_cols`)
    bool        isLocal  = false;  // an owned `std::vector` local (not a buffer ptr)
    std::string dataExpr;          // the element-pointer EXPRESSION — `<name>` for a
                                   // param/output buffer, `<name>.data()` for a local
    bool        isND     = false;  // a rank-N (N>=3) array (column-major flat storage)
    std::vector<std::string> ndDims;  // when isND: per-dim size EXPRESSIONS (rank = size())
    bool        ndRuntimeLocal = false;  // an N-D LOCAL whose dims are runtime: ndDims are
                                         // `<name>_dN` size_t vars hoisted at fn entry and
                                         // set from the zeros/ones args at the assignment
                                         // (vs a const-dim local, whose ndDims are literals)
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

// Row/Col orientation of a 1-D buffer from its compile-time shape (Unknown when
// the shape doesn't pin it down — then size(vec,dim) is not folded).
VecOrient orientOf(const InferredType &t)
{
    switch (t.shape.kind) {
    case ShapeKind::RowVector: return VecOrient::Row;
    case ShapeKind::ColVector: return VecOrient::Col;
    case ShapeKind::KnownDims:
        if (t.shape.rows == 1) return VecOrient::Row;
        if (t.shape.cols == 1) return VecOrient::Col;
        return VecOrient::Unknown;
    default: return VecOrient::Unknown;
    }
}

// A concrete numeric/logical buffer of non-scalar shape (raw-buffer array).
bool isBufferArrayType(const InferredType &t)
{
    if (!t.isConcrete() || t.shape.isScalar()) return false;
    return isBufferArray(AbstractValue{t, ConstVal::unknown()});
}

// A numeric (double/complex) buffer returnable BY VALUE across an interproc call
// as a flat std::vector (column-major): 1-D vectors, 2-D KnownDims matrices, and
// fully-known N-D arrays. The data is self-describing (.size()) and the dims are
// compile-time-known on BOTH sides (the caller monomorphises the callee to the
// same return type), so no runtime dims need to travel with the buffer. A
// runtime-dim N-D result (an `nd` entry of 0) is excluded — its dims are not
// compile-time-known, so the caller could not hoist a matching local.
bool isByValueReturnArrayType(const InferredType &t)
{
    if (!isBufferArrayType(t)) return false;
    if (t.dtype != ValueType::DOUBLE && t.dtype != ValueType::COMPLEX
        && t.dtype != ValueType::CHAR)  // CHAR -> uint16 buffer (string-building helpers)
        return false;
    if (t.shape.isNDims())
        for (std::size_t d : t.shape.nd)
            if (d == 0) return false;
    return true;
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
        c += "n";  // ranked: rank + each dim (0 = unknown); 'x'-joined, NO '_'
        for (std::size_t d : t.shape.nd) c += std::to_string(d) + "x";  // '_'-free: mangle stays __-free
        break;
    case ShapeKind::Unknown:   c += "u"; break;
    }
    return c;
}

// Escape a base name so the mangle is injective AND free of "__" (which the
// C++ standard reserves to the implementation, [lex.name]): each '_' in the
// base becomes "_0", and argument segments are introduced by the "_1"
// separator — which escaping guarantees the base can never itself produce.
// typeCode is '_'-free, so every '_' in a mangled symbol is immediately
// followed by a digit (0/1): no "__" can ever form. The symbol also starts
// with the base's (letter) first char, so it is a conforming global identifier
// (no leading underscore). Distinct (base, args) -> distinct symbol.
std::string escapeBase(const std::string &base)
{
    std::string e;
    for (char ch : base) {
        e += ch;
        if (ch == '_') e += '0';
    }
    return e;
}

std::string mangle(const std::string &base, const std::vector<InferredType> &args)
{
    std::string m = escapeBase(base);
    if (args.empty()) return m + "_1v";
    for (const auto &a : args) m += "_1" + typeCode(a);
    return m;
}

// A reserved-companion variable name (`_nk_<escaped-base><suffix>`) for an
// array's length / dim sizes (suffix = "_len" / "_rows" / "_cols" / "_d0"...).
// The `_nk_` prefix lives in the underscore namespace a MATLAB identifier can
// never enter, so it cannot collide with a user variable — yet it is a
// CONFORMING block-scope name: a leading underscore is reserved only for the
// GLOBAL namespace ([lex.name]), and companions are always params/locals. The
// base is escaped so a name like `x_` cannot form a reserved "__".
std::string companion(const std::string &base, const std::string &suffix)
{
    return "_nk_" + escapeBase(base) + suffix;
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
            bool bridge = false, bool opsKernels = false)
        : types_(std::move(types)), reg_(reg), arrays_(std::move(arrays)),
          opt_(std::move(opt)), ctx_(ctx), classes_(classes), bridge_(bridge),
          opsKernels_(opsKernels)
    {}

    // Tell the emitter how an early `return` statement should lower (it matches
    // the end-of-function return chosen in emitOneFunction).
    void setReturnInfo(bool returnsValue, std::string name, bool dynamic)
    {
        returnsValue_  = returnsValue;
        returnName_    = std::move(name);
        returnDynamic_ = dynamic;
    }

    // Record a local that was hoisted as Dynamic (nk_val); see dynamicLocals_.
    void addDynamicLocal(const std::string &n) { dynamicLocals_.insert(n); }

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
        // Dynamic tier (DESIGN.md §10 C1): an un-typeable local is a boxed
        // runtime value (null until assigned). Reachable only under bridge_
        // (the caller's refusal check gates it); cppScalarType(Dynamic) throws.
        if (t.isDynamic()) {
            line("nk_rt::val " + name + ";");
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
            line("std::vector<" + cppArrayElemType(dtype) + "> " + name + ";");
        // Runtime-dim N-D locals also need their size_t dim companions declared
        // (set at the zeros/ones assignment; read by indexN / size / numel).
        for (const auto &[name, dtype] : ordered) {
            const ArrayInfo &ai = arrays_.at(name);
            if (!ai.ndRuntimeLocal) continue;
            std::string decl = "std::size_t ";
            for (std::size_t d = 0; d < ai.ndDims.size(); ++d)
                decl += (d ? ", " : "") + ai.ndDims[d] + " = 0";
            line(decl + ";");
        }
    }

    void emitStmt(const ASTNode &s);
    void emitReturnScalar(const std::string &name) { line("return " + name + ";"); }
    // Dynamic tier (DESIGN.md §10 C1): a boxed (nk_val) return transfers handle
    // ownership OUT to the caller (val::take nulls the local so its dtor is a no-op).
    void emitReturnDynamic(const std::string &name) { line("return " + name + ".take();"); }

    const std::string &out() const { return out_; }

private:
    void line(const std::string &s) { out_ += std::string(indent_ * 4, ' ') + s + "\n"; }
    void open(const std::string &head) { line(head + " {"); ++indent_; }
    void close() { --indent_; line("}"); }

    std::string emitExpr(const ASTNode &e);
    // Dynamic tier (DESIGN.md §10 C1): emit `e` as an `nk_rt::val` C++
    // expression (a boxed runtime value), for an operand/result the inference
    // could not type. Operators dispatch to the runtime (binop/unop); an
    // un-typeable builtin result stays boxed (call_dyn). Only reachable under
    // bridge_ (it needs the C-ABI). The sound fallback to the typed emitExpr.
    std::string emitDynamicExpr(const ASTNode &e);
    // A control-flow condition (if / while): a typed condition is the bare C++
    // boolean; a Dynamic condition is evaluated in the Value tier and reduced to
    // MATLAB truthiness (non-empty AND every element non-zero). DESIGN.md §10 C1.
    std::string emitCondition(const ASTNode &c);
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
    // The size_t expression for axis k (0-based) of array `a`: N-D companion /
    // 2-D rows,cols / 1-D orientation (row = 1 x len, col = len x 1). Axes
    // beyond the rank, or an orientation-unknown vector, yield "1" -- callers
    // that must distinguish unknown orientation guard on a.orient first.
    std::string dimExpr(const ArrayInfo &a, std::size_t k) const;
    void        emitAssign(const ASTNode &s);
    void        emitMultiAssign(const ASTNode &s);
    void        emitIndexWrite(const ASTNode &lhsCall, const ASTNode &rhs);
    void        emitFor(const ASTNode &s);
    void        emitWhile(const ASTNode &s);
    void        emitIf(const ASTNode &s);
    void        emitSwitch(const ASTNode &s);
    void        emitTry(const ASTNode &s);

    bool isArrayVar(const std::string &n) const { return arrays_.count(n) != 0; }

    // The AbstractValue for an array variable (non-scalar -> buffer).
    AbstractValue arrayValue(const std::string &n) const
    {
        return {InferredType::concrete(arrays_.at(n).dtype, Shape::rowVector()),
                ConstVal::unknown()};
    }

    std::string                                 out_;
    int                                         indent_        = 1;
    int                                         switchCounter_ = 0;  // unique switch-selector temp ids
    TypeEnv                                     types_;
    const TransferRegistry                     &reg_;
    std::unordered_map<std::string, ArrayInfo>  arrays_;
    OptFacts                                    opt_;
    // Non-null inside a promoted clean-index loop: the 0-based size_t
    // counter that replaced the 1-based double loop variable.
    const std::string                          *promotedCounter_ = nullptr;
    std::vector<std::string>                    endStack_;  // `end` extents (active index ctx)
    // Non-empty inside an elementwise-array fill loop: the 0-based size_t
    // index, so a bare whole-array `x` emits the element `x[<idx>]`.
    std::string                                 elementCtx_;
    // The function's RETURN ABI (set by emitOneFunction before the body is
    // emitted), so an early `return` lowers to the SAME form as the
    // end-of-function return: a value output (`return name;`, or `.take()` if
    // boxed), or a bare `return;` for a void / out-param / multi-ref function.
    bool                                        returnsValue_  = false;
    std::string                                 returnName_;
    bool                                        returnDynamic_ = false;
    // Locals hoisted as Dynamic (nk_val) — arising from markAssignedDynamic
    // constructs (e.g. try/catch bodies). Every assignment to one must box (there
    // is no nk_val = <typed>), and it stays Dynamic so later reads route through
    // the runtime.
    std::set<std::string>                       dynamicLocals_;
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
    // Ops-kernel lowering: when true, a heavy array op with a matching ops
    // kernel (matmul, …) emits a numkit::ops:: call instead of an inline loop.
    bool                                        opsKernels_ = false;
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

// MATLAB unary-math name -> the numkit::ops:: SIMD transcendental facade entry
// (kernels.hpp), for the opt-in ops-kernel tier. Restricted to the real-total
// transcendentals fusedTransAffine handles (no complex-domain decline) that the
// codegen also lowers inline — so routing here is a faithful, faster swap
// (inline std::<fn> does not auto-vectorise on MSVC). floor/ceil/round/abs/fix/
// erf/erfc are NOT here (cheap or no SIMD kernel) -> they stay inline.
const char *opsTranscendentalFn(const std::string &name)
{
    static const std::unordered_map<std::string, const char *> kMap = {
        {"sin", "sinDouble"},     {"cos", "cosDouble"},   {"tan", "tanDouble"},
        {"atan", "atanDouble"},   {"sinh", "sinhDouble"}, {"cosh", "coshDouble"},
        {"tanh", "tanhDouble"},   {"exp", "expDouble"},   {"asinh", "asinhDouble"},
        {"expm1", "expm1Double"}};
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
    case NodeType::STRING_LITERAL:
        // A 1x1 char literal ('a') is a uint16 code-unit scalar. A multi-char
        // literal is a char ARRAY -> handled at the statement level (emitAssign);
        // it cannot be a C++ scalar expression, so refuse it here.
        if (e.strValue.size() != 1)
            unsupported("multi-char string literal in expression position (char array)");
        return "std::uint16_t("
               + std::to_string(static_cast<unsigned>(static_cast<unsigned char>(e.strValue[0])))
               + ")";
    case NodeType::IDENTIFIER:
        if (isArrayVar(e.strValue)) {
            // Inside an elementwise-array fill loop a whole array means its
            // current element; in any other (scalar) context it is an error.
            if (!elementCtx_.empty())
                return arrays_.at(e.strValue).dataExpr + "[" + elementCtx_ + "]";
            unsupported("bare array identifier '" + e.strValue + "' in scalar context");
        }
        return e.strValue;
    case NodeType::END_VAL:
        // `end` inside an index -> the current indexed array's extent (pushed by
        // emitIndexRead as a 1-D length). MATLAB `end` is a scalar; emit it as a
        // double so it composes in index arithmetic (end-1, end/2, ...). Outside an
        // index context the stack is empty -> refuse (sound; never a wrong value).
        if (endStack_.empty())
            unsupported("'end' outside an indexing context");
        return "static_cast<double>(" + endStack_.back() + ")";
    case NodeType::BINARY_OP:
        if (e.children.size() != 2) unsupported("binary op arity");
        return emitBinOpJoin(e.strValue, emitExpr(*e.children[0]),
                             emitExpr(*e.children[1]));
    case NodeType::UNARY_OP:
        if (e.children.size() != 1) unsupported("unary op arity");
        if (e.strValue == "'" || e.strValue == ".'") {
            // Transpose in expression position is only the SCALAR case here
            // (identity, except ctranspose ' conjugates a complex scalar). A
            // whole-vector/matrix transpose is a statement-level producer
            // (emitAssign); transposing a non-scalar sub-expression is refused.
            const AbstractValue a = inferExpr(*e.children[0], types_, reg_, classes_);
            if (!a.type.shape.isScalar())
                unsupported("transpose of a non-scalar sub-expression");
            const std::string inner = emitExpr(*e.children[0]);
            return (e.strValue == "'" && a.type.dtype == ValueType::COMPLEX)
                       ? "std::conj(" + inner + ")"
                       : inner;
        }
        return emitUnOpJoin(e.strValue, emitExpr(*e.children[0]));
    case NodeType::CALL: {
        if (e.children.empty()) unsupported("empty call");
        const ASTNode &callee = *e.children[0];
        if (callee.type == NodeType::FIELD_ACCESS) {
            // s.v(k): index a struct array FIELD (field-flattening) when the base is
            // a non-object plain var -> index the field-local array. (An object
            // method obj.m(args) routes to emitMethodCall below.)
            if (!callee.children.empty() && callee.children[0]->type == NodeType::IDENTIFIER
                && !inferExpr(*callee.children[0], types_, reg_, classes_).type.isObject()) {
                const std::string fld =
                    "_nk_fld_" + callee.children[0]->strValue + "_" + callee.strValue;
                if (isArrayVar(fld)) return emitIndexRead(fld, e);
            }
            if (classes_) return emitMethodCall(e);  // obj.method(args)
        }
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
        // Plain struct: the synthesized scalar field-local (field-flattening).
        if (!base.type.isObject() && e.children[0]->type == NodeType::IDENTIFIER)
            return "_nk_fld_" + e.children[0]->strValue + "_" + e.strValue;
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

// Dynamic tier (DESIGN.md §10 C1): emit `e` as an nk_rt::val expression. An
// operand the inference DID type is boxed at the boundary (val::scalar); a
// Dynamic operand is already a val (binds by const-ref into binop/unop); an
// un-typeable builtin result stays boxed (call_dyn). v1 covers real-double
// scalars + Dynamic values — a complex/array operand, a Dynamic call argument,
// or short-circuit && / || is an explicit boundary (A3/A4/A5), never wrong code.
std::string Emitter::emitDynamicExpr(const ASTNode &e)
{
    switch (e.type) {
    case NodeType::NUMBER_LITERAL:
        return "nk_rt::val::scalar(" + formatDoubleLiteral(e.numValue) + ")";
    case NodeType::IDENTIFIER: {
        if (isArrayVar(e.strValue)) {
            // Box a typed array at the boundary (shape-preserving, column-major).
            const ArrayInfo &ai = arrays_.at(e.strValue);
            if (ai.dtype == ValueType::DOUBLE) {
                if (ai.is2D)
                    return "nk_rt::val::matrix(" + ai.dataExpr + ", " + ai.rowsVar + ", "
                           + ai.colsVar + ")";
                if (ai.isND) {
                    std::string dims;
                    for (std::size_t k = 0; k < ai.ndDims.size(); ++k)
                        dims += (k ? ", " : "") + ai.ndDims[k];
                    return "nk_rt::val::array_nd(" + ai.dataExpr + ", {" + dims + "})";
                }
                return "nk_rt::val::array(" + ai.dataExpr + ", " + ai.lenVar + ")";  // 1-D
            }
            if (ai.dtype == ValueType::COMPLEX) {
                if (ai.is2D)
                    return "nk_rt::val::complex_matrix(" + ai.dataExpr + ", " + ai.rowsVar + ", "
                           + ai.colsVar + ")";
                if (ai.isND) {
                    std::string dims;
                    for (std::size_t k = 0; k < ai.ndDims.size(); ++k)
                        dims += (k ? ", " : "") + ai.ndDims[k];
                    return "nk_rt::val::complex_array_nd(" + ai.dataExpr + ", {" + dims + "})";
                }
                return "nk_rt::val::complex_array(" + ai.dataExpr + ", " + ai.lenVar + ")";  // 1-D complex
            }
            unsupported("Dynamic tier: array operand '" + e.strValue + "' unsupported dtype (v1)");
        }
        const AbstractValue av = inferExpr(e, types_, reg_, classes_);
        if (av.type.isDynamic())
            return e.strValue;  // an existing Dynamic local (already an nk_rt::val)
        if (av.type.dtype == ValueType::DOUBLE && av.type.shape.isScalar())
            return "nk_rt::val::scalar(" + e.strValue + ")";  // box a typed double scalar
        unsupported("Dynamic tier: non-double-scalar operand '" + e.strValue + "' (v1)");
    }
    case NodeType::BINARY_OP:
        if (e.children.size() != 2) unsupported("Dynamic tier: binary op arity");
        if (e.strValue == "&&" || e.strValue == "||")
            unsupported("Dynamic tier: short-circuit && / || operand (v1, A5)");
        return "nk_rt::binop(\"" + e.strValue + "\", " + emitDynamicExpr(*e.children[0]) + ", "
               + emitDynamicExpr(*e.children[1]) + ")";
    case NodeType::UNARY_OP:
        if (e.children.size() != 1) unsupported("Dynamic tier: unary op arity");
        if (e.strValue == "'" || e.strValue == ".'")
            unsupported("Dynamic tier: transpose operand (v1)");
        return "nk_rt::unop(\"" + e.strValue + "\", " + emitDynamicExpr(*e.children[0]) + ")";
    case NodeType::CALL: {
        // An un-typeable builtin result, kept boxed. A plain builtin name; each
        // argument is a real-double scalar, a Dynamic value (A3), or a 1-D double
        // array (A4) — emitDynamicExpr is the gatekeeper that boxes/refuses each.
        // Index reads and method calls are not this path.
        if (e.children.empty()) unsupported("Dynamic tier: empty call");
        const ASTNode &callee = *e.children[0];
        if (callee.type != NodeType::IDENTIFIER)
            unsupported("Dynamic tier: non-identifier callee");
        // Indexing a Dynamic value: z(i) where z is a Dynamic VARIABLE in scope
        // (not a builtin, not a typed array). `z(i)` is index/call-ambiguous —
        // the runtime (nk_index) resolves it. v1: a single subscript.
        if (types_.has(callee.strValue)
            && inferExpr(callee, types_, reg_, classes_).type.isDynamic()) {
            if (e.children.size() < 2)
                unsupported("Dynamic tier: index with no subscript");
            std::string subs;
            for (std::size_t k = 1; k < e.children.size(); ++k)
                subs += (k > 1 ? ", " : "") + emitDynamicExpr(*e.children[k]);
            return "nk_rt::index_dyn(" + callee.strValue + ", {" + subs + "})";
        }
        if (isArrayVar(callee.strValue))
            unsupported("Dynamic tier: index read '" + callee.strValue + "' (v1, A4)");
        // A boxed-result call to a CO-COMPILED user function (in this program's
        // function table) cannot use the call_dyn-by-NAME ABI: that resolves
        // against the runtime registry, which holds builtins / engine functions —
        // NOT the compiled specialisations emitted alongside this code. Emitting
        // call_dyn here would look up a name that is not the compiled spec, i.e.
        // miscompile in a standalone artifact. Refuse instead (refuse-not-
        // miscompile). This is the path a recursive call takes (the monomorphiser
        // breaks recursion to Dynamic), so recursion cleanly refuses under the
        // bridge rather than miscompiling. (An EXTERNAL callee — not in the table —
        // keeps call_dyn below: it genuinely resolves in the runtime/engine.) A
        // future enhancement can call the compiled spec directly and box its result.
        if (ctx_ && ctx_->funcs && !types_.has(callee.strValue)
            && ctx_->funcs->has(callee.strValue))
            unsupported("Dynamic tier: boxed result of a co-compiled user call '"
                        + callee.strValue + "' (e.g. recursion) — the compiled "
                        "specialisation is not resolvable by name in the runtime registry (v1)");
        bool allDoubleScalar = true;
        for (std::size_t i = 1; i < e.children.size(); ++i) {
            const AbstractValue av = inferExpr(*e.children[i], types_, reg_, classes_);
            if (!(av.type.isConcrete() && av.type.dtype == ValueType::DOUBLE
                  && av.type.shape.isScalar()))
                allDoubleScalar = false;  // Dynamic / array arg -> call_dynv (emitDynamicExpr boxes)
        }
        std::string args;
        for (std::size_t i = 1; i < e.children.size(); ++i)
            args += (i > 1 ? ", " : "")
                    + (allDoubleScalar ? emitExpr(*e.children[i]) : emitDynamicExpr(*e.children[i]));
        // All real-double scalars -> call_dyn (boxes doubles directly, fewer
        // allocations); any Dynamic arg -> call_dynv (each arg is a boxed val).
        return allDoubleScalar
                   ? ("nk_rt::call_dyn(\"" + callee.strValue + "\", {" + args + "})")
                   : ("nk_rt::call_dynv(\"" + callee.strValue + "\", {" + args + "})");
    }
    default:
        unsupported("Dynamic tier: expression node kind");
    }
}

std::string Emitter::emitCondition(const ASTNode &c)
{
    const AbstractValue cv = inferExpr(c, types_, reg_, classes_);
    // Dynamic tier (DESIGN.md §10 C1): a Dynamic condition reduces to MATLAB
    // truthiness via the runtime — the non-poisoning sink that lets an
    // un-typeable value steer control flow while the branches stay typed.
    if (bridge_ && cv.type.isDynamic())
        return emitDynamicExpr(c) + ".truth()";
    return emitExpr(c);
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

    // Type/shape query predicates -> a LOGICAL scalar. Pure expressions (no loop /
    // no bridge) -> they compose in any context, incl. an `if` condition.
    //   isempty(x)  : numel == 0   (a scalar is never empty   -> "false")
    //   isscalar(x) : numel == 1   (a scalar has one element  -> "true")
    // An array var lowers to a numel compare (1-D length, 2-D rows*cols, N-D
    // product of dims); a scalar operand to a compile-time constant. A non-array /
    // non-scalar (Dynamic) arg falls through to the boundary (refuse, never
    // miscompile).
    if ((name == "isempty" || name == "isscalar") && nargs == 1) {
        const ASTNode &arg = *call.children[1];
        const char    *cmp = name == "isempty" ? ") == 0)" : ") == 1)";
        if (arg.type == NodeType::IDENTIFIER && isArrayVar(arg.strValue)) {
            const ArrayInfo &ai = arrays_.at(arg.strValue);
            std::string      n  = ai.lenVar;
            if (ai.isND) {
                n = ai.ndDims[0];
                for (std::size_t i = 1; i < ai.ndDims.size(); ++i) n += " * " + ai.ndDims[i];
            } else if (ai.is2D) {
                n = ai.rowsVar + " * " + ai.colsVar;
            }
            return "((" + n + cmp;
        }
        if (inferExpr(arg, types_, reg_, classes_).type.shape.kind == ShapeKind::Scalar)
            return name == "isempty" ? "false" : "true";
    }

    // isreal(x): true iff x has no imaginary part. A codegen value's complexity is
    // fixed by its STATIC dtype, so this is a compile-time constant -- a COMPLEX
    // operand -> "false", any other concrete dtype -> "true". A Dynamic arg falls
    // through to the boundary.
    if (name == "isreal" && nargs == 1) {
        const InferredType at = inferExpr(*call.children[1], types_, reg_, classes_).type;
        if (at.isConcrete())
            return at.dtype == ValueType::COMPLEX ? "false" : "true";
    }

    // isrow / iscolumn / isvector: 2-D orientation predicates -> a LOGICAL scalar.
    // MATLAB: isrow = (ndims==2 && rows==1); iscolumn = (ndims==2 && cols==1);
    // isvector = (ndims==2 && (rows==1 || cols==1)). A rank-N (N>=3) array is none
    // of these. A 2-D matrix compares its known dims. A 1-D buffer is always a
    // vector; its row/col answer comes from the recorded orientation (a row is
    // also a column iff length 1, and vice-versa). A 1-D buffer of ERASED
    // orientation can't answer isrow/iscolumn -> boundary. A scalar is all three.
    if ((name == "isrow" || name == "iscolumn" || name == "isvector") && nargs == 1) {
        const ASTNode &arg = *call.children[1];
        if (arg.type == NodeType::IDENTIFIER && isArrayVar(arg.strValue)) {
            const ArrayInfo &ai = arrays_.at(arg.strValue);
            if (ai.isND)
                return "false";  // ndims >= 3 -> not a row / column / vector
            if (ai.is2D) {
                if (name == "isrow")    return "((" + ai.rowsVar + ") == 1)";
                if (name == "iscolumn") return "((" + ai.colsVar + ") == 1)";
                return "((" + ai.rowsVar + ") == 1 || (" + ai.colsVar + ") == 1)";
            }
            if (name == "isvector")
                return "true";  // a 1-D buffer is always a vector
            if (ai.orient == VecOrient::Row)
                return name == "isrow" ? std::string("true") : "(" + ai.lenVar + " == 1)";
            if (ai.orient == VecOrient::Col)
                return name == "iscolumn" ? std::string("true") : "(" + ai.lenVar + " == 1)";
            // orientation erased -> can't decide isrow/iscolumn -> fall through (boundary)
        } else if (inferExpr(arg, types_, reg_, classes_).type.shape.kind == ShapeKind::Scalar) {
            return "true";  // a 1x1 scalar is a row, a column, and a vector
        }
    }

    // dtype-classification predicates -> a LOGICAL scalar. A codegen value's dtype
    // is STATIC, so each is a compile-time constant from the operand's dtype:
    //   isnumeric : float (double/single) | integer (int*/uint*) | complex
    //   isfloat   : double | single | complex   (MATLAB: complex doubles are float)
    //   isinteger : int* | uint*
    //   ischar    : char        islogical : logical
    // A Dynamic arg falls through to the boundary (refuse, never miscompile).
    if ((name == "isnumeric" || name == "isfloat" || name == "isinteger"
         || name == "ischar" || name == "islogical")
        && nargs == 1) {
        const InferredType at = inferExpr(*call.children[1], types_, reg_, classes_).type;
        if (at.isConcrete()) {
            const ValueType dt = at.dtype;
            bool             v;
            if (name == "isnumeric")
                v = isIntegerType(dt) || isFloatType(dt) || dt == ValueType::COMPLEX;
            else if (name == "isfloat")
                v = isFloatType(dt) || dt == ValueType::COMPLEX;
            else if (name == "isinteger")
                v = isIntegerType(dt);
            else if (name == "ischar")
                v = dt == ValueType::CHAR;
            else  // islogical
                v = dt == ValueType::LOGICAL;
            return v ? "true" : "false";
        }
    }

    // size(A, dim) with a compile-time literal dim: the dim's size (2-D
    // rows/cols, N-D the dim, out-of-range -> 1). A 1-D buffer's row/col
    // orientation is erased by the RawBuffer ABI but recorded from the
    // compile-time type (ai.orient): a row is 1 x len (size(.,1)=1,
    // size(.,2)=len), a col is len x 1. Unknown orientation still falls
    // through (bridge / explicit boundary).
    if (name == "size" && nargs == 2 && call.children[1]->type == NodeType::IDENTIFIER
        && isArrayVar(call.children[1]->strValue)
        && call.children[2]->type == NodeType::NUMBER_LITERAL) {
        const ArrayInfo &ai = arrays_.at(call.children[1]->strValue);
        const double     kd = call.children[2]->numValue;
        const auto       k  = static_cast<std::size_t>(kd);
        const bool       kok = kd >= 1.0 && static_cast<double>(k) == kd;
        // Emit only once the axis is determinate: any 2-D/N-D array, or a 1-D
        // vector with a known orientation. A 1-D unknown-orientation vector
        // falls through (-> bridge), since dimExpr can't place its single axis.
        if (kok && (ai.isND || ai.is2D || ai.orient != VecOrient::Unknown))
            return "static_cast<double>(" + dimExpr(ai, k - 1) + ")";
    }

    // Complex accessors: real/imag/angle/conj (scalar). std::real / std::imag /
    // std::arg accept both double and complex; conj of a REAL value is the
    // identity (std::conj(double) would return a std::complex, mismatching the
    // real-preserving transfer), so emit the bare value there.
    if (nargs == 1
        && (name == "real" || name == "imag" || name == "angle" || name == "conj")) {
        const std::string arg = emitExpr(*call.children[1]);
        if (name == "real")  return "std::real(" + arg + ")";
        if (name == "imag")  return "std::imag(" + arg + ")";
        if (name == "angle") return "std::arg(" + arg + ")";
        const ValueType adt = inferExpr(*call.children[1], types_, reg_, classes_).type.dtype;
        return adt == ValueType::COMPLEX ? ("std::conj(" + arg + ")") : ("(" + arg + ")");  // conj
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
        // A 1-D / 2-D-KnownDims / fully-known-N-D double/complex array result is
        // returned BY VALUE (flat std::vector) — the callee is emitted with
        // interprocByValueReturn so its signature returns std::vector<T>
        // (self-describing .size(); dims compile-time-known on both sides). A
        // runtime-dim N-D result stays an explicit boundary (its dims would need to
        // travel with the buffer; sound refusal below).
        const bool arrayByValue = isByValueReturnArrayType(ret);
        if (!isUnboxableScalarType(ret) && !arrayByValue)
            unsupported("interprocedural call result must be an unboxed scalar or a 1-D / "
                        "2-D / fully-known-N-D double/complex array (v1): '" + name + "'");
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

    // `end` inside this 1-D scalar index resolves to the array's length; push it so
    // x(end) / x(end-1) / x(end/2) lower correctly, then pop after the arg is emitted.
    endStack_.push_back(ai.lenVar);
    const std::string idxExpr = emitExpr(*call.children[1]);
    endStack_.pop_back();
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

    // LOGICAL-INDEXING WRITE: `x(m) = c` where m is a LOGICAL-array var and c a
    // SCALAR -> scatter c into x at the positions where m is true (MATLAB
    // x(logical)=scalar, e.g. clamp x(x<0)=0). x must be writable (local/output).
    // v1: a single LOGICAL-array subscript that is a VARIABLE; x + m 1-D; scalar
    // rhs. Bound on min(len) so a too-long mask cannot write x out of bounds.
    if (lhsCall.children.size() == 2 && lhsCall.children[1]->type == NodeType::IDENTIFIER
        && isArrayVar(lhsCall.children[1]->strValue)
        && arrays_.at(lhsCall.children[1]->strValue).dtype == ValueType::LOGICAL && !ai.is2D) {
        if (!ai.isLocal && !ai.isOutput)
            unsupported("logical-indexing write to a read-only array parameter '" + base + "'");
        const ArrayInfo &bm = arrays_.at(lhsCall.children[1]->strValue);  // m (mask)
        if (bm.is2D || bm.isND) unsupported("logical-indexing write: a 1-D mask only (v1)");
        const AbstractValue rhsScalar = inferExpr(rhs, types_, reg_, classes_);
        if (!rhsScalar.type.shape.isScalar())
            unsupported("logical-indexing write: a scalar rhs only (v1)");
        line("{");
        ++indent_;
        line("const " + cppScalarType(ai.dtype) + " _nk_c = " + emitExpr(rhs) + ";");
        line("const std::size_t _nk_n = " + bm.lenVar + " < " + ai.lenVar + " ? "
             + bm.lenVar + " : " + ai.lenVar + ";");
        open("for (std::size_t _nk_i = 0; _nk_i < _nk_n; ++_nk_i)");
        line("if (" + bm.dataExpr + "[_nk_i]) " + ptr + "[_nk_i] = _nk_c;");
        close();
        --indent_;
        line("}");
        return;
    }

    // 1-D SLICE WRITE: x(a:b) = rhs / x(a:s:b) = rhs. The lhs colon ranges over x's
    // 1-based positions (`end` -> x's length, pushed here). count = the colon count;
    // then either broadcast a SCALAR rhs into each position, or copy a matched-length
    // 1-D array rhs element-wise. x must be writable (local/output) + DOUBLE; bounds-
    // checked, and (array rhs) length-checked. Aliasing (rhs is x itself) is refused
    // (overlapping copy) -- sound. v1: x 1-D DOUBLE.
    if (lhsCall.children.size() == 2 && lhsCall.children[1]->type == NodeType::COLON_EXPR
        && !ai.is2D && !ai.isND && ai.dtype == ValueType::DOUBLE
        && (lhsCall.children[1]->children.size() == 2 || lhsCall.children[1]->children.size() == 3)) {
        if (!ai.isLocal && !ai.isOutput)
            unsupported("slice write to a read-only array parameter '" + base + "'");
        const ASTNode      &colon     = *lhsCall.children[1];
        const bool          three     = colon.children.size() == 3;
        const AbstractValue rhsAV     = inferExpr(rhs, types_, reg_, classes_);
        const bool          rhsScalar = rhsAV.type.isConcrete() && rhsAV.type.shape.isScalar();
        const bool          rhsArr    = rhs.type == NodeType::IDENTIFIER && isArrayVar(rhs.strValue)
                                     && rhs.strValue != base && !arrays_.at(rhs.strValue).is2D
                                     && !arrays_.at(rhs.strValue).isND
                                     && arrays_.at(rhs.strValue).dtype == ValueType::DOUBLE;
        if (!rhsScalar && !rhsArr)
            unsupported("slice write rhs: a scalar or a distinct 1-D DOUBLE array var (v1)");
        endStack_.push_back(ai.lenVar);
        const std::string start = emitExpr(*colon.children[0]);
        const std::string step  = three ? emitExpr(*colon.children[1]) : std::string("1.0");
        const std::string stop  = emitExpr(*colon.children[three ? 2 : 1]);
        endStack_.pop_back();
        line("{");
        ++indent_;
        line("const double _nk_start = " + start + ";");
        line("const double _nk_step = " + step + ";");
        line("const double _nk_stop = " + stop + ";");
        line("const double _nk_nr = (_nk_stop - _nk_start) / _nk_step;");
        line("const std::ptrdiff_t _nk_cnt = (_nk_step == 0.0 || _nk_nr < 0.0)");
        line("    ? 0 : static_cast<std::ptrdiff_t>(_nk_nr + 1e-10) + 1;");
        line("const std::ptrdiff_t _nk_s0 = static_cast<std::ptrdiff_t>(_nk_start) - 1;");
        line("const std::ptrdiff_t _nk_st = static_cast<std::ptrdiff_t>(_nk_step);");
        open("if (_nk_cnt > 0)");
        line("const std::ptrdiff_t _nk_last = _nk_s0 + (_nk_cnt - 1) * _nk_st;");
        line("const std::ptrdiff_t _nk_len = static_cast<std::ptrdiff_t>(" + ai.lenVar + ");");
        line("if (_nk_s0 < 0 || _nk_s0 >= _nk_len || _nk_last < 0 || _nk_last >= _nk_len)");
        line("    throw std::out_of_range(\"numkit: index out of bounds\");");
        close();
        if (rhsScalar) {
            line("const double _nk_v = " + emitExpr(rhs) + ";");
            open("for (std::ptrdiff_t _nk_k = 0; _nk_k < _nk_cnt; ++_nk_k)");
            line(ptr + "[static_cast<std::size_t>(_nk_s0 + _nk_k * _nk_st)] = _nk_v;");
            close();
        } else {
            const ArrayInfo &ra = arrays_.at(rhs.strValue);
            line("if (static_cast<std::ptrdiff_t>(" + ra.lenVar + ") != _nk_cnt)");
            line("    throw std::out_of_range(\"numkit: slice assignment length mismatch\");");
            open("for (std::ptrdiff_t _nk_k = 0; _nk_k < _nk_cnt; ++_nk_k)");
            line(ptr + "[static_cast<std::size_t>(_nk_s0 + _nk_k * _nk_st)] = " + ra.dataExpr
                 + "[static_cast<std::size_t>(_nk_k)];");
            close();
        }
        --indent_;
        line("}");
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
        // transfer name). Unconditionally elementwise: + - .* ./ .\ .^. The
        // matrix forms * / are elementwise ONLY in their scalar-scaling cases —
        // mtimes (*) when EITHER operand is scalar (s*X == s.*X), mrdivide (/)
        // when the DENOMINATOR is scalar (X/s == X./s); s/X is a matrix divide.
        if (e.children.size() != 2) return false;
        static const std::set<std::string> kElementwise = {
            "+", "-", ".*", "./", ".\\", ".^",
            // Relational + elementwise-logical ops: the same broadcast shape rules
            // as arithmetic, a LOGICAL result. (Short-circuit && / || are
            // scalar-only and stay excluded.)
            "<", ">", "<=", ">=", "==", "~=", "&", "|"};
        auto isScalarArg = [&](const ASTNode &n) {
            return inferExpr(n, types_, reg_, classes_).type.shape.isScalar();
        };
        if (e.strValue == "*") {
            if (!isScalarArg(*e.children[0]) && !isScalarArg(*e.children[1])) return false;
        } else if (e.strValue == "/") {
            if (!isScalarArg(*e.children[1])) return false;
        } else if (kElementwise.count(e.strValue) == 0) {
            return false;
        }
        return collectElementwise(*e.children[0], arrays)
               && collectElementwise(*e.children[1], arrays);
    }
    case NodeType::UNARY_OP:
        return (e.strValue == "-" || e.strValue == "+" || e.strValue == "~")
               && e.children.size() == 1 && collectElementwise(*e.children[0], arrays);
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

std::string Emitter::dimExpr(const ArrayInfo &a, std::size_t k) const
{
    if (a.isND) return k < a.ndDims.size() ? a.ndDims[k] : std::string("1");
    if (a.is2D) return k == 0 ? a.rowsVar : k == 1 ? a.colsVar : std::string("1");
    if (a.orient == VecOrient::Row) return k == 1 ? a.lenVar : std::string("1");
    if (a.orient == VecOrient::Col) return k == 0 ? a.lenVar : std::string("1");
    return std::string("1");
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
            && (arrays_.at(name).isOutput || arrays_.at(name).isLocal)  // local/output, any rank
            && rhs.type == NodeType::CALL && !rhs.children.empty()
            && rhs.children[0]->type == NodeType::IDENTIFIER
            && (rhs.children[0]->strValue == "zeros"
                || rhs.children[0]->strValue == "ones")) {
            const ArrayInfo  &ai   = arrays_.at(name);
            const std::string fill = rhs.children[0]->strValue == "zeros"
                                         ? zeroLiteral(ai.dtype)
                                         : "1";
            if (ai.ndRuntimeLocal) {
                // Capture each runtime dim into its companion var, then size the
                // owned vector to their product (column-major flat storage).
                std::string prod;
                for (std::size_t d = 0; d < ai.ndDims.size(); ++d) {
                    line(ai.ndDims[d] + " = nk_rt::dim(" + emitExpr(*rhs.children[d + 1]) + ");");
                    prod += (d ? " * " : "") + ai.ndDims[d];
                }
                line(name + ".assign(" + prod + ", " + fill + ");");
            } else if (ai.isLocal) {
                std::string numel;  // product of the size args (zeros(1,n) -> 1*n)
                for (std::size_t i = 1; i < rhs.children.size(); ++i)
                    numel += (i > 1 ? " * " : "")
                             + ("nk_rt::dim(" + emitExpr(*rhs.children[i]) + ")");
                if (numel.empty()) numel = "0";
                line(name + ".assign(" + numel + ", " + fill + ");");
            } else {
                open("for (std::size_t _nk_i = 0; _nk_i < " + ai.lenVar + "; ++_nk_i)");
                line(ai.dataExpr + "[_nk_i] = " + fill + ";");
                close();
            }
            // Record the array type so a later `name(k)` infers element-access
            // (the env, not arrays_, drives inferExpr). A 2-D / N-D array records
            // its TRUE shape (inferExpr of the zeros/ones RHS) instead of a 1-D
            // row-vector stand-in — accurate, no type-lie; arrays_ still drives
            // indexing/queries either way.
            if (ai.isND || ai.is2D)
                types_.set(name, inferExpr(rhs, types_, reg_, classes_));
            else
                types_.set(name, {InferredType::concrete(ai.dtype, Shape::rowVector()),
                                  ConstVal::unknown()});
            return;
        }
        // linspace(a, b [, n]) -> n linearly-spaced points from a to b, a 1 x n
        // owned-vector LOCAL (the size-constructor transfer gives it a 1 x n /
        // 1 x 100 / unknown shape). Like zeros/ones it sizes the local, then fills a
        // ramp -- but MATLAB forces the LAST point to exactly b (no rounding drift),
        // so we seed every slot to b and overwrite [0, n-1) with a + i*step. n==1 ->
        // {b}; n<=0 -> empty. v1: an owned LOCAL, 2- or 3-arg; an OUTPUT-param target
        // (caller-sized) falls through to the bridge.
        if (isArrayVar(name) && arrays_.at(name).isLocal
            && rhs.type == NodeType::CALL && !rhs.children.empty()
            && rhs.children[0]->type == NodeType::IDENTIFIER
            && rhs.children[0]->strValue == "linspace"
            && (rhs.children.size() == 3 || rhs.children.size() == 4)) {
            const std::string a = emitExpr(*rhs.children[1]);
            const std::string b = emitExpr(*rhs.children[2]);
            const std::string n = rhs.children.size() == 4
                                      ? ("nk_rt::dim(" + emitExpr(*rhs.children[3]) + ")")
                                      : std::string("100");  // 2-arg default (MATLAB)
            line("{");
            ++indent_;
            line("const double _nk_a = " + a + ";");
            line("const double _nk_b = " + b + ";");
            line("const std::size_t _nk_n = " + n + ";");
            line(name + ".assign(_nk_n, _nk_b);");  // size n; the last point is already b
            open("if (_nk_n >= 2)");
            line("const double _nk_step = (_nk_b - _nk_a) / static_cast<double>(_nk_n - 1);");
            open("for (std::size_t _nk_i = 0; _nk_i + 1 < _nk_n; ++_nk_i)");
            line(name + "[_nk_i] = _nk_a + static_cast<double>(_nk_i) * _nk_step;");
            close();
            close();
            --indent_;
            line("}");
            const ArrayInfo &ai = arrays_.at(name);
            if (ai.isND || ai.is2D)
                types_.set(name, inferExpr(rhs, types_, reg_, classes_));
            return;
        }
        // Colon range MATERIALISED to an array: v = a:b (step 1) / v = a:s:b. A
        // double row built into the owned 1-D local. Parser child order (verified):
        // 2 children = [start, stop]; 3 children = [start, step, stop]. MATLAB count
        // n = floor((stop-start)/step + tol) + 1, clamped to >=0 (empty when step==0
        // or the direction disagrees); a small tol matches MATLAB's float-range
        // length (e.g. 0:0.1:1 -> 11). The `for i=a:b` HEADER is special-cased
        // elsewhere (no array); this is the value/materialise form. v1: owned 1-D
        // LOCAL. (Exact MATLAB tolerance for pathological float ranges is deferred.)
        if (isArrayVar(name) && arrays_.at(name).isLocal && !arrays_.at(name).is2D
            && !arrays_.at(name).isND && rhs.type == NodeType::COLON_EXPR
            && (rhs.children.size() == 2 || rhs.children.size() == 3)) {
            const bool        three = rhs.children.size() == 3;
            const std::string start = emitExpr(*rhs.children[0]);
            const std::string step  = three ? emitExpr(*rhs.children[1]) : std::string("1.0");
            const std::string stop  = emitExpr(*rhs.children[three ? 2 : 1]);
            line("{");
            ++indent_;
            line("const double _nk_start = " + start + ";");
            line("const double _nk_step = " + step + ";");
            line("const double _nk_stop = " + stop + ";");
            line("const double _nk_nr = (_nk_stop - _nk_start) / _nk_step;");
            line("const std::ptrdiff_t _nk_cnt = (_nk_step == 0.0 || _nk_nr < 0.0)");
            line("    ? 0 : static_cast<std::ptrdiff_t>(_nk_nr + 1e-10) + 1;");
            line(name + ".assign(static_cast<std::size_t>(_nk_cnt), 0.0);");
            open("for (std::ptrdiff_t _nk_i = 0; _nk_i < _nk_cnt; ++_nk_i)");
            line(name + "[static_cast<std::size_t>(_nk_i)] = _nk_start + static_cast<double>(_nk_i) * _nk_step;");
            close();
            --indent_;
            line("}");
            return;
        }
        // 1-D SLICE read: y = x(a:b) / y = x(a:s:b) -> a sub-array copied into the
        // owned 1-D local y. The colon ranges over x's 1-based positions; `end`
        // inside it = x's length (pushed here, so x(2:end) works). count = the colon
        // count; y[k] = x[(start-1) + k*step] (0-based, forward or reverse step).
        // Bounds-checked against x's length (MATLAB errors on an out-of-range slice).
        // v1: x a 1-D DOUBLE array VAR, y an owned 1-D LOCAL.
        if (isArrayVar(name) && arrays_.at(name).isLocal && !arrays_.at(name).is2D
            && !arrays_.at(name).isND && rhs.type == NodeType::CALL && rhs.children.size() == 2
            && rhs.children[0]->type == NodeType::IDENTIFIER && isArrayVar(rhs.children[0]->strValue)
            && rhs.children[1]->type == NodeType::COLON_EXPR
            && !arrays_.at(rhs.children[0]->strValue).is2D
            && !arrays_.at(rhs.children[0]->strValue).isND
            && arrays_.at(rhs.children[0]->strValue).dtype == ValueType::DOUBLE
            && (rhs.children[1]->children.size() == 2 || rhs.children[1]->children.size() == 3)) {
            const ArrayInfo  &xa    = arrays_.at(rhs.children[0]->strValue);
            const ASTNode    &colon = *rhs.children[1];
            const bool        three = colon.children.size() == 3;
            endStack_.push_back(xa.lenVar);  // `end` inside the slice = x's length
            const std::string start = emitExpr(*colon.children[0]);
            const std::string step  = three ? emitExpr(*colon.children[1]) : std::string("1.0");
            const std::string stop  = emitExpr(*colon.children[three ? 2 : 1]);
            endStack_.pop_back();
            line("{");
            ++indent_;
            line("const double _nk_start = " + start + ";");
            line("const double _nk_step = " + step + ";");
            line("const double _nk_stop = " + stop + ";");
            line("const double _nk_nr = (_nk_stop - _nk_start) / _nk_step;");
            line("const std::ptrdiff_t _nk_cnt = (_nk_step == 0.0 || _nk_nr < 0.0)");
            line("    ? 0 : static_cast<std::ptrdiff_t>(_nk_nr + 1e-10) + 1;");
            line("const std::ptrdiff_t _nk_s0 = static_cast<std::ptrdiff_t>(_nk_start) - 1;");
            line("const std::ptrdiff_t _nk_st = static_cast<std::ptrdiff_t>(_nk_step);");
            open("if (_nk_cnt > 0)");
            line("const std::ptrdiff_t _nk_last = _nk_s0 + (_nk_cnt - 1) * _nk_st;");
            line("const std::ptrdiff_t _nk_len = static_cast<std::ptrdiff_t>(" + xa.lenVar + ");");
            line("if (_nk_s0 < 0 || _nk_s0 >= _nk_len || _nk_last < 0 || _nk_last >= _nk_len)");
            line("    throw std::out_of_range(\"numkit: index out of bounds\");");
            close();
            line(name + ".assign(static_cast<std::size_t>(_nk_cnt), 0.0);");
            open("for (std::ptrdiff_t _nk_k = 0; _nk_k < _nk_cnt; ++_nk_k)");
            line(name + "[static_cast<std::size_t>(_nk_k)] = " + xa.dataExpr
                 + "[static_cast<std::size_t>(_nk_s0 + _nk_k * _nk_st)];");
            close();
            --indent_;
            line("}");
            return;
        }
        // `s = size(A)` (no dim): fill a 1 x rank row with A's per-axis sizes.
        // rank is compile-time (2 for a scalar/vector/matrix; the array's rank
        // for N-D). Native + self-contained — the first array-RESULT-from-a-
        // builtin producer beyond zeros/ones. BEFORE the bridged path, so it
        // lowers natively instead of boxing into numkit::size.
        if (isArrayVar(name) && !arrays_.at(name).is2D && !arrays_.at(name).isND
            && rhs.type == NodeType::CALL && rhs.children.size() == 2
            && rhs.children[0]->type == NodeType::IDENTIFIER && rhs.children[0]->strValue == "size"
            && rhs.children[1]->type == NodeType::IDENTIFIER) {
            const std::string &opnd = rhs.children[1]->strValue;
            const ArrayInfo   &sai  = arrays_.at(name);
            if (isArrayVar(opnd)) {
                const ArrayInfo  &aai  = arrays_.at(opnd);
                const std::size_t rank = aai.isND ? aai.ndDims.size() : 2;
                if (sai.isLocal) line(name + ".assign(" + std::to_string(rank) + ", 0.0);");
                for (std::size_t k = 0; k < rank; ++k)
                    line(sai.dataExpr + "[" + std::to_string(k) + "] = static_cast<double>("
                         + dimExpr(aai, k) + ");");
                types_.set(name, {InferredType::concrete(sai.dtype, Shape::rowVector()),
                                  ConstVal::unknown()});
                return;
            }
            // A scalar operand: size is [1 1] (rank 2). Native + self-contained.
            if (inferExpr(*rhs.children[1], types_, reg_, classes_).type.shape.kind
                == ShapeKind::Scalar) {
                if (sai.isLocal) line(name + ".assign(2, 1.0);");
                else { line(sai.dataExpr + "[0] = 1.0;"); line(sai.dataExpr + "[1] = 1.0;"); }
                types_.set(name, {InferredType::concrete(sai.dtype, Shape::rowVector()),
                                  ConstVal::unknown()});
                return;
            }
        }
        // Transpose: y = x' (ctranspose) / y = x.' (transpose). A 1-D vector
        // flips orientation; a 2-D matrix swaps its dims (column-major). The
        // data is copied, and ctranspose (') additionally conjugates a complex
        // operand. The result shape comes from the transfer. N-D transpose is
        // undefined in MATLAB -> refused (an explicit boundary). Native +
        // self-contained.
        if (isArrayVar(name) && (arrays_.at(name).isOutput || arrays_.at(name).isLocal)
            && rhs.type == NodeType::UNARY_OP && (rhs.strValue == "'" || rhs.strValue == ".'")
            && rhs.children.size() == 1 && rhs.children[0]->type == NodeType::IDENTIFIER
            && isArrayVar(rhs.children[0]->strValue)) {
            const ArrayInfo &dst = arrays_.at(name);
            const ArrayInfo &src = arrays_.at(rhs.children[0]->strValue);
            if (dst.isND || src.isND)
                unsupported("N-D transpose (undefined in MATLAB)");
            if (name == rhs.children[0]->strValue)
                unsupported("in-place transpose (y = y')");
            const bool conj = rhs.strValue == "'" && src.dtype == ValueType::COMPLEX;
            if (src.is2D || dst.is2D) {
                // 2-D matrix transpose, column-major: y is n x m, A is m x n,
                // and y(p,q) = A(q,p) -> y[p + q*yrows] = op(A[q + p*Arows]).
                if (!src.is2D || !dst.is2D)
                    unsupported("transpose dest/source rank mismatch");
                if (dst.isLocal)
                    line(name + ".assign(" + dst.rowsVar + " * " + dst.colsVar + ", "
                         + zeroLiteral(dst.dtype) + ");");
                open("for (std::size_t _nk_j = 0; _nk_j < " + dst.colsVar + "; ++_nk_j)");
                open("for (std::size_t _nk_i = 0; _nk_i < " + dst.rowsVar + "; ++_nk_i)");
                const std::string rd = src.dataExpr + "[_nk_j + _nk_i * " + src.rowsVar + "]";
                line(dst.dataExpr + "[_nk_i + _nk_j * " + dst.rowsVar + "] = "
                     + (conj ? ("std::conj(" + rd + ")") : rd) + ";");
                close();
                close();
            } else {
                // 1-D vector transpose: orientation flip, element-for-element
                // copy (ctranspose conjugates a complex operand).
                if (dst.isLocal) line(name + ".resize(" + src.lenVar + ");");
                open("for (std::size_t _nk_i = 0; _nk_i < " + src.lenVar + "; ++_nk_i)");
                const std::string rd = src.dataExpr + "[_nk_i]";
                line(dst.dataExpr + "[_nk_i] = " + (conj ? ("std::conj(" + rd + ")") : rd) + ";");
                close();
            }
            types_.set(name, inferExpr(rhs, types_, reg_, classes_));
            return;
        }
        // Matrix product: C = A * B (both 2-D). C is m x n (A is m x k, B is
        // k x n), C(i,j) = sum_l A(i,l)*B(l,j), column-major. The shared dim
        // must agree (runtime guard, MATLAB-like). With ops kernels enabled +
        // a DOUBLE result, lower to numkit::ops::matmulDouble (the SIMD kernel
        // ops owns); otherwise an inline triple loop (the deletable fallback +
        // the complex path — no complex ops kernel yet). (Scalar*X / X*scalar
        // are elementwise scaling, handled above; matrix*vector needs a 1-D
        // operand and is not yet lowered.)
        if (isArrayVar(name) && (arrays_.at(name).isOutput || arrays_.at(name).isLocal)
            && arrays_.at(name).is2D && rhs.type == NodeType::BINARY_OP && rhs.strValue == "*"
            && rhs.children.size() == 2 && rhs.children[0]->type == NodeType::IDENTIFIER
            && isArrayVar(rhs.children[0]->strValue)
            && rhs.children[1]->type == NodeType::IDENTIFIER
            && isArrayVar(rhs.children[1]->strValue)) {
            const ArrayInfo &dst = arrays_.at(name);
            const ArrayInfo &A   = arrays_.at(rhs.children[0]->strValue);
            const ArrayInfo &B   = arrays_.at(rhs.children[1]->strValue);
            if (!A.is2D || !B.is2D)
                unsupported("matrix product with a non-matrix operand (matrix*vector not yet lowered)");
            if (name == rhs.children[0]->strValue || name == rhs.children[1]->strValue)
                unsupported("in-place matrix product (C = C * B)");
            line("if (" + A.colsVar + " != " + B.rowsVar
                 + ") throw std::out_of_range(\"numkit: inner matrix dimensions must agree\");");
            if (opsKernels_
                && (dst.dtype == ValueType::DOUBLE || dst.dtype == ValueType::COMPLEX)) {
                // ops owns the kernel: M=dst.rows, N=dst.cols, K=A.cols (==B.rows,
                // guarded). The kernel zeroes+accumulates; a LOCAL still needs
                // its owned vector sized first. DOUBLE -> SIMD matmulDouble;
                // COMPLEX -> portable matmulComplex (the call is amortised over
                // O(M·N·K), so no per-element overhead vs inline).
                const char *fn =
                    dst.dtype == ValueType::DOUBLE ? "matmulDouble" : "matmulComplex";
                if (dst.isLocal)
                    line(name + ".resize(" + dst.rowsVar + " * " + dst.colsVar + ");");
                line("numkit::ops::" + std::string(fn) + "(" + A.dataExpr + ", " + B.dataExpr
                     + ", " + dst.dataExpr + ", " + dst.rowsVar + ", " + dst.colsVar + ", "
                     + A.colsVar + ");");
            } else {
                if (dst.isLocal)
                    line(name + ".assign(" + dst.rowsVar + " * " + dst.colsVar + ", "
                         + zeroLiteral(dst.dtype) + ");");
                open("for (std::size_t _nk_j = 0; _nk_j < " + dst.colsVar + "; ++_nk_j)");
                open("for (std::size_t _nk_i = 0; _nk_i < " + dst.rowsVar + "; ++_nk_i)");
                line(cppScalarType(dst.dtype) + " _nk_acc = " + zeroLiteral(dst.dtype) + ";");
                open("for (std::size_t _nk_l = 0; _nk_l < " + A.colsVar + "; ++_nk_l)");
                line("_nk_acc += " + A.dataExpr + "[_nk_i + _nk_l * " + A.rowsVar + "] * "
                     + B.dataExpr + "[_nk_l + _nk_j * " + B.rowsVar + "];");
                close();
                line(dst.dataExpr + "[_nk_i + _nk_j * " + dst.rowsVar + "] = _nk_acc;");
                close();
                close();
            }
            types_.set(name, inferExpr(rhs, types_, reg_, classes_));
            return;
        }
        // Matrix * vector / vector * matrix -> a vector. A*x (A m x k, x k x 1)
        // -> y m x 1: y[i] = sum_l A[i+l*Arows]*x[l]. r*A (r 1 x k, A k x n)
        // -> y 1 x n: y[j] = sum_l r[l]*A[l+j*Arows]. Native double loop +
        // shared-dim guard. (vector*vector inner/outer products not yet
        // lowered.) Self-contained.
        if (isArrayVar(name) && (arrays_.at(name).isOutput || arrays_.at(name).isLocal)
            && !arrays_.at(name).is2D && !arrays_.at(name).isND
            && rhs.type == NodeType::BINARY_OP && rhs.strValue == "*"
            && rhs.children.size() == 2 && rhs.children[0]->type == NodeType::IDENTIFIER
            && isArrayVar(rhs.children[0]->strValue)
            && rhs.children[1]->type == NodeType::IDENTIFIER
            && isArrayVar(rhs.children[1]->strValue)) {
            const ArrayInfo &dst = arrays_.at(name);
            const ArrayInfo &L   = arrays_.at(rhs.children[0]->strValue);
            const ArrayInfo &R   = arrays_.at(rhs.children[1]->strValue);
            if (name == rhs.children[0]->strValue || name == rhs.children[1]->strValue)
                unsupported("in-place matrix*vector (y = y * x)");
            if (L.isND || R.isND) unsupported("N-D operand in matrix*vector");
            const std::string ty  = cppScalarType(dst.dtype);
            const std::string zer = zeroLiteral(dst.dtype);
            if (L.is2D && !R.is2D) {  // A * x -> column vector
                line("if (" + L.colsVar + " != " + R.lenVar + ") throw std::out_of_range(\""
                     "numkit: inner matrix dimensions must agree\");");
                if (dst.isLocal) line(name + ".resize(" + L.rowsVar + ");");
                open("for (std::size_t _nk_i = 0; _nk_i < " + L.rowsVar + "; ++_nk_i)");
                line(ty + " _nk_acc = " + zer + ";");
                open("for (std::size_t _nk_l = 0; _nk_l < " + L.colsVar + "; ++_nk_l)");
                line("_nk_acc += " + L.dataExpr + "[_nk_i + _nk_l * " + L.rowsVar + "] * "
                     + R.dataExpr + "[_nk_l];");
                close();
                line(dst.dataExpr + "[_nk_i] = _nk_acc;");
                close();
            } else if (!L.is2D && R.is2D) {  // r * A -> row vector
                line("if (" + L.lenVar + " != " + R.rowsVar + ") throw std::out_of_range(\""
                     "numkit: inner matrix dimensions must agree\");");
                if (dst.isLocal) line(name + ".resize(" + R.colsVar + ");");
                open("for (std::size_t _nk_j = 0; _nk_j < " + R.colsVar + "; ++_nk_j)");
                line(ty + " _nk_acc = " + zer + ";");
                open("for (std::size_t _nk_l = 0; _nk_l < " + R.rowsVar + "; ++_nk_l)");
                line("_nk_acc += " + L.dataExpr + "[_nk_l] * " + R.dataExpr + "[_nk_l + _nk_j * "
                     + R.rowsVar + "];");
                close();
                line(dst.dataExpr + "[_nk_j] = _nk_acc;");
                close();
            } else {
                unsupported("vector*vector with a vector result (use the outer-product path)");
            }
            types_.set(name, inferExpr(rhs, types_, reg_, classes_));
            return;
        }
        // Outer product: y = c * r (c m x 1, r 1 x n) -> y m x n, column-major
        // y[i + j*m] = c[i]*r[j]. The dims are the vector lengths (runtime), so y
        // is a runtime-dim rank-2 array. A LOCAL sets its dim vars from the
        // operand lengths + sizes its vector; an OUTPUT guards its caller-passed
        // dims. Complex accumulates in std::complex. Native + self-contained.
        if (isArrayVar(name) && (arrays_.at(name).isOutput || arrays_.at(name).isLocal)
            && arrays_.at(name).isND && arrays_.at(name).ndDims.size() == 2
            && rhs.type == NodeType::BINARY_OP && rhs.strValue == "*"
            && rhs.children.size() == 2 && rhs.children[0]->type == NodeType::IDENTIFIER
            && isArrayVar(rhs.children[0]->strValue)
            && rhs.children[1]->type == NodeType::IDENTIFIER
            && isArrayVar(rhs.children[1]->strValue)) {
            const ArrayInfo &dst = arrays_.at(name);
            const ArrayInfo &c   = arrays_.at(rhs.children[0]->strValue);
            const ArrayInfo &r   = arrays_.at(rhs.children[1]->strValue);
            if (c.is2D || c.isND || r.is2D || r.isND)
                unsupported("outer product over a non-vector operand");
            if (name == rhs.children[0]->strValue || name == rhs.children[1]->strValue)
                unsupported("in-place outer product");
            const std::string &d0 = dst.ndDims[0];  // rows = length(c)
            const std::string &d1 = dst.ndDims[1];  // cols = length(r)
            if (dst.ndRuntimeLocal) {
                line(d0 + " = " + c.lenVar + ";");
                line(d1 + " = " + r.lenVar + ";");
                line(name + ".assign(" + d0 + " * " + d1 + ", " + zeroLiteral(dst.dtype) + ");");
            } else {  // OUTPUT: caller-allocated + caller-passed dims; guard agreement
                line("if (" + d0 + " != " + c.lenVar + " || " + d1 + " != " + r.lenVar
                     + ") throw std::out_of_range(\"numkit: outer product output dimensions "
                       "must agree\");");
            }
            open("for (std::size_t _nk_j = 0; _nk_j < " + d1 + "; ++_nk_j)");
            open("for (std::size_t _nk_i = 0; _nk_i < " + d0 + "; ++_nk_i)");
            line(dst.dataExpr + "[_nk_i + _nk_j * " + d0 + "] = " + c.dataExpr + "[_nk_i] * "
                 + r.dataExpr + "[_nk_j];");
            close();
            close();
            types_.set(name, inferExpr(rhs, types_, reg_, classes_));
            return;
        }
        // Inner / dot product: s = r * c (r 1 x k, c k x 1) = sum_l r[l]*c[l].
        // The result is a SCALAR (s is not an array), so this is a reduction
        // loop into a scalar, not an array producer — placed where the scalar
        // LHS is handled. A complex product accumulates in std::complex.
        if (!isArrayVar(name) && rhs.type == NodeType::BINARY_OP && rhs.strValue == "*"
            && rhs.children.size() == 2 && rhs.children[0]->type == NodeType::IDENTIFIER
            && isArrayVar(rhs.children[0]->strValue)
            && rhs.children[1]->type == NodeType::IDENTIFIER
            && isArrayVar(rhs.children[1]->strValue)) {
            const AbstractValue res = inferExpr(rhs, types_, reg_, classes_);
            if (res.type.isConcrete() && res.type.shape.isScalar()) {
                const ArrayInfo &L = arrays_.at(rhs.children[0]->strValue);
                const ArrayInfo &R = arrays_.at(rhs.children[1]->strValue);
                if (L.is2D || R.is2D || L.isND || R.isND)
                    unsupported("inner product over a non-vector operand");
                line("if (" + L.lenVar + " != " + R.lenVar + ") throw std::out_of_range(\""
                     "numkit: inner matrix dimensions must agree\");");
                line(cppScalarType(res.type.dtype) + " _nk_acc = "
                     + zeroLiteral(res.type.dtype) + ";");
                open("for (std::size_t _nk_l = 0; _nk_l < " + L.lenVar + "; ++_nk_l)");
                line("_nk_acc += " + L.dataExpr + "[_nk_l] * " + R.dataExpr + "[_nk_l];");
                close();
                line(name + " = _nk_acc;");
                types_.set(name, res);
                return;
            }
        }
        // Native any(x) / all(x) -> LOGICAL scalar (self-contained; EXACT). An
        // inline short-circuit loop over a 1-D array (double or logical), so no
        // runtime is needed — preferred over the bridged reduction below. NaN is
        // nonzero (any([NaN])=true, all([NaN])=true), matching MATLAB. v1: a single
        // 1-D non-complex array arg, the result directly assigned to the scalar.
        if (!isArrayVar(name) && rhs.type == NodeType::CALL && rhs.children.size() == 2
            && rhs.children[0]->type == NodeType::IDENTIFIER
            && (rhs.children[0]->strValue == "any" || rhs.children[0]->strValue == "all")
            && rhs.children[1]->type == NodeType::IDENTIFIER
            && isArrayVar(rhs.children[1]->strValue)) {
            const ArrayInfo    &a   = arrays_.at(rhs.children[1]->strValue);
            const AbstractValue res = inferExpr(rhs, types_, reg_, classes_);
            if (!a.is2D && !a.isND && a.dtype != ValueType::COMPLEX && res.type.isConcrete()
                && res.type.shape.isScalar()) {
                const bool isAny = rhs.children[0]->strValue == "any";
                line("{");  // scope _nk_acc so repeated any/all in one fn don't collide
                ++indent_;
                line(std::string("bool _nk_acc = ") + (isAny ? "false;" : "true;"));
                open("for (std::size_t _nk_i = 0; _nk_i < " + a.lenVar + "; ++_nk_i)");
                line(isAny ? ("if (" + a.dataExpr + "[_nk_i] != 0) { _nk_acc = true; break; }")
                           : ("if (" + a.dataExpr + "[_nk_i] == 0) { _nk_acc = false; break; }"));
                close();
                line(name + " = _nk_acc;");
                --indent_;
                line("}");
                types_.set(name, res);
                return;
            }
        }
        // Native min(x) / max(x) -> scalar (self-contained; EXACT, NaN-skipping
        // like MATLAB). An inline fold over a 1-D array, preferred over the bridged
        // reduction. Seed with element 0; for a float dtype `acc != acc` replaces a
        // NaN seed, and a NaN x[i] fails the compare so it is skipped
        // (max([1 NaN 3])=3, max([NaN NaN])=NaN). v1: a single 1-D non-complex
        // array arg (assumed non-empty -> a scalar), the result directly assigned.
        if (!isArrayVar(name) && rhs.type == NodeType::CALL && rhs.children.size() == 2
            && rhs.children[0]->type == NodeType::IDENTIFIER
            && (rhs.children[0]->strValue == "min" || rhs.children[0]->strValue == "max")
            && rhs.children[1]->type == NodeType::IDENTIFIER
            && isArrayVar(rhs.children[1]->strValue)) {
            const ArrayInfo    &a   = arrays_.at(rhs.children[1]->strValue);
            const AbstractValue res = inferExpr(rhs, types_, reg_, classes_);
            if (!a.is2D && !a.isND && a.dtype != ValueType::COMPLEX && res.type.isConcrete()
                && res.type.shape.isScalar()) {
                const std::string cmp = rhs.children[0]->strValue == "max" ? ">" : "<";
                const bool        isFloat =
                    a.dtype == ValueType::DOUBLE || a.dtype == ValueType::SINGLE;
                const std::string nanClause = isFloat ? " || _nk_acc != _nk_acc" : "";
                line("{");  // scope _nk_acc so repeated reductions in one fn don't collide
                ++indent_;
                line(cppScalarType(a.dtype) + " _nk_acc = " + a.dataExpr + "[0];");
                open("for (std::size_t _nk_i = 1; _nk_i < " + a.lenVar + "; ++_nk_i)");
                line("if (" + a.dataExpr + "[_nk_i] " + cmp + " _nk_acc" + nanClause + ") _nk_acc = "
                     + a.dataExpr + "[_nk_i];");
                close();
                line(name + " = _nk_acc;");
                --indent_;
                line("}");
                types_.set(name, res);
                return;
            }
        }
        // Native sum / prod / mean(x) -> scalar, but ONLY with no bridge. UNLIKE
        // min/max these are order-dependent: a sequential reduction can differ in
        // the last ULP from the runtime's order, so the bridged path stays the
        // EXACT tier when the bridge is on; native is the self-contained fallback.
        // sum: acc += (seed 0); prod: acc *= (seed 1); mean: sum / len. Empty input
        // matches MATLAB: sum([])=0, prod([])=1, mean([])=NaN (0/0). v1: a single
        // 1-D DOUBLE array arg (logical/int dtype subtleties -> bridged), direct
        // assign.
        if (!isArrayVar(name) && !bridge_ && rhs.type == NodeType::CALL
            && rhs.children.size() == 2 && rhs.children[0]->type == NodeType::IDENTIFIER
            && (rhs.children[0]->strValue == "sum" || rhs.children[0]->strValue == "prod"
                || rhs.children[0]->strValue == "mean")
            && rhs.children[1]->type == NodeType::IDENTIFIER
            && isArrayVar(rhs.children[1]->strValue)
            && arrays_.at(rhs.children[1]->strValue).dtype == ValueType::DOUBLE) {
            const ArrayInfo    &a   = arrays_.at(rhs.children[1]->strValue);
            const AbstractValue res = inferExpr(rhs, types_, reg_, classes_);
            if (!a.is2D && !a.isND && res.type.isConcrete() && res.type.shape.isScalar()) {
                const std::string callee = rhs.children[0]->strValue;
                const bool        isProd = callee == "prod";
                line("{");  // scope _nk_acc so repeated reductions in one fn don't collide
                ++indent_;
                line(std::string("double _nk_acc = ") + (isProd ? "1.0;" : "0.0;"));
                open("for (std::size_t _nk_i = 0; _nk_i < " + a.lenVar + "; ++_nk_i)");
                line(std::string("_nk_acc ") + (isProd ? "*=" : "+=") + " " + a.dataExpr
                     + "[_nk_i];");
                close();
                if (callee == "mean")
                    line(name + " = _nk_acc / static_cast<double>(" + a.lenVar + ");");
                else
                    line(name + " = _nk_acc;");
                --indent_;
                line("}");
                types_.set(name, res);
                return;
            }
        }
        // Native norm/var/std(x) scalar reductions (1-D DOUBLE), no-bridge tier
        // (gate !bridge_; the bridged path stays exact when the bridge is on). norm =
        // the 2-norm sqrt(sum x^2); var = sum((x-mean)^2)/(n-1) (MATLAB's sample
        // default; 0 for a scalar, NaN for empty); std = sqrt(var). Two passes for
        // var/std (mean, then sum of squared deviations). v1: a single 1-D DOUBLE
        // array var; the result a scalar.
        if (!isArrayVar(name) && !bridge_ && rhs.type == NodeType::CALL
            && rhs.children.size() == 2 && rhs.children[0]->type == NodeType::IDENTIFIER
            && (rhs.children[0]->strValue == "norm" || rhs.children[0]->strValue == "var"
                || rhs.children[0]->strValue == "std")
            && rhs.children[1]->type == NodeType::IDENTIFIER
            && isArrayVar(rhs.children[1]->strValue)
            && arrays_.at(rhs.children[1]->strValue).dtype == ValueType::DOUBLE) {
            const ArrayInfo    &a   = arrays_.at(rhs.children[1]->strValue);
            const std::string  &fn  = rhs.children[0]->strValue;
            const AbstractValue res = inferExpr(rhs, types_, reg_, classes_);
            if (!a.is2D && !a.isND && res.type.isConcrete() && res.type.shape.isScalar()) {
                line("{");
                ++indent_;
                line("const std::size_t _nk_n = " + a.lenVar + ";");
                if (fn == "norm") {
                    line("double _nk_ss = 0.0;");
                    open("for (std::size_t _nk_i = 0; _nk_i < _nk_n; ++_nk_i)");
                    line("_nk_ss += " + a.dataExpr + "[_nk_i] * " + a.dataExpr + "[_nk_i];");
                    close();
                    line(name + " = std::sqrt(_nk_ss);");
                } else {
                    line("double _nk_m = 0.0;");
                    open("for (std::size_t _nk_i = 0; _nk_i < _nk_n; ++_nk_i)");
                    line("_nk_m += " + a.dataExpr + "[_nk_i];");
                    close();
                    line("_nk_m = _nk_n ? _nk_m / static_cast<double>(_nk_n) : 0.0;");
                    line("double _nk_ss = 0.0;");
                    open("for (std::size_t _nk_i = 0; _nk_i < _nk_n; ++_nk_i)");
                    line("const double _nk_d = " + a.dataExpr + "[_nk_i] - _nk_m;");
                    line("_nk_ss += _nk_d * _nk_d;");
                    close();
                    line("double _nk_var;");
                    line("if (_nk_n > 1) _nk_var = _nk_ss / static_cast<double>(_nk_n - 1);");
                    line("else _nk_var = _nk_n == 1 ? 0.0 "
                         ": std::numeric_limits<double>::quiet_NaN();");
                    line(name + (fn == "std" ? " = std::sqrt(_nk_var);" : " = _nk_var;"));
                }
                --indent_;
                line("}");
                types_.set(name, res);
                return;
            }
        }
        // Native median(x) -> the middle of a sorted copy (no-bridge tier, !bridge_).
        // Sort a temp copy with the NaN-last comparator (a valid strict-weak-ordering).
        // MATLAB's default median propagates NaN (any NaN -> NaN): a NaN sorts last,
        // so a NaN present <=> the last post-sort element is NaN. empty -> NaN; n odd
        // -> tmp[n/2]; n even -> (tmp[n/2-1]+tmp[n/2])/2. v1: a 1-D DOUBLE array var.
        if (!isArrayVar(name) && !bridge_ && rhs.type == NodeType::CALL
            && rhs.children.size() == 2 && rhs.children[0]->type == NodeType::IDENTIFIER
            && rhs.children[0]->strValue == "median"
            && rhs.children[1]->type == NodeType::IDENTIFIER
            && isArrayVar(rhs.children[1]->strValue)
            && arrays_.at(rhs.children[1]->strValue).dtype == ValueType::DOUBLE) {
            const ArrayInfo    &a   = arrays_.at(rhs.children[1]->strValue);
            const AbstractValue res = inferExpr(rhs, types_, reg_, classes_);
            if (!a.is2D && !a.isND && res.type.isConcrete() && res.type.shape.isScalar()) {
                line("{");
                ++indent_;
                line("std::vector<double> _nk_t(" + a.dataExpr + ", " + a.dataExpr + " + "
                     + a.lenVar + ");");
                line("const std::size_t _nk_n = _nk_t.size();");
                line("std::sort(_nk_t.begin(), _nk_t.end(),");
                line("    [](double _a, double _b){ return _a < _b || (_b != _b && _a == _a); });");
                line("if (_nk_n == 0 || _nk_t[_nk_n - 1] != _nk_t[_nk_n - 1])");
                line("    " + name + " = std::numeric_limits<double>::quiet_NaN();");
                line("else if (_nk_n % 2 == 1) " + name + " = _nk_t[_nk_n / 2];");
                line("else " + name + " = (_nk_t[_nk_n / 2 - 1] + _nk_t[_nk_n / 2]) / 2.0;");
                --indent_;
                line("}");
                types_.set(name, res);
                return;
            }
        }
        // Native dot(a,b) -> sum of a[i]*b[i] (real inner product), but ONLY with
        // no bridge (order-dependent like sum -> the bridged path stays exact when
        // the bridge is on). A length-match guard then an accumulation loop. v1:
        // two 1-D DOUBLE array vars. (Mirrors the inner-product `s = r*c` path for
        // the dot() builtin form, which works for any vector orientation.)
        if (!isArrayVar(name) && !bridge_ && rhs.type == NodeType::CALL
            && rhs.children.size() == 3 && rhs.children[0]->type == NodeType::IDENTIFIER
            && rhs.children[0]->strValue == "dot"
            && rhs.children[1]->type == NodeType::IDENTIFIER && isArrayVar(rhs.children[1]->strValue)
            && rhs.children[2]->type == NodeType::IDENTIFIER
            && isArrayVar(rhs.children[2]->strValue)) {
            const ArrayInfo    &A   = arrays_.at(rhs.children[1]->strValue);
            const ArrayInfo    &B   = arrays_.at(rhs.children[2]->strValue);
            const AbstractValue res = inferExpr(rhs, types_, reg_, classes_);
            if (!A.is2D && !A.isND && !B.is2D && !B.isND && A.dtype == ValueType::DOUBLE
                && B.dtype == ValueType::DOUBLE && res.type.isConcrete()
                && res.type.shape.isScalar()) {
                line("if (" + A.lenVar + " != " + B.lenVar
                     + ") throw std::out_of_range(\"numkit: dot inputs must be the same length\");");
                line("{");
                ++indent_;
                line("double _nk_acc = 0.0;");
                open("for (std::size_t _nk_i = 0; _nk_i < " + A.lenVar + "; ++_nk_i)");
                line("_nk_acc += " + A.dataExpr + "[_nk_i] * " + B.dataExpr + "[_nk_i];");
                close();
                line(name + " = _nk_acc;");
                --indent_;
                line("}");
                types_.set(name, res);
                return;
            }
        }
        // Native strcmp(a,b) -> LOGICAL scalar: true iff the two arrays are equal
        // (same length AND elementwise equal). Self-contained; works for char
        // (uint16) and numeric arrays. v1: two 1-D array vars, result assigned.
        if (!isArrayVar(name) && rhs.type == NodeType::CALL && rhs.children.size() == 3
            && rhs.children[0]->type == NodeType::IDENTIFIER
            && rhs.children[0]->strValue == "strcmp"
            && rhs.children[1]->type == NodeType::IDENTIFIER && isArrayVar(rhs.children[1]->strValue)
            && rhs.children[2]->type == NodeType::IDENTIFIER
            && isArrayVar(rhs.children[2]->strValue)) {
            const ArrayInfo    &A   = arrays_.at(rhs.children[1]->strValue);
            const ArrayInfo    &B   = arrays_.at(rhs.children[2]->strValue);
            const AbstractValue res = inferExpr(rhs, types_, reg_, classes_);
            if (!A.is2D && !A.isND && !B.is2D && !B.isND && res.type.isConcrete()
                && res.type.shape.isScalar()) {
                line("{");
                ++indent_;
                line("bool _nk_eq = (" + A.lenVar + " == " + B.lenVar + ");");
                open("if (_nk_eq)");
                open("for (std::size_t _nk_i = 0; _nk_i < " + A.lenVar + "; ++_nk_i)");
                line("if (" + A.dataExpr + "[_nk_i] != " + B.dataExpr
                     + "[_nk_i]) { _nk_eq = false; break; }");
                close();
                close();
                line(name + " = _nk_eq;");
                --indent_;
                line("}");
                types_.set(name, res);
                return;
            }
        }
        // Bridged array reduction -> scalar (opt-in): s = sum(x) / prod / mean /
        // max / min. The array arg can't be a scalar C++ expression, so box it
        // (like the bridged array-RESULT path) and call bridge_scalar_arr —
        // exact runtime result (summation order, NaN). Sound only when inference
        // proves a real (non-complex) scalar result AND an arg is an array (a
        // pure-scalar call uses the bridge_scalar path in emitBuiltinCall).
        if (!isArrayVar(name) && bridge_ && rhs.type == NodeType::CALL && !rhs.children.empty()
            && rhs.children[0]->type == NodeType::IDENTIFIER) {
            const AbstractValue res = inferExpr(rhs, types_, reg_, classes_);
            bool                anyArrayArg = false;
            for (std::size_t i = 1; i < rhs.children.size(); ++i)
                if (rhs.children[i]->type == NodeType::IDENTIFIER
                    && isArrayVar(rhs.children[i]->strValue))
                    anyArrayArg = true;
            if (anyArrayArg && res.type.isUnboxableScalar()
                && res.type.dtype != ValueType::COMPLEX) {
                const std::string callee = rhs.children[0]->strValue;
                const std::size_t nargs  = rhs.children.size() - 1;
                std::string       boxed;
                for (std::size_t i = 1; i < rhs.children.size(); ++i) {
                    const ASTNode &arg = *rhs.children[i];
                    if (i > 1) boxed += ", ";
                    if (arg.type == NodeType::IDENTIFIER && isArrayVar(arg.strValue)) {
                        const ArrayInfo &aai = arrays_.at(arg.strValue);
                        if (aai.is2D || aai.isND)
                            unsupported("bridged reduction: 2-D/N-D array argument (v1)");
                        if (aai.dtype == ValueType::COMPLEX)
                            unsupported("bridged reduction: complex array argument (v1)");
                        boxed += "nk_box_array(" + aai.dataExpr + ", " + aai.lenVar + ")";
                    } else {
                        if (inferExpr(arg, types_, reg_, classes_).type.dtype == ValueType::COMPLEX)
                            unsupported("bridged reduction: complex scalar argument (v1)");
                        boxed += "nk_box_scalar(" + emitExpr(arg) + ")";
                    }
                }
                open("");  // fresh scope for the temporary arg array
                line("nk_val _nk_args[] = { " + boxed + " };");
                line(name + " = nk_rt::bridge_scalar_arr(\"" + callee + "\", _nk_args, "
                     + std::to_string(nargs) + ");");
                close();
                types_.set(name, res);
                return;
            }
        }
        // Native find(m) -> the 1-based positions of the nonzero (true) elements,
        // a runtime-sized 1-D DOUBLE column vector (MATLAB find). A native filter
        // loop pushing (i+1); EXACT (deterministic positions), so preferred over
        // the bridged array-result path below. v1: a single 1-D array arg (double
        // or logical) that is a VARIABLE; the result a 1-D array LOCAL (push_back).
        if (isArrayVar(name) && arrays_.at(name).isLocal && !arrays_.at(name).is2D
            && !arrays_.at(name).isND && rhs.type == NodeType::CALL
            && rhs.children.size() == 2 && rhs.children[0]->type == NodeType::IDENTIFIER
            && rhs.children[0]->strValue == "find"
            && rhs.children[1]->type == NodeType::IDENTIFIER
            && isArrayVar(rhs.children[1]->strValue)) {
            const ArrayInfo    &m   = arrays_.at(rhs.children[1]->strValue);
            const AbstractValue res = inferExpr(rhs, types_, reg_, classes_);
            if (!m.is2D && !m.isND && res.type.isConcrete() && !res.type.shape.isScalar()) {
                line("{");
                ++indent_;
                line(name + ".clear();");
                open("for (std::size_t _nk_i = 0; _nk_i < " + m.lenVar + "; ++_nk_i)");
                line("if (" + m.dataExpr + "[_nk_i]) " + name
                     + ".push_back(static_cast<double>(_nk_i + 1));");
                close();
                --indent_;
                line("}");
                types_.set(name, res);
                return;
            }
        }
        // Native diff(x) -> consecutive differences (y[i] = x[i+1]-x[i], length
        // n-1), a runtime-sized 1-D LOCAL preserving x's dtype. EXACT (subtraction)
        // -> preferred over the bridged array-result path; works for double and
        // complex. v1: a single 1-D array arg that is a VARIABLE; result a LOCAL.
        if (isArrayVar(name) && arrays_.at(name).isLocal && !arrays_.at(name).is2D
            && !arrays_.at(name).isND && rhs.type == NodeType::CALL
            && rhs.children.size() == 2 && rhs.children[0]->type == NodeType::IDENTIFIER
            && rhs.children[0]->strValue == "diff"
            && rhs.children[1]->type == NodeType::IDENTIFIER
            && isArrayVar(rhs.children[1]->strValue)) {
            const ArrayInfo    &xa  = arrays_.at(rhs.children[1]->strValue);
            const AbstractValue res = inferExpr(rhs, types_, reg_, classes_);
            if (!xa.is2D && !xa.isND && res.type.isConcrete() && !res.type.shape.isScalar()) {
                line("{");
                ++indent_;
                line(name + ".clear();");
                open("for (std::size_t _nk_i = 1; _nk_i < " + xa.lenVar + "; ++_nk_i)");
                line(name + ".push_back(" + xa.dataExpr + "[_nk_i] - " + xa.dataExpr
                     + "[_nk_i - 1]);");
                close();
                --indent_;
                line("}");
                types_.set(name, res);
                return;
            }
        }
        // Native cumsum/cumprod(x) -> running accumulation, a SAME-LENGTH 1-D
        // result, but ONLY with no bridge (order-dependent rounding -> the bridged
        // array-result path stays the exact tier when the bridge is on). cumsum:
        // acc += (seed 0); cumprod: acc *= (seed 1). v1: a single 1-D DOUBLE array
        // arg that is a VARIABLE; the result a 1-D array LOCAL (push_back).
        if (isArrayVar(name) && arrays_.at(name).isLocal && !bridge_ && !arrays_.at(name).is2D
            && !arrays_.at(name).isND && rhs.type == NodeType::CALL
            && rhs.children.size() == 2 && rhs.children[0]->type == NodeType::IDENTIFIER
            && (rhs.children[0]->strValue == "cumsum" || rhs.children[0]->strValue == "cumprod")
            && rhs.children[1]->type == NodeType::IDENTIFIER
            && isArrayVar(rhs.children[1]->strValue)
            && arrays_.at(rhs.children[1]->strValue).dtype == ValueType::DOUBLE) {
            const ArrayInfo    &xa  = arrays_.at(rhs.children[1]->strValue);
            const AbstractValue res = inferExpr(rhs, types_, reg_, classes_);
            if (!xa.is2D && !xa.isND && res.type.isConcrete() && !res.type.shape.isScalar()) {
                const bool isProd = rhs.children[0]->strValue == "cumprod";
                line("{");
                ++indent_;
                line(std::string("double _nk_acc = ") + (isProd ? "1.0;" : "0.0;"));
                line(name + ".clear();");
                open("for (std::size_t _nk_i = 0; _nk_i < " + xa.lenVar + "; ++_nk_i)");
                line(std::string("_nk_acc ") + (isProd ? "*=" : "+=") + " " + xa.dataExpr
                     + "[_nk_i];");
                line(name + ".push_back(_nk_acc);");
                close();
                --indent_;
                line("}");
                types_.set(name, res);
                return;
            }
        }
        // Native cummax/cummin(x) -> running max/min, a SAME-LENGTH 1-D LOCAL. Exact
        // (no rounding), so it runs in every tier. The running accumulator uses the
        // single-output max/min NaN logic (update on the first element, on a strict
        // cmp, or when acc is NaN and the candidate isn't -> the first non-NaN seeds
        // it), so NaN is ignored mid-stream but a LEADING NaN is preserved (MATLAB
        // cummax([1 NaN 3]) = [1 1 3]; cummax([NaN 3]) = [NaN 3]). v1: a single 1-D
        // DOUBLE array var; the result a 1-D array LOCAL (push_back).
        if (isArrayVar(name) && arrays_.at(name).isLocal && !arrays_.at(name).is2D
            && !arrays_.at(name).isND && rhs.type == NodeType::CALL && rhs.children.size() == 2
            && rhs.children[0]->type == NodeType::IDENTIFIER
            && (rhs.children[0]->strValue == "cummax" || rhs.children[0]->strValue == "cummin")
            && rhs.children[1]->type == NodeType::IDENTIFIER
            && isArrayVar(rhs.children[1]->strValue)
            && arrays_.at(rhs.children[1]->strValue).dtype == ValueType::DOUBLE) {
            const ArrayInfo    &xa  = arrays_.at(rhs.children[1]->strValue);
            const AbstractValue res = inferExpr(rhs, types_, reg_, classes_);
            if (!xa.is2D && !xa.isND && res.type.isConcrete() && !res.type.shape.isScalar()) {
                const std::string cmp = rhs.children[0]->strValue == "cummax" ? ">" : "<";
                line("{");
                ++indent_;
                line(name + ".clear();");
                line("double _nk_acc = 0.0;");
                open("for (std::size_t _nk_i = 0; _nk_i < " + xa.lenVar + "; ++_nk_i)");
                line("const double _nk_v = " + xa.dataExpr + "[_nk_i];");
                line("if (_nk_i == 0 || _nk_v " + cmp + " _nk_acc"
                     " || (_nk_acc != _nk_acc && _nk_v == _nk_v)) _nk_acc = _nk_v;");
                line(name + ".push_back(_nk_acc);");
                close();
                --indent_;
                line("}");
                types_.set(name, res);
                return;
            }
        }
        // Native flip/fliplr/flipud(x) on a 1-D vector -> a fresh same-length 1-D
        // LOCAL. flip reverses a vector; fliplr reverses along columns (a ROW is
        // reversed, a column is unchanged); flipud reverses along rows (a COLUMN is
        // reversed, a row is unchanged). Whether a reversal happens is decided from
        // the operand's orientation; fliplr/flipud on an ERASED-orientation 1-D
        // buffer can't be decided -> fall through (refuse). dtype-general (a plain
        // element copy). v1: a single 1-D array VAR; the result a 1-D array LOCAL.
        if (isArrayVar(name) && arrays_.at(name).isLocal && !arrays_.at(name).is2D
            && !arrays_.at(name).isND && rhs.type == NodeType::CALL && rhs.children.size() == 2
            && rhs.children[0]->type == NodeType::IDENTIFIER
            && (rhs.children[0]->strValue == "flip" || rhs.children[0]->strValue == "fliplr"
                || rhs.children[0]->strValue == "flipud")
            && rhs.children[1]->type == NodeType::IDENTIFIER
            && isArrayVar(rhs.children[1]->strValue)
            && !arrays_.at(rhs.children[1]->strValue).is2D
            && !arrays_.at(rhs.children[1]->strValue).isND) {
            const ArrayInfo    &xa  = arrays_.at(rhs.children[1]->strValue);
            const std::string  &fn  = rhs.children[0]->strValue;
            const AbstractValue res = inferExpr(rhs, types_, reg_, classes_);
            bool                reverse   = true;
            bool                decidable = true;
            if (fn == "flip")
                reverse = true;  // a vector flip reverses regardless of orientation
            else if (xa.orient == VecOrient::Unknown)
                decidable = false;  // fliplr/flipud need a known orientation
            else if (fn == "fliplr")
                reverse = xa.orient == VecOrient::Row;
            else  // flipud
                reverse = xa.orient == VecOrient::Col;
            if (decidable && res.type.isConcrete() && !res.type.shape.isScalar()) {
                line("{");
                ++indent_;
                line(name + ".assign(" + xa.lenVar + ", " + zeroLiteral(xa.dtype) + ");");
                open("for (std::size_t _nk_i = 0; _nk_i < " + xa.lenVar + "; ++_nk_i)");
                line(name + "[_nk_i] = " + xa.dataExpr + "["
                     + (reverse ? (xa.lenVar + " - 1 - _nk_i") : std::string("_nk_i")) + "];");
                close();
                --indent_;
                line("}");
                types_.set(name, res);
                return;
            }
        }
        // Native sort(x) ascending -> a sorted copy of x in a fresh 1-D LOCAL. The
        // comparator puts NaN last (MATLAB's order) and is a valid strict-weak-
        // ordering (NaN treated as the maximum), so std::sort stays well-defined even
        // with NaNs. Bridged otherwise; native closes it for the standalone tier.
        // v1: a single 1-D DOUBLE array VAR; ascending single-output (sort(x,'descend')
        // and [s,i]=sort are deferred). result a 1-D array LOCAL. `!bridge_` keeps the
        // bridged array-result path as the tier when the bridge is on (sort is exact,
        // but this preserves the existing bridged-sort behaviour); native is the
        // standalone (no-bridge) tier.
        if (isArrayVar(name) && arrays_.at(name).isLocal && !bridge_ && !arrays_.at(name).is2D
            && !arrays_.at(name).isND && rhs.type == NodeType::CALL
            && (rhs.children.size() == 2
                || (rhs.children.size() == 3 && rhs.children[2]->type == NodeType::STRING_LITERAL))
            && rhs.children[0]->type == NodeType::IDENTIFIER && rhs.children[0]->strValue == "sort"
            && rhs.children[1]->type == NodeType::IDENTIFIER
            && isArrayVar(rhs.children[1]->strValue)
            && !arrays_.at(rhs.children[1]->strValue).is2D
            && !arrays_.at(rhs.children[1]->strValue).isND
            && arrays_.at(rhs.children[1]->strValue).dtype == ValueType::DOUBLE) {
            const ArrayInfo    &xa  = arrays_.at(rhs.children[1]->strValue);
            const AbstractValue res = inferExpr(rhs, types_, reg_, classes_);
            // 'descend' reverses the order; MATLAB puts NaN first descending (last
            // ascending). Both comparators are valid strict-weak-orderings (NaN as
            // the extremum), so std::sort stays well-defined with NaNs.
            const bool descend = rhs.children.size() == 3
                                 && rhs.children[2]->strValue == "descend";
            if (res.type.isConcrete() && !res.type.shape.isScalar()) {
                line("{");
                ++indent_;
                line(name + ".assign(" + xa.dataExpr + ", " + xa.dataExpr + " + " + xa.lenVar
                     + ");");
                line("std::sort(" + name + ".begin(), " + name + ".end(),");
                line(descend
                         ? "    [](double _a, double _b){ return _a > _b || (_a != _a && _b == _b); });"
                         : "    [](double _a, double _b){ return _a < _b || (_b != _b && _a == _a); });");
                --indent_;
                line("}");
                types_.set(name, res);
                return;
            }
        }
        // Native upper(s)/lower(s) -> a char-array ASCII case transform (A-Z <-> a-z),
        // a runtime-sized 1-D char LOCAL. Self-contained. v1: a single 1-D CHAR
        // array var; a non-char arg is not handled here (-> bridged/refused).
        if (isArrayVar(name) && arrays_.at(name).isLocal && !arrays_.at(name).is2D
            && !arrays_.at(name).isND && rhs.type == NodeType::CALL
            && rhs.children.size() == 2 && rhs.children[0]->type == NodeType::IDENTIFIER
            && (rhs.children[0]->strValue == "upper" || rhs.children[0]->strValue == "lower")
            && rhs.children[1]->type == NodeType::IDENTIFIER
            && isArrayVar(rhs.children[1]->strValue)
            && arrays_.at(rhs.children[1]->strValue).dtype == ValueType::CHAR) {
            const ArrayInfo &s = arrays_.at(rhs.children[1]->strValue);
            if (!s.is2D && !s.isND) {
                const bool          isUpper = rhs.children[0]->strValue == "upper";
                const AbstractValue rv      = inferExpr(rhs, types_, reg_, classes_);
                line("{");
                ++indent_;
                line(name + ".clear();");
                open("for (std::size_t _nk_i = 0; _nk_i < " + s.lenVar + "; ++_nk_i)");
                line("std::uint16_t _nk_c = " + s.dataExpr + "[_nk_i];");
                line(isUpper ? "if (_nk_c >= 97 && _nk_c <= 122) _nk_c = std::uint16_t(_nk_c - 32);"
                             : "if (_nk_c >= 65 && _nk_c <= 90) _nk_c = std::uint16_t(_nk_c + 32);");
                line(name + ".push_back(_nk_c);");
                close();
                --indent_;
                line("}");
                types_.set(name, rv);
                return;
            }
        }
        // Output array from a BRIDGED call (opt-in): y = sin(x). Sound ONLY
        // when inference proves the RHS is a concrete array (Contract 2); box
        // the (array-var / scalar) args, call the runtime (1 output), and
        // unbox into the caller-allocated out-param. v1: a DOUBLE output.
        if (bridge_ && isArrayVar(name) && !arrays_.at(name).is2D
            && !arrays_.at(name).isND  // 1-D dest only; an N-D bridge needs dim handling
            && (arrays_.at(name).isOutput || arrays_.at(name).isLocal)
            && (arrays_.at(name).dtype == ValueType::DOUBLE
                || arrays_.at(name).dtype == ValueType::COMPLEX)
            && rhs.type == NodeType::CALL && !rhs.children.empty()
            && rhs.children[0]->type == NodeType::IDENTIFIER
            // An elementwise-math call (sin/erf/…) lowers NATIVELY below — only
            // bridge a call the emitter cannot lower (sort, fft, …).
            && unaryMathStd(rhs.children[0]->strValue) == nullptr
            && binaryMathStd(rhs.children[0]->strValue) == nullptr
            // A USER function is compiled, not a runtime builtin — never bridge it
            // (nk_call by name would fail). It returns its 1-D array BY VALUE and
            // is assigned via the scalar tail (`name = <mangled>(args);`).
            && !(ctx_ && ctx_->funcs && !types_.has(rhs.children[0]->strValue)
                 && ctx_->funcs->has(rhs.children[0]->strValue))) {
            const AbstractValue res = inferExpr(rhs, types_, reg_, classes_);
            if (res.type.isConcrete() && !res.type.shape.isScalar()
                && (res.type.dtype == ValueType::DOUBLE
                    || res.type.dtype == ValueType::COMPLEX)) {
                const ArrayInfo  &ai     = arrays_.at(name);
                const std::string callee = rhs.children[0]->strValue;
                const std::size_t nargs  = rhs.children.size() - 1;
                std::string       boxed;
                for (std::size_t i = 1; i < rhs.children.size(); ++i) {
                    const ASTNode &arg = *rhs.children[i];
                    if (i > 1) boxed += ", ";
                    if (arg.type == NodeType::IDENTIFIER && isArrayVar(arg.strValue)) {
                        const ArrayInfo &aai = arrays_.at(arg.strValue);
                        if (aai.is2D || aai.isND)
                            unsupported("bridged call: 2-D/N-D array argument (v1)");
                        // a complex array boxes via the interleaved-re,im C ABI
                        boxed += aai.dtype == ValueType::COMPLEX
                                     ? ("nk_box_complex_array(reinterpret_cast<const double*>("
                                        + aai.dataExpr + "), " + aai.lenVar + ")")
                                     : ("nk_box_array(" + aai.dataExpr + ", " + aai.lenVar + ")");
                    } else {
                        if (inferExpr(arg, types_, reg_, classes_).type.dtype == ValueType::COMPLEX)
                            unsupported("bridged call: complex scalar argument (v1)");
                        boxed += "nk_box_scalar(" + emitExpr(arg) + ")";
                    }
                }
                // A LOCAL resizes its owned vector (bridge_to_vec); the OUTPUT
                // fills its fixed, caller-sized out-param (bridge_into). A COMPLEX
                // result routes to the _cx variants (complex unbox).
                const bool        cx   = res.type.dtype == ValueType::COMPLEX;
                const std::string fn   =
                    ai.isLocal ? (cx ? "nk_rt::bridge_to_vec_cx(\"" : "nk_rt::bridge_to_vec(\"")
                               : (cx ? "nk_rt::bridge_into_cx(\"" : "nk_rt::bridge_into(\"");
                const std::string dest = ai.isLocal ? (", " + name + ");")
                                                    : (", " + name + ", " + ai.lenVar + ");");
                if (nargs == 0) {
                    line(fn + callee + "\", nullptr, 0" + dest);
                } else {
                    open("");  // a fresh scope for the temporary arg array
                    line("nk_val _nk_args[] = { " + boxed + " };");
                    line(fn + callee + "\", _nk_args, " + std::to_string(nargs) + dest);
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
        if (isArrayVar(name)
            && (arrays_.at(name).dtype == ValueType::DOUBLE
                || arrays_.at(name).dtype == ValueType::COMPLEX
                || arrays_.at(name).dtype == ValueType::LOGICAL)) {
            std::set<std::string> srcArrays;
            if (collectElementwise(rhs, srcArrays) && !srcArrays.empty()) {
                const bool dst2D = arrays_.at(name).is2D;
                const bool dstND = arrays_.at(name).isND;
                // The flat per-element loop bounds on numel (column-major
                // storage, so elementwise is rank-agnostic over the flat
                // buffer). Rank discipline: every array operand must match the
                // dest's rank (and, for N-D, its rank count) — no implicit
                // broadcast. The soundness guard below enforces matching shapes
                // PER AXIS (rows/cols for 2-D, every dim for N-D — equal numel is
                // not equal shape); 1-D compares numel. Any number of array
                // operands at each rank. Explicit boundary, never wrong code.
                bool rankMismatch = false;
                for (const std::string &an : srcArrays) {
                    const ArrayInfo &sa = arrays_.at(an);
                    if (sa.is2D != dst2D || sa.isND != dstND
                        || (dstND && sa.ndDims.size() != arrays_.at(name).ndDims.size()))
                        rankMismatch = true;
                }
                const AbstractValue res = inferExpr(rhs, types_, reg_, classes_);
                if (!rankMismatch && res.type.isConcrete()
                    && !res.type.shape.isScalar()
                    && (res.type.dtype == ValueType::DOUBLE
                        || res.type.dtype == ValueType::COMPLEX
                        || res.type.dtype == ValueType::LOGICAL)) {
                    const ArrayInfo &ai = arrays_.at(name);
                    // Loop count = numel; the per-element loop is flat
                    // (column-major, so elementwise is rank-agnostic). SOUNDNESS
                    // guard = every array operand agrees with a reference shape
                    // (the OUTPUT's own dims, else the first operand's). 1-D
                    // compares numel; 2-D compares BOTH dims (equal numel is not
                    // equal shape — 2x3 vs 3x2). MATLAB errors on a mismatch too.
                    std::string bound, guard;
                    if (dst2D) {
                        const ArrayInfo &ref = ai.isLocal ? arrays_.at(*srcArrays.begin()) : ai;
                        bound = "(" + ref.rowsVar + " * " + ref.colsVar + ")";
                        for (const std::string &an : srcArrays) {
                            const ArrayInfo &sa = arrays_.at(an);
                            if (sa.rowsVar == ref.rowsVar && sa.colsVar == ref.colsVar) continue;
                            guard += (guard.empty() ? "" : " || ")
                                     + ("(" + sa.rowsVar + " != " + ref.rowsVar + " || "
                                        + sa.colsVar + " != " + ref.colsVar + ")");
                        }
                    } else if (dstND) {
                        // N-D: numel = product of the per-axis dims; the guard
                        // compares EVERY axis (equal numel is not equal shape —
                        // 2x3x4 vs 4x3x2). ref = the OUTPUT's dims, else the first
                        // operand's. Same-rank is already enforced above.
                        const ArrayInfo &ref = ai.isLocal ? arrays_.at(*srcArrays.begin()) : ai;
                        std::string      prod;
                        for (const std::string &d : ref.ndDims)
                            prod += (prod.empty() ? "" : " * ") + d;
                        bound = "(" + prod + ")";
                        for (const std::string &an : srcArrays) {
                            const ArrayInfo &sa = arrays_.at(an);
                            for (std::size_t k = 0; k < ref.ndDims.size(); ++k) {
                                if (sa.ndDims[k] == ref.ndDims[k]) continue;
                                guard += (guard.empty() ? "" : " || ")
                                         + (sa.ndDims[k] + " != " + ref.ndDims[k]);
                            }
                        }
                    } else {
                        bound = ai.isLocal ? arrays_.at(*srcArrays.begin()).lenVar : ai.lenVar;
                        for (const std::string &an : srcArrays) {
                            const std::string &alen = arrays_.at(an).lenVar;
                            if (alen == bound) continue;  // trivially equal
                            guard += (guard.empty() ? "" : " || ") + (alen + " != " + bound);
                        }
                    }
                    if (!guard.empty())
                        line("if (" + guard
                             + ") throw std::out_of_range(\"numkit: array dimensions must match\");");
                    if (ai.isLocal) line(name + ".resize(" + bound + ");");
                    // Opt-in ops-kernel tier: a SINGLE binary op over exactly
                    // two whole DOUBLE arrays (out = a OP b) maps 1:1 to an ops
                    // SIMD kernel (flat over numel; internal small-N gate). Any
                    // scalar operand, compound/math expression, or complex dtype
                    // has no matching kernel -> the inline fill loop (which is
                    // also the self-contained default when ops kernels are off,
                    // and already auto-vectorises cheap arithmetic — A3).
                    const char *opsFn     = nullptr;  // binary: a OP b
                    const char *opsMathFn = nullptr;  // unary transcendental: fn(x)
                    if (opsKernels_ && ai.dtype == ValueType::DOUBLE) {
                        if (rhs.type == NodeType::BINARY_OP && rhs.children.size() == 2
                            && rhs.children[0]->type == NodeType::IDENTIFIER
                            && isArrayVar(rhs.children[0]->strValue)
                            && rhs.children[1]->type == NodeType::IDENTIFIER
                            && isArrayVar(rhs.children[1]->strValue)) {
                            const std::string &op = rhs.strValue;
                            opsFn = op == "+"    ? "plusDouble"
                                    : op == "-"  ? "minusDouble"
                                    : op == ".*" ? "timesDouble"
                                    : op == "./" ? "rdivideDouble"
                                                 : nullptr;
                        } else if (rhs.type == NodeType::CALL && rhs.children.size() == 2
                                   && rhs.children[0]->type == NodeType::IDENTIFIER
                                   && rhs.children[1]->type == NodeType::IDENTIFIER
                                   && isArrayVar(rhs.children[1]->strValue)) {
                            // fn(x): a single transcendental over a whole array.
                            opsMathFn = opsTranscendentalFn(rhs.children[0]->strValue);
                        }
                    }
                    if (opsFn) {
                        line("numkit::ops::" + std::string(opsFn) + "("
                             + arrays_.at(rhs.children[0]->strValue).dataExpr + ", "
                             + arrays_.at(rhs.children[1]->strValue).dataExpr + ", " + ai.dataExpr
                             + ", " + bound + ");");
                    } else if (opsMathFn) {
                        line("numkit::ops::" + std::string(opsMathFn) + "("
                             + arrays_.at(rhs.children[1]->strValue).dataExpr + ", " + ai.dataExpr
                             + ", " + bound + ");");
                    } else {
                        elementCtx_               = "_nk_i";
                        const std::string rhsExpr = emitExpr(rhs);  // whole arrays -> [_nk_i]
                        elementCtx_.clear();
                        open("for (std::size_t _nk_i = 0; _nk_i < " + bound + "; ++_nk_i)");
                        line(ai.dataExpr + "[_nk_i] = " + rhsExpr + ";");
                        close();
                    }
                    // A 2-D / N-D dest keeps its true dims (res); a 1-D dest a
                    // row stand-in (arrays_ drives indexing/queries either way).
                    if (dst2D || dstND)
                        types_.set(name, res);
                    else
                        types_.set(name, {InferredType::concrete(ai.dtype, Shape::rowVector()),
                                          ConstVal::unknown()});
                    return;
                }
            }
        }

        // Interproc 1-D array RETURN (typed): `arrLocal = g(args)` where g is a
        // compiled user function returning a 1-D array. The callee returns an
        // owned std::vector<double> (interprocByValueReturn); the array LOCAL is a
        // std::vector too, so this is a move/copy-assign. emitExpr routes the CALL
        // to emitUserCall (queues g's specialisation; allows the 1-D array result).
        // Only a LOCAL dest (the output out-param can't take a vector — that
        // vector->buffer copy is a later step; sound refusal below otherwise).
        if (isArrayVar(name) && arrays_.at(name).isLocal && rhs.type == NodeType::CALL
            && !rhs.children.empty() && rhs.children[0]->type == NodeType::IDENTIFIER
            && ctx_ && ctx_->funcs && !types_.has(rhs.children[0]->strValue)
            && ctx_->funcs->has(rhs.children[0]->strValue)) {
            const AbstractValue rv = inferExpr(rhs, types_, reg_, classes_);
            line(name + " = " + emitExpr(rhs) + ";");  // std::vector<double> = g_spec(args)
            types_.set(name, rv);
            return;
        }

        // Interproc 1-D array RETURN into the OUTPUT out-param (P1.5): like the
        // LOCAL case but the dest is the caller-allocated buffer (T* + _len), so
        // copy the callee's self-describing vector into it, bounded by the
        // caller's _len — the same size contract the zeros-fill output already
        // trusts (the caller allocates the true output length). 1-D only; 2-D/N-D
        // results are refused upstream in emitUserCall (need dims with the buffer).
        if (isArrayVar(name) && arrays_.at(name).isOutput && !arrays_.at(name).is2D
            && !arrays_.at(name).isND && rhs.type == NodeType::CALL
            && !rhs.children.empty() && rhs.children[0]->type == NodeType::IDENTIFIER
            && ctx_ && ctx_->funcs && !types_.has(rhs.children[0]->strValue)
            && ctx_->funcs->has(rhs.children[0]->strValue)) {
            const AbstractValue rv = inferExpr(rhs, types_, reg_, classes_);
            const ArrayInfo    &ai = arrays_.at(name);
            line("{");
            ++indent_;
            line("const std::vector<" + cppScalarType(ai.dtype) + "> _nk_ret = "
                 + emitExpr(rhs) + ";");
            line("const std::size_t _nk_n = _nk_ret.size() < (std::size_t)(" + ai.lenVar
                 + ") ? _nk_ret.size() : (std::size_t)(" + ai.lenVar + ");");
            open("for (std::size_t _nk_i = 0; _nk_i < _nk_n; ++_nk_i)");
            line(ai.dataExpr + "[_nk_i] = _nk_ret[_nk_i];");
            close();
            --indent_;
            line("}");
            types_.set(name, rv);
            return;
        }

        // LOGICAL-INDEXING READ: `y = x(m)` where x is an array var and m a LOGICAL
        // array (mask). Build a runtime-sized result by FILTERING x — the elements
        // of x where m is true (MATLAB x(logical)). v1: a single LOGICAL-array
        // subscript that is itself a VARIABLE (an inline mask would need
        // materialising first); x and m both 1-D; y a 1-D array LOCAL (push_back).
        // Bound on min(len) so a too-long mask cannot read x out of bounds.
        if (isArrayVar(name) && arrays_.at(name).isLocal && !arrays_.at(name).is2D
            && !arrays_.at(name).isND && rhs.type == NodeType::CALL
            && rhs.children.size() == 2 && rhs.children[0]->type == NodeType::IDENTIFIER
            && isArrayVar(rhs.children[0]->strValue)
            && rhs.children[1]->type == NodeType::IDENTIFIER
            && isArrayVar(rhs.children[1]->strValue)
            && arrays_.at(rhs.children[1]->strValue).dtype == ValueType::LOGICAL) {
            const ArrayInfo &bx = arrays_.at(rhs.children[0]->strValue);  // x (source)
            const ArrayInfo &bm = arrays_.at(rhs.children[1]->strValue);  // m (mask)
            if (bx.is2D || bx.isND || bm.is2D || bm.isND)
                unsupported("logical indexing: a 1-D array + a 1-D mask only (v1)");
            line("{");
            ++indent_;
            line(name + ".clear();");
            line("const std::size_t _nk_n = " + bm.lenVar + " < " + bx.lenVar + " ? "
                 + bm.lenVar + " : " + bx.lenVar + ";");
            open("for (std::size_t _nk_i = 0; _nk_i < _nk_n; ++_nk_i)");
            line("if (" + bm.dataExpr + "[_nk_i]) " + name + ".push_back("
                 + bx.dataExpr + "[_nk_i]);");
            close();
            --indent_;
            line("}");
            types_.set(name, {InferredType::concrete(bx.dtype, Shape::rowVector()),
                              ConstVal::unknown()});
            return;
        }

        // CHAR row-vector literal: `c = 'abc'` -> a char-array LOCAL initialised
        // from the literal's code units (1 per element; v1: ASCII/BMP, so a UTF-8
        // byte == the UTF-16 unit, matching the inference's byte-count length). char
        // is a uint16 buffer (cppArrayElemType). A 1x1 char ('a') is a Scalar ->
        // not handled here (char scalars deferred).
        if (isArrayVar(name) && arrays_.at(name).isLocal
            && arrays_.at(name).dtype == ValueType::CHAR
            && rhs.type == NodeType::STRING_LITERAL) {
            std::string codes;
            for (unsigned char ch : rhs.strValue)
                codes += (codes.empty() ? "" : ", ")
                         + ("std::uint16_t(" + std::to_string(static_cast<unsigned>(ch)) + ")");
            line(name + ".assign({" + codes + "});");
            types_.set(name, inferExpr(rhs, types_, reg_, classes_));
            return;
        }

        // 1-D horzcat: `r = [a b ...]` (a single row of 1-D array operands of one
        // dtype) -> a runtime-sized 1-D array LOCAL built by appending each operand
        // in order. Works for char (uint16) + numeric. v1: every element is an
        // array VARIABLE; r a 1-D LOCAL. (Mixed-dtype / scalar element / multi-row
        // give a Dynamic or non-buffer inference -> skipped here.)
        if (isArrayVar(name) && arrays_.at(name).isLocal && !arrays_.at(name).is2D
            && !arrays_.at(name).isND && rhs.type == NodeType::MATRIX_LITERAL
            && rhs.children.size() == 1 && rhs.children[0]
            && !rhs.children[0]->children.empty()) {
            const AbstractValue rv = inferExpr(rhs, types_, reg_, classes_);
            if (rv.type.isConcrete() && !rv.type.shape.isScalar()) {
                const ASTNode &row = *rhs.children[0];
                line("{");
                ++indent_;
                line(name + ".clear();");
                for (const auto &el : row.children) {
                    if (el->type == NodeType::IDENTIFIER && isArrayVar(el->strValue)) {
                        const ArrayInfo &op = arrays_.at(el->strValue);  // append a 1-D array
                        open("for (std::size_t _nk_i = 0; _nk_i < " + op.lenVar + "; ++_nk_i)");
                        line(name + ".push_back(" + op.dataExpr + "[_nk_i]);");
                        close();
                    } else if (el->type == NodeType::STRING_LITERAL) {
                        for (unsigned char ch : el->strValue)  // append a char literal's units
                            line(name + ".push_back(std::uint16_t("
                                 + std::to_string(static_cast<unsigned>(ch)) + "));");
                    } else if (inferExpr(*el, types_, reg_, classes_).type.shape.isScalar()) {
                        line(name + ".push_back(" + emitExpr(*el) + ");");  // append one scalar
                    } else {
                        unsupported("horzcat element (v1: an array var, char/string literal, "
                                    "or scalar)");
                    }
                }
                --indent_;
                line("}");
                types_.set(name, rv);
                return;
            }
        }

        // Whole-array struct-field READ: `y = s.v` where s.v is an array field-local
        // (field-flattening). emitExpr(s.v) yields the field-local vector name, so
        // this is a vector copy-assign. v1: a 1-D array field; y a 1-D array LOCAL.
        if (isArrayVar(name) && arrays_.at(name).isLocal && rhs.type == NodeType::FIELD_ACCESS
            && !rhs.children.empty() && rhs.children[0]->type == NodeType::IDENTIFIER) {
            const AbstractValue rbase = inferExpr(*rhs.children[0], types_, reg_, classes_);
            const AbstractValue rv    = inferExpr(rhs, types_, reg_, classes_);
            if (!rbase.type.isObject() && rv.type.isConcrete() && !rv.type.shape.isScalar()
                && !is2DMatrixType(rv.type) && !rv.type.shape.isNDims()) {
                line(name + " = " + emitExpr(rhs) + ";");  // vector copy from the field-local
                types_.set(name, rv);
                return;
            }
        }

        if (isArrayVar(name))
            unsupported("array-valued assignment to '" + name
                        + "' (only size-constructor init of the output in RawBuffer ABI)");

        // Scalar assignment (the local was hoisted at function entry).
        const AbstractValue rv = inferExpr(rhs, types_, reg_, classes_);
        // Dynamic tier (DESIGN.md §10 C1): the RHS type could not be inferred —
        // keep it boxed as an nk_rt::val (the local was hoisted as one) and
        // dispatch its operations to the runtime. The universal sound fallback;
        // needs the C-ABI, so only under bridge_ (else the typed path throws).
        // A Dynamic-hoisted local (nk_val) keeps EVERY assignment boxed: there is
        // no nk_val = <typed> conversion, and its later reads must stay Dynamic so
        // they route through the runtime. Box when the RHS is Dynamic OR the LHS
        // local was hoisted Dynamic (from markAssignedDynamic — e.g. a try/catch
        // body); the local then stays Dynamic in the flow env.
        const bool lhsDynamic = dynamicLocals_.count(name) != 0;
        if (bridge_ && (rv.type.isDynamic() || lhsDynamic)) {
            line(name + " = " + emitDynamicExpr(rhs) + ";");
            types_.set(name, lhsDynamic ? AbstractValue::dynamic() : rv);
            return;
        }
        // numel/length return a count; assigning into a double local needs
        // no cast (the builtin emitter already produced a double). But the
        // companion length is size_t, so the cast is inside emitBuiltinCall.
        line(name + " = " + emitExpr(rhs) + ";");
        types_.set(name, rv);
        return;
    }

    if (lhs.type == NodeType::FIELD_ACCESS) {  // obj.field = rhs
        if (lhs.children.empty()) unsupported("field write arity");
        const AbstractValue base = inferExpr(*lhs.children[0], types_, reg_, classes_);
        // Plain struct: flatten `s.f = rhs` to the synthesized scalar field-local
        // (field-flattening; no struct type). v1: a scalar (unboxable) field.
        if (!base.type.isObject() && lhs.children[0]->type == NodeType::IDENTIFIER) {
            const std::string   fld = "_nk_fld_" + lhs.children[0]->strValue + "_" + lhs.strValue;
            const AbstractValue rv  = inferExpr(rhs, types_, reg_, classes_);
            if (rv.type.isUnboxableScalar()) {
                line(fld + " = " + emitExpr(rhs) + ";");  // scalar field
                types_.set(fld, rv);
                return;
            }
            // Array field `s.v = x` (x a 1-D array VAR): the field-local vector
            // (hoisted from the inferred array decl) copies x's elements.
            if (rv.type.isConcrete() && !rv.type.shape.isScalar() && !is2DMatrixType(rv.type)
                && !rv.type.shape.isNDims() && rhs.type == NodeType::IDENTIFIER
                && isArrayVar(rhs.strValue)) {
                const ArrayInfo &x = arrays_.at(rhs.strValue);
                line(fld + ".assign(" + x.dataExpr + ", " + x.dataExpr + " + " + x.lenVar + ");");
                types_.set(fld, rv);
                return;
            }
            unsupported("struct field write (v1: a scalar, or a 1-D array var, field)");
        }
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

    // [r, c] = size(A): native two-output size. r = axis 0; c = the remaining
    // axes folded (cols for a matrix, the trailing-dim product for N-D) -- so
    // [r,c]=size(rand(2,3,4)) gives r=2, c=12, matching MATLAB. Both targets
    // are real scalars. BEFORE the user-function path so it lowers natively
    // instead of bridging into numkit::size. v1 is exactly this two-output
    // idiom; any other nargout (or a user-shadowed `size`) falls through.
    if (rhs.type == NodeType::CALL && rhs.children.size() == 2
        && rhs.children[0]->type == NodeType::IDENTIFIER
        && rhs.children[0]->strValue == "size"
        && rhs.children[1]->type == NodeType::IDENTIFIER
        && isArrayVar(rhs.children[1]->strValue) && s.returnNames.size() == 2
        && !(ctx_ && ctx_->funcs && ctx_->funcs->has("size"))) {
        const std::string &rn0 = s.returnNames[0];
        const std::string &rn1 = s.returnNames[1];
        if (rn0.empty() || rn0 == "~" || rn1.empty() || rn1 == "~")
            unsupported("[r,c]=size with an ignored (~) output (v1)");
        if (isArrayVar(rn0) || isArrayVar(rn1))
            unsupported("[r,c]=size into a non-scalar target");
        const ArrayInfo &aai = arrays_.at(rhs.children[1]->strValue);
        if (!aai.is2D && !aai.isND && aai.orient == VecOrient::Unknown)
            unsupported("[r,c]=size of an orientation-unknown vector");
        const std::size_t rank = aai.isND ? aai.ndDims.size() : 2;
        std::string       cols = dimExpr(aai, 1);  // fold dims 1..rank-1 into c
        for (std::size_t k = 2; k < rank; ++k) cols += " * " + dimExpr(aai, k);
        line(rn0 + " = static_cast<double>(" + dimExpr(aai, 0) + ");");
        line(rn1 + " = static_cast<double>(" + cols + ");");
        types_.set(rn0, {InferredType::scalar(ValueType::DOUBLE), ConstVal::unknown()});
        types_.set(rn1, {InferredType::scalar(ValueType::DOUBLE), ConstVal::unknown()});
        return;
    }
    // Bridged builtin multi-output (opt-in, DESIGN.md §10 C1): `[a, b, ...] =
    // builtin(args)` where builtin is NOT a compiled user function (nor a
    // variable). The runtime owns nargout and computes all outputs; each result
    // is kept BOXED as a Dynamic nk_rt::val (sound — no per-builtin type
    // assumption; reuses the Dynamic tier). Targets are Dynamic locals (the
    // inference types them so); a ~ slot discards its output. v1: 2+ outputs;
    // args boxed by emitDynamicExpr (scalar / 1-D/2-D/N-D array / Dynamic).
    if (bridge_ && rhs.type == NodeType::CALL && !rhs.children.empty()
        && rhs.children[0]->type == NodeType::IDENTIFIER && s.returnNames.size() >= 2
        && !types_.has(rhs.children[0]->strValue)  // not a variable (would be indexing)
        && !(ctx_ && ctx_->funcs && ctx_->funcs->has(rhs.children[0]->strValue))) {  // not a user fn
        const std::string callee = rhs.children[0]->strValue;
        const std::size_t nout   = s.returnNames.size();
        std::string       boxedArgs;
        for (std::size_t i = 1; i < rhs.children.size(); ++i)
            boxedArgs += (i > 1 ? ", " : "") + emitDynamicExpr(*rhs.children[i]);
        const std::string call = "nk_rt::call_dyn_multi(\"" + callee + "\", {" + boxedArgs + "}, "
                                 + std::to_string(nout) + ", _nk_mo)";
        open("");  // scope for the extra-outputs array (outputs 1..nout-1)
        line("nk_rt::val _nk_mo[" + std::to_string(nout - 1) + "];");
        const std::string &t0 = s.returnNames[0];  // output 0 = call_dyn_multi's return
        if (t0.empty() || t0 == "~") {
            line(call + ";");  // discarded
        } else {
            line(t0 + " = " + call + ";");
            types_.set(t0, {InferredType::dynamic(), ConstVal::unknown()});
        }
        for (std::size_t k = 1; k < nout; ++k) {  // outputs 1.. = the extras
            const std::string &tk = s.returnNames[k];
            if (tk.empty() || tk == "~") continue;  // discarded (released at block close)
            line(tk + " = std::move(_nk_mo[" + std::to_string(k - 1) + "]);");
            types_.set(tk, {InferredType::dynamic(), ConstVal::unknown()});
        }
        close();
        return;
    }
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
    // Output 0 may be returned BY VALUE — a leading 1-D array (the callee's ABI
    // mirror in emitOneFunction). Then target 0 takes the call's RETURN value and
    // is NOT a reference out-arg; outputs 1.. stay reference out-args. Otherwise
    // every output is a reference out-arg (the all-scalar multi-output case).
    const InferredType out0 = !outs.empty() ? outs[0] : InferredType::dynamic();
    const bool         out0ByValue = isByValueReturnArrayType(out0);
    // An ignored (~) output still has a reference out-param the callee writes, so
    // it gets a throwaway local. The throwaways are scoped in a fresh block so
    // repeated `[~,...] = f()` statements never collide on the throwaway name.
    std::vector<std::string> ignoreDecls;
    for (std::size_t i = (out0ByValue ? 1 : 0); i < s.returnNames.size(); ++i) {
        const std::string &rn = s.returnNames[i];
        const InferredType  ot = i < outs.size() ? outs[i] : InferredType::dynamic();
        if (!argList.empty()) argList += ", ";
        if (rn.empty() || rn == "~") {
            if (!isUnboxableScalarType(ot))
                unsupported("ignored (~) multi-output must be a scalar output (v1): '" + name + "'");
            const std::string tmp = "_nk_ignore_" + std::to_string(i);
            ignoreDecls.push_back(cppScalarType(ot.dtype) + " " + tmp + " = "
                                  + zeroLiteral(ot.dtype) + ";");
            argList += tmp;
        } else {
            argList += rn;  // out-arg, bound to the callee's reference out-param
            types_.set(rn, {ot, ConstVal::unknown()});
        }
    }

    const std::string mangled = mangle(name, argTypes);
    if (ctx_->seen.insert(mangled).second)
        ctx_->pending.push_back({def, argTypes, mangled});
    // With a by-value output 0, target 0 takes the RETURN (an array LOCAL —
    // std::vector); a ~ slot discards the returned vector.
    std::string callStmt;
    if (out0ByValue && !(s.returnNames[0].empty() || s.returnNames[0] == "~")) {
        callStmt = s.returnNames[0] + " = " + mangled + "(" + argList + ");";
        types_.set(s.returnNames[0], {out0, ConstVal::unknown()});
    } else {
        callStmt = mangled + "(" + argList + ");";  // all-ref outputs, or out0 discarded
    }
    if (ignoreDecls.empty()) {
        line(callStmt);
    } else {
        open("");  // fresh scope for the ~ throwaway locals
        for (const std::string &d : ignoreDecls) line(d);
        line(callStmt);
        close();
    }
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

    open("while (" + emitCondition(*s.children[0]) + ")");
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
        const std::string cond = emitCondition(*s.branches[i].first);
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

void Emitter::emitSwitch(const ASTNode &s)
{
    // MATLAB switch: evaluate the selector ONCE, then take the first case whose
    // value — or ANY element of a `case {a,b}` cell-list — equals it (isequal),
    // else `otherwise`. Lowered to an if-else chain over a selector temp. v1: an
    // unboxed-scalar selector + unboxed-scalar case values, so isequal reduces to
    // `==` (NaN matches nothing, as in MATLAB). A string/array selector or case
    // value (full isequal), or a 2-D cell case-list, is refused.
    if (s.children.empty()) unsupported("switch with no selector expression");
    const ASTNode      &sel  = *s.children[0];
    const AbstractValue selv = inferExpr(sel, types_, reg_, classes_);
    if (!isUnboxableScalarType(selv.type))
        unsupported("switch selector must be an unboxed scalar (v1)");
    const std::string tmp = "_nk_switch" + std::to_string(switchCounter_++);

    const TypeEnv entry = types_;
    TypeEnv       merged;
    bool          have = false;
    auto mergeIn = [&](const TypeEnv &e) { merged = have ? joinEnv(merged, e) : e; have = true; };

    open("");  // scope the selector temp (no leak / nested-switch name collision)
    line("const " + cppScalarType(selv.type.dtype) + " " + tmp + " = " + emitExpr(sel) + ";");

    auto eqTerm = [&](const ASTNode &v) -> std::string {
        const AbstractValue vv = inferExpr(v, types_, reg_, classes_);
        if (!isUnboxableScalarType(vv.type))
            unsupported("switch case value must be an unboxed scalar (v1)");
        return "(" + tmp + " == " + emitExpr(v) + ")";
    };

    for (std::size_t i = 0; i < s.branches.size(); ++i) {
        types_ = entry;  // each case body typed from the incoming env
        const ASTNode &caseVal = *s.branches[i].first;
        std::string    cond;
        if (caseVal.type == NodeType::CELL_LITERAL) {
            // `case {a,b,...}` matches the selector against ANY element. A cell
            // literal stores each row as a BLOCK child (a 1-row cell -> one row
            // block), so descend a row block to reach the elements; flatten all
            // rows (matching any element is rank-agnostic).
            auto addElem = [&](const ASTNode &el) {
                cond += (cond.empty() ? "" : " || ") + eqTerm(el);
            };
            for (const auto &child : caseVal.children) {
                if (!child) continue;
                if (child->type == NodeType::BLOCK)
                    for (const auto &el : child->children) { if (el) addElem(*el); }
                else
                    addElem(*child);
            }
            if (cond.empty()) cond = "false";  // `case {}` matches nothing
        } else {
            cond = eqTerm(caseVal);
        }
        open((i == 0 ? "if (" : "else if (") + cond + ")");
        if (s.branches[i].second) emitStmt(*s.branches[i].second);
        close();
        mergeIn(types_);
    }
    if (s.elseBranch) {
        types_ = entry;
        open(s.branches.empty() ? "" : "else");  // otherwise; a bare block if no cases
        emitStmt(*s.elseBranch);
        close();
        mergeIn(types_);
    } else {
        mergeIn(entry);  // no case matched — fall-through
    }
    close();  // selector-temp scope
    types_ = have ? merged : entry;
}

void Emitter::emitTry(const ASTNode &s)
{
    // try/catch -> C++ `try { <body> } catch (...) { <handler> }`. A numkit runtime
    // error surfaces as a C++ exception, so `catch (...)` catches it. The inference
    // marks try/catch-assigned vars Dynamic (a throw mid-try leaves them uncertain
    // — sound over-approximation), so the bodies ride the Dynamic tier (bridge).
    // v1: the catch's bound exception variable (MATLAB binds an MException object)
    // is NOT represented -> refuse a `catch err` form; a bare `catch`, or
    // `try ... end` with no catch (which silently swallows), is supported.
    if (s.children.empty()) unsupported("try with no body");
    if (!s.strValue.empty())
        unsupported("try/catch with a bound exception variable '" + s.strValue
                    + "' (MException object not represented; v1)");

    const TypeEnv entry = types_;
    TypeEnv       merged;
    bool          have = false;
    auto mergeIn = [&](const TypeEnv &e) { merged = have ? joinEnv(merged, e) : e; have = true; };

    open("try");
    emitStmt(*s.children[0]);  // try body
    close();
    mergeIn(types_);  // try ran to completion
    mergeIn(entry);   // try threw before assigning -> the incoming types

    types_ = entry;   // catch body typed from the incoming env
    open("catch (...)");
    if (s.children.size() > 1 && s.children[1]) emitStmt(*s.children[1]);
    close();
    mergeIn(types_);

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
    case NodeType::IF_STMT:     emitIf(s);     return;
    case NodeType::SWITCH_STMT: emitSwitch(s); return;
    case NodeType::TRY_STMT:    emitTry(s);    return;
    // break / continue lower directly to the C++ loop-control keywords. They only
    // ever appear inside a loop body (MATLAB semantics + the parser enforce it,
    // and the emitted code mirrors that structure), so the keyword lands inside
    // the corresponding emitted for/while. No effect on the index/bound analyses
    // (those are per-expression, not control-flow sensitive); an early exit just
    // leaves arrays filled up to the exit point, matching MATLAB.
    case NodeType::BREAK_STMT:    line("break;");    return;
    case NodeType::CONTINUE_STMT: line("continue;"); return;
    case NodeType::RETURN_STMT:
        // Early return -> the SAME form as the end-of-function return (set by
        // emitOneFunction via setReturnInfo). A value function returns its output
        // var (boxed via .take() for a Dynamic result); a void / out-param /
        // multi-ref function emits a bare `return;`.
        if (!returnsValue_)      line("return;");
        else if (returnDynamic_) emitReturnDynamic(returnName_);
        else                     emitReturnScalar(returnName_);
        return;
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
    "#include <algorithm>\n"
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
    "// Convert a (double) array dimension to size_t. A negative dimension is a\n"
    "// hard error (the interpreter errors on negative dims too); the check\n"
    "// PRECEDES the cast so a negative value never reaches the float->unsigned\n"
    "// conversion (which would be UB). A fractional dim truncates toward zero, as\n"
    "// the interpreter does.\n"
    "inline std::size_t dim(double d) {\n"
    "    // Mirrors the interpreter's ops::toDim (MATLAB parity, post fix/zeros-size-args):\n"
    "    // non-integer/non-finite -> error; a NEGATIVE dim clamps to 0 (zeros(-1,3) ->\n"
    "    // 0x3 empty); a size beyond size_t -> error. Guards PRECEDE the cast (a neg/\n"
    "    // non-finite/huge value would be UB to cast). Keeps codegen == the interpreter.\n"
    "    if (!std::isfinite(d) || d != std::floor(d))\n"
    "        throw std::runtime_error(\"Size inputs must be integers.\");\n"
    "    if (d < 0.0) return std::size_t{0};\n"
    "    if (d >= static_cast<double>(std::numeric_limits<std::size_t>::max()))\n"
    "        throw std::runtime_error(\"Requested array size is too large.\");\n"
    "    return static_cast<std::size_t>(d);\n"
    "}\n"
    "template <class T>\n"  // all index helpers check `< 1.0` BEFORE the cast: a
    "inline T index(const T* a, std::size_t len, double idx1) {\n"  // negative idx
    "    if (idx1 < 1.0) throw std::out_of_range(\"numkit: index out of bounds\");\n"  // never UB-casts
    "    const std::size_t i = static_cast<std::size_t>(idx1);\n"
    "    if (i > len) throw std::out_of_range(\"numkit: index out of bounds\");\n"
    "    return a[i - 1];\n"
    "}\n"
    "template <class T>\n"
    "inline void index_set(T* a, std::size_t len, double idx1, T v) {\n"
    "    if (idx1 < 1.0)\n"
    "        throw std::out_of_range(\"numkit: index out of bounds (RawBuffer ABI cannot grow)\");\n"
    "    const std::size_t i = static_cast<std::size_t>(idx1);\n"
    "    if (i > len)\n"
    "        throw std::out_of_range(\"numkit: index out of bounds (RawBuffer ABI cannot grow)\");\n"
    "    a[i - 1] = v;\n"
    "}\n"
    "template <class T>\n"  // A(i,j) read, column-major; 2-D writes are a later step
    "inline T index2(const T* a, std::size_t rows, std::size_t cols, double i1, double j1) {\n"
    "    if (i1 < 1.0 || j1 < 1.0) throw std::out_of_range(\"numkit: 2-D index out of bounds\");\n"
    "    const std::size_t i = static_cast<std::size_t>(i1), j = static_cast<std::size_t>(j1);\n"
    "    if (i > rows || j > cols) throw std::out_of_range(\"numkit: 2-D index out of bounds\");\n"
    "    return a[(j - 1) * rows + (i - 1)];\n"
    "}\n"
    "template <class T>\n"  // A(i,j) = v write, column-major (mutable 2-D: local/output)
    "inline void index2_set(T* a, std::size_t rows, std::size_t cols, double i1, double j1, T v) {\n"
    "    if (i1 < 1.0 || j1 < 1.0) throw std::out_of_range(\"numkit: 2-D index out of bounds\");\n"
    "    const std::size_t i = static_cast<std::size_t>(i1), j = static_cast<std::size_t>(j1);\n"
    "    if (i > rows || j > cols) throw std::out_of_range(\"numkit: 2-D index out of bounds\");\n"
    "    a[(j - 1) * rows + (i - 1)] = v;\n"
    "}\n"
    "// N-D (rank>=3) column-major linear offset from 1-based subscripts + dims;\n"
    "// bounds-checked per axis. dims and subs are parallel (same rank).\n"
    "inline std::size_t nd_off(std::initializer_list<std::size_t> dims,\n"
    "                          std::initializer_list<double> subs) {\n"
    "    const std::size_t* d = dims.begin(); const double* s = subs.begin();\n"
    "    std::size_t off = 0, stride = 1;\n"
    "    for (std::size_t _nk_a = 0; _nk_a < dims.size(); ++_nk_a) {\n"
    "        if (s[_nk_a] < 1.0) throw std::out_of_range(\"numkit: N-D index out of bounds\");\n"
    "        const std::size_t ik = static_cast<std::size_t>(s[_nk_a]);\n"
    "        if (ik > d[_nk_a]) throw std::out_of_range(\"numkit: N-D index out of bounds\");\n"
    "        off += (ik - 1) * stride; stride *= d[_nk_a];\n"
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
           "#include <complex>\n"
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
           "// Pre-boxed args (incl. arrays) -> call (1 output) -> unbox a SCALAR\n"
           "// result; releases args + result; errors throw. For array-arg\n"
           "// reductions (sum, prod, mean, max, min) whose arg can't be a scalar\n"
           "// C++ expression. Mirrors bridge_into but the result is a scalar.\n"
           "inline double bridge_scalar_arr(const char* name, nk_val* args, std::size_t nargs) {\n"
           "    nk_error err; err.code = 0;\n"
           "    nk_val r = nk_call(name, args, nargs, 1, nullptr, &err);\n"
           "    for (std::size_t i = 0; i < nargs; ++i) nk_release(args[i]);\n"
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
           "// Complex-result variants: unbox a complex array result via the\n"
           "// interleaved-re,im C ABI (reinterpret the std::complex buffer).\n"
           "inline void bridge_into_cx(const char* name, nk_val* args, std::size_t nargs,\n"
           "                           std::complex<double>* out, std::size_t out_len) {\n"
           "    nk_error err; err.code = 0;\n"
           "    nk_val r = nk_call(name, args, nargs, 1, nullptr, &err);\n"
           "    for (std::size_t i = 0; i < nargs; ++i) nk_release(args[i]);\n"
           "    if (!r || err.code) { nk_release(r);\n"
           "        throw std::runtime_error(err.code ? err.message : \"numkit bridged call failed\"); }\n"
           "    nk_unbox_complex_array(r, reinterpret_cast<double*>(out), out_len); nk_release(r);\n"
           "}\n"
           "inline void bridge_to_vec_cx(const char* name, nk_val* args, std::size_t nargs,\n"
           "                             std::vector<std::complex<double>>& out) {\n"
           "    nk_error err; err.code = 0;\n"
           "    nk_val r = nk_call(name, args, nargs, 1, nullptr, &err);\n"
           "    for (std::size_t i = 0; i < nargs; ++i) nk_release(args[i]);\n"
           "    if (!r || err.code) { nk_release(r);\n"
           "        throw std::runtime_error(err.code ? err.message : \"numkit bridged call failed\"); }\n"
           "    out.resize(nk_numel(r));\n"
           "    nk_unbox_complex_array(r, reinterpret_cast<double*>(out.data()), out.size()); nk_release(r);\n"
           "}\n"
           "// ---- Dynamic tier (DESIGN.md §10 C1) ----------------------------------\n"
           "// A value whose type the codegen could not infer: held BOXED as an owned\n"
           "// runtime handle (RAII), its operations dispatched to the runtime — the\n"
           "// universal sound fallback when no typed inline form exists. Copyable via\n"
           "// deep clone (MATLAB value semantics); errors throw (never across the C ABI).\n"
           "class val {\n"
           "    nk_val h_ = nullptr;\n"
           "public:\n"
           "    val() = default;\n"
           "    explicit val(nk_val h) : h_(h) {}\n"
           "    ~val() { nk_release(h_); }\n"
           "    val(const val& o) : h_(nk_clone(o.h_)) {}\n"
           "    val(val&& o) noexcept : h_(o.h_) { o.h_ = nullptr; }\n"
           "    val& operator=(const val& o) {\n"
           "        if (this != &o) { nk_val n = nk_clone(o.h_); nk_release(h_); h_ = n; } return *this; }\n"
           "    val& operator=(val&& o) noexcept {\n"
           "        if (this != &o) { nk_release(h_); h_ = o.h_; o.h_ = nullptr; } return *this; }\n"
           "    nk_val get() const { return h_; }\n"
           "    nk_val take() { nk_val h = h_; h_ = nullptr; return h; }  // release ownership out\n"
           "    static val scalar(double x) { return val(nk_box_scalar(x)); }\n"
           "    static val array(const double* p, std::size_t n) { return val(nk_box_array(p, n)); }\n"
           "    static val matrix(const double* p, std::size_t r, std::size_t c) {\n"
           "        return val(nk_box_matrix(p, r, c)); }\n"
           "    static val array_nd(const double* p, std::initializer_list<std::size_t> dims) {\n"
           "        return val(nk_box_array_nd(p, dims.begin(), (int)dims.size())); }\n"
           "    static val complex_array(const std::complex<double>* p, std::size_t n) {\n"
           "        return val(nk_box_complex_array(reinterpret_cast<const double*>(p), n)); }\n"
           "    static val complex_matrix(const std::complex<double>* p, std::size_t r, std::size_t c) {\n"
           "        return val(nk_box_complex_matrix(reinterpret_cast<const double*>(p), r, c)); }\n"
           "    static val complex_array_nd(const std::complex<double>* p, std::initializer_list<std::size_t> dims) {\n"
           "        return val(nk_box_complex_array_nd(reinterpret_cast<const double*>(p), dims.begin(), (int)dims.size())); }\n"
           "    double to_scalar() const { return nk_unbox_scalar(h_); }\n"
           "    bool truth() const {\n"
           "        nk_error e; e.code = 0; int t = nk_truth(h_, &e);\n"
           "        if (e.code) throw std::runtime_error(e.message); return t != 0; }\n"
           "};\n"
           "inline val _checked(nk_val r, nk_error& e) {\n"
           "    if (!r || e.code) { nk_release(r);\n"
           "        throw std::runtime_error(e.code ? e.message : \"numkit dynamic op failed\"); }\n"
           "    return val(r); }\n"
           "inline val binop(const char* op, const val& a, const val& b) {\n"
           "    nk_error e; e.code = 0; return _checked(nk_binop(op, a.get(), b.get(), &e), e); }\n"
           "inline val unop(const char* op, const val& a) {\n"
           "    nk_error e; e.code = 0; return _checked(nk_unop(op, a.get(), &e), e); }\n"
           "// Box scalar args, call `name` (1 output), keep the result BOXED (the\n"
           "// Dynamic tier does not unbox an un-typeable result). Args released; throws.\n"
           "inline val call_dyn(const char* name, std::initializer_list<double> args) {\n"
           "    std::vector<nk_val> argv; argv.reserve(args.size());\n"
           "    for (double x : args) argv.push_back(nk_box_scalar(x));\n"
           "    nk_error e; e.code = 0;\n"
           "    nk_val r = nk_call(name, argv.data(), argv.size(), 1, nullptr, &e);\n"
           "    for (nk_val h : argv) nk_release(h);\n"
           "    return _checked(r, e); }\n"
           "// Like call_dyn but the args are already boxed vals (a Dynamic / mixed\n"
           "// argument list, A3) — borrowed; their val owners release them. 1 output.\n"
           "inline val call_dynv(const char* name, std::initializer_list<val> args) {\n"
           "    std::vector<nk_val> argv; argv.reserve(args.size());\n"
           "    for (const val& a : args) argv.push_back(a.get());\n"
           "    nk_error e; e.code = 0;\n"
           "    nk_val r = nk_call(name, argv.data(), argv.size(), 1, nullptr, &e);\n"
           "    return _checked(r, e); }\n"
           "// Index a Dynamic value: a(subs) -> a new Dynamic value (A4). subs are\n"
           "// boxed vals (borrowed). The runtime resolves index-vs-call (a handle is\n"
           "// CALLED, an array is INDEXED) — sound whatever `a` is at runtime.\n"
           "inline val index_dyn(const val& a, std::initializer_list<val> subs) {\n"
           "    std::vector<nk_val> sv; sv.reserve(subs.size());\n"
           "    for (const val& s : subs) sv.push_back(s.get());\n"
           "    nk_error e; e.code = 0;\n"
           "    return _checked(nk_index(a.get(), sv.data(), sv.size(), &e), e); }\n"
           "// Multi-output bridged call: `[o0, o1, ...] = name(args)`. Returns output\n"
           "// 0; outputs 1..nargout-1 are written (owned) into extra[0..nargout-2].\n"
           "// Args borrowed (their val owners release them). Errors throw.\n"
           "inline val call_dyn_multi(const char* name, std::initializer_list<val> args,\n"
           "                          std::size_t nargout, val* extra) {\n"
           "    std::vector<nk_val> argv; argv.reserve(args.size());\n"
           "    for (const val& a : args) argv.push_back(a.get());\n"
           "    std::vector<nk_val> ex(nargout > 1 ? nargout - 1 : 0, nullptr);\n"
           "    nk_error e; e.code = 0;\n"
           "    nk_val r = nk_call(name, argv.data(), argv.size(), nargout, ex.data(), &e);\n"
           "    if (!r || e.code) { nk_release(r); for (nk_val h : ex) nk_release(h);\n"
           "        throw std::runtime_error(e.code ? e.message : \"numkit dynamic op failed\"); }\n"
           "    for (std::size_t i = 0; i < ex.size(); ++i) extra[i] = val(ex[i]);\n"
           "    return val(r); }\n"
           "} // namespace nk_rt\n";
}

// Emit ONE function (no prelude) under the RawBuffer ABI. `cppName`
// overrides the emitted symbol (for a mangled specialisation; empty -> the
// source name). `ctx` (when set) routes user-function calls and collects
// further specialisations. Returns {signature, definition}.
OneFn emitOneFunction(const ASTNode &funcDef, const std::vector<ParamSpec> &params,
                      const TransferRegistry &reg, ProgramEmitCtx *ctx,
                      const std::string &cppName, const ClassRegistry *classes,
                      const std::vector<ParamSpec> &extraSeed = {}, bool bridge = false,
                      bool opsKernels = false, bool interprocByValueReturn = false)
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
            ai.rowsVar  = companion(p.name, "_rows");
            ai.colsVar  = companion(p.name, "_cols");
            ai.lenVar   = "(" + ai.rowsVar + " * " + ai.colsVar + ")";  // numel (elementwise)
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
            std::string prod;  // numel = product of the dim companions (elementwise)
            for (std::size_t d = 0; d < p.type.shape.nd.size(); ++d) {
                const std::string dv = companion(p.name, "_d" + std::to_string(d));
                ai.ndDims.push_back(dv);
                sig += ", std::size_t " + dv;
                prod += (prod.empty() ? "" : " * ") + dv;
            }
            ai.lenVar       = "(" + prod + ")";
            arrays[p.name] = ai;
            sigParams.push_back(sig);
        } else if (isBufferArrayType(p.type)) {
            ArrayInfo ai;
            ai.dtype       = p.type.dtype;
            ai.lenVar      = companion(p.name, "_len");
            ai.dataExpr    = p.name;
            ai.orient      = orientOf(p.type);
            arrays[p.name] = ai;
            sigParams.push_back("const " + cppScalarType(p.type.dtype) + "* " + p.name
                                + ", std::size_t " + ai.lenVar);
        } else if (p.type.isObject()) {
            // value class -> by value (value semantics); handle -> wrapper.
            sigParams.push_back(cppObjectType(p.type.classId, classes) + " " + p.name);
        } else if (bridge && p.type.isDynamic()) {
            // Dynamic tier (DESIGN.md §10 C1): an un-typeable parameter is a boxed
            // nk_rt::val passed BY VALUE — MATLAB pass-by-value is a copy (val's
            // copy = deep clone). Directly usable as a Dynamic local in the body.
            sigParams.push_back("nk_rt::val " + p.name);
        } else {
            unsupported("parameter '" + p.name + "' has an unsupported type for RawBuffer ABI");
        }
    }
    for (const ParamSpec &es : extraSeed)  // pre-typed locals (e.g. ctor output)
        entry.set(es.name, {es.type, ConstVal::unknown()});

    const DeclTypeMap decls = computeDeclTypes(body, entry, reg, classes);

    // Reserved-identifier guard. A user variable / parameter is emitted VERBATIM
    // as a C++ name (synthesised names escape "__", but user names stay readable
    // as-is). "__" anywhere in an identifier is reserved to the implementation
    // ([lex.name]) — UB to declare. A leading "_"+uppercase is impossible (a
    // MATLAB identifier cannot begin with "_"), so "__" is the only reserved
    // shape a user name can take. Refuse it with a clear message rather than
    // emit UB. (A function/method NAME may contain "__"; it is mangled through
    // escapeBase, so it never reaches the output verbatim — only var names do.)
    {
        auto refuseDunder = [](const std::string &name) {
            if (name.find("__") != std::string::npos)
                unsupported("identifier '" + name + "' contains '__', which is reserved in C++ "
                            "([lex.name]) — rename the variable");
        };
        for (const std::string &p : paramSet) refuseDunder(p);
        for (const auto &[n, t] : decls) refuseDunder(n);
    }

    // Return classification:
    //   0 outputs -> void (e.g. a handle class's in-place mutator);
    //   1 output  -> scalar (by value) / array (out-param) / object (by value);
    //   N outputs -> void + a reference out-param per (scalar) output.
    std::string retCpp             = "void";
    std::string retName;
    bool        arrayReturn        = false;
    bool        dynamicReturn      = false;
    bool        multiByValueReturn = false;  // nout>=2 + leading 1-D array (output 0 by value)
    if (nout == 1) {
        retName             = funcDef.returnNames[0];
        const auto retIt    = decls.find(retName);
        if (retIt == decls.end())
            unsupported("output '" + retName + "' is never assigned");
        const InferredType retType = retIt->second;
        if (isUnboxableScalarType(retType)) {
            retCpp = cppScalarType(retType.dtype);
        } else if (interprocByValueReturn && isByValueReturnArrayType(retType)) {
            // Interproc callee returning a 1-D / 2-D-KnownDims / fully-known-N-D
            // array BY VALUE as a flat std::vector (column-major; self-describing
            // .size()). The dims are compile-time-known on BOTH sides (the caller
            // monomorphises the callee to the same return type), so no runtime dims
            // travel with the buffer. retName is NOT registered as an output -> it
            // falls to the array-LOCAL hoist (1-D vector / 2-D KnownDims / known
            // N-D), the body fills it, and the scalar-return path emits
            // `return retName;`. Checked BEFORE the N-D/2-D/1-D out-param branches
            // (which serve the ENTRY, emitted with interprocByValueReturn=false).
            retCpp = "std::vector<" + cppScalarType(retType.dtype) + ">";
        } else if (retType.isConcrete() && retType.shape.isNDims()) {
            // N-D (rank>=3) OUTPUT -> caller-allocated out-param: a MUTABLE
            // pointer + one size_t companion per dim (column-major), all passed
            // IN by value. The caller allocates product-of-dims elements and
            // passes the sizes; the body fills it (indexN_set) and
            // size/ndims/numel read the companions — so a runtime-dim N-D
            // output works too. Checked BEFORE isBufferArrayType (which also
            // matches an N-D shape). The dims are companion VARS (not the
            // KnownDims literals a local uses) because the caller owns them.
            arrayReturn = true;
            retCpp      = "void";
            ArrayInfo ai;
            ai.dtype    = retType.dtype;
            ai.isND     = true;
            ai.isOutput = true;
            ai.dataExpr = retName;
            // `__restrict` on the OUTPUT pointer: it does not alias any input
            // (the caller allocates the output buffer distinctly — a documented
            // ABI precondition). This is the one aliasing fact the compiler needs
            // to auto-vectorize the fill/elementwise loops (write↔read overlap is
            // the blocker). Inputs stay un-restricted, so passing the same array
            // for two read-only inputs (f(v,v)) remains safe.
            std::string sig  = cppScalarType(retType.dtype) + "* __restrict " + retName;
            std::string prod;  // total length = product of the companions
            for (std::size_t d = 0; d < retType.shape.nd.size(); ++d) {
                const std::string dv = companion(retName, "_d" + std::to_string(d));
                ai.ndDims.push_back(dv);
                sig  += ", std::size_t " + dv;
                prod += (prod.empty() ? "" : " * ") + dv;
            }
            ai.lenVar       = "(" + prod + ")";  // drives the zeros/ones fill loop
            arrays[retName] = ai;
            sigParams.push_back(sig);
        } else if (is2DMatrixType(retType)) {
            // 2-D matrix OUTPUT -> caller-allocated out-param: a MUTABLE pointer
            // + rows/cols companions (column-major), passed IN by value (the
            // caller allocates rows*cols and passes the dims). The body fills it
            // (zeros + index2_set); size/numel read the companions. Mirror of the
            // N-D-output branch on the KnownDims rank-2 fast path. Checked BEFORE
            // isBufferArrayType (which also matches a 2-D shape).
            arrayReturn = true;
            retCpp      = "void";
            ArrayInfo ai;
            ai.dtype    = retType.dtype;
            ai.is2D     = true;
            ai.isOutput = true;
            ai.dataExpr = retName;
            ai.rowsVar  = companion(retName, "_rows");
            ai.colsVar  = companion(retName, "_cols");
            ai.lenVar   = "(" + ai.rowsVar + " * " + ai.colsVar + ")";  // zeros/ones fill bound
            arrays[retName] = ai;
            sigParams.push_back(cppScalarType(retType.dtype) + "* __restrict " + retName
                                + ", std::size_t " + ai.rowsVar + ", std::size_t " + ai.colsVar);
        } else if (isBufferArrayType(retType)) {
            arrayReturn = true;
            retCpp      = "void";
            ArrayInfo ai;
            ai.dtype        = retType.dtype;
            ai.lenVar       = companion(retName, "_len");
            ai.isOutput     = true;
            ai.dataExpr     = retName;
            ai.orient       = orientOf(retType);
            arrays[retName] = ai;
            sigParams.push_back(cppScalarType(retType.dtype) + "* __restrict " + retName
                                + ", std::size_t " + ai.lenVar);
        } else if (retType.isObject()) {
            // returned BY VALUE (value class) / handle wrapper — not an
            // out-param; the scalar-return path (return retName;) applies.
            retCpp = cppObjectType(retType.classId, classes);
        } else if (bridge && retType.isDynamic()) {
            // Dynamic tier (DESIGN.md §10 C1): an un-typeable result is returned
            // BOXED as an owned nk_val — the path for a Dynamic VALUE to cross the
            // typed RawBuffer boundary (the local is an nk_rt::val whose handle is
            // transferred out via take()). The caller owns it (nk_release).
            retCpp        = "nk_val";
            dynamicReturn = true;
        } else {
            unsupported("output '" + retName + "' has an unsupported type");
        }
    } else if (nout >= 2) {
        // Outputs are reference out-params the body writes directly — EXCEPT a
        // leading array (output 0), which is returned BY VALUE as a flat
        // std::vector (self-describing; no caller pre-alloc). This composes the
        // interproc-by-value return (1-D / 2-D-KnownDims / fully-known N-D) with
        // scalar reference outputs. v1: outputs 1.. must be unboxed scalars (a
        // trailing array output would need caller pre-alloc — sound refusal).
        for (std::size_t oi = 0; oi < funcDef.returnNames.size(); ++oi) {
            const std::string &rn = funcDef.returnNames[oi];
            if (rn.empty()) unsupported("multi-output with an unnamed output");
            const auto         it = decls.find(rn);
            const InferredType t  = (it != decls.end()) ? it->second : InferredType::dynamic();
            if (oi == 0 && isByValueReturnArrayType(t)) {
                // Leading array -> by-value std::vector return. Falls to the
                // array-LOCAL hoist (1-D / 2-D KnownDims / known N-D; not a
                // sigParam/paramSet); the body fills it; `return rn;` is emitted at
                // the end (emitReturnScalar).
                retCpp             = "std::vector<" + cppScalarType(t.dtype) + ">";
                retName            = rn;
                multiByValueReturn = true;
                continue;
            }
            if (!isUnboxableScalarType(t))
                unsupported("multi-output '" + rn + "' must be an unboxed scalar "
                            "(only a leading array output is returned by value; v1)");
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
            // also matches an N-D shape). Flat owned vector, column-major.
            ArrayInfo ai;
            ai.dtype    = t.dtype;
            ai.isLocal  = true;
            ai.isND     = true;
            ai.dataExpr = name + ".data()";
            ai.lenVar   = name + ".size()";
            bool allKnown = true;
            for (std::size_t d : t.shape.nd)
                if (d == 0) { allKnown = false; break; }
            if (allKnown) {
                // Compile-time dims -> literals baked into indexN / queries.
                for (std::size_t d : t.shape.nd) ai.ndDims.push_back(std::to_string(d));
            } else {
                // A runtime dim -> per-dim `<name>_dN` size_t vars, hoisted at
                // fn entry and set from the zeros/ones args at the assignment.
                ai.ndRuntimeLocal = true;
                for (std::size_t d = 0; d < t.shape.nd.size(); ++d)
                    ai.ndDims.push_back(companion(name, "_d" + std::to_string(d)));
            }
            arrays[name] = ai;
        } else if (isBufferArrayType(t)) {
            ArrayInfo ai;
            ai.dtype     = t.dtype;
            ai.isLocal   = true;
            ai.dataExpr  = name + ".data()";
            ai.lenVar    = name + ".size()";
            ai.orient    = orientOf(t);
            arrays[name] = ai;
        }
    }

    // (No companion-collision check needed: companion names carry the `_nk_`
    // prefix, and a MATLAB identifier can never begin with '_', so a synthesised
    // companion can never equal a user variable — the collision is structurally
    // impossible rather than detected.)

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
    Emitter em(entry, reg, arrays, opt, ctx, classes, bridge, opsKernels);
    em.hoistArrayLocals();  // owned-vector array locals first
    std::map<std::string, InferredType> ordered(decls.begin(), decls.end());
    for (const auto &[name, t] : ordered) {
        if (paramSet.count(name) || arrays.count(name) || promotedVars.count(name))
            continue;  // signature params / arrays / promoted loop counters
        // A Dynamic local is allowed under bridging — it lives in the Dynamic
        // tier as a boxed nk_rt::val (DESIGN.md §10 C1). Without bridging there
        // is no runtime to hold it, so it stays the explicit refusal.
        if (!isUnboxableScalarType(t) && !t.isObject() && !(bridge && t.isDynamic()))
            unsupported("local '" + name + "' is not an unboxable scalar or object (type "
                        + t.str() + ") — unsupported in RawBuffer ABI");
        em.hoistLocal(name, t);
        if (bridge && t.isDynamic()) em.addDynamicLocal(name);  // boxed-assign discipline
    }
    em.setReturnInfo(multiByValueReturn || (nout == 1 && !arrayReturn), retName, dynamicReturn);
    em.emitStmt(body);
    // Trailing return for the fall-through path. Skipped when the body already
    // ends in an explicit `return` (its emitStmt emitted the return), so we don't
    // emit a dead duplicate; a CONDITIONAL early return leaves the trailing return
    // reachable, so it stays.
    const bool bodyEndsInReturn = body.type == NodeType::BLOCK && !body.children.empty()
                                  && body.children.back()
                                  && body.children.back()->type == NodeType::RETURN_STMT;
    if (!bodyEndsInReturn && (multiByValueReturn || (nout == 1 && !arrayReturn)))
        dynamicReturn ? em.emitReturnDynamic(retName) : em.emitReturnScalar(retName);

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
                             const BridgeOptions &bridge, const OpsKernelOptions &opsKernels)
{
    const OneFn f = emitOneFunction(funcDef, params, reg, /*ctx=*/nullptr, /*cppName=*/"", classes,
                                    /*extraSeed=*/{}, bridge.enabled, opsKernels.enabled);
    std::string source = kPrelude;
    source += "\n";
    if (opsKernels.enabled) source += "#include <numkit/ops/kernels.hpp>\n";
    if (bridge.enabled) source += bridgePrelude(bridge.runtimeHeader);
    source += emitAllStructs(classes);
    source += f.definition;
    return {source, funcDef.strValue, f.signature};
}

// ── whole-program emission (§12 brick 1b) ─────────────────────────────
EmittedFunction emitProgram(const ASTNode &entryDef,
                            const std::vector<ParamSpec> &params,
                            const FunctionTable &table, const TransferRegistry &reg,
                            const ClassRegistry *classes, const BridgeOptions &bridge,
                            const OpsKernelOptions &opsKernels)
{
    ProgramEmitCtx ctx;
    ctx.funcs = &table;

    std::vector<InferredType> entryArgTypes;
    entryArgTypes.reserve(params.size());
    for (const auto &p : params) entryArgTypes.push_back(p.type);
    const std::string entryMangled = mangle(entryDef.strValue, entryArgTypes);
    ctx.seen.insert(entryMangled);

    std::vector<std::string> sigs, defs;
    const OneFn ef = emitOneFunction(entryDef, params, reg, &ctx, entryMangled, classes, {},
                                     bridge.enabled, opsKernels.enabled);
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
        const OneFn cf = emitOneFunction(*cs.def, ps, reg, &ctx, cs.mangled, classes, cs.extraSeed,
                                         bridge.enabled, opsKernels.enabled,
                                         /*interprocByValueReturn=*/true);
        sigs.push_back(cf.signature);
        defs.push_back(cf.definition);
    }

    std::string source = kPrelude;
    source += "\n";
    if (opsKernels.enabled) source += "#include <numkit/ops/kernels.hpp>\n";
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
