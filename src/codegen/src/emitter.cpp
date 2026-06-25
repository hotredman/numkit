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

// The flattened field-local name for a plain-struct field chain (field-flattening).
// `s.a.b` (a FIELD_ACCESS) -> "_nk_fld_s_a_b"; "" if not rooted at a plain identifier
// -> not a flattenable struct field. A single-level `s.a` gives "_nk_fld_s_a"
// (unchanged). MUST match inference.cpp's copy.
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
        {"asinh", "asinh"}, {"erf", "erf"}, {"erfc", "erfc"}, {"expm1", "expm1"},
        {"gammaln", "lgamma"}};  // numkit gammaln(x) == std::lgamma(x) (bit-identical)
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
        {"atan2", "atan2"}, {"hypot", "hypot"},
        {"rem", "fmod"},   // numkit rem(a,b) == std::fmod(a,b) (bit-identical)
        // MATLAB's 2-arg max/min ignore NaN (the result is the non-NaN operand), which is
        // bit-identical to std::fmax/std::fmin. The 1-arg reduction max(x)/min(x) is a
        // different arity (handled at the statement level), so it never reaches this 2-arg
        // path. maxMinTransfer types the 2-arg form (real DOUBLE) for the elementwise fill.
        {"max", "fmax"}, {"min", "fmin"}};
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
        // Plain struct: the synthesized field-local (field-flattening), generalised
        // to a NESTED chain s.a.b via the chain helper. The immediate base being
        // non-object gates struct-vs-object at every level.
        if (!base.type.isObject()) {
            const std::string fld = structFieldLocal(e);
            if (!fld.empty()) return fld;
        }
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

    // deg2rad / rad2deg: a real scalar scaling. No std:: fn -> emit numkit's exact
    // constant (misc.cpp: x * k, k computed from these literals as pi/180 or 180/pi) so
    // it is bit-identical to the interpreter. Typed scalar-only (realScalarMathUnary-
    // Transfer); an array / complex arg is Dynamic -> bridged.
    if (nargs == 1 && (name == "deg2rad" || name == "rad2deg")) {
        const std::string x = emitExpr(*call.children[1]);
        return name == "deg2rad" ? "((" + x + ") * (3.14159265358979323846 / 180.0))"
                                 : "((" + x + ") * (180.0 / 3.14159265358979323846))";
    }

    // sign(x): MATLAB's signum for a REAL operand -> 1 / -1 / 0, with sign(0)=0 and
    // sign(NaN)=NaN. Not a std:: fn; inline via a single-eval IIFE. realOnlyMathUnary-
    // Transfer types sign for real DOUBLE/SINGLE only (complex -> Dynamic -> bridged), so
    // the operand here is always real. Lights up the scalar form AND the elementwise fill
    // (collectElementwise recognises sign), e.g. C = sign(A).
    if (nargs == 1 && name == "sign") {
        const std::string x = emitExpr(*call.children[1]);
        return "([](double _nk_x){ return _nk_x > 0.0 ? 1.0 : (_nk_x < 0.0 ? -1.0 "
               ": (_nk_x != _nk_x ? _nk_x : 0.0)); })(" + x + ")";
    }

    // any(<expr>) / all(<expr>) over a single 1-D array -> a LOGICAL scalar, as an IIFE so it
    // works in ANY expression position (an if-condition, a sub-expression, a whole rhs):
    // any short-circuits to true on the first nonzero, all to false on the first zero. EXACT +
    // order-independent. The arg is FUSED (elementCtx_ makes the whole array emit arr[_nk_aa_i]
    // -- a fresh loop var, save/restore so a nesting fusion is unaffected). SINGLE-array 1-D
    // elementwise only; a 2-D any(A) is column-wise (a ROW VECTOR, not a scalar -> the scalar
    // gate fails -> bridged). A bare-VAR arg works too (collectElementwise of a whole array).
    if (nargs == 1 && (name == "any" || name == "all")) {
        std::set<std::string> srcArrays;
        const bool            pureEw = collectElementwise(*call.children[1], srcArrays);
        if (pureEw && srcArrays.size() == 1 && !arrays_.at(*srcArrays.begin()).is2D
            && !arrays_.at(*srcArrays.begin()).isND
            && inferExpr(call, types_, reg_, classes_).type.shape.isScalar()) {
            const ArrayInfo  &ba    = arrays_.at(*srcArrays.begin());
            const bool        isAny = name == "any";
            const std::string saved = elementCtx_;
            elementCtx_              = "_nk_aa_i";  // whole array in the arg -> arr[_nk_aa_i]
            const std::string maskExpr = emitExpr(*call.children[1]);
            elementCtx_                = saved;
            return "([&]() -> bool { for (std::size_t _nk_aa_i = 0; _nk_aa_i < (" + ba.lenVar
                   + "); ++_nk_aa_i) if ((" + maskExpr + ") "
                   + (isAny ? "!= 0) return true; return false; }())"
                            : "== 0) return false; return true; }())");
        }
    }

    // min(<expr>) / max(<expr>) over a single 1-D DOUBLE array -> a scalar, as an IIFE (works
    // in any expr position). min/max are EXACT + order-independent (already native for a VAR),
    // so an inline elementwise arg (min(abs(x)), max(x.^2), ...) folds natively. NaN-skipping
    // mirrors the VAR reduction: seed = expr-at-0, then for i>=1 update on (v cmp acc) OR when
    // acc is NaN (so the first non-NaN seeds it; all-NaN stays NaN). SINGLE-array 1-D
    // elementwise only -- a 2-D min(A) is column-wise (a ROW VECTOR -> scalar gate fails ->
    // bridged). A bare-VAR arg works too (the statement-level VAR reduction takes a whole-rhs
    // r=min(v); this catches min(v) in expr position).
    if (nargs == 1 && (name == "min" || name == "max")) {
        std::set<std::string> srcArrays;
        const bool            pureEw = collectElementwise(*call.children[1], srcArrays);
        if (pureEw && srcArrays.size() == 1 && !arrays_.at(*srcArrays.begin()).is2D
            && !arrays_.at(*srcArrays.begin()).isND
            && arrays_.at(*srcArrays.begin()).dtype == ValueType::DOUBLE
            && inferExpr(call, types_, reg_, classes_).type.shape.isScalar()) {
            const ArrayInfo  &ba    = arrays_.at(*srcArrays.begin());
            const std::string cmp   = name == "max" ? ">" : "<";
            const std::string saved = elementCtx_;
            elementCtx_              = "0";  // seed: expr at element 0
            const std::string seed  = emitExpr(*call.children[1]);
            elementCtx_              = "_nk_aa_i";  // body: expr at the loop index
            const std::string bodyE = emitExpr(*call.children[1]);
            elementCtx_             = saved;
            return "([&]() -> double { double _nk_acc = (" + seed
                   + "); for (std::size_t _nk_aa_i = 1; _nk_aa_i < (" + ba.lenVar
                   + "); ++_nk_aa_i) { double _nk_v = (" + bodyE + "); if (_nk_v " + cmp
                   + " _nk_acc || _nk_acc != _nk_acc) _nk_acc = _nk_v; } return _nk_acc; }())";
        }
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

    // Whole-array FILL: `A(:) = rhs` over the flat column-major buffer (the bare colon is an
    // empty COLON_EXPR). A must be writable (local/output). Two rhs forms: a SCALAR s
    // (broadcast to every element) or a MATCHING-NUMEL array b (copied column-major,
    // preserving A's shape -- MATLAB's A(:) = b). numel = 1-D length / 2-D rows*cols / N-D
    // product. Placed before the N-D write branch so a runtime-dim 2-D A(:) = ... is filled
    // rather than rejected on subscript arity. v1: scalar rhs, or a DOUBLE array VAR rhs (an
    // array EXPRESSION rhs is deferred -> falls through to a clean refusal).
    if (lhsCall.children.size() == 2 && lhsCall.children[1]->type == NodeType::COLON_EXPR
        && lhsCall.children[1]->children.empty()) {
        if (!ai.isLocal && !ai.isOutput)
            unsupported("whole-array fill of a read-only parameter '" + base + "'");
        const AbstractValue rhsAV = inferExpr(rhs, types_, reg_, classes_);
        // Element count of any ranked/runtime array over its flat column-major buffer.
        const auto arrayNumel = [](const ArrayInfo &x) -> std::string {
            if (x.isND) {
                std::string s = x.ndDims[0];
                for (std::size_t i = 1; i < x.ndDims.size(); ++i) s += " * " + x.ndDims[i];
                return s;
            }
            if (x.is2D) return x.rowsVar + " * " + x.colsVar;
            return x.lenVar;
        };
        if (rhsAV.type.isConcrete() && rhsAV.type.shape.isScalar()) {
            const std::string numel = arrayNumel(ai);
            line("{");
            ++indent_;
            line(cppScalarType(ai.dtype) + " _nk_fv = " + emitExpr(rhs) + ";");
            open("for (std::size_t _nk_i = 0; _nk_i < (" + numel + "); ++_nk_i)");
            line(ptr + "[_nk_i] = _nk_fv;");
            close();
            --indent_;
            line("}");
            return;
        }
        if (rhsAV.type.isConcrete() && !rhsAV.type.shape.isScalar()
            && ai.dtype == ValueType::DOUBLE
            && rhs.type == NodeType::IDENTIFIER && isArrayVar(rhs.strValue)
            && arrays_.at(rhs.strValue).dtype == ValueType::DOUBLE) {
            const ArrayInfo &b = arrays_.at(rhs.strValue);
            line("{");
            ++indent_;
            line("const std::size_t _nk_nA = (" + arrayNumel(ai) + ");");
            line("const std::size_t _nk_nB = (" + arrayNumel(b) + ");");
            line("if (_nk_nA != _nk_nB)");
            line("    throw std::runtime_error(\"numkit: A(:) assignment element-count mismatch\");");
            open("for (std::size_t _nk_i = 0; _nk_i < _nk_nA; ++_nk_i)");
            line(ptr + "[_nk_i] = " + b.dataExpr + "[_nk_i];");
            close();
            --indent_;
            line("}");
            return;
        }
        // A(:) = <elementwise expr>: a pure-elementwise DOUBLE expression over arrays
        // (A(:)=A*2, A(:)=b+c, A(:)=A+1, ...) -> FUSE into the flat fill, A[i] = <expr at i>.
        // Sound: A(:) fills in place; an elementwise expr is per-element, so even a self-ref
        // (A[i]=f(A[i])) is fine, and a non-elementwise rhs (sort(A)) is rejected by collect-
        // Elementwise -> bridged. Each array operand's numel is runtime-checked == A's (MATLAB
        // errors on a mismatch). v1: A + every operand DOUBLE; reached only for a compound expr
        // (a bare array var took the arm above, a scalar the first arm).
        {
            std::set<std::string> srcArrays;
            const bool            pureEw = collectElementwise(rhs, srcArrays);
            bool                  allDouble = ai.dtype == ValueType::DOUBLE && !srcArrays.empty();
            for (const auto &s : srcArrays)
                if (arrays_.at(s).dtype != ValueType::DOUBLE) allDouble = false;
            if (pureEw && allDouble && rhsAV.type.isConcrete() && !rhsAV.type.shape.isScalar()) {
                line("{");
                ++indent_;
                line("const std::size_t _nk_nA = (" + arrayNumel(ai) + ");");
                for (const auto &s : srcArrays) {
                    line("if ((" + arrayNumel(arrays_.at(s)) + ") != _nk_nA)");
                    line("    throw std::runtime_error("
                         "\"numkit: A(:) = expr element-count mismatch\");");
                }
                elementCtx_ = "_nk_i";  // whole arrays in the expr -> arr[_nk_i]
                const std::string e = emitExpr(rhs);
                elementCtx_.clear();
                open("for (std::size_t _nk_i = 0; _nk_i < _nk_nA; ++_nk_i)");
                line(ptr + "[_nk_i] = " + e + ";");
                close();
                --indent_;
                line("}");
                return;
            }
        }
    }

    // 2-D COLUMN slice write: A(:, j) = col -> overwrite column j (1-based) of a 2-D
    // matrix with the 1-D vector col (length = rows). Works for a KnownDims 2-D OR a
    // runtime-dim 2-D (dims via the rank-agnostic dimExpr). Column-major: column j is
    // the CONTIGUOUS block A[(j-1)*rows .. +rows), so it is a straight copy. Bounds-
    // checked j, length-checked col; A must be writable (local/output). Placed before
    // the N-D write branch so it intercepts a runtime-dim 2-D target (whose other
    // colon writes that branch refuses). v1: A 2-D DOUBLE, col a distinct 1-D DOUBLE
    // array var, j scalar. (A(i,:)=row strided write is the sibling branch below.)
    if (lhsCall.children.size() == 3
        && (ai.is2D || (ai.isND && ai.ndDims.size() == 2)) && ai.dtype == ValueType::DOUBLE
        && lhsCall.children[1]->type == NodeType::COLON_EXPR
        && lhsCall.children[1]->children.empty()
        && lhsCall.children[2]->type != NodeType::COLON_EXPR
        && rhs.type == NodeType::IDENTIFIER && isArrayVar(rhs.strValue)
        && rhs.strValue != base && !arrays_.at(rhs.strValue).is2D
        && !arrays_.at(rhs.strValue).isND
        && arrays_.at(rhs.strValue).dtype == ValueType::DOUBLE
        && inferExpr(*lhsCall.children[2], types_, reg_, classes_).type.shape.isScalar()) {
        if (!ai.isLocal && !ai.isOutput)
            unsupported("column-slice write to a read-only matrix parameter '" + base + "'");
        const std::string Arows = dimExpr(ai, 0), Acols = dimExpr(ai, 1);
        const ArrayInfo  &col   = arrays_.at(rhs.strValue);
        endStack_.push_back(Acols);  // `end` in the column index = cols
        const std::string j = emitExpr(*lhsCall.children[2]);
        endStack_.pop_back();
        line("{");
        ++indent_;
        line("const std::ptrdiff_t _nk_j = static_cast<std::ptrdiff_t>(" + j + ");");
        line("if (_nk_j < 1 || _nk_j > static_cast<std::ptrdiff_t>(" + Acols + "))");
        line("    throw std::out_of_range(\"numkit: column index out of bounds\");");
        line("if (" + col.lenVar + " != " + Arows + ")");
        line("    throw std::out_of_range(\"numkit: column assignment length mismatch\");");
        line("const std::size_t _nk_off = static_cast<std::size_t>(_nk_j - 1) * " + Arows + ";");
        open("for (std::size_t _nk_i = 0; _nk_i < " + Arows + "; ++_nk_i)");
        line(ptr + "[_nk_off + _nk_i] = " + col.dataExpr + "[_nk_i];");
        close();
        --indent_;
        line("}");
        return;
    }

    // 2-D ROW slice write: A(i, :) = row -> overwrite row i (1-based) of a 2-D matrix
    // with the 1-D vector row (length = cols). The mirror of the column write above; in
    // column-major storage row i is STRIDED: A[(i-1) + j*rows] = row[j] for j in
    // [0, cols). Works for a KnownDims 2-D OR a runtime-dim 2-D (dims via dimExpr).
    // Bounds-checked i, length-checked row; A must be writable. v1: A 2-D DOUBLE, row a
    // distinct 1-D DOUBLE array var, i scalar.
    if (lhsCall.children.size() == 3
        && (ai.is2D || (ai.isND && ai.ndDims.size() == 2)) && ai.dtype == ValueType::DOUBLE
        && lhsCall.children[1]->type != NodeType::COLON_EXPR
        && lhsCall.children[2]->type == NodeType::COLON_EXPR
        && lhsCall.children[2]->children.empty()
        && rhs.type == NodeType::IDENTIFIER && isArrayVar(rhs.strValue)
        && rhs.strValue != base && !arrays_.at(rhs.strValue).is2D
        && !arrays_.at(rhs.strValue).isND
        && arrays_.at(rhs.strValue).dtype == ValueType::DOUBLE
        && inferExpr(*lhsCall.children[1], types_, reg_, classes_).type.shape.isScalar()) {
        if (!ai.isLocal && !ai.isOutput)
            unsupported("row-slice write to a read-only matrix parameter '" + base + "'");
        const std::string Arows = dimExpr(ai, 0), Acols = dimExpr(ai, 1);
        const ArrayInfo  &row   = arrays_.at(rhs.strValue);
        endStack_.push_back(Arows);  // `end` in the row index = rows
        const std::string i = emitExpr(*lhsCall.children[1]);
        endStack_.pop_back();
        line("{");
        ++indent_;
        line("const std::ptrdiff_t _nk_i0 = static_cast<std::ptrdiff_t>(" + i + ") - 1;");
        line("if (_nk_i0 < 0 || _nk_i0 >= static_cast<std::ptrdiff_t>(" + Arows + "))");
        line("    throw std::out_of_range(\"numkit: row index out of bounds\");");
        line("if (" + row.lenVar + " != " + Acols + ")");
        line("    throw std::out_of_range(\"numkit: row assignment length mismatch\");");
        open("for (std::size_t _nk_j = 0; _nk_j < " + Acols + "; ++_nk_j)");
        line(ptr + "[static_cast<std::size_t>(_nk_i0) + _nk_j * " + Arows + "] = "
             + row.dataExpr + "[_nk_j];");
        close();
        --indent_;
        line("}");
        return;
    }

    // 2-D COLUMN scalar-fill: A(:, j) = s -> set every element of column j (1-based) to
    // the scalar s (broadcast). Sibling of the column VECTOR write above; reached only for
    // a scalar rhs (the vector branch already returned for an array-var rhs, so the two are
    // disjoint). Common idiom: A(:,j) = 0. Column-major, so column j is the contiguous
    // block A[(j-1)*rows .. +rows). Works for KnownDims or runtime-dim 2-D (dims via dimExpr).
    if (lhsCall.children.size() == 3
        && (ai.is2D || (ai.isND && ai.ndDims.size() == 2)) && ai.dtype == ValueType::DOUBLE
        && lhsCall.children[1]->type == NodeType::COLON_EXPR
        && lhsCall.children[1]->children.empty()
        && lhsCall.children[2]->type != NodeType::COLON_EXPR
        && inferExpr(rhs, types_, reg_, classes_).type.isConcrete()
        && inferExpr(rhs, types_, reg_, classes_).type.shape.isScalar()
        && inferExpr(*lhsCall.children[2], types_, reg_, classes_).type.shape.isScalar()) {
        if (!ai.isLocal && !ai.isOutput)
            unsupported("column-slice fill to a read-only matrix parameter '" + base + "'");
        const std::string Arows = dimExpr(ai, 0), Acols = dimExpr(ai, 1);
        endStack_.push_back(Acols);  // `end` in the column index = cols
        const std::string j = emitExpr(*lhsCall.children[2]);
        endStack_.pop_back();
        line("{");
        ++indent_;
        line(cppScalarType(ai.dtype) + " _nk_fv = " + emitExpr(rhs) + ";");
        line("const std::ptrdiff_t _nk_j = static_cast<std::ptrdiff_t>(" + j + ");");
        line("if (_nk_j < 1 || _nk_j > static_cast<std::ptrdiff_t>(" + Acols + "))");
        line("    throw std::out_of_range(\"numkit: column index out of bounds\");");
        line("const std::size_t _nk_off = static_cast<std::size_t>(_nk_j - 1) * " + Arows + ";");
        open("for (std::size_t _nk_i = 0; _nk_i < " + Arows + "; ++_nk_i)");
        line(ptr + "[_nk_off + _nk_i] = _nk_fv;");
        close();
        --indent_;
        line("}");
        return;
    }

    // 2-D ROW scalar-fill: A(i, :) = s -> set every element of row i (1-based) to the
    // scalar s (broadcast). Mirror of the column scalar-fill; in column-major storage row i
    // is STRIDED: A[(i-1) + j*rows] for j in [0, cols). Reached only for a scalar rhs.
    if (lhsCall.children.size() == 3
        && (ai.is2D || (ai.isND && ai.ndDims.size() == 2)) && ai.dtype == ValueType::DOUBLE
        && lhsCall.children[1]->type != NodeType::COLON_EXPR
        && lhsCall.children[2]->type == NodeType::COLON_EXPR
        && lhsCall.children[2]->children.empty()
        && inferExpr(rhs, types_, reg_, classes_).type.isConcrete()
        && inferExpr(rhs, types_, reg_, classes_).type.shape.isScalar()
        && inferExpr(*lhsCall.children[1], types_, reg_, classes_).type.shape.isScalar()) {
        if (!ai.isLocal && !ai.isOutput)
            unsupported("row-slice fill to a read-only matrix parameter '" + base + "'");
        const std::string Arows = dimExpr(ai, 0), Acols = dimExpr(ai, 1);
        endStack_.push_back(Arows);  // `end` in the row index = rows
        const std::string i = emitExpr(*lhsCall.children[1]);
        endStack_.pop_back();
        line("{");
        ++indent_;
        line(cppScalarType(ai.dtype) + " _nk_fv = " + emitExpr(rhs) + ";");
        line("const std::ptrdiff_t _nk_i0 = static_cast<std::ptrdiff_t>(" + i + ") - 1;");
        line("if (_nk_i0 < 0 || _nk_i0 >= static_cast<std::ptrdiff_t>(" + Arows + "))");
        line("    throw std::out_of_range(\"numkit: row index out of bounds\");");
        open("for (std::size_t _nk_j = 0; _nk_j < " + Acols + "; ++_nk_j)");
        line(ptr + "[static_cast<std::size_t>(_nk_i0) + _nk_j * " + Arows + "] = _nk_fv;");
        close();
        --indent_;
        line("}");
        return;
    }

    // page-slice WRITE A(:,:,k) = M: write a 2-D m x n matrix M into page k of a rank-3 A
    // (phase N4, the WRITE sibling of the N3 page-slice read). Column-major: page k (1-based)
    // is the CONTIGUOUS block A[(k-1)*m*n ..] -> a straight copy from M. A writable; bounds-
    // checked k (end = size(A,3)); m,n size guard. v1: rank-3 DOUBLE A, two leading bare colons
    // + a scalar k, rhs a distinct 2-D DOUBLE matrix var. Placed BEFORE the N-D write branch
    // (which refuses colon subscripts).
    if (ai.isND && ai.ndDims.size() == 3 && ai.dtype == ValueType::DOUBLE
        && lhsCall.children.size() == 4
        && lhsCall.children[1]->type == NodeType::COLON_EXPR && lhsCall.children[1]->children.empty()
        && lhsCall.children[2]->type == NodeType::COLON_EXPR && lhsCall.children[2]->children.empty()
        && lhsCall.children[3]->type != NodeType::COLON_EXPR
        && rhs.type == NodeType::IDENTIFIER && isArrayVar(rhs.strValue) && rhs.strValue != base
        && (arrays_.at(rhs.strValue).is2D
            || (arrays_.at(rhs.strValue).isND && arrays_.at(rhs.strValue).ndDims.size() == 2))
        && arrays_.at(rhs.strValue).dtype == ValueType::DOUBLE
        && inferExpr(*lhsCall.children[3], types_, reg_, classes_).type.shape.isScalar()) {
        if (!ai.isLocal && !ai.isOutput)
            unsupported("page-slice write to a read-only array parameter '" + base + "'");
        const ArrayInfo  &M = arrays_.at(rhs.strValue);  // the 2-D page source
        const std::string m = dimExpr(ai, 0), n = dimExpr(ai, 1), p = dimExpr(ai, 2);
        endStack_.push_back(p);  // `end` in the page index = size(A,3)
        const std::string k = emitExpr(*lhsCall.children[3]);
        endStack_.pop_back();
        line("{");
        ++indent_;
        line("const std::size_t _nk_m = " + m + ";");
        line("const std::size_t _nk_n = " + n + ";");
        line("if (" + dimExpr(M, 0) + " != _nk_m || " + dimExpr(M, 1) + " != _nk_n)");
        line("    throw std::out_of_range(\"numkit: page-slice assignment size mismatch\");");
        line("const std::ptrdiff_t _nk_k = static_cast<std::ptrdiff_t>(" + k + ");");
        line("if (_nk_k < 1 || _nk_k > static_cast<std::ptrdiff_t>(" + p + "))");
        line("    throw std::out_of_range(\"numkit: page index out of bounds\");");
        line("const std::size_t _nk_pg = _nk_m * _nk_n;");
        line("const std::size_t _nk_off = static_cast<std::size_t>(_nk_k - 1) * _nk_pg;");
        open("for (std::size_t _nk_i = 0; _nk_i < _nk_pg; ++_nk_i)");
        line(ptr + "[_nk_off + _nk_i] = " + M.dataExpr + "[_nk_i];");
        close();
        --indent_;
        line("}");
        return;
    }

    // 2-D COLUMN-RANGE WRITE A(:, j1:j2) = B: write a 2-D rows x (j2-j1+1) matrix B into columns
    // j1..j2 of a 2-D A (phase N22, the write sibling of the 2-D column-range read). Column-major:
    // columns j1..j2 are the CONTIGUOUS block A[(j1-1)*rows .. j2*rows] -> a straight copy from B.
    // A writable; bounds-checked range (end = cols); a numel guard on B (rows*(j2-j1+1)). v1: 2-D
    // DOUBLE A (KnownDims or runtime), a leading bare colon + a step-1 range j1:j2, rhs a distinct
    // 2-D DOUBLE array var. Placed before the rank-3+ trailing-range write.
    if ((ai.is2D || (ai.isND && ai.ndDims.size() == 2)) && ai.dtype == ValueType::DOUBLE
        && lhsCall.children.size() == 3
        && lhsCall.children[1]->type == NodeType::COLON_EXPR && lhsCall.children[1]->children.empty()
        && lhsCall.children[2]->type == NodeType::COLON_EXPR && lhsCall.children[2]->children.size() == 2
        && rhs.type == NodeType::IDENTIFIER && isArrayVar(rhs.strValue) && rhs.strValue != base
        && (arrays_.at(rhs.strValue).is2D
            || (arrays_.at(rhs.strValue).isND && arrays_.at(rhs.strValue).ndDims.size() == 2))
        && arrays_.at(rhs.strValue).dtype == ValueType::DOUBLE) {
        if (!ai.isLocal && !ai.isOutput)
            unsupported("column-range write to a read-only matrix parameter '" + base + "'");
        const ArrayInfo  &B    = arrays_.at(rhs.strValue);  // value source
        const std::string rows = dimExpr(ai, 0), cols = dimExpr(ai, 1);
        const ASTNode    &rng  = *lhsCall.children[2];  // j1:j2
        endStack_.push_back(cols);                      // `end` in the column index = cols
        const std::string j1 = emitExpr(*rng.children[0]);
        const std::string j2 = emitExpr(*rng.children[1]);
        endStack_.pop_back();
        line("{");
        ++indent_;
        line("const std::size_t _nk_rows = " + rows + ";");
        line("const std::ptrdiff_t _nk_j1 = static_cast<std::ptrdiff_t>(" + j1 + ");");
        line("const std::ptrdiff_t _nk_j2 = static_cast<std::ptrdiff_t>(" + j2 + ");");
        line("if (_nk_j1 < 1 || _nk_j2 > static_cast<std::ptrdiff_t>(" + cols
             + ") || _nk_j2 < _nk_j1)");
        line("    throw std::out_of_range(\"numkit: column range out of bounds\");");
        line("const std::size_t _nk_nc = static_cast<std::size_t>(_nk_j2 - _nk_j1 + 1);");
        line("if ((" + dimExpr(B, 0) + " * " + dimExpr(B, 1) + ") != _nk_rows * _nk_nc)");
        line("    throw std::out_of_range(\"numkit: column-range assignment size mismatch\");");
        line("const std::size_t _nk_off = static_cast<std::size_t>(_nk_j1 - 1) * _nk_rows;");
        open("for (std::size_t _nk_i = 0; _nk_i < _nk_rows * _nk_nc; ++_nk_i)");
        line(ptr + "[_nk_off + _nk_i] = " + B.dataExpr + "[_nk_i];");
        close();
        --indent_;
        line("}");
        return;
    }

    // 2-D ROW-RANGE WRITE A(i1:i2, :) = B: write a 2-D (i2-i1+1) x cols matrix B into rows i1..i2
    // of a 2-D A (phase N23, the write sibling of the 2-D row-range read). STRIDED (a row block is
    // not contiguous in column-major): for each column j write the run A[(i1-1)+j*rows .. i2+j*rows)
    // from B's column j. A writable; bounds-checked range (end = rows); a numel guard on B
    // ((i2-i1+1)*cols). v1: 2-D DOUBLE A, a leading step-1 range i1:i2 + a trailing bare colon, rhs
    // a distinct 2-D DOUBLE array var. Placed before the rank-3+ trailing-range write.
    if ((ai.is2D || (ai.isND && ai.ndDims.size() == 2)) && ai.dtype == ValueType::DOUBLE
        && lhsCall.children.size() == 3
        && lhsCall.children[1]->type == NodeType::COLON_EXPR && lhsCall.children[1]->children.size() == 2
        && lhsCall.children[2]->type == NodeType::COLON_EXPR && lhsCall.children[2]->children.empty()
        && rhs.type == NodeType::IDENTIFIER && isArrayVar(rhs.strValue) && rhs.strValue != base
        && (arrays_.at(rhs.strValue).is2D
            || (arrays_.at(rhs.strValue).isND && arrays_.at(rhs.strValue).ndDims.size() == 2))
        && arrays_.at(rhs.strValue).dtype == ValueType::DOUBLE) {
        if (!ai.isLocal && !ai.isOutput)
            unsupported("row-range write to a read-only matrix parameter '" + base + "'");
        const ArrayInfo  &B    = arrays_.at(rhs.strValue);  // value source
        const std::string rows = dimExpr(ai, 0), cols = dimExpr(ai, 1);
        const ASTNode    &rng  = *lhsCall.children[1];  // i1:i2
        endStack_.push_back(rows);                      // `end` in the row index = rows
        const std::string i1 = emitExpr(*rng.children[0]);
        const std::string i2 = emitExpr(*rng.children[1]);
        endStack_.pop_back();
        line("{");
        ++indent_;
        line("const std::size_t _nk_rows = " + rows + ";");
        line("const std::size_t _nk_cols = " + cols + ";");
        line("const std::ptrdiff_t _nk_i1 = static_cast<std::ptrdiff_t>(" + i1 + ");");
        line("const std::ptrdiff_t _nk_i2 = static_cast<std::ptrdiff_t>(" + i2 + ");");
        line("if (_nk_i1 < 1 || _nk_i2 > static_cast<std::ptrdiff_t>(_nk_rows) || _nk_i2 < _nk_i1)");
        line("    throw std::out_of_range(\"numkit: row range out of bounds\");");
        line("const std::size_t _nk_nr = static_cast<std::size_t>(_nk_i2 - _nk_i1 + 1);");
        line("if ((" + dimExpr(B, 0) + " * " + dimExpr(B, 1) + ") != _nk_nr * _nk_cols)");
        line("    throw std::out_of_range(\"numkit: row-range assignment size mismatch\");");
        line("const std::size_t _nk_r0 = static_cast<std::size_t>(_nk_i1 - 1);");
        open("for (std::size_t _nk_j = 0; _nk_j < _nk_cols; ++_nk_j)");
        open("for (std::size_t _nk_r = 0; _nk_r < _nk_nr; ++_nk_r)");
        line(ptr + "[(_nk_r0 + _nk_r) + _nk_j * _nk_rows] = " + B.dataExpr
             + "[_nk_r + _nk_j * _nk_nr];");
        close();
        close();
        --indent_;
        line("}");
        return;
    }

    // 2-D BOTH-RANGE WRITE A(i1:i2, j1:j2) = B: write a 2-D (i2-i1+1) x (j2-j1+1) matrix B into the
    // sub-block rows i1..i2 x columns j1..j2 of a 2-D A (phase N24, the write sibling of the both-
    // range read). STRIDED: for each kept column cc = j1-1+c, write the row run A[(i1-1)+cc*rows ..
    // i2+cc*rows) from B's column c. A writable; both ranges bounds-checked; a numel guard on B
    // ((i2-i1+1)*(j2-j1+1)). v1: 2-D DOUBLE A, two step-1 ranges, rhs a distinct 2-D DOUBLE var.
    if ((ai.is2D || (ai.isND && ai.ndDims.size() == 2)) && ai.dtype == ValueType::DOUBLE
        && lhsCall.children.size() == 3
        && lhsCall.children[1]->type == NodeType::COLON_EXPR && lhsCall.children[1]->children.size() == 2
        && lhsCall.children[2]->type == NodeType::COLON_EXPR && lhsCall.children[2]->children.size() == 2
        && rhs.type == NodeType::IDENTIFIER && isArrayVar(rhs.strValue) && rhs.strValue != base
        && (arrays_.at(rhs.strValue).is2D
            || (arrays_.at(rhs.strValue).isND && arrays_.at(rhs.strValue).ndDims.size() == 2))
        && arrays_.at(rhs.strValue).dtype == ValueType::DOUBLE) {
        if (!ai.isLocal && !ai.isOutput)
            unsupported("block-range write to a read-only matrix parameter '" + base + "'");
        const ArrayInfo  &B    = arrays_.at(rhs.strValue);  // value source
        const std::string rows = dimExpr(ai, 0), cols = dimExpr(ai, 1);
        const ASTNode    &rr   = *lhsCall.children[1];  // i1:i2
        const ASTNode    &cr   = *lhsCall.children[2];  // j1:j2
        endStack_.push_back(rows);
        const std::string i1 = emitExpr(*rr.children[0]);
        const std::string i2 = emitExpr(*rr.children[1]);
        endStack_.pop_back();
        endStack_.push_back(cols);
        const std::string j1 = emitExpr(*cr.children[0]);
        const std::string j2 = emitExpr(*cr.children[1]);
        endStack_.pop_back();
        line("{");
        ++indent_;
        line("const std::size_t _nk_rows = " + rows + ";");
        line("const std::size_t _nk_cols = " + cols + ";");
        line("const std::ptrdiff_t _nk_i1 = static_cast<std::ptrdiff_t>(" + i1 + ");");
        line("const std::ptrdiff_t _nk_i2 = static_cast<std::ptrdiff_t>(" + i2 + ");");
        line("const std::ptrdiff_t _nk_j1 = static_cast<std::ptrdiff_t>(" + j1 + ");");
        line("const std::ptrdiff_t _nk_j2 = static_cast<std::ptrdiff_t>(" + j2 + ");");
        line("if (_nk_i1 < 1 || _nk_i2 > static_cast<std::ptrdiff_t>(_nk_rows) || _nk_i2 < _nk_i1)");
        line("    throw std::out_of_range(\"numkit: row range out of bounds\");");
        line("if (_nk_j1 < 1 || _nk_j2 > static_cast<std::ptrdiff_t>(_nk_cols) || _nk_j2 < _nk_j1)");
        line("    throw std::out_of_range(\"numkit: column range out of bounds\");");
        line("const std::size_t _nk_nr = static_cast<std::size_t>(_nk_i2 - _nk_i1 + 1);");
        line("const std::size_t _nk_nc = static_cast<std::size_t>(_nk_j2 - _nk_j1 + 1);");
        line("if ((" + dimExpr(B, 0) + " * " + dimExpr(B, 1) + ") != _nk_nr * _nk_nc)");
        line("    throw std::out_of_range(\"numkit: block-range assignment size mismatch\");");
        line("const std::size_t _nk_r0 = static_cast<std::size_t>(_nk_i1 - 1);");
        line("const std::size_t _nk_c0 = static_cast<std::size_t>(_nk_j1 - 1);");
        open("for (std::size_t _nk_c = 0; _nk_c < _nk_nc; ++_nk_c)");
        open("for (std::size_t _nk_r = 0; _nk_r < _nk_nr; ++_nk_r)");
        line(ptr + "[(_nk_r0 + _nk_r) + (_nk_c0 + _nk_c) * _nk_rows] = " + B.dataExpr
             + "[_nk_r + _nk_c * _nk_nr];");
        close();
        close();
        --indent_;
        line("}");
        return;
    }

    // trailing-RANGE WRITE A(:,...,:,k1:k2) = B: write a rank-r array B into the contiguous
    // trailing-dim block k1..k2 of a rank-r A (r >= 3; phase N20 r=3 page-range + N21 r=4 slab-
    // range write, the WRITE sibling of the trailing-range read). Column-major: the leading
    // (r-1) dims form a slab of size d0*..*d(r-2), and the trailing values k1..k2 are the
    // CONTIGUOUS block A[(k1-1)*slab .. k2*slab] -> a straight copy from B. A writable; bounds-
    // checked range (end = size(A,r)); a numel guard on B (slab*(k2-k1+1)). v1: rank-3/4 DOUBLE
    // A, (r-1) leading bare colons + a step-1 range k1:k2, rhs a distinct rank-r DOUBLE array
    // var. Placed before the N-D write branch.
    if (ai.isND && ai.ndDims.size() >= 3 && ai.dtype == ValueType::DOUBLE
        && lhsCall.children.size() == ai.ndDims.size() + 1
        && rhs.type == NodeType::IDENTIFIER && isArrayVar(rhs.strValue) && rhs.strValue != base
        && arrays_.at(rhs.strValue).isND
        && arrays_.at(rhs.strValue).ndDims.size() == ai.ndDims.size()
        && arrays_.at(rhs.strValue).dtype == ValueType::DOUBLE) {
        const std::size_t r = ai.ndDims.size();
        bool              ok = lhsCall.children[r]->type == NodeType::COLON_EXPR
                  && lhsCall.children[r]->children.size() == 2;
        for (std::size_t i = 1; i < r && ok; ++i)
            if (!(lhsCall.children[i]->type == NodeType::COLON_EXPR
                  && lhsCall.children[i]->children.empty()))
                ok = false;
        if (ok) {
            if (!ai.isLocal && !ai.isOutput)
                unsupported("range write to a read-only array parameter '" + base + "'");
            const ArrayInfo  &B       = arrays_.at(rhs.strValue);  // value source
            const std::string lastDim = dimExpr(ai, r - 1);
            const ASTNode    &rng     = *lhsCall.children[r];  // k1:k2
            endStack_.push_back(lastDim);                      // `end` in the range = size(A,r)
            const std::string k1 = emitExpr(*rng.children[0]);
            const std::string k2 = emitExpr(*rng.children[1]);
            endStack_.pop_back();
            line("{");
            ++indent_;
            std::string slabExpr, bNumel;
            for (std::size_t i = 0; i + 1 < r; ++i) {
                line("const std::size_t _nk_d" + std::to_string(i) + " = " + dimExpr(ai, i) + ";");
                slabExpr += (i ? " * " : "") + ("_nk_d" + std::to_string(i));
            }
            for (std::size_t i = 0; i < r; ++i) bNumel += (i ? " * " : "") + dimExpr(B, i);
            line("const std::size_t _nk_slab = " + slabExpr + ";");
            line("const std::ptrdiff_t _nk_k1 = static_cast<std::ptrdiff_t>(" + k1 + ");");
            line("const std::ptrdiff_t _nk_k2 = static_cast<std::ptrdiff_t>(" + k2 + ");");
            line("if (_nk_k1 < 1 || _nk_k2 > static_cast<std::ptrdiff_t>(" + lastDim
                 + ") || _nk_k2 < _nk_k1)");
            line("    throw std::out_of_range(\"numkit: range out of bounds\");");
            line("const std::size_t _nk_nb = static_cast<std::size_t>(_nk_k2 - _nk_k1 + 1);");
            line("if ((" + bNumel + ") != _nk_slab * _nk_nb)");
            line("    throw std::out_of_range(\"numkit: range assignment size mismatch\");");
            line("const std::size_t _nk_off = static_cast<std::size_t>(_nk_k1 - 1) * _nk_slab;");
            open("for (std::size_t _nk_i = 0; _nk_i < _nk_slab * _nk_nb; ++_nk_i)");
            line(ptr + "[_nk_off + _nk_i] = " + B.dataExpr + "[_nk_i];");
            close();
            --indent_;
            line("}");
            return;
        }
    }

    // GENERAL scalar/colon slice WRITE A(s_0..s_{r-1}) = rhs for a rank r >= 4 A (the WRITE
    // sibling of the rank-4+ general slice read). Each subscript is a bare colon or a scalar
    // (>=1 of each). The kept slice dims (colon -> size(A,k), scalar -> 1, trailing scalars
    // drop) describe a column-major region; rhs (a distinct array var) supplies the values in
    // column-major order. STRIDED scatter: A flat = base(from the scalar subscripts, kept AND
    // dropped) + sum over kept COLON dims of j_k * As_k; A[flat] = rhs[o]. Per-axis bounds
    // checks on the scalars + a numel guard on rhs. v1: rank-4+ writable DOUBLE A, all bare-
    // colon/scalar subscripts, rhs a distinct DOUBLE array var. Placed before the N-D write
    // branch (which refuses colon subscripts).
    if (ai.isND && ai.ndDims.size() >= 4 && ai.dtype == ValueType::DOUBLE
        && lhsCall.children.size() - 1 == ai.ndDims.size()
        && rhs.type == NodeType::IDENTIFIER && isArrayVar(rhs.strValue) && rhs.strValue != base
        && arrays_.at(rhs.strValue).dtype == ValueType::DOUBLE) {
        const std::size_t r = ai.ndDims.size();
        std::vector<bool> colon(r, false);
        int               lastCol = -1, scalarN = 0;
        bool              allCS = true;
        for (std::size_t k = 0; k < r; ++k) {
            const ASTNode &s     = *lhsCall.children[k + 1];
            const bool     isCol = s.type == NodeType::COLON_EXPR && s.children.empty();
            colon[k]             = isCol;
            if (isCol)
                lastCol = static_cast<int>(k);
            else if (s.type != NodeType::COLON_EXPR
                     && inferExpr(s, types_, reg_, classes_).type.shape.isScalar())
                ++scalarN;
            else {
                allCS = false;
                break;
            }
        }
        if (allCS && lastCol >= 0 && scalarN > 0) {
            if (!ai.isLocal && !ai.isOutput)
                unsupported("slice write to a read-only array parameter '" + base + "'");
            const std::size_t keptRank = static_cast<std::size_t>(lastCol) + 1 < 2
                                             ? 2u
                                             : static_cast<std::size_t>(lastCol) + 1;
            const ArrayInfo  &R        = arrays_.at(rhs.strValue);  // value source
            line("{");
            ++indent_;
            line("const std::size_t _nk_As0 = 1;");  // A column-major strides
            for (std::size_t k = 1; k < r; ++k)
                line("const std::size_t _nk_As" + std::to_string(k) + " = _nk_As"
                     + std::to_string(k - 1) + " * (" + dimExpr(ai, k - 1) + ");");
            line("std::size_t _nk_base = 0;");  // fixed offset from the scalar subscripts
            for (std::size_t k = 0; k < r; ++k) {
                if (colon[k]) continue;
                endStack_.push_back(dimExpr(ai, k));
                const std::string sk = emitExpr(*lhsCall.children[k + 1]);
                endStack_.pop_back();
                line("{ const std::ptrdiff_t _nk_s = static_cast<std::ptrdiff_t>(" + sk + ") - 1;");
                line("  if (_nk_s < 0 || _nk_s >= static_cast<std::ptrdiff_t>(" + dimExpr(ai, k)
                     + ")) throw std::out_of_range(\"numkit: slice index out of bounds\");");
                line("  _nk_base += static_cast<std::size_t>(_nk_s) * _nk_As" + std::to_string(k)
                     + "; }");
            }
            for (std::size_t kk = 0; kk < keptRank; ++kk)  // kept (output) dims
                line("const std::size_t _nk_Bd" + std::to_string(kk) + " = "
                     + (colon[kk] ? dimExpr(ai, kk) : std::string("1")) + ";");
            line("const std::size_t _nk_Bs0 = 1;");  // kept-region strides
            for (std::size_t kk = 1; kk < keptRank; ++kk)
                line("const std::size_t _nk_Bs" + std::to_string(kk) + " = _nk_Bs"
                     + std::to_string(kk - 1) + " * _nk_Bd" + std::to_string(kk - 1) + ";");
            std::string numel = "_nk_Bd0";
            for (std::size_t kk = 1; kk < keptRank; ++kk) numel += " * _nk_Bd" + std::to_string(kk);
            std::string rNumel;  // rhs element count
            if (R.isND) {
                rNumel = R.ndDims[0];
                for (std::size_t i = 1; i < R.ndDims.size(); ++i) rNumel += " * " + R.ndDims[i];
            } else if (R.is2D) {
                rNumel = dimExpr(R, 0) + " * " + dimExpr(R, 1);
            } else {
                rNumel = R.lenVar;
            }
            line("if ((" + rNumel + ") != (" + numel + "))");
            line("    throw std::out_of_range(\"numkit: slice assignment size mismatch\");");
            open("for (std::size_t _nk_o = 0; _nk_o < (" + numel + "); ++_nk_o)");
            line("std::size_t _nk_af = _nk_base;");
            for (std::size_t kk = 0; kk < keptRank; ++kk)
                if (colon[kk])
                    line("_nk_af += ((_nk_o / _nk_Bs" + std::to_string(kk) + ") % _nk_Bd"
                         + std::to_string(kk) + ") * _nk_As" + std::to_string(kk) + ";");
            line(ptr + "[_nk_af] = " + R.dataExpr + "[_nk_o];");
            close();
            --indent_;
            line("}");
            return;
        }
    }

    // Rank-N (N>=3) AND runtime-dim 2-D write A(i,j,k,...) = v -> column-major
    // nk_rt::indexN_set. The companions in ai.ndDims give the per-axis sizes, so a
    // SCALAR-subscript element store works for any rank, including a runtime-dim 2-D
    // matrix (the canonical `A = zeros(m,n); A(i,j) = v` fill). A colon subscript (a
    // slice write A(i,:) = row) is refused here -- scalar subscripts only (v1) -- so it
    // is a clean boundary rather than a mis-emit of emitExpr on a bare colon. EXCEPTION:
    // a single LOGICAL-array subscript A(A>0)=... is a masked scatter (linear over the
    // flat buffer), not an N-D element write -> fall through to the masked-write branch.
    bool singleLogicalMask = false;
    if (lhsCall.children.size() == 2) {
        const InferredType st = inferExpr(*lhsCall.children[1], types_, reg_, classes_).type;
        singleLogicalMask =
            st.isConcrete() && st.dtype == ValueType::LOGICAL && !st.shape.isScalar();
    }
    if (ai.isND && !singleLogicalMask) {
        if (!ai.isLocal && !ai.isOutput)
            unsupported("N-D write to a read-only matrix parameter '" + base + "'");
        if (lhsCall.children.size() - 1 != ai.ndDims.size())
            unsupported("N-D index arity for '" + base + "' (expected "
                        + std::to_string(ai.ndDims.size()) + " subscripts)");
        for (std::size_t i = 1; i < lhsCall.children.size(); ++i)
            if (lhsCall.children[i]->type == NodeType::COLON_EXPR)
                unsupported("N-D slice write (a colon subscript) for '" + base
                            + "' -- scalar subscripts only (v1)");
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

    // LOGICAL-INDEXING WRITE with an INLINE elementwise mask: `x(<expr>) = c` where <expr>
    // is a pure-elementwise LOGICAL array over x ITSELF (x(x<0)=0, A(A>lo & A<hi)=v, ...) and
    // c is a SCALAR. The mask is FUSED into the scatter loop -- no temp vector: for each flat
    // element i, if the per-element mask holds, x[i] = c. Works for a 1-D vector OR a 2-D / N-D
    // matrix -- the buffer is column-major flat, the mask is flat-elementwise, and the write is
    // flat, so it is rank-agnostic (bound on NUMEL). Restricted to a SELF-mask (the only array
    // operand is x): every reference is to x so indexing is in bounds, AND the fusion is sound
    // -- a per-element mask over x alone matches MATLAB's compute-all-then-scatter (setting x[i]
    // cannot change x[j!=i]'s mask). A non-elementwise mask (A(A>mean(A))=0) is rejected by
    // collectElementwise -> bridged, so fusion is never wrong. x writable; a mask over OTHER
    // arrays / an array rhs -> refused. After the logical-VAR write (a pre-bound mask var takes
    // that simpler branch).
    if (lhsCall.children.size() == 2 && lhsCall.children[1]->type != NodeType::COLON_EXPR
        && lhsCall.children[1]->type != NodeType::IDENTIFIER) {
        const AbstractValue   maskAV = inferExpr(*lhsCall.children[1], types_, reg_, classes_);
        std::set<std::string> maskArrays;
        const bool            pureEw = collectElementwise(*lhsCall.children[1], maskArrays);
        if (maskAV.type.isConcrete() && maskAV.type.dtype == ValueType::LOGICAL
            && !maskAV.type.shape.isScalar() && pureEw && maskArrays.size() == 1
            && *maskArrays.begin() == base) {
            if (!ai.isLocal && !ai.isOutput)
                unsupported("logical-indexing write to a read-only array parameter '" + base + "'");
            const AbstractValue rhsScalar = inferExpr(rhs, types_, reg_, classes_);
            if (!rhsScalar.type.isConcrete() || !rhsScalar.type.shape.isScalar())
                unsupported("logical-indexing write (inline mask): a scalar rhs only (v1)");
            std::string numel;  // flat element count (1-D length / 2-D rows*cols / N-D product)
            if (ai.isND) {
                numel = ai.ndDims[0];
                for (std::size_t i = 1; i < ai.ndDims.size(); ++i) numel += " * " + ai.ndDims[i];
            } else if (ai.is2D) {
                numel = ai.rowsVar + " * " + ai.colsVar;
            } else {
                numel = ai.lenVar;
            }
            line("{");
            ++indent_;
            line("const " + cppScalarType(ai.dtype) + " _nk_c = " + emitExpr(rhs) + ";");
            elementCtx_ = "_nk_i";  // whole x in the mask -> x[_nk_i] (flat)
            const std::string maskExpr = emitExpr(*lhsCall.children[1]);
            elementCtx_.clear();
            open("for (std::size_t _nk_i = 0; _nk_i < (" + numel + "); ++_nk_i)");
            line("if (" + maskExpr + ") " + ptr + "[_nk_i] = _nk_c;");
            close();
            --indent_;
            line("}");
            return;
        }
    }

    // NUMERIC SCATTER WRITE: `x(idx) = v` where idx is a 1-D NUMERIC (DOUBLE) index vector ->
    // assign into x at the 1-based positions idx. rhs is EITHER a SCALAR (broadcast: x(idx[i])
    // = c for all i) OR a distinct 1-D DOUBLE array of length numel(idx) (x(idx[i]) = rhs[i]).
    // The WRITE sibling of the numeric gather read. Each index is range+integer checked BEFORE
    // the cast (a NaN fails `>= 1.0`, so no UB; the throw is faithful to MATLAB's subscript
    // error). Repeated indices keep the LAST write -- a forward loop matches MATLAB. x writable
    // 1-D DOUBLE; idx a DOUBLE array VAR distinct from x. An inline-literal idx / a 2-D x / a
    // length-mismatched or aliasing array rhs -> refused (sound). Placed after the logical
    // writes (a LOGICAL idx takes those; this is the numeric sibling).
    if (lhsCall.children.size() == 2 && !ai.is2D && !ai.isND && ai.dtype == ValueType::DOUBLE
        && lhsCall.children[1]->type == NodeType::IDENTIFIER
        && isArrayVar(lhsCall.children[1]->strValue)
        && arrays_.at(lhsCall.children[1]->strValue).dtype == ValueType::DOUBLE
        && !arrays_.at(lhsCall.children[1]->strValue).is2D
        && !arrays_.at(lhsCall.children[1]->strValue).isND
        && lhsCall.children[1]->strValue != base) {
        if (!ai.isLocal && !ai.isOutput)
            unsupported("numeric scatter write to a read-only array parameter '" + base + "'");
        const ArrayInfo    &bi  = arrays_.at(lhsCall.children[1]->strValue);  // index vector
        const AbstractValue rav = inferExpr(rhs, types_, reg_, classes_);
        const bool rhsScalar = rav.type.isConcrete() && rav.type.shape.isScalar();
        const bool rhsArr    = rhs.type == NodeType::IDENTIFIER && isArrayVar(rhs.strValue)
                            && rhs.strValue != base && rhs.strValue != lhsCall.children[1]->strValue
                            && !arrays_.at(rhs.strValue).is2D && !arrays_.at(rhs.strValue).isND
                            && arrays_.at(rhs.strValue).dtype == ValueType::DOUBLE;
        if (!rhsScalar && !rhsArr)
            unsupported("numeric scatter write rhs: a scalar or a distinct 1-D DOUBLE array (v1)");
        line("{");
        ++indent_;
        if (rhsScalar)
            line("const double _nk_c = " + emitExpr(rhs) + ";");
        else {
            line("if (" + arrays_.at(rhs.strValue).lenVar + " != " + bi.lenVar + ")");
            line("    throw std::out_of_range(\"numkit: scatter assignment length mismatch\");");
        }
        const std::string rhsElem =
            rhsScalar ? std::string("_nk_c") : (arrays_.at(rhs.strValue).dataExpr + "[_nk_i]");
        open("for (std::size_t _nk_i = 0; _nk_i < " + bi.lenVar + "; ++_nk_i)");
        line("const double _nk_d = " + bi.dataExpr + "[_nk_i];");
        line("if (!(_nk_d >= 1.0 && _nk_d <= static_cast<double>(" + ai.lenVar
             + ") && _nk_d == std::floor(_nk_d)))");
        line("    throw std::out_of_range(\"numkit: array index out of bounds\");");
        line(ptr + "[static_cast<std::size_t>(_nk_d) - 1] = " + rhsElem + ";");
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
        const bool isMath = (nargs == 1 && (unaryMathStd(callee) != nullptr || callee == "sign"))
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
        // logspace(a, b [, n]) -> n decade-spaced points 10^a .. 10^b (= 10^linspace).
        // Mirrors the linspace fill but each slot is 10^(a + i*step); the last point
        // is seeded to exactly 10^b. n==1 -> {10^b}; n<=0 -> empty; the 2-arg form
        // defaults n=50 (MATLAB logspace, NOT 100). v1: an owned LOCAL, 2- or 3-arg.
        // (The logspace(a,pi,n) "upper limit = pi" quirk is a deferred gap.)
        if (isArrayVar(name) && arrays_.at(name).isLocal
            && rhs.type == NodeType::CALL && !rhs.children.empty()
            && rhs.children[0]->type == NodeType::IDENTIFIER
            && rhs.children[0]->strValue == "logspace"
            && (rhs.children.size() == 3 || rhs.children.size() == 4)) {
            const std::string a = emitExpr(*rhs.children[1]);
            const std::string b = emitExpr(*rhs.children[2]);
            const std::string n = rhs.children.size() == 4
                                      ? ("nk_rt::dim(" + emitExpr(*rhs.children[3]) + ")")
                                      : std::string("50");  // 2-arg default (MATLAB logspace)
            line("{");
            ++indent_;
            line("const double _nk_a = " + a + ";");
            line("const double _nk_b = " + b + ";");
            line("const std::size_t _nk_n = " + n + ";");
            line(name + ".assign(_nk_n, std::pow(10.0, _nk_b));");  // last point already 10^b
            open("if (_nk_n >= 2)");
            line("const double _nk_step = (_nk_b - _nk_a) / static_cast<double>(_nk_n - 1);");
            open("for (std::size_t _nk_i = 0; _nk_i + 1 < _nk_n; ++_nk_i)");
            line(name + "[_nk_i] = std::pow(10.0, _nk_a + static_cast<double>(_nk_i) * _nk_step);");
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
        // y = x(:) -> all elements of x as a column vector (column-major order), a
        // fresh 1-D LOCAL. The bare colon parses as an EMPTY COLON_EXPR. Column-major
        // storage means the flat buffer IS the column order, so x(:) is a straight
        // copy of numel(x) elements (1-D length / 2-D rows*cols / N-D product of dims).
        // v1: x a 1-D/2-D/N-D DOUBLE array var; result a 1-D LOCAL.
        if (isArrayVar(name) && arrays_.at(name).isLocal && !arrays_.at(name).is2D
            && !arrays_.at(name).isND && rhs.type == NodeType::CALL && rhs.children.size() == 2
            && rhs.children[0]->type == NodeType::IDENTIFIER && isArrayVar(rhs.children[0]->strValue)
            && rhs.children[1]->type == NodeType::COLON_EXPR && rhs.children[1]->children.empty()
            && arrays_.at(rhs.children[0]->strValue).dtype == ValueType::DOUBLE) {
            const ArrayInfo    &x   = arrays_.at(rhs.children[0]->strValue);
            const AbstractValue res = inferExpr(rhs, types_, reg_, classes_);
            if (res.type.isConcrete() && !res.type.shape.isScalar()) {
                std::string numel;
                if (x.isND) {
                    numel = x.ndDims[0];
                    for (std::size_t i = 1; i < x.ndDims.size(); ++i) numel += " * " + x.ndDims[i];
                } else if (x.is2D) {
                    numel = x.rowsVar + " * " + x.colsVar;
                } else {
                    numel = x.lenVar;
                }
                line(name + ".assign(" + x.dataExpr + ", " + x.dataExpr + " + (" + numel + "));");
                types_.set(name, res);
                return;
            }
        }
        // A(:, j) -> column j of a 2-D matrix as a column vector, a fresh 1-D LOCAL.
        // Column-major storage: column j (1-based) is the CONTIGUOUS slice
        // A[(j-1)*rows .. +rows), so it's a straight copy. `end` in the column index
        // = cols (pushed). Bounds-checked j in [1, cols]. v1: A a 2-D DOUBLE matrix
        // var, j a scalar; result a 1-D LOCAL of `rows`. (A(i,:) row slice = strided,
        // deferred; A(:,:) -> both colons, deferred.)
        if (isArrayVar(name) && arrays_.at(name).isLocal && !arrays_.at(name).is2D
            && !arrays_.at(name).isND && rhs.type == NodeType::CALL && rhs.children.size() == 3
            && rhs.children[0]->type == NodeType::IDENTIFIER && isArrayVar(rhs.children[0]->strValue)
            && (arrays_.at(rhs.children[0]->strValue).is2D
                || (arrays_.at(rhs.children[0]->strValue).isND
                    && arrays_.at(rhs.children[0]->strValue).ndDims.size() == 2))
            && arrays_.at(rhs.children[0]->strValue).dtype == ValueType::DOUBLE
            && rhs.children[1]->type == NodeType::COLON_EXPR && rhs.children[1]->children.empty()
            && rhs.children[2]->type != NodeType::COLON_EXPR) {
            const ArrayInfo    &A     = arrays_.at(rhs.children[0]->strValue);
            const std::string   Arows = dimExpr(A, 0), Acols = dimExpr(A, 1);  // rank-agnostic
            const AbstractValue res   = inferExpr(rhs, types_, reg_, classes_);
            if (res.type.isConcrete() && !res.type.shape.isScalar()
                && inferExpr(*rhs.children[2], types_, reg_, classes_).type.shape.isScalar()) {
                endStack_.push_back(Acols);  // `end` in the column index = cols
                const std::string j = emitExpr(*rhs.children[2]);
                endStack_.pop_back();
                line("{");
                ++indent_;
                line("const std::ptrdiff_t _nk_j = static_cast<std::ptrdiff_t>(" + j + ");");
                line("if (_nk_j < 1 || _nk_j > static_cast<std::ptrdiff_t>(" + Acols + "))");
                line("    throw std::out_of_range(\"numkit: column index out of bounds\");");
                line("const std::size_t _nk_off = static_cast<std::size_t>(_nk_j - 1) * "
                     + Arows + ";");
                line(name + ".assign(" + A.dataExpr + " + _nk_off, " + A.dataExpr + " + _nk_off + "
                     + Arows + ");");
                --indent_;
                line("}");
                types_.set(name, res);
                return;
            }
        }
        // A(i, :) -> row i of a 2-D matrix as a row vector, a fresh 1-D LOCAL. Row i
        // (1-based) is STRIDED in column-major storage: out[j] = A[(i-1) + j*rows] for
        // j in [0, cols). `end` in the ROW index = rows (pushed). Bounds-checked i in
        // [1, rows]. v1: A a 2-D DOUBLE matrix var, i a scalar; result a 1-D LOCAL of
        // `cols`. (The mirror of A(:,j); A(:,:) -- both colons -- is deferred.)
        if (isArrayVar(name) && arrays_.at(name).isLocal && !arrays_.at(name).is2D
            && !arrays_.at(name).isND && rhs.type == NodeType::CALL && rhs.children.size() == 3
            && rhs.children[0]->type == NodeType::IDENTIFIER && isArrayVar(rhs.children[0]->strValue)
            && (arrays_.at(rhs.children[0]->strValue).is2D
                || (arrays_.at(rhs.children[0]->strValue).isND
                    && arrays_.at(rhs.children[0]->strValue).ndDims.size() == 2))
            && arrays_.at(rhs.children[0]->strValue).dtype == ValueType::DOUBLE
            && rhs.children[1]->type != NodeType::COLON_EXPR
            && rhs.children[2]->type == NodeType::COLON_EXPR && rhs.children[2]->children.empty()) {
            const ArrayInfo    &A     = arrays_.at(rhs.children[0]->strValue);
            const std::string   Arows = dimExpr(A, 0), Acols = dimExpr(A, 1);  // rank-agnostic
            const AbstractValue res   = inferExpr(rhs, types_, reg_, classes_);
            if (res.type.isConcrete() && !res.type.shape.isScalar()
                && inferExpr(*rhs.children[1], types_, reg_, classes_).type.shape.isScalar()) {
                endStack_.push_back(Arows);  // `end` in the row index = rows
                const std::string i = emitExpr(*rhs.children[1]);
                endStack_.pop_back();
                line("{");
                ++indent_;
                line("const std::ptrdiff_t _nk_i0 = static_cast<std::ptrdiff_t>(" + i + ") - 1;");
                line("if (_nk_i0 < 0 || _nk_i0 >= static_cast<std::ptrdiff_t>(" + Arows + "))");
                line("    throw std::out_of_range(\"numkit: row index out of bounds\");");
                line(name + ".assign(" + Acols + ", 0.0);");
                open("for (std::size_t _nk_j = 0; _nk_j < " + Acols + "; ++_nk_j)");
                line(name + "[_nk_j] = " + A.dataExpr + "[static_cast<std::size_t>(_nk_i0) + _nk_j * "
                     + Arows + "];");
                close();
                --indent_;
                line("}");
                types_.set(name, res);
                return;
            }
        }
        // Native diag(A) -> the diagonal of a 2-D matrix as a fresh 1-D LOCAL. Column-
        // major: the (i,i) element is A[i + i*rows]; the length is min(rows, cols).
        // v1: A a 2-D DOUBLE matrix var; result a 1-D LOCAL (push_back).
        if (isArrayVar(name) && arrays_.at(name).isLocal && !arrays_.at(name).is2D
            && !arrays_.at(name).isND && rhs.type == NodeType::CALL && rhs.children.size() == 2
            && rhs.children[0]->type == NodeType::IDENTIFIER && rhs.children[0]->strValue == "diag"
            && rhs.children[1]->type == NodeType::IDENTIFIER && isArrayVar(rhs.children[1]->strValue)
            && arrays_.at(rhs.children[1]->strValue).is2D
            && arrays_.at(rhs.children[1]->strValue).dtype == ValueType::DOUBLE) {
            const ArrayInfo    &A   = arrays_.at(rhs.children[1]->strValue);
            const AbstractValue res = inferExpr(rhs, types_, reg_, classes_);
            if (res.type.isConcrete() && !res.type.shape.isScalar()) {
                line("{");
                ++indent_;
                line("const std::size_t _nk_d = " + A.rowsVar + " < " + A.colsVar + " ? " + A.rowsVar
                     + " : " + A.colsVar + ";");
                line(name + ".clear();");
                open("for (std::size_t _nk_i = 0; _nk_i < _nk_d; ++_nk_i)");
                line(name + ".push_back(" + A.dataExpr + "[_nk_i + _nk_i * " + A.rowsVar + "]);");
                close();
                --indent_;
                line("}");
                types_.set(name, res);
                return;
            }
        }
        // diag(v[,k]) with v a VECTOR -> an N x N diagonal MATRIX (N = numel(v) + k), a
        // rank-2 ndRuntimeLocal. k=0: M[i + i*N] = v[i] (main diagonal). A non-negative
        // LITERAL offset k places v on the k-th super-diagonal: M[i + (i+k)*N] = v[i]. v1:
        // v a 1-D DOUBLE vector var; k (if present) a non-negative literal integer. A
        // negative k (`diag(v,-1)`, a sub-diagonal) parses as a unary-minus -> not folded
        // -> diagTransfer keeps it Dynamic -> bridged; a runtime k is likewise bridged.
        if (isArrayVar(name) && arrays_.at(name).isLocal && arrays_.at(name).ndRuntimeLocal
            && arrays_.at(name).ndDims.size() == 2 && rhs.type == NodeType::CALL
            && (rhs.children.size() == 2
                || (rhs.children.size() == 3 && rhs.children[2]->type == NodeType::NUMBER_LITERAL
                    && rhs.children[2]->numValue == std::floor(rhs.children[2]->numValue)
                    && rhs.children[2]->numValue >= 0.0))
            && rhs.children[0]->type == NodeType::IDENTIFIER && rhs.children[0]->strValue == "diag"
            && rhs.children[1]->type == NodeType::IDENTIFIER && isArrayVar(rhs.children[1]->strValue)
            && !arrays_.at(rhs.children[1]->strValue).is2D
            && !arrays_.at(rhs.children[1]->strValue).isND
            && arrays_.at(rhs.children[1]->strValue).dtype == ValueType::DOUBLE) {
            const ArrayInfo    &v   = arrays_.at(rhs.children[1]->strValue);
            const ArrayInfo    &M   = arrays_.at(name);
            const AbstractValue res = inferExpr(rhs, types_, reg_, classes_);
            const std::string   k =  // non-negative literal offset (0 = main diagonal)
                rhs.children.size() == 3
                    ? std::to_string(static_cast<long>(rhs.children[2]->numValue))
                    : std::string("0");
            const std::string N = "(" + v.lenVar + " + " + k + ")";  // N = n + k
            line("{");
            ++indent_;
            line("const std::size_t _nk_N = " + N + ";");
            line(M.ndDims[0] + " = _nk_N;");  // rows = N
            line(M.ndDims[1] + " = _nk_N;");  // cols = N
            line(name + ".assign(_nk_N * _nk_N, 0.0);");
            open("for (std::size_t _nk_i = 0; _nk_i < " + v.lenVar + "; ++_nk_i)");
            // v[i] on the k-th super-diagonal: (row i, col i+k), column-major.
            line(name + "[_nk_i + (_nk_i + " + k + ") * _nk_N] = " + v.dataExpr + "[_nk_i];");
            close();
            --indent_;
            line("}");
            types_.set(name, res);
            return;
        }
        // Native cross(a, b) -> the 3-D cross product, a fresh 3-element 1-D LOCAL:
        // c = [a2*b3-a3*b2, a3*b1-a1*b3, a1*b2-a2*b1] (1-based; 0-based in the emit).
        // v1: a, b 1-D DOUBLE array vars (length 3).
        if (isArrayVar(name) && arrays_.at(name).isLocal && !arrays_.at(name).is2D
            && !arrays_.at(name).isND && rhs.type == NodeType::CALL && rhs.children.size() == 3
            && rhs.children[0]->type == NodeType::IDENTIFIER && rhs.children[0]->strValue == "cross"
            && rhs.children[1]->type == NodeType::IDENTIFIER && isArrayVar(rhs.children[1]->strValue)
            && rhs.children[2]->type == NodeType::IDENTIFIER && isArrayVar(rhs.children[2]->strValue)
            && !arrays_.at(rhs.children[1]->strValue).is2D
            && !arrays_.at(rhs.children[1]->strValue).isND
            && !arrays_.at(rhs.children[2]->strValue).is2D
            && !arrays_.at(rhs.children[2]->strValue).isND
            && arrays_.at(rhs.children[1]->strValue).dtype == ValueType::DOUBLE
            && arrays_.at(rhs.children[2]->strValue).dtype == ValueType::DOUBLE) {
            const ArrayInfo    &a   = arrays_.at(rhs.children[1]->strValue);
            const ArrayInfo    &b   = arrays_.at(rhs.children[2]->strValue);
            const AbstractValue res = inferExpr(rhs, types_, reg_, classes_);
            if (res.type.isConcrete() && !res.type.shape.isScalar()) {
                const std::string ad = a.dataExpr, bd = b.dataExpr;
                line("{");
                ++indent_;
                line(name + ".clear();");
                line(name + ".push_back(" + ad + "[1] * " + bd + "[2] - " + ad + "[2] * " + bd
                     + "[1]);");
                line(name + ".push_back(" + ad + "[2] * " + bd + "[0] - " + ad + "[0] * " + bd
                     + "[2]);");
                line(name + ".push_back(" + ad + "[0] * " + bd + "[1] - " + ad + "[1] * " + bd
                     + "[0]);");
                --indent_;
                line("}");
                types_.set(name, res);
                return;
            }
        }
        // reshape(x, m, n) -> reinterpret x's elements as an m x n matrix. Column-major
        // storage means a reshape is just the SAME flat buffer reinterpreted, so copy
        // x's elements into the 2-D KnownDims LOCAL (numel must match: m*n==numel(x),
        // runtime-checked). v(i,j) 2-D indexing then works on the result. v1: x a
        // 1-D/2-D DOUBLE array var, m,n compile-time literals -> a 2-D LOCAL.
        if (isArrayVar(name) && arrays_.at(name).isLocal && arrays_.at(name).is2D
            && rhs.type == NodeType::CALL && rhs.children.size() == 4
            && rhs.children[0]->type == NodeType::IDENTIFIER
            && rhs.children[0]->strValue == "reshape"
            && rhs.children[1]->type == NodeType::IDENTIFIER && isArrayVar(rhs.children[1]->strValue)
            && arrays_.at(rhs.children[1]->strValue).dtype == ValueType::DOUBLE) {
            const ArrayInfo    &v   = arrays_.at(name);  // the 2-D result
            const ArrayInfo    &x   = arrays_.at(rhs.children[1]->strValue);
            const AbstractValue res = inferExpr(rhs, types_, reg_, classes_);
            if (res.type.isConcrete() && res.type.shape.kind == ShapeKind::KnownDims) {
                std::string xnumel;
                if (x.isND) {
                    xnumel = x.ndDims[0];
                    for (std::size_t i = 1; i < x.ndDims.size(); ++i) xnumel += " * " + x.ndDims[i];
                } else if (x.is2D) {
                    xnumel = x.rowsVar + " * " + x.colsVar;
                } else {
                    xnumel = x.lenVar;
                }
                line("{");
                ++indent_;
                line("if ((" + v.rowsVar + " * " + v.colsVar + ") != (" + xnumel + "))");
                line("    throw std::out_of_range(\"numkit: reshape element count mismatch\");");
                line(name + ".assign(" + x.dataExpr + ", " + x.dataExpr + " + (" + v.rowsVar + " * "
                     + v.colsVar + "));");
                --indent_;
                line("}");
                types_.set(name, res);
                return;
            }
        }
        // reshape(x, m, n) with a RUNTIME dim -> reinterpret x's flat data as a runtime-dim
        // 2-D matrix (same column-major buffer). Copy x's elements, set the dim companions
        // from the args via nk_rt::dim, numel-guard (m*n == numel(x)). The runtime mirror of
        // the KnownDims reshape above. v1: x a DOUBLE array var, distinct from the dest (an
        // in-place A=reshape(A,..) would self-alias the .assign -> refused via the guard).
        if (isArrayVar(name) && arrays_.at(name).isLocal && arrays_.at(name).ndRuntimeLocal
            && arrays_.at(name).ndDims.size() == 2 && rhs.type == NodeType::CALL
            && rhs.children.size() == 4 && rhs.children[0]->type == NodeType::IDENTIFIER
            && rhs.children[0]->strValue == "reshape"
            && rhs.children[1]->type == NodeType::IDENTIFIER && isArrayVar(rhs.children[1]->strValue)
            && rhs.children[1]->strValue != name
            && arrays_.at(rhs.children[1]->strValue).dtype == ValueType::DOUBLE) {
            const ArrayInfo    &M   = arrays_.at(name);
            const ArrayInfo    &x   = arrays_.at(rhs.children[1]->strValue);
            const AbstractValue res = inferExpr(rhs, types_, reg_, classes_);
            if (res.type.isConcrete()) {
                std::string xnumel;
                if (x.isND) {
                    xnumel = x.ndDims[0];
                    for (std::size_t i = 1; i < x.ndDims.size(); ++i) xnumel += " * " + x.ndDims[i];
                } else if (x.is2D) {
                    xnumel = x.rowsVar + " * " + x.colsVar;
                } else {
                    xnumel = x.lenVar;
                }
                line("{");
                ++indent_;
                line("const std::size_t _nk_m = nk_rt::dim(" + emitExpr(*rhs.children[2]) + ");");
                line("const std::size_t _nk_n = nk_rt::dim(" + emitExpr(*rhs.children[3]) + ");");
                line("const std::size_t _nk_xn = (" + xnumel + ");");
                line("if (_nk_m * _nk_n != _nk_xn)");
                line("    throw std::out_of_range(\"numkit: reshape element count mismatch\");");
                line(M.ndDims[0] + " = _nk_m;");
                line(M.ndDims[1] + " = _nk_n;");
                line(name + ".assign(" + x.dataExpr + ", " + x.dataExpr + " + _nk_xn);");
                --indent_;
                line("}");
                types_.set(name, res);
                return;
            }
        }
        // reshape(x, d1, ..., dN) -> reinterpret x's flat data as a RANK-N array (N = number of
        // dim args >= 3; same column-major buffer; phases N5/N14). Copy x's elements, set the N
        // dim companions from nk_rt::dim(arg), numel-guard (product == numel(x)). Rank-agnostic
        // (covers rank-3, rank-4, ...). v1: x a DOUBLE array var distinct from the dest (an
        // in-place A=reshape(A,..) would self-alias the .assign -> refused).
        if (isArrayVar(name) && arrays_.at(name).isLocal && arrays_.at(name).ndRuntimeLocal
            && arrays_.at(name).ndDims.size() >= 3 && rhs.type == NodeType::CALL
            && rhs.children.size() == arrays_.at(name).ndDims.size() + 2
            && rhs.children[0]->type == NodeType::IDENTIFIER
            && rhs.children[0]->strValue == "reshape"
            && rhs.children[1]->type == NodeType::IDENTIFIER && isArrayVar(rhs.children[1]->strValue)
            && rhs.children[1]->strValue != name
            && arrays_.at(rhs.children[1]->strValue).dtype == ValueType::DOUBLE) {
            const ArrayInfo    &M   = arrays_.at(name);
            const ArrayInfo    &x   = arrays_.at(rhs.children[1]->strValue);
            const std::size_t   N   = M.ndDims.size();  // result rank = number of dim args
            const AbstractValue res = inferExpr(rhs, types_, reg_, classes_);
            if (res.type.isConcrete()) {
                std::string xnumel;
                if (x.isND) {
                    xnumel = x.ndDims[0];
                    for (std::size_t i = 1; i < x.ndDims.size(); ++i) xnumel += " * " + x.ndDims[i];
                } else if (x.is2D) {
                    xnumel = x.rowsVar + " * " + x.colsVar;
                } else {
                    xnumel = x.lenVar;
                }
                line("{");
                ++indent_;
                for (std::size_t k = 0; k < N; ++k)
                    line("const std::size_t _nk_d" + std::to_string(k) + " = nk_rt::dim("
                         + emitExpr(*rhs.children[2 + k]) + ");");
                std::string prod = "_nk_d0";
                for (std::size_t k = 1; k < N; ++k) prod += " * _nk_d" + std::to_string(k);
                line("const std::size_t _nk_xn = (" + xnumel + ");");
                line("if ((" + prod + ") != _nk_xn)");
                line("    throw std::out_of_range(\"numkit: reshape element count mismatch\");");
                for (std::size_t k = 0; k < N; ++k)
                    line(M.ndDims[k] + " = _nk_d" + std::to_string(k) + ";");
                line(name + ".assign(" + x.dataExpr + ", " + x.dataExpr + " + _nk_xn);");
                --indent_;
                line("}");
                types_.set(name, res);
                return;
            }
        }
        // repmat(s, m, n) / repmat(s, n) with s a SCALAR -> an m x n (or n x n) matrix
        // all = s, a 2-D KnownDims LOCAL (the tiling of a 1x1). Like zeros/ones but the
        // fill is the scalar value. v1: s a DOUBLE scalar, m,n literals. (repmat of a
        // vector/matrix = true tiling, deferred.)
        if (isArrayVar(name) && arrays_.at(name).isLocal && arrays_.at(name).is2D
            && rhs.type == NodeType::CALL
            && (rhs.children.size() == 3 || rhs.children.size() == 4)
            && rhs.children[0]->type == NodeType::IDENTIFIER
            && rhs.children[0]->strValue == "repmat"
            && inferExpr(*rhs.children[1], types_, reg_, classes_).type.shape.isScalar()) {
            const ArrayInfo    &v   = arrays_.at(name);
            const AbstractValue res = inferExpr(rhs, types_, reg_, classes_);
            if (res.type.isConcrete() && res.type.shape.kind == ShapeKind::KnownDims) {
                const std::string s = emitExpr(*rhs.children[1]);
                line(name + ".assign(static_cast<std::size_t>(" + v.rowsVar + " * " + v.colsVar
                     + "), " + s + ");");
                types_.set(name, res);
                return;
            }
        }
        // repmat(x, 1, q) with x a ROW vector -> tile q copies into a 1 x (q*n) row,
        // a fresh 1-D LOCAL: out[k*n + i] = x[i] for k in [0,q), i in [0,n). The down-
        // rep (children[2]) must be the literal 1. v1: x a 1-D DOUBLE row var.
        if (isArrayVar(name) && arrays_.at(name).isLocal && !arrays_.at(name).is2D
            && !arrays_.at(name).isND && rhs.type == NodeType::CALL && rhs.children.size() == 4
            && rhs.children[0]->type == NodeType::IDENTIFIER
            && rhs.children[0]->strValue == "repmat"
            && rhs.children[1]->type == NodeType::IDENTIFIER && isArrayVar(rhs.children[1]->strValue)
            && !arrays_.at(rhs.children[1]->strValue).is2D
            && !arrays_.at(rhs.children[1]->strValue).isND
            && arrays_.at(rhs.children[1]->strValue).dtype == ValueType::DOUBLE
            && rhs.children[2]->type == NodeType::NUMBER_LITERAL
            && rhs.children[2]->numValue == 1.0) {
            const ArrayInfo    &x   = arrays_.at(rhs.children[1]->strValue);
            const AbstractValue res = inferExpr(rhs, types_, reg_, classes_);
            if (res.type.isConcrete() && !res.type.shape.isScalar()) {
                const std::string q = emitExpr(*rhs.children[3]);
                line("{");
                ++indent_;
                line("const std::size_t _nk_q = nk_rt::dim(" + q + ");");
                line(name + ".clear();");
                open("for (std::size_t _nk_k = 0; _nk_k < _nk_q; ++_nk_k)");
                open("for (std::size_t _nk_i = 0; _nk_i < " + x.lenVar + "; ++_nk_i)");
                line(name + ".push_back(" + x.dataExpr + "[_nk_i]);");
                close();
                close();
                --indent_;
                line("}");
                types_.set(name, res);
                return;
            }
        }
        // repmat(x, p, 1) with x a COL vector -> p copies stacked into a (p*n) x 1
        // column, a fresh 1-D LOCAL: out[k*n + i] = x[i]. The mirror of the row tile;
        // the across-rep (children[3]) must be the literal 1, reps = children[2]. A
        // row operand here yields a Dynamic result (2-D) -> the concrete-result guard
        // gates it out. v1: x a 1-D DOUBLE col var.
        if (isArrayVar(name) && arrays_.at(name).isLocal && !arrays_.at(name).is2D
            && !arrays_.at(name).isND && rhs.type == NodeType::CALL && rhs.children.size() == 4
            && rhs.children[0]->type == NodeType::IDENTIFIER
            && rhs.children[0]->strValue == "repmat"
            && rhs.children[1]->type == NodeType::IDENTIFIER && isArrayVar(rhs.children[1]->strValue)
            && !arrays_.at(rhs.children[1]->strValue).is2D
            && !arrays_.at(rhs.children[1]->strValue).isND
            && arrays_.at(rhs.children[1]->strValue).dtype == ValueType::DOUBLE
            && rhs.children[3]->type == NodeType::NUMBER_LITERAL
            && rhs.children[3]->numValue == 1.0) {
            const ArrayInfo    &x   = arrays_.at(rhs.children[1]->strValue);
            const AbstractValue res = inferExpr(rhs, types_, reg_, classes_);
            if (res.type.isConcrete() && !res.type.shape.isScalar()) {
                const std::string p = emitExpr(*rhs.children[2]);
                line("{");
                ++indent_;
                line("const std::size_t _nk_p = nk_rt::dim(" + p + ");");
                line(name + ".clear();");
                open("for (std::size_t _nk_k = 0; _nk_k < _nk_p; ++_nk_k)");
                open("for (std::size_t _nk_i = 0; _nk_i < " + x.lenVar + "; ++_nk_i)");
                line(name + ".push_back(" + x.dataExpr + "[_nk_i]);");
                close();
                close();
                --indent_;
                line("}");
                types_.set(name, res);
                return;
            }
        }
        // repmat(x, p, q) with x a ROW vector and p>1 -> a true 2-D p x (q*n) tiling, a
        // rank-2 ndRuntimeLocal (rows = p known, cols = q*n runtime; n = x's length).
        // Column-major: M[r + c*p] = x[c % n] -- every output row is a copy of the 1xn
        // source, and the columns repeat the source every n. The (1,q) row tile and the
        // (p,1) col tile above stay 1-D; this is the genuinely-2-D case. v1: x a 1-D
        // DOUBLE row var, p,q literals, p>1.
        if (isArrayVar(name) && arrays_.at(name).isLocal && arrays_.at(name).ndRuntimeLocal
            && arrays_.at(name).ndDims.size() == 2 && rhs.type == NodeType::CALL
            && rhs.children.size() == 4 && rhs.children[0]->type == NodeType::IDENTIFIER
            && rhs.children[0]->strValue == "repmat"
            && rhs.children[1]->type == NodeType::IDENTIFIER && isArrayVar(rhs.children[1]->strValue)
            && !arrays_.at(rhs.children[1]->strValue).is2D
            && !arrays_.at(rhs.children[1]->strValue).isND
            && arrays_.at(rhs.children[1]->strValue).dtype == ValueType::DOUBLE
            && inferExpr(*rhs.children[1], types_, reg_, classes_).type.shape.kind
                   == ShapeKind::RowVector) {
            const ArrayInfo    &x   = arrays_.at(rhs.children[1]->strValue);
            const ArrayInfo    &M   = arrays_.at(name);
            const AbstractValue res = inferExpr(rhs, types_, reg_, classes_);
            const std::string   p   = emitExpr(*rhs.children[2]);
            const std::string   q   = emitExpr(*rhs.children[3]);
            line("{");
            ++indent_;
            line("const std::size_t _nk_p = nk_rt::dim(" + p + ");");
            line("const std::size_t _nk_n = " + x.lenVar + ";");
            line("const std::size_t _nk_cols = nk_rt::dim(" + q + ") * _nk_n;");
            line(M.ndDims[0] + " = _nk_p;");
            line(M.ndDims[1] + " = _nk_cols;");
            line(name + ".assign(_nk_p * _nk_cols, 0.0);");
            open("for (std::size_t _nk_c = 0; _nk_c < _nk_cols; ++_nk_c)");
            open("for (std::size_t _nk_r = 0; _nk_r < _nk_p; ++_nk_r)");
            line(name + "[_nk_r + _nk_c * _nk_p] = " + x.dataExpr + "[_nk_c % _nk_n];");
            close();
            close();
            --indent_;
            line("}");
            types_.set(name, res);
            return;
        }
        // repmat(x, p, q) with x a COLUMN vector and q>1 -> a true 2-D (p*n) x q tiling, a
        // rank-2 ndRuntimeLocal (rows = p*n runtime, cols = q known; n = x's length).
        // Column-major: M[r + c*(p*n)] = x[r % n] -- every output column is p stacked copies
        // of the n x 1 source. The mirror of the row tile above; gated on a ColVector
        // operand. v1: x a 1-D DOUBLE col var, q>1.
        if (isArrayVar(name) && arrays_.at(name).isLocal && arrays_.at(name).ndRuntimeLocal
            && arrays_.at(name).ndDims.size() == 2 && rhs.type == NodeType::CALL
            && rhs.children.size() == 4 && rhs.children[0]->type == NodeType::IDENTIFIER
            && rhs.children[0]->strValue == "repmat"
            && rhs.children[1]->type == NodeType::IDENTIFIER && isArrayVar(rhs.children[1]->strValue)
            && !arrays_.at(rhs.children[1]->strValue).is2D
            && !arrays_.at(rhs.children[1]->strValue).isND
            && arrays_.at(rhs.children[1]->strValue).dtype == ValueType::DOUBLE
            && inferExpr(*rhs.children[1], types_, reg_, classes_).type.shape.kind
                   == ShapeKind::ColVector) {
            const ArrayInfo    &x   = arrays_.at(rhs.children[1]->strValue);
            const ArrayInfo    &M   = arrays_.at(name);
            const AbstractValue res = inferExpr(rhs, types_, reg_, classes_);
            const std::string   p   = emitExpr(*rhs.children[2]);
            const std::string   q   = emitExpr(*rhs.children[3]);
            line("{");
            ++indent_;
            line("const std::size_t _nk_n = " + x.lenVar + ";");
            line("const std::size_t _nk_rows = nk_rt::dim(" + p + ") * _nk_n;");
            line("const std::size_t _nk_q = nk_rt::dim(" + q + ");");
            line(M.ndDims[0] + " = _nk_rows;");
            line(M.ndDims[1] + " = _nk_q;");
            line(name + ".assign(_nk_rows * _nk_q, 0.0);");
            open("for (std::size_t _nk_c = 0; _nk_c < _nk_q; ++_nk_c)");
            open("for (std::size_t _nk_r = 0; _nk_r < _nk_rows; ++_nk_r)");
            line(name + "[_nk_r + _nk_c * _nk_rows] = " + x.dataExpr + "[_nk_r % _nk_n];");
            close();
            close();
            --indent_;
            line("}");
            types_.set(name, res);
            return;
        }
        // repmat(A, p, q) with A a 2-D MATRIX -> a (p*Arows) x (q*Acols) block tiling, a
        // rank-2 ndRuntimeLocal. Column-major: M[R + C*(p*Arows)] = A[(R % Arows) +
        // (C % Acols)*Arows] -- A repeats every Arows rows and every Acols cols. A's dims
        // via dimExpr (KnownDims or runtime-dim 2-D). v1: A a DOUBLE matrix var, distinct
        // from the dest (an in-place M=repmat(M,..) would read the zeroed/resized buffer).
        if (isArrayVar(name) && arrays_.at(name).isLocal && arrays_.at(name).ndRuntimeLocal
            && arrays_.at(name).ndDims.size() == 2 && rhs.type == NodeType::CALL
            && rhs.children.size() == 4 && rhs.children[0]->type == NodeType::IDENTIFIER
            && rhs.children[0]->strValue == "repmat"
            && rhs.children[1]->type == NodeType::IDENTIFIER && isArrayVar(rhs.children[1]->strValue)
            && rhs.children[1]->strValue != name
            && (arrays_.at(rhs.children[1]->strValue).is2D
                || (arrays_.at(rhs.children[1]->strValue).isND
                    && arrays_.at(rhs.children[1]->strValue).ndDims.size() == 2))
            && arrays_.at(rhs.children[1]->strValue).dtype == ValueType::DOUBLE) {
            const ArrayInfo    &A   = arrays_.at(rhs.children[1]->strValue);
            const ArrayInfo    &M   = arrays_.at(name);
            const AbstractValue res = inferExpr(rhs, types_, reg_, classes_);
            const std::string   p   = emitExpr(*rhs.children[2]);
            const std::string   q   = emitExpr(*rhs.children[3]);
            line("{");
            ++indent_;
            line("const std::size_t _nk_ar = " + dimExpr(A, 0) + ";");
            line("const std::size_t _nk_ac = " + dimExpr(A, 1) + ";");
            line("const std::size_t _nk_rows = nk_rt::dim(" + p + ") * _nk_ar;");
            line("const std::size_t _nk_cols = nk_rt::dim(" + q + ") * _nk_ac;");
            line(M.ndDims[0] + " = _nk_rows;");
            line(M.ndDims[1] + " = _nk_cols;");
            line(name + ".assign(_nk_rows * _nk_cols, 0.0);");
            open("for (std::size_t _nk_C = 0; _nk_C < _nk_cols; ++_nk_C)");
            open("for (std::size_t _nk_R = 0; _nk_R < _nk_rows; ++_nk_R)");
            line(name + "[_nk_R + _nk_C * _nk_rows] = " + A.dataExpr
                 + "[(_nk_R % _nk_ar) + (_nk_C % _nk_ac) * _nk_ar];");
            close();
            close();
            --indent_;
            line("}");
            types_.set(name, res);
            return;
        }
        // eye(n) / eye(m, n) -> the identity matrix (1 on the main diagonal, 0 else), a
        // 2-D KnownDims LOCAL. Like zeros but the diagonal is set to 1. v1: literal dims.
        if (isArrayVar(name) && arrays_.at(name).isLocal && arrays_.at(name).is2D
            && rhs.type == NodeType::CALL && !rhs.children.empty()
            && rhs.children[0]->type == NodeType::IDENTIFIER && rhs.children[0]->strValue == "eye") {
            const ArrayInfo    &v   = arrays_.at(name);
            const AbstractValue res = inferExpr(rhs, types_, reg_, classes_);
            if (res.type.isConcrete() && res.type.shape.kind == ShapeKind::KnownDims) {
                line("{");
                ++indent_;
                line(name + ".assign(static_cast<std::size_t>(" + v.rowsVar + " * " + v.colsVar
                     + "), 0.0);");
                line("const std::size_t _nk_d = " + v.rowsVar + " < " + v.colsVar + " ? " + v.rowsVar
                     + " : " + v.colsVar + ";");
                open("for (std::size_t _nk_i = 0; _nk_i < _nk_d; ++_nk_i)");
                line(name + "[_nk_i + _nk_i * " + v.rowsVar + "] = 1.0;");
                close();
                --indent_;
                line("}");
                types_.set(name, res);
                return;
            }
        }
        // eye(n) / eye(m,n) with a RUNTIME dim -> the identity as a rank-2 ndRuntimeLocal
        // (the runtime mirror of the KnownDims eye above + zeros(m,n) runtime). Dims from
        // the args via nk_rt::dim (eye(n): cols = rows); zero the buffer, set the main
        // diagonal M[i + i*rows] = 1 for i < min(rows, cols). v1: 1 or 2 scalar dim args.
        if (isArrayVar(name) && arrays_.at(name).isLocal && arrays_.at(name).ndRuntimeLocal
            && arrays_.at(name).ndDims.size() == 2 && rhs.type == NodeType::CALL
            && (rhs.children.size() == 2 || rhs.children.size() == 3)
            && rhs.children[0]->type == NodeType::IDENTIFIER
            && rhs.children[0]->strValue == "eye") {
            const ArrayInfo    &M   = arrays_.at(name);
            const AbstractValue res = inferExpr(rhs, types_, reg_, classes_);
            if (res.type.isConcrete()) {
                line("{");
                ++indent_;
                line("const std::size_t _nk_r = nk_rt::dim(" + emitExpr(*rhs.children[1]) + ");");
                if (rhs.children.size() == 3)
                    line("const std::size_t _nk_c = nk_rt::dim(" + emitExpr(*rhs.children[2]) + ");");
                else
                    line("const std::size_t _nk_c = _nk_r;");  // eye(n): n x n
                line(M.ndDims[0] + " = _nk_r;");
                line(M.ndDims[1] + " = _nk_c;");
                line(name + ".assign(_nk_r * _nk_c, 0.0);");
                line("const std::size_t _nk_d = _nk_r < _nk_c ? _nk_r : _nk_c;");
                open("for (std::size_t _nk_i = 0; _nk_i < _nk_d; ++_nk_i)");
                line(name + "[_nk_i + _nk_i * _nk_r] = 1.0;");
                close();
                --indent_;
                line("}");
                types_.set(name, res);
                return;
            }
        }
        // tril(A[,k]) / triu(A[,k]) -> the lower/upper triangular part of a 2-D matrix (the
        // other triangle zeroed), SAME shape as A. The optional diagonal offset k shifts the
        // boundary: tril keeps A(i,j) where i >= j - k (k>0 keeps super-diagonals, k<0 drops
        // sub-diagonals); triu keeps i <= j - k (k=0 is the main diagonal). Both tiers via
        // dimExpr (KnownDims 2-D or runtime-dim 2-D). Column-major out[i+j*rows] = keep ?
        // A[...] : 0. v1: A a DOUBLE matrix var, a DISTINCT local (in-place refused -- the
        // zero-then-copy would alias).
        if (isArrayVar(name) && arrays_.at(name).isLocal
            && (arrays_.at(name).is2D
                || (arrays_.at(name).isND && arrays_.at(name).ndDims.size() == 2))
            && rhs.type == NodeType::CALL
            && (rhs.children.size() == 2 || rhs.children.size() == 3)
            && rhs.children[0]->type == NodeType::IDENTIFIER
            && (rhs.children[0]->strValue == "tril" || rhs.children[0]->strValue == "triu")
            && rhs.children[1]->type == NodeType::IDENTIFIER && isArrayVar(rhs.children[1]->strValue)
            && rhs.children[1]->strValue != name
            && (arrays_.at(rhs.children[1]->strValue).is2D
                || (arrays_.at(rhs.children[1]->strValue).isND
                    && arrays_.at(rhs.children[1]->strValue).ndDims.size() == 2))
            && arrays_.at(rhs.children[1]->strValue).dtype == ValueType::DOUBLE) {
            const ArrayInfo    &A   = arrays_.at(rhs.children[1]->strValue);
            const ArrayInfo    &M   = arrays_.at(name);
            const AbstractValue res = inferExpr(rhs, types_, reg_, classes_);
            const std::string   cmp = rhs.children[0]->strValue == "tril" ? ">=" : "<=";
            if (res.type.isConcrete()) {
                line("{");
                ++indent_;
                line("const std::size_t _nk_r = " + dimExpr(A, 0) + ";");
                line("const std::size_t _nk_c = " + dimExpr(A, 1) + ";");
                line("const std::ptrdiff_t _nk_k = "
                     + (rhs.children.size() == 3
                            ? ("static_cast<std::ptrdiff_t>(" + emitExpr(*rhs.children[2]) + ")")
                            : std::string("0"))
                     + ";");
                if (M.isND && M.ndDims.size() == 2) {  // runtime-dim dst: set companions
                    line(M.ndDims[0] + " = _nk_r;");
                    line(M.ndDims[1] + " = _nk_c;");
                }
                line(name + ".assign(_nk_r * _nk_c, 0.0);");
                open("for (std::size_t _nk_j = 0; _nk_j < _nk_c; ++_nk_j)");
                open("for (std::size_t _nk_i = 0; _nk_i < _nk_r; ++_nk_i)");
                line("if (static_cast<std::ptrdiff_t>(_nk_i) " + cmp
                     + " static_cast<std::ptrdiff_t>(_nk_j) - _nk_k) " + name
                     + "[_nk_i + _nk_j * _nk_r] = " + A.dataExpr + "[_nk_i + _nk_j * _nk_r];");
                close();
                close();
                --indent_;
                line("}");
                types_.set(name, res);
                return;
            }
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
            // A runtime-dim 2-D matrix is an NDims rank-2 (ndRuntimeLocal); its dims live
            // in ndDims, not rowsVar/colsVar. Allow the rank-2 N-D case through; only a
            // true N-D (rank>=3) transpose is undefined in MATLAB -> refused.
            const bool srcRank2ND = src.isND && src.ndDims.size() == 2;
            const bool dstRank2ND = dst.isND && dst.ndDims.size() == 2;
            if ((src.isND && !srcRank2ND) || (dst.isND && !dstRank2ND))
                unsupported("N-D transpose (undefined in MATLAB)");
            if (name == rhs.children[0]->strValue)
                unsupported("in-place transpose (y = y')");
            const bool conj = rhs.strValue == "'" && src.dtype == ValueType::COMPLEX;
            if (srcRank2ND || dstRank2ND) {
                // Runtime-dim 2-D transpose: y is n x m (= src cols x src rows), and
                // y(p,q) = A(q,p) -> y[p + q*yrows] = op(A[q + p*Arows]). Dims and the
                // buffer come from the ndDims companions; yrows = src cols = src.ndDims[1].
                if (!srcRank2ND || !dstRank2ND)
                    unsupported("transpose dest/source rank mismatch");
                line(dst.ndDims[0] + " = " + src.ndDims[1] + ";");  // dst rows = src cols
                line(dst.ndDims[1] + " = " + src.ndDims[0] + ";");  // dst cols = src rows
                line(name + ".assign(" + dst.ndDims[0] + " * " + dst.ndDims[1] + ", "
                     + zeroLiteral(dst.dtype) + ");");
                open("for (std::size_t _nk_j = 0; _nk_j < " + dst.ndDims[1] + "; ++_nk_j)");
                open("for (std::size_t _nk_i = 0; _nk_i < " + dst.ndDims[0] + "; ++_nk_i)");
                const std::string rd = src.dataExpr + "[_nk_j + _nk_i * " + src.ndDims[0] + "]";
                line(name + "[_nk_i + _nk_j * " + dst.ndDims[0] + "] = "
                     + (conj ? ("std::conj(" + rd + ")") : rd) + ";");
                close();
                close();
            } else if (src.is2D || dst.is2D) {
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
            && (arrays_.at(name).is2D
                || (arrays_.at(name).isND && arrays_.at(name).ndDims.size() == 2))
            && rhs.type == NodeType::BINARY_OP && rhs.strValue == "*"
            && rhs.children.size() == 2 && rhs.children[0]->type == NodeType::IDENTIFIER
            && isArrayVar(rhs.children[0]->strValue)
            && rhs.children[1]->type == NodeType::IDENTIFIER
            && isArrayVar(rhs.children[1]->strValue)) {
            const ArrayInfo &dst = arrays_.at(name);
            const ArrayInfo &A   = arrays_.at(rhs.children[0]->strValue);
            const ArrayInfo &B   = arrays_.at(rhs.children[1]->strValue);
            // A rank-2 operand is a matrix — KnownDims 2-D OR a runtime-dim 2-D (NDims
            // rank-2). Only matrix*matrix is lowered here; with a vector operand (a
            // matrix*vector, or a col*row outer product whose result is also 2-D) this
            // branch falls through to the matrix*vector / outer-product branches below.
            const bool aMatrix = A.is2D || (A.isND && A.ndDims.size() == 2);
            const bool bMatrix = B.is2D || (B.isND && B.ndDims.size() == 2);
            if (aMatrix && bMatrix) {
                if (name == rhs.children[0]->strValue || name == rhs.children[1]->strValue)
                    unsupported("in-place matrix product (C = C * B)");
                // dimExpr is rank-agnostic: a KnownDims matrix yields its rows/colsVar
                // literals, a runtime-dim 2-D yields its ndDims companions. So the same
                // lowering serves both (the runtime dst gets its companions set first).
                const std::string Arows = dimExpr(A, 0), Ak = dimExpr(A, 1);  // A is m x k
                const std::string Brows = dimExpr(B, 0), Bn = dimExpr(B, 1);  // B is k x n
                const bool        dstRuntime = dst.isND && dst.ndDims.size() == 2;
                line("if (" + Ak + " != " + Brows
                     + ") throw std::out_of_range(\"numkit: inner matrix dimensions must agree\");");
                if (dstRuntime) {
                    line(dst.ndDims[0] + " = " + Arows + ";");  // C rows = A rows (m)
                    line(dst.ndDims[1] + " = " + Bn + ";");     // C cols = B cols (n)
                }
                const std::string Crows = dimExpr(dst, 0), Ccols = dimExpr(dst, 1);
                if (opsKernels_
                    && (dst.dtype == ValueType::DOUBLE || dst.dtype == ValueType::COMPLEX)) {
                    // ops owns the kernel: M=C rows, N=C cols, K=A cols (==B rows,
                    // guarded). The kernel zeroes+accumulates; a LOCAL still needs its
                    // owned vector sized first. DOUBLE -> SIMD matmulDouble; COMPLEX ->
                    // portable matmulComplex (amortised over O(M·N·K), no per-elem cost).
                    const char *fn =
                        dst.dtype == ValueType::DOUBLE ? "matmulDouble" : "matmulComplex";
                    if (dst.isLocal)
                        line(name + ".resize(" + Crows + " * " + Ccols + ");");
                    line("numkit::ops::" + std::string(fn) + "(" + A.dataExpr + ", "
                         + B.dataExpr + ", " + dst.dataExpr + ", " + Crows + ", " + Ccols
                         + ", " + Ak + ");");
                } else {
                    if (dst.isLocal)
                        line(name + ".assign(" + Crows + " * " + Ccols + ", "
                             + zeroLiteral(dst.dtype) + ");");
                    open("for (std::size_t _nk_j = 0; _nk_j < " + Ccols + "; ++_nk_j)");
                    open("for (std::size_t _nk_i = 0; _nk_i < " + Crows + "; ++_nk_i)");
                    line(cppScalarType(dst.dtype) + " _nk_acc = " + zeroLiteral(dst.dtype) + ";");
                    open("for (std::size_t _nk_l = 0; _nk_l < " + Ak + "; ++_nk_l)");
                    line("_nk_acc += " + A.dataExpr + "[_nk_i + _nk_l * " + Arows + "] * "
                         + B.dataExpr + "[_nk_l + _nk_j * " + Brows + "];");
                    close();
                    line(dst.dataExpr + "[_nk_i + _nk_j * " + Crows + "] = _nk_acc;");
                    close();
                    close();
                }
                types_.set(name, inferExpr(rhs, types_, reg_, classes_));
                return;
            }
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
            // A matrix operand is KnownDims 2-D OR a runtime-dim 2-D (NDims rank-2); a
            // vector is 1-D (neither). dimExpr reads the matrix dims rank-agnostically
            // (rows/colsVar literals or ndDims companions). A true N-D (rank>=3) operand
            // is refused. (matrix*matrix went to the matmul branch above; both-vector
            // falls to the outer-product / refusal below.)
            if ((L.isND && L.ndDims.size() != 2) || (R.isND && R.ndDims.size() != 2))
                unsupported("N-D operand in matrix*vector");
            const bool        lMat = L.is2D || (L.isND && L.ndDims.size() == 2);
            const bool        rMat = R.is2D || (R.isND && R.ndDims.size() == 2);
            const bool        lVec = !L.is2D && !L.isND;
            const bool        rVec = !R.is2D && !R.isND;
            const std::string ty   = cppScalarType(dst.dtype);
            const std::string zer  = zeroLiteral(dst.dtype);
            if (lMat && rVec) {  // A * x -> column vector
                const std::string Arows = dimExpr(L, 0), Acols = dimExpr(L, 1);
                line("if (" + Acols + " != " + R.lenVar + ") throw std::out_of_range(\""
                     "numkit: inner matrix dimensions must agree\");");
                if (dst.isLocal) line(name + ".resize(" + Arows + ");");
                open("for (std::size_t _nk_i = 0; _nk_i < " + Arows + "; ++_nk_i)");
                line(ty + " _nk_acc = " + zer + ";");
                open("for (std::size_t _nk_l = 0; _nk_l < " + Acols + "; ++_nk_l)");
                line("_nk_acc += " + L.dataExpr + "[_nk_i + _nk_l * " + Arows + "] * "
                     + R.dataExpr + "[_nk_l];");
                close();
                line(dst.dataExpr + "[_nk_i] = _nk_acc;");
                close();
            } else if (lVec && rMat) {  // r * A -> row vector
                const std::string Brows = dimExpr(R, 0), Bcols = dimExpr(R, 1);
                line("if (" + L.lenVar + " != " + Brows + ") throw std::out_of_range(\""
                     "numkit: inner matrix dimensions must agree\");");
                if (dst.isLocal) line(name + ".resize(" + Bcols + ");");
                open("for (std::size_t _nk_j = 0; _nk_j < " + Bcols + "; ++_nk_j)");
                line(ty + " _nk_acc = " + zer + ";");
                open("for (std::size_t _nk_l = 0; _nk_l < " + Brows + "; ++_nk_l)");
                line("_nk_acc += " + L.dataExpr + "[_nk_l] * " + R.dataExpr + "[_nk_l + _nk_j * "
                     + Brows + "];");
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
        // Native trapz(y) -> trapezoidal integral with unit spacing, a DOUBLE scalar:
        // sum over i of (y[i]+y[i+1])/2. n<2 -> 0 (no interval). Gated !bridge_
        // (order-dependent summation -> the bridged path stays exact under the
        // bridge). v1: a single 1-D DOUBLE array var.
        if (!isArrayVar(name) && !bridge_ && rhs.type == NodeType::CALL
            && rhs.children.size() == 2 && rhs.children[0]->type == NodeType::IDENTIFIER
            && rhs.children[0]->strValue == "trapz"
            && rhs.children[1]->type == NodeType::IDENTIFIER
            && isArrayVar(rhs.children[1]->strValue)
            && arrays_.at(rhs.children[1]->strValue).dtype == ValueType::DOUBLE) {
            const ArrayInfo    &a   = arrays_.at(rhs.children[1]->strValue);
            const AbstractValue res = inferExpr(rhs, types_, reg_, classes_);
            if (!a.is2D && !a.isND && res.type.isConcrete() && res.type.shape.isScalar()) {
                line("{");
                ++indent_;
                line("double _nk_acc = 0.0;");
                open("for (std::size_t _nk_i = 0; _nk_i + 1 < " + a.lenVar + "; ++_nk_i)");
                line("_nk_acc += (" + a.dataExpr + "[_nk_i] + " + a.dataExpr
                     + "[_nk_i + 1]) * 0.5;");
                close();
                line(name + " = _nk_acc;");
                --indent_;
                line("}");
                types_.set(name, res);
                return;
            }
        }
        // Native trace(A) -> the sum of the diagonal of a 2-D matrix, a DOUBLE scalar.
        // Column-major: acc += A[i + i*rows] for i in [0, min(rows,cols)). Gated
        // !bridge_ (order-dependent sum). v1: A a 2-D DOUBLE matrix var.
        if (!isArrayVar(name) && !bridge_ && rhs.type == NodeType::CALL
            && rhs.children.size() == 2 && rhs.children[0]->type == NodeType::IDENTIFIER
            && rhs.children[0]->strValue == "trace"
            && rhs.children[1]->type == NodeType::IDENTIFIER && isArrayVar(rhs.children[1]->strValue)
            && arrays_.at(rhs.children[1]->strValue).is2D
            && arrays_.at(rhs.children[1]->strValue).dtype == ValueType::DOUBLE) {
            const ArrayInfo    &A   = arrays_.at(rhs.children[1]->strValue);
            const AbstractValue res = inferExpr(rhs, types_, reg_, classes_);
            if (res.type.isConcrete() && res.type.shape.isScalar()) {
                line("{");
                ++indent_;
                line("const std::size_t _nk_d = " + A.rowsVar + " < " + A.colsVar + " ? " + A.rowsVar
                     + " : " + A.colsVar + ";");
                line("double _nk_acc = 0.0;");
                open("for (std::size_t _nk_i = 0; _nk_i < _nk_d; ++_nk_i)");
                line("_nk_acc += " + A.dataExpr + "[_nk_i + _nk_i * " + A.rowsVar + "];");
                close();
                line(name + " = _nk_acc;");
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
        // Native find(m) -> the 1-based LINEAR (column-major) positions of the nonzero (true)
        // elements, a runtime-sized 1-D DOUBLE column vector (MATLAB find). A native filter
        // loop over the flat buffer pushing (i+1); EXACT (deterministic positions), preferred
        // over the bridged array-result path below. Works for a 1-D vector OR a 2-D / N-D
        // matrix arg (the flat index +1 is the MATLAB linear index) -> bound on NUMEL. v1: a
        // single array VARIABLE (double or logical); the result a 1-D array LOCAL (push_back).
        if (isArrayVar(name) && arrays_.at(name).isLocal && !arrays_.at(name).is2D
            && !arrays_.at(name).isND && rhs.type == NodeType::CALL
            && rhs.children.size() == 2 && rhs.children[0]->type == NodeType::IDENTIFIER
            && rhs.children[0]->strValue == "find"
            && rhs.children[1]->type == NodeType::IDENTIFIER
            && isArrayVar(rhs.children[1]->strValue)) {
            const ArrayInfo    &m   = arrays_.at(rhs.children[1]->strValue);
            const AbstractValue res = inferExpr(rhs, types_, reg_, classes_);
            if (res.type.isConcrete() && !res.type.shape.isScalar()) {
                std::string numel;  // flat element count (1-D len / 2-D rows*cols / N-D prod)
                if (m.isND) {
                    numel = m.ndDims[0];
                    for (std::size_t i = 1; i < m.ndDims.size(); ++i) numel += " * " + m.ndDims[i];
                } else if (m.is2D) {
                    numel = m.rowsVar + " * " + m.colsVar;
                } else {
                    numel = m.lenVar;
                }
                line("{");
                ++indent_;
                line(name + ".clear();");
                open("for (std::size_t _nk_i = 0; _nk_i < (" + numel + "); ++_nk_i)");
                line("if (" + m.dataExpr + "[_nk_i]) " + name
                     + ".push_back(static_cast<double>(_nk_i + 1));");
                close();
                --indent_;
                line("}");
                types_.set(name, res);
                return;
            }
        }
        // Native find(<expr>) with an INLINE elementwise expression -> the 1-based positions
        // where the per-element value is nonzero/true: find(x>0), find(A>lo & A<hi), find(x-3)
        // (x!=3), ... The inline sibling of find(VAR): the expression is FUSED into the filter
        // loop (no temp) -- elementCtx_ makes the whole array emit arr[_nk_i] (flat), then for
        // each flat element i, if the value is nonzero push (i+1). For a 2-D/N-D operand the
        // flat index +1 IS MATLAB's column-major LINEAR index. Restricted to a SINGLE-array
        // pure-elementwise expression (collectElementwise srcArrays of size 1) so the bound is
        // that array's NUMEL and per-element emission is valid (a multi-array find(x>y) ->
        // bridged). Result a 1-D DOUBLE LOCAL (push_back). v1: 1-D or 2-D/N-D operand.
        if (isArrayVar(name) && arrays_.at(name).isLocal && !arrays_.at(name).is2D
            && !arrays_.at(name).isND && rhs.type == NodeType::CALL
            && rhs.children.size() == 2 && rhs.children[0]->type == NodeType::IDENTIFIER
            && rhs.children[0]->strValue == "find"
            && rhs.children[1]->type != NodeType::IDENTIFIER) {
            std::set<std::string> srcArrays;
            const bool            pureEw = collectElementwise(*rhs.children[1], srcArrays);
            const AbstractValue   maskAV = inferExpr(*rhs.children[1], types_, reg_, classes_);
            const AbstractValue   res    = inferExpr(rhs, types_, reg_, classes_);
            if (pureEw && srcArrays.size() == 1 && maskAV.type.isConcrete()
                && !maskAV.type.shape.isScalar()
                && res.type.isConcrete() && !res.type.shape.isScalar()) {
                const ArrayInfo &ba = arrays_.at(*srcArrays.begin());
                std::string      numel;  // flat element count (1-D len / 2-D rows*cols / N-D prod)
                if (ba.isND) {
                    numel = ba.ndDims[0];
                    for (std::size_t i = 1; i < ba.ndDims.size(); ++i) numel += " * " + ba.ndDims[i];
                } else if (ba.is2D) {
                    numel = ba.rowsVar + " * " + ba.colsVar;
                } else {
                    numel = ba.lenVar;
                }
                line("{");
                ++indent_;
                line(name + ".clear();");
                elementCtx_ = "_nk_i";  // whole array in the expr -> arr[_nk_i] (flat)
                const std::string maskExpr = emitExpr(*rhs.children[1]);
                elementCtx_.clear();
                open("for (std::size_t _nk_i = 0; _nk_i < (" + numel + "); ++_nk_i)");
                line("if (" + maskExpr + ") " + name
                     + ".push_back(static_cast<double>(_nk_i + 1));");
                close();
                --indent_;
                line("}");
                types_.set(name, res);
                return;
            }
        }
        // Native any(A) / all(A) on a 2-D MATRIX -> column-wise -> a LOGICAL 1 x n ROW vector (a
        // 1-D LOGICAL LOCAL, uint8 buffer). result(j) = whether column j has ANY nonzero (any) or
        // is ALL nonzero (all). NaN counts as nonzero (NaN != 0), matching MATLAB. EXACT -> every
        // tier, no bridge guard. An empty column -> any false / all true (the seeds). Column-major.
        // v1: a single DOUBLE/LOGICAL matrix var (KnownDims or runtime rank-2).
        if (isArrayVar(name) && arrays_.at(name).isLocal && !arrays_.at(name).is2D
            && !arrays_.at(name).isND && rhs.type == NodeType::CALL && rhs.children.size() == 2
            && rhs.children[0]->type == NodeType::IDENTIFIER
            && (rhs.children[0]->strValue == "any" || rhs.children[0]->strValue == "all")
            && rhs.children[1]->type == NodeType::IDENTIFIER && isArrayVar(rhs.children[1]->strValue)
            && (arrays_.at(rhs.children[1]->strValue).is2D
                || (arrays_.at(rhs.children[1]->strValue).isND
                    && arrays_.at(rhs.children[1]->strValue).ndDims.size() == 2))
            && (arrays_.at(rhs.children[1]->strValue).dtype == ValueType::DOUBLE
                || arrays_.at(rhs.children[1]->strValue).dtype == ValueType::LOGICAL)) {
            const ArrayInfo    &A   = arrays_.at(rhs.children[1]->strValue);
            const AbstractValue res = inferExpr(rhs, types_, reg_, classes_);
            if (res.type.isConcrete() && !res.type.shape.isScalar()) {
                const bool isAny = rhs.children[0]->strValue == "any";
                line("{");
                ++indent_;
                line("const std::size_t _nk_m = " + dimExpr(A, 0) + ";");
                line("const std::size_t _nk_n = " + dimExpr(A, 1) + ";");
                line(name + ".assign(_nk_n, 0);");
                open("for (std::size_t _nk_j = 0; _nk_j < _nk_n; ++_nk_j)");
                line(std::string("bool _nk_acc = ") + (isAny ? "false;" : "true;"));
                open("for (std::size_t _nk_i = 0; _nk_i < _nk_m; ++_nk_i)");
                line("const double _nk_v = static_cast<double>(" + A.dataExpr
                     + "[_nk_i + _nk_j * _nk_m]);");
                line(std::string("_nk_acc = _nk_acc ") + (isAny ? "|| _nk_v != 0.0;" : "&& _nk_v != 0.0;"));
                close();
                line(name + "[_nk_j] = _nk_acc;");
                close();
                --indent_;
                line("}");
                types_.set(name, res);
                return;
            }
        }
        // Native max(A) / min(A) on a 2-D MATRIX -> column-wise reduction -> a 1 x n ROW vector
        // (a 1-D LOCAL). result(j) = the max/min over column j, NaN-skipping (seed on the column's
        // first element, update on a strict cmp OR when acc is NaN -> the first non-NaN seeds it;
        // an all-NaN column stays NaN). EXACT + order-independent -> runs in every tier, no bridge
        // guard. Column-major. v1: a single DOUBLE matrix var (KnownDims or runtime rank-2); the
        // result a 1-D array LOCAL.
        if (isArrayVar(name) && arrays_.at(name).isLocal && !arrays_.at(name).is2D
            && !arrays_.at(name).isND && rhs.type == NodeType::CALL && rhs.children.size() == 2
            && rhs.children[0]->type == NodeType::IDENTIFIER
            && (rhs.children[0]->strValue == "max" || rhs.children[0]->strValue == "min")
            && rhs.children[1]->type == NodeType::IDENTIFIER && isArrayVar(rhs.children[1]->strValue)
            && (arrays_.at(rhs.children[1]->strValue).is2D
                || (arrays_.at(rhs.children[1]->strValue).isND
                    && arrays_.at(rhs.children[1]->strValue).ndDims.size() == 2))
            && arrays_.at(rhs.children[1]->strValue).dtype == ValueType::DOUBLE) {
            const ArrayInfo    &A   = arrays_.at(rhs.children[1]->strValue);
            const AbstractValue res = inferExpr(rhs, types_, reg_, classes_);
            if (res.type.isConcrete() && !res.type.shape.isScalar()) {
                const std::string cmp = rhs.children[0]->strValue == "max" ? ">" : "<";
                line("{");
                ++indent_;
                line("const std::size_t _nk_m = " + dimExpr(A, 0) + ";");
                line("const std::size_t _nk_n = " + dimExpr(A, 1) + ";");
                line(name + ".assign(_nk_n, 0.0);");
                open("for (std::size_t _nk_j = 0; _nk_j < _nk_n; ++_nk_j)");
                line("double _nk_acc = 0.0;");  // seeded on _nk_i == 0 (safe when _nk_m == 0)
                open("for (std::size_t _nk_i = 0; _nk_i < _nk_m; ++_nk_i)");
                line("const double _nk_v = " + A.dataExpr + "[_nk_i + _nk_j * _nk_m];");
                line("if (_nk_i == 0 || _nk_v " + cmp + " _nk_acc"
                     " || (_nk_acc != _nk_acc && _nk_v == _nk_v)) _nk_acc = _nk_v;");
                close();
                line(name + "[_nk_j] = _nk_acc;");
                close();
                --indent_;
                line("}");
                types_.set(name, res);
                return;
            }
        }
        // Native diff(A[, 1, dim]) on a 2-D MATRIX -> consecutive differences along dim 1 (down
        // columns -> (m-1) x n) or dim 2 (across rows -> m x (n-1)). EXACT (subtraction), no bridge
        // guard. diff(A) defaults to dim 1 ONLY when rows are statically > 1 (a KnownDims matrix);
        // a runtime-dim 2-D could be 1 x n (first-non-singleton-dim ambiguity) -> there an explicit
        // dim is required via the 3-arg diff(A, 1, dim) form. Column-major. v1: a DOUBLE matrix var
        // distinct from the dest; order 1 only.
        {
            const bool diff3Arg =
                rhs.type == NodeType::CALL && rhs.children.size() == 4
                && rhs.children[0]->type == NodeType::IDENTIFIER && rhs.children[0]->strValue == "diff"
                && rhs.children[2]->type == NodeType::NUMBER_LITERAL
                && rhs.children[2]->numValue == 1.0  // order 1
                && rhs.children[3]->type == NodeType::NUMBER_LITERAL
                && (rhs.children[3]->numValue == 1.0 || rhs.children[3]->numValue == 2.0);
            const bool diff1Arg = rhs.type == NodeType::CALL && rhs.children.size() == 2
                                  && rhs.children[0]->type == NodeType::IDENTIFIER
                                  && rhs.children[0]->strValue == "diff";
            if (isArrayVar(name) && arrays_.at(name).isLocal
                && (arrays_.at(name).is2D
                    || (arrays_.at(name).isND && arrays_.at(name).ndDims.size() == 2))
                && (diff1Arg || diff3Arg) && rhs.children[1]->type == NodeType::IDENTIFIER
                && isArrayVar(rhs.children[1]->strValue) && rhs.children[1]->strValue != name
                && (arrays_.at(rhs.children[1]->strValue).is2D
                    || (arrays_.at(rhs.children[1]->strValue).isND
                        && arrays_.at(rhs.children[1]->strValue).ndDims.size() == 2))
                && arrays_.at(rhs.children[1]->strValue).dtype == ValueType::DOUBLE) {
                const ArrayInfo &A = arrays_.at(rhs.children[1]->strValue);
                // 1-arg default-dim diff needs rows statically > 1 (A.is2D = KnownDims matrix); a
                // runtime-dim 2-D 1-arg diff stays bridged (ambiguous first non-singleton dim).
                if (diff3Arg || A.is2D) {
                    const ArrayInfo    &M   = arrays_.at(name);
                    const AbstractValue res = inferExpr(rhs, types_, reg_, classes_);
                    if (res.type.isConcrete() && !res.type.shape.isScalar()) {
                        const bool dim2 = diff3Arg && rhs.children[3]->numValue == 2.0;
                        line("{");
                        ++indent_;
                        line("const std::size_t _nk_m = " + dimExpr(A, 0) + ";");
                        line("const std::size_t _nk_n = " + dimExpr(A, 1) + ";");
                        line(std::string("const std::size_t _nk_rm = ")
                             + (dim2 ? "_nk_m;" : "_nk_m - 1;"));  // result rows
                        line(std::string("const std::size_t _nk_rn = ")
                             + (dim2 ? "_nk_n - 1;" : "_nk_n;"));  // result cols
                        if (M.isND && M.ndDims.size() == 2) {
                            line(M.ndDims[0] + " = _nk_rm;");
                            line(M.ndDims[1] + " = _nk_rn;");
                        }
                        line(name + ".assign(_nk_rm * _nk_rn, 0.0);");
                        open("for (std::size_t _nk_j = 0; _nk_j < _nk_rn; ++_nk_j)");
                        open("for (std::size_t _nk_i = 0; _nk_i < _nk_rm; ++_nk_i)");
                        if (dim2)
                            line(name + "[_nk_i + _nk_j * _nk_rm] = " + A.dataExpr
                                 + "[_nk_i + (_nk_j + 1) * _nk_m] - " + A.dataExpr
                                 + "[_nk_i + _nk_j * _nk_m];");
                        else
                            line(name + "[_nk_i + _nk_j * _nk_rm] = " + A.dataExpr
                                 + "[(_nk_i + 1) + _nk_j * _nk_m] - " + A.dataExpr
                                 + "[_nk_i + _nk_j * _nk_m];");
                        close();
                        close();
                        --indent_;
                        line("}");
                        types_.set(name, res);
                        return;
                    }
                }
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
        // Native cumsum/cumprod(A[, dim]) on a 2-D MATRIX -> a fresh same-shape matrix, running
        // accumulation along dim 1 (down each column, the default) or dim 2 (across each row).
        // Like the 1-D form, ONLY with no bridge (order-dependent rounding -> the bridged path
        // stays the exact tier when the bridge is on). Column-major per-column (dim 1) or per-row
        // (dim 2) prefix scan. 1-arg -> dim 1; 2-arg cumsum(A, dim) with a LITERAL dim 1|2. v1: a
        // DOUBLE matrix var distinct from the dest.
        {
            const bool cumTwoArg =
                rhs.type == NodeType::CALL && rhs.children.size() == 3
                && rhs.children[0]->type == NodeType::IDENTIFIER
                && (rhs.children[0]->strValue == "cumsum" || rhs.children[0]->strValue == "cumprod")
                && rhs.children[2]->type == NodeType::NUMBER_LITERAL
                && (rhs.children[2]->numValue == 1.0 || rhs.children[2]->numValue == 2.0);
            if (isArrayVar(name) && arrays_.at(name).isLocal && !bridge_
                && (arrays_.at(name).is2D
                    || (arrays_.at(name).isND && arrays_.at(name).ndDims.size() == 2))
                && rhs.type == NodeType::CALL && (rhs.children.size() == 2 || cumTwoArg)
                && rhs.children[0]->type == NodeType::IDENTIFIER
                && (rhs.children[0]->strValue == "cumsum" || rhs.children[0]->strValue == "cumprod")
                && rhs.children[1]->type == NodeType::IDENTIFIER
                && isArrayVar(rhs.children[1]->strValue) && rhs.children[1]->strValue != name
                && (arrays_.at(rhs.children[1]->strValue).is2D
                    || (arrays_.at(rhs.children[1]->strValue).isND
                        && arrays_.at(rhs.children[1]->strValue).ndDims.size() == 2))
                && arrays_.at(rhs.children[1]->strValue).dtype == ValueType::DOUBLE) {
                const ArrayInfo    &A   = arrays_.at(rhs.children[1]->strValue);
                const ArrayInfo    &M   = arrays_.at(name);
                const AbstractValue res = inferExpr(rhs, types_, reg_, classes_);
                if (res.type.isConcrete() && !res.type.shape.isScalar()) {
                    const bool        isProd = rhs.children[0]->strValue == "cumprod";
                    const bool        dim2   = cumTwoArg && rhs.children[2]->numValue == 2.0;
                    const std::string seed   = isProd ? "1.0" : "0.0";
                    const std::string op     = isProd ? "*=" : "+=";
                    line("{");
                    ++indent_;
                    line("const std::size_t _nk_m = " + dimExpr(A, 0) + ";");
                    line("const std::size_t _nk_n = " + dimExpr(A, 1) + ";");
                    if (M.isND && M.ndDims.size() == 2) {  // runtime-dim dst: set its companions
                        line(M.ndDims[0] + " = _nk_m;");
                        line(M.ndDims[1] + " = _nk_n;");
                    }
                    line(name + ".assign(_nk_m * _nk_n, 0.0);");
                    if (dim2) {  // dim 2: across each row -> outer over rows, inner over columns
                        open("for (std::size_t _nk_i = 0; _nk_i < _nk_m; ++_nk_i)");
                        line("double _nk_acc = " + seed + ";");
                        open("for (std::size_t _nk_j = 0; _nk_j < _nk_n; ++_nk_j)");
                    } else {  // dim 1 (default): down each column -> outer over columns, inner rows
                        open("for (std::size_t _nk_j = 0; _nk_j < _nk_n; ++_nk_j)");
                        line("double _nk_acc = " + seed + ";");
                        open("for (std::size_t _nk_i = 0; _nk_i < _nk_m; ++_nk_i)");
                    }
                    line("_nk_acc " + op + " " + A.dataExpr + "[_nk_i + _nk_j * _nk_m];");
                    line(name + "[_nk_i + _nk_j * _nk_m] = _nk_acc;");
                    close();
                    close();
                    --indent_;
                    line("}");
                    types_.set(name, res);
                    return;
                }
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
        // Native cummax/cummin(A[, dim]) on a 2-D MATRIX -> a fresh same-shape matrix, running
        // max/min along dim 1 (down each column, the default) or dim 2 (across each row). EXACT
        // (no rounding), so no bridge guard. Per scan-line the accumulator uses the single-output
        // max/min NaN logic (seed on the line's first element, update on a strict cmp, or when acc
        // is NaN and the candidate is not -> the first non-NaN seeds it). 1-arg -> dim 1; 2-arg
        // cummax(A, dim) with a LITERAL dim 1|2. v1: a DOUBLE matrix var distinct from the dest.
        {
            const bool cmTwoArg =
                rhs.type == NodeType::CALL && rhs.children.size() == 3
                && rhs.children[0]->type == NodeType::IDENTIFIER
                && (rhs.children[0]->strValue == "cummax" || rhs.children[0]->strValue == "cummin")
                && rhs.children[2]->type == NodeType::NUMBER_LITERAL
                && (rhs.children[2]->numValue == 1.0 || rhs.children[2]->numValue == 2.0);
            if (isArrayVar(name) && arrays_.at(name).isLocal
                && (arrays_.at(name).is2D
                    || (arrays_.at(name).isND && arrays_.at(name).ndDims.size() == 2))
                && rhs.type == NodeType::CALL && (rhs.children.size() == 2 || cmTwoArg)
                && rhs.children[0]->type == NodeType::IDENTIFIER
                && (rhs.children[0]->strValue == "cummax" || rhs.children[0]->strValue == "cummin")
                && rhs.children[1]->type == NodeType::IDENTIFIER
                && isArrayVar(rhs.children[1]->strValue) && rhs.children[1]->strValue != name
                && (arrays_.at(rhs.children[1]->strValue).is2D
                    || (arrays_.at(rhs.children[1]->strValue).isND
                        && arrays_.at(rhs.children[1]->strValue).ndDims.size() == 2))
                && arrays_.at(rhs.children[1]->strValue).dtype == ValueType::DOUBLE) {
                const ArrayInfo    &A   = arrays_.at(rhs.children[1]->strValue);
                const ArrayInfo    &M   = arrays_.at(name);
                const AbstractValue res = inferExpr(rhs, types_, reg_, classes_);
                if (res.type.isConcrete() && !res.type.shape.isScalar()) {
                    const std::string cmp  = rhs.children[0]->strValue == "cummax" ? ">" : "<";
                    const bool        dim2 = cmTwoArg && rhs.children[2]->numValue == 2.0;
                    line("{");
                    ++indent_;
                    line("const std::size_t _nk_m = " + dimExpr(A, 0) + ";");
                    line("const std::size_t _nk_n = " + dimExpr(A, 1) + ";");
                    if (M.isND && M.ndDims.size() == 2) {  // runtime-dim dst: set its companions
                        line(M.ndDims[0] + " = _nk_m;");
                        line(M.ndDims[1] + " = _nk_n;");
                    }
                    line(name + ".assign(_nk_m * _nk_n, 0.0);");
                    // `first` is the line's leading element: i==0 for dim 1, j==0 for dim 2.
                    const std::string first = dim2 ? "_nk_j == 0" : "_nk_i == 0";
                    if (dim2) {  // dim 2: across each row -> outer over rows, inner over columns
                        open("for (std::size_t _nk_i = 0; _nk_i < _nk_m; ++_nk_i)");
                        line("double _nk_acc = 0.0;");
                        open("for (std::size_t _nk_j = 0; _nk_j < _nk_n; ++_nk_j)");
                    } else {  // dim 1 (default): down each column -> outer over columns, inner rows
                        open("for (std::size_t _nk_j = 0; _nk_j < _nk_n; ++_nk_j)");
                        line("double _nk_acc = 0.0;");
                        open("for (std::size_t _nk_i = 0; _nk_i < _nk_m; ++_nk_i)");
                    }
                    line("const double _nk_v = " + A.dataExpr + "[_nk_i + _nk_j * _nk_m];");
                    line("if (" + first + " || _nk_v " + cmp + " _nk_acc"
                         " || (_nk_acc != _nk_acc && _nk_v == _nk_v)) _nk_acc = _nk_v;");
                    line(name + "[_nk_i + _nk_j * _nk_m] = _nk_acc;");
                    close();
                    close();
                    --indent_;
                    line("}");
                    types_.set(name, res);
                    return;
                }
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
        // Native circshift(x, k) -> a circularly-shifted copy, SAME shape as x. A
        // positive k shifts toward higher indices (MATLAB circshift([1 2 3 4],1) =
        // [4 1 2 3]); out[i] = x[((i-k) mod n + n) mod n], which is well-defined for
        // any k (negative / larger than n). dtype-general (a plain element copy).
        // v1: a 1-D array var + a scalar integer shift.
        if (isArrayVar(name) && arrays_.at(name).isLocal && !arrays_.at(name).is2D
            && !arrays_.at(name).isND && rhs.type == NodeType::CALL && rhs.children.size() == 3
            && rhs.children[0]->type == NodeType::IDENTIFIER
            && rhs.children[0]->strValue == "circshift"
            && rhs.children[1]->type == NodeType::IDENTIFIER
            && isArrayVar(rhs.children[1]->strValue)
            && !arrays_.at(rhs.children[1]->strValue).is2D
            && !arrays_.at(rhs.children[1]->strValue).isND) {
            const ArrayInfo    &xa  = arrays_.at(rhs.children[1]->strValue);
            const AbstractValue res = inferExpr(rhs, types_, reg_, classes_);
            if (res.type.isConcrete() && !res.type.shape.isScalar()
                && inferExpr(*rhs.children[2], types_, reg_, classes_).type.shape.isScalar()) {
                const std::string k = emitExpr(*rhs.children[2]);
                line("{");
                ++indent_;
                line("const std::ptrdiff_t _nk_n = static_cast<std::ptrdiff_t>(" + xa.lenVar + ");");
                line("const std::ptrdiff_t _nk_k = static_cast<std::ptrdiff_t>(" + k + ");");
                line(name + ".assign(static_cast<std::size_t>(_nk_n), " + zeroLiteral(xa.dtype)
                     + ");");
                open("for (std::ptrdiff_t _nk_i = 0; _nk_i < _nk_n; ++_nk_i)");
                line("const std::ptrdiff_t _nk_s = ((_nk_i - _nk_k) % _nk_n + _nk_n) % _nk_n;");
                line(name + "[static_cast<std::size_t>(_nk_i)] = " + xa.dataExpr
                     + "[static_cast<std::size_t>(_nk_s)];");
                close();
                --indent_;
                line("}");
                types_.set(name, res);
                return;
            }
        }
        // Native permute(A, [literal perm]) -> reorder A's dimensions (phase N1, the defining
        // N-D op). B's dim k = A's dim perm[k] (1-based); flat over the column-major buffers --
        // for each output o, decompose into B's multi-index (j_k = (o / Bstride_k) % Bdim_k) and
        // gather A at sum_k j_k * Astride_{perm[k]-1}. perm must be a LITERAL permutation of
        // 1..rank (read here; a runtime perm hits the scalar/refuse path). Runtime dims via
        // dimExpr; B a fresh rank-r ndRuntimeLocal; built into a temp so an in-place
        // A = permute(A, perm) is aliasing-safe. v1: DOUBLE A (NDims or KnownDims-2-D), rank 2-4.
        if (isArrayVar(name) && arrays_.at(name).isLocal && arrays_.at(name).ndRuntimeLocal
            && rhs.type == NodeType::CALL && rhs.children.size() == 3
            && rhs.children[0]->type == NodeType::IDENTIFIER
            && rhs.children[0]->strValue == "permute"
            && rhs.children[1]->type == NodeType::IDENTIFIER
            && isArrayVar(rhs.children[1]->strValue)
            && rhs.children[2]->type == NodeType::MATRIX_LITERAL) {
            const ArrayInfo  &A = arrays_.at(rhs.children[1]->strValue);
            const ArrayInfo  &B = arrays_.at(name);
            const std::size_t r = A.isND ? A.ndDims.size() : (A.is2D ? 2u : 0u);
            const ASTNode    &pn = *rhs.children[2];
            // The literal perm: a single row of r integer literals, a permutation of 1..r.
            std::vector<std::size_t> perm;
            if (r >= 2 && r <= 4 && B.ndDims.size() == r && A.dtype == ValueType::DOUBLE
                && pn.children.size() == 1 && pn.children[0]
                && pn.children[0]->children.size() == r) {
                bool              permOk = true;
                std::vector<bool> seen(r, false);
                for (const auto &el : pn.children[0]->children) {
                    if (!el || el->type != NodeType::NUMBER_LITERAL
                        || el->numValue != std::floor(el->numValue) || el->numValue < 1.0
                        || el->numValue > static_cast<double>(r)) {
                        permOk = false;
                        break;
                    }
                    const std::size_t v = static_cast<std::size_t>(el->numValue);  // 1-based
                    if (seen[v - 1]) {
                        permOk = false;  // a repeated axis -> not a permutation
                        break;
                    }
                    seen[v - 1] = true;
                    perm.push_back(v);
                }
                const AbstractValue res = inferExpr(rhs, types_, reg_, classes_);
                if (permOk && perm.size() == r && res.type.isConcrete()
                    && !res.type.shape.isScalar()) {
                    line("{");
                    ++indent_;
                    line("const std::size_t _nk_As0 = 1;");  // A column-major strides (runtime)
                    for (std::size_t d = 1; d < r; ++d)
                        line("const std::size_t _nk_As" + std::to_string(d) + " = _nk_As"
                             + std::to_string(d - 1) + " * (" + dimExpr(A, d - 1) + ");");
                    for (std::size_t k = 0; k < r; ++k)  // B dims = A dims permuted
                        line("const std::size_t _nk_Bd" + std::to_string(k) + " = ("
                             + dimExpr(A, perm[k] - 1) + ");");
                    line("const std::size_t _nk_Bs0 = 1;");  // B strides
                    for (std::size_t k = 1; k < r; ++k)
                        line("const std::size_t _nk_Bs" + std::to_string(k) + " = _nk_Bs"
                             + std::to_string(k - 1) + " * _nk_Bd" + std::to_string(k - 1) + ";");
                    std::string numel = "_nk_Bd0";
                    for (std::size_t k = 1; k < r; ++k) numel += " * _nk_Bd" + std::to_string(k);
                    for (std::size_t k = 0; k < r; ++k)  // set B's dim companions (permuted)
                        line(B.ndDims[k] + " = _nk_Bd" + std::to_string(k) + ";");
                    line("std::vector<double> _nk_out(" + numel + ");");
                    open("for (std::size_t _nk_o = 0; _nk_o < (" + numel + "); ++_nk_o)");
                    line("std::size_t _nk_af = 0;");
                    for (std::size_t k = 0; k < r; ++k)
                        line("_nk_af += ((_nk_o / _nk_Bs" + std::to_string(k) + ") % _nk_Bd"
                             + std::to_string(k) + ") * _nk_As" + std::to_string(perm[k] - 1) + ";");
                    line("_nk_out[_nk_o] = " + A.dataExpr + "[_nk_af];");
                    close();
                    line(name + ".assign(_nk_out.begin(), _nk_out.end());");
                    --indent_;
                    line("}");
                    types_.set(name, res);
                    return;
                }
            }
        }
        // Native circshift(A, k[, dim]) on a 2-D MATRIX with a SCALAR shift, MATLAB semantics.
        // dim 1 (default, rows): each column is circularly shifted, B(i,j)=A(mod(i-k,m),j); the
        // column index passes through. dim 2 (columns): B(i,j)=A(i,mod(j-k,n)); the row index
        // passes through. SAME shape as A. Runtime-dim 2-D operand (dims via dimExpr); a fresh
        // rank-2 ndRuntimeLocal result. dtype-general. Built into a temp first, so an in-place
        // A = circshift(A, k[, dim]) is aliasing-safe. v1: a scalar shift; a LITERAL dim in
        // {1,2} (the transfer keeps a runtime / other dim Dynamic -> bridged); KnownDims-2-D
        // deferred. children: [circshift, A, k] (dim 1) or [circshift, A, k, dim].
        if (isArrayVar(name) && arrays_.at(name).isLocal && arrays_.at(name).ndRuntimeLocal
            && arrays_.at(name).ndDims.size() == 2 && rhs.type == NodeType::CALL
            && (rhs.children.size() == 3
                || (rhs.children.size() == 4 && rhs.children[3]->type == NodeType::NUMBER_LITERAL
                    && (rhs.children[3]->numValue == 1.0 || rhs.children[3]->numValue == 2.0)))
            && rhs.children[0]->type == NodeType::IDENTIFIER
            && rhs.children[0]->strValue == "circshift"
            && rhs.children[1]->type == NodeType::IDENTIFIER
            && isArrayVar(rhs.children[1]->strValue)
            && (arrays_.at(rhs.children[1]->strValue).is2D
                || (arrays_.at(rhs.children[1]->strValue).isND
                    && arrays_.at(rhs.children[1]->strValue).ndDims.size() == 2))) {
            const ArrayInfo    &A   = arrays_.at(rhs.children[1]->strValue);
            const ArrayInfo    &B   = arrays_.at(name);
            const AbstractValue res = inferExpr(rhs, types_, reg_, classes_);
            if (res.type.isConcrete() && !res.type.shape.isScalar()
                && inferExpr(*rhs.children[2], types_, reg_, classes_).type.shape.isScalar()) {
                const long dimv =
                    rhs.children.size() == 4 ? static_cast<long>(rhs.children[3]->numValue) : 1;
                const std::string ct = cppScalarType(B.dtype);
                const std::string m = dimExpr(A, 0), n = dimExpr(A, 1);
                const std::string k = emitExpr(*rhs.children[2]);
                line("{");
                ++indent_;
                line("const std::ptrdiff_t _nk_m = static_cast<std::ptrdiff_t>(" + m + ");");
                line("const std::ptrdiff_t _nk_n = static_cast<std::ptrdiff_t>(" + n + ");");
                line("const std::ptrdiff_t _nk_k = static_cast<std::ptrdiff_t>(" + k + ");");
                line("std::vector<" + ct + "> _nk_out(static_cast<std::size_t>(_nk_m * _nk_n));");
                open("for (std::ptrdiff_t _nk_j = 0; _nk_j < _nk_n; ++_nk_j)");
                open("for (std::ptrdiff_t _nk_i = 0; _nk_i < _nk_m; ++_nk_i)");
                if (dimv == 1) {
                    line("const std::ptrdiff_t _nk_ri = ((_nk_i - _nk_k) % _nk_m + _nk_m) % _nk_m;");
                    line("_nk_out[static_cast<std::size_t>(_nk_i + _nk_j * _nk_m)] = " + A.dataExpr
                         + "[static_cast<std::size_t>(_nk_ri + _nk_j * _nk_m)];");
                } else {  // dim 2: wrap the COLUMN index, the row passes through
                    line("const std::ptrdiff_t _nk_rj = ((_nk_j - _nk_k) % _nk_n + _nk_n) % _nk_n;");
                    line("_nk_out[static_cast<std::size_t>(_nk_i + _nk_j * _nk_m)] = " + A.dataExpr
                         + "[static_cast<std::size_t>(_nk_i + _nk_rj * _nk_m)];");
                }
                close();
                close();
                line(B.ndDims[0] + " = static_cast<std::size_t>(_nk_m);");
                line(B.ndDims[1] + " = static_cast<std::size_t>(_nk_n);");
                line(name + ".assign(_nk_out.begin(), _nk_out.end());");
                --indent_;
                line("}");
                types_.set(name, res);
                return;
            }
        }
        // Native gradient(y) -> numerical gradient, unit spacing, SAME length as y.
        // Edge points use one-sided differences, the interior centered: g[0]=y[1]-y[0];
        // g[i]=(y[i+1]-y[i-1])/2; g[n-1]=y[n-1]-y[n-2]; n==1 -> {0}; n==0 -> empty.
        // Exact -> every tier. v1: a single 1-D DOUBLE array var; result a 1-D LOCAL.
        if (isArrayVar(name) && arrays_.at(name).isLocal && !arrays_.at(name).is2D
            && !arrays_.at(name).isND && rhs.type == NodeType::CALL && rhs.children.size() == 2
            && rhs.children[0]->type == NodeType::IDENTIFIER
            && rhs.children[0]->strValue == "gradient"
            && rhs.children[1]->type == NodeType::IDENTIFIER
            && isArrayVar(rhs.children[1]->strValue)
            && !arrays_.at(rhs.children[1]->strValue).is2D
            && !arrays_.at(rhs.children[1]->strValue).isND
            && arrays_.at(rhs.children[1]->strValue).dtype == ValueType::DOUBLE) {
            const ArrayInfo    &y   = arrays_.at(rhs.children[1]->strValue);
            const AbstractValue res = inferExpr(rhs, types_, reg_, classes_);
            if (res.type.isConcrete() && !res.type.shape.isScalar()) {
                const std::string d = y.dataExpr;
                line("{");
                ++indent_;
                line("const std::size_t _nk_n = " + y.lenVar + ";");
                line(name + ".assign(_nk_n, 0.0);");
                open("for (std::size_t _nk_i = 0; _nk_i < _nk_n; ++_nk_i)");
                line("if (_nk_n == 1) " + name + "[_nk_i] = 0.0;");
                line("else if (_nk_i == 0) " + name + "[_nk_i] = " + d + "[1] - " + d + "[0];");
                line("else if (_nk_i + 1 == _nk_n) " + name + "[_nk_i] = " + d + "[_nk_i] - " + d
                     + "[_nk_i - 1];");
                line("else " + name + "[_nk_i] = (" + d + "[_nk_i + 1] - " + d
                     + "[_nk_i - 1]) * 0.5;");
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
        // Native flip/fliplr/flipud(A) on a 2-D MATRIX (KnownDims or runtime-dim) -> a fresh
        // same-shape matrix. fliplr reverses the COLUMN order, flipud (and flip's default
        // dim-1) reverses the ROW order. The 2-arg flip(A, dim) form with a LITERAL dim 1|2
        // is also handled: dim 1 reverses rows (= flipud), dim 2 reverses columns (= fliplr).
        // Column-major, both tiers via dimExpr: fliplr M[i+j*m] = A[i+(n-1-j)*m]; flipud
        // M[i+j*m] = A[(m-1-i)+j*m]. v1: a DOUBLE matrix var distinct from the dest (an in-place
        // B=flip(B) would self-alias the read).
        {
            const bool flipTwoArg =
                rhs.type == NodeType::CALL && rhs.children.size() == 3
                && rhs.children[0]->type == NodeType::IDENTIFIER
                && rhs.children[0]->strValue == "flip"
                && rhs.children[2]->type == NodeType::NUMBER_LITERAL
                && (rhs.children[2]->numValue == 1.0 || rhs.children[2]->numValue == 2.0);
            if (isArrayVar(name) && arrays_.at(name).isLocal
                && (arrays_.at(name).is2D
                    || (arrays_.at(name).isND && arrays_.at(name).ndDims.size() == 2))
                && rhs.type == NodeType::CALL && (rhs.children.size() == 2 || flipTwoArg)
                && rhs.children[0]->type == NodeType::IDENTIFIER
                && (rhs.children[0]->strValue == "flip" || rhs.children[0]->strValue == "fliplr"
                    || rhs.children[0]->strValue == "flipud")
                && rhs.children[1]->type == NodeType::IDENTIFIER
                && isArrayVar(rhs.children[1]->strValue) && rhs.children[1]->strValue != name
                && (arrays_.at(rhs.children[1]->strValue).is2D
                    || (arrays_.at(rhs.children[1]->strValue).isND
                        && arrays_.at(rhs.children[1]->strValue).ndDims.size() == 2))) {
            const ArrayInfo    &A   = arrays_.at(rhs.children[1]->strValue);
            const ArrayInfo    &M   = arrays_.at(name);
            const std::string  &fn  = rhs.children[0]->strValue;
            const AbstractValue res = inferExpr(rhs, types_, reg_, classes_);
            if (res.type.isConcrete()) {
                const bool flipRows = flipTwoArg ? (rhs.children[2]->numValue == 1.0)
                                                 : (fn == "flipud" || fn == "flip");  // dim 1 = rows
                line("{");
                ++indent_;
                line("const std::size_t _nk_m = " + dimExpr(A, 0) + ";");
                line("const std::size_t _nk_n = " + dimExpr(A, 1) + ";");
                if (M.isND && M.ndDims.size() == 2) {  // runtime-dim dst: set its companions
                    line(M.ndDims[0] + " = _nk_m;");
                    line(M.ndDims[1] + " = _nk_n;");
                }
                line(name + ".assign(_nk_m * _nk_n, " + zeroLiteral(M.dtype) + ");");
                open("for (std::size_t _nk_j = 0; _nk_j < _nk_n; ++_nk_j)");
                open("for (std::size_t _nk_i = 0; _nk_i < _nk_m; ++_nk_i)");
                line(name + "[_nk_i + _nk_j * _nk_m] = " + A.dataExpr + "["
                     + (flipRows ? "(_nk_m - 1 - _nk_i) + _nk_j * _nk_m"
                                 : "_nk_i + (_nk_n - 1 - _nk_j) * _nk_m")
                     + "];");
                close();
                close();
                --indent_;
                line("}");
                types_.set(name, res);
                return;
            }
            }
        }
        // Native flip(A, dim) on a rank-3 ARRAY -> a fresh same-shape rank-3 array, reversing along
        // a LITERAL dim 1|2|3: dim 1 reverses rows within each page, dim 2 reverses columns, dim 3
        // reverses the page order. Column-major: B[i + j*m + k*m*n] = A[i' + j'*m + k'*m*n] with the
        // chosen axis coordinate reflected (m-1-i / n-1-j / p-1-k). A runtime-dim rank-3
        // ndRuntimeLocal dest. v1: the 2-arg explicit-dim form (1-arg flip(A) on a rank-3 stays
        // bridged -- first-non-singleton-dim ambiguity); a DOUBLE rank-3 var distinct from the dest.
        if (isArrayVar(name) && arrays_.at(name).isLocal && arrays_.at(name).ndRuntimeLocal
            && arrays_.at(name).ndDims.size() == 3 && rhs.type == NodeType::CALL
            && rhs.children.size() == 3 && rhs.children[0]->type == NodeType::IDENTIFIER
            && rhs.children[0]->strValue == "flip"
            && rhs.children[2]->type == NodeType::NUMBER_LITERAL
            && (rhs.children[2]->numValue == 1.0 || rhs.children[2]->numValue == 2.0
                || rhs.children[2]->numValue == 3.0)
            && rhs.children[1]->type == NodeType::IDENTIFIER && isArrayVar(rhs.children[1]->strValue)
            && rhs.children[1]->strValue != name && arrays_.at(rhs.children[1]->strValue).isND
            && arrays_.at(rhs.children[1]->strValue).ndDims.size() == 3
            && arrays_.at(rhs.children[1]->strValue).dtype == ValueType::DOUBLE) {
            const ArrayInfo    &A   = arrays_.at(rhs.children[1]->strValue);
            const ArrayInfo    &M   = arrays_.at(name);
            const AbstractValue res = inferExpr(rhs, types_, reg_, classes_);
            if (res.type.isConcrete() && !res.type.shape.isScalar()) {
                const int d = static_cast<int>(rhs.children[2]->numValue);  // 1, 2, or 3
                line("{");
                ++indent_;
                line("const std::size_t _nk_m = " + dimExpr(A, 0) + ";");
                line("const std::size_t _nk_n = " + dimExpr(A, 1) + ";");
                line("const std::size_t _nk_p = " + dimExpr(A, 2) + ";");
                line(M.ndDims[0] + " = _nk_m;");
                line(M.ndDims[1] + " = _nk_n;");
                line(M.ndDims[2] + " = _nk_p;");
                line(name + ".assign(_nk_m * _nk_n * _nk_p, 0.0);");
                const std::string ip = d == 1 ? "(_nk_m - 1 - _nk_i)" : "_nk_i";
                const std::string jp = d == 2 ? "(_nk_n - 1 - _nk_j)" : "_nk_j";
                const std::string kp = d == 3 ? "(_nk_p - 1 - _nk_k)" : "_nk_k";
                open("for (std::size_t _nk_k = 0; _nk_k < _nk_p; ++_nk_k)");
                open("for (std::size_t _nk_j = 0; _nk_j < _nk_n; ++_nk_j)");
                open("for (std::size_t _nk_i = 0; _nk_i < _nk_m; ++_nk_i)");
                line(name + "[_nk_i + _nk_j * _nk_m + _nk_k * _nk_m * _nk_n] = " + A.dataExpr + "["
                     + ip + " + " + jp + " * _nk_m + " + kp + " * _nk_m * _nk_n];");
                close();
                close();
                close();
                --indent_;
                line("}");
                types_.set(name, res);
                return;
            }
        }
        // Native rot90(A) on a 2-D matrix -> a 90deg-CCW rotation, dims swapped (A m x n ->
        // B n x m). Column-major: B[i + j*n] = A[j + (n-1-i)*m] (B(i,j) = A(j, n-1-i)). Both
        // tiers via dimExpr; a runtime dst gets its companions set (rows=n, cols=m). v1: the
        // 1-arg form, a DOUBLE matrix var distinct from the dest. (rot90(A,k) / vector rot90
        // stay bridged.)
        if (isArrayVar(name) && arrays_.at(name).isLocal
            && (arrays_.at(name).is2D
                || (arrays_.at(name).isND && arrays_.at(name).ndDims.size() == 2))
            && rhs.type == NodeType::CALL && rhs.children.size() == 2
            && rhs.children[0]->type == NodeType::IDENTIFIER && rhs.children[0]->strValue == "rot90"
            && rhs.children[1]->type == NodeType::IDENTIFIER
            && isArrayVar(rhs.children[1]->strValue) && rhs.children[1]->strValue != name
            && (arrays_.at(rhs.children[1]->strValue).is2D
                || (arrays_.at(rhs.children[1]->strValue).isND
                    && arrays_.at(rhs.children[1]->strValue).ndDims.size() == 2))
            && arrays_.at(rhs.children[1]->strValue).dtype == ValueType::DOUBLE) {
            const ArrayInfo    &A   = arrays_.at(rhs.children[1]->strValue);
            const ArrayInfo    &M   = arrays_.at(name);
            const AbstractValue res = inferExpr(rhs, types_, reg_, classes_);
            if (res.type.isConcrete()) {
                line("{");
                ++indent_;
                line("const std::size_t _nk_m = " + dimExpr(A, 0) + ";");  // A rows
                line("const std::size_t _nk_n = " + dimExpr(A, 1) + ";");  // A cols
                if (M.isND && M.ndDims.size() == 2) {  // result is n x m
                    line(M.ndDims[0] + " = _nk_n;");
                    line(M.ndDims[1] + " = _nk_m;");
                }
                line(name + ".assign(_nk_n * _nk_m, 0.0);");
                open("for (std::size_t _nk_j = 0; _nk_j < _nk_m; ++_nk_j)");  // B cols
                open("for (std::size_t _nk_i = 0; _nk_i < _nk_n; ++_nk_i)");  // B rows
                line(name + "[_nk_i + _nk_j * _nk_n] = " + A.dataExpr
                     + "[_nk_j + (_nk_n - 1 - _nk_i) * _nk_m];");
                close();
                close();
                --indent_;
                line("}");
                types_.set(name, res);
                return;
            }
        }
        // Native kron(A, B) -> the Kronecker product, A m x n and B p x q -> C (m*p)x(n*q),
        // a rank-2 ndRuntimeLocal. Each result element C(R,C) = A(R/p, C/q) * B(R%p, C%q)
        // (0-based; column-major). Both tiers via dimExpr; the runtime dst gets its
        // companions set. v1: two DOUBLE matrix vars, distinct from the dest.
        if (isArrayVar(name) && arrays_.at(name).isLocal && arrays_.at(name).ndRuntimeLocal
            && arrays_.at(name).ndDims.size() == 2 && rhs.type == NodeType::CALL
            && rhs.children.size() == 3 && rhs.children[0]->type == NodeType::IDENTIFIER
            && rhs.children[0]->strValue == "kron"
            && rhs.children[1]->type == NodeType::IDENTIFIER && isArrayVar(rhs.children[1]->strValue)
            && rhs.children[2]->type == NodeType::IDENTIFIER && isArrayVar(rhs.children[2]->strValue)
            && rhs.children[1]->strValue != name && rhs.children[2]->strValue != name) {
            const ArrayInfo &A = arrays_.at(rhs.children[1]->strValue);
            const ArrayInfo &B = arrays_.at(rhs.children[2]->strValue);
            const bool aMat = A.is2D || (A.isND && A.ndDims.size() == 2);
            const bool bMat = B.is2D || (B.isND && B.ndDims.size() == 2);
            if (aMat && bMat && A.dtype == ValueType::DOUBLE && B.dtype == ValueType::DOUBLE) {
                const ArrayInfo    &M   = arrays_.at(name);
                const AbstractValue res = inferExpr(rhs, types_, reg_, classes_);
                line("{");
                ++indent_;
                line("const std::size_t _nk_m = " + dimExpr(A, 0) + ";");  // A rows
                line("const std::size_t _nk_n = " + dimExpr(A, 1) + ";");  // A cols
                line("const std::size_t _nk_p = " + dimExpr(B, 0) + ";");  // B rows
                line("const std::size_t _nk_q = " + dimExpr(B, 1) + ";");  // B cols
                line("const std::size_t _nk_mp = _nk_m * _nk_p;");         // result rows
                line("const std::size_t _nk_nq = _nk_n * _nk_q;");         // result cols
                line(M.ndDims[0] + " = _nk_mp;");
                line(M.ndDims[1] + " = _nk_nq;");
                line(name + ".assign(_nk_mp * _nk_nq, 0.0);");
                open("for (std::size_t _nk_C = 0; _nk_C < _nk_nq; ++_nk_C)");
                open("for (std::size_t _nk_R = 0; _nk_R < _nk_mp; ++_nk_R)");
                line(name + "[_nk_R + _nk_C * _nk_mp] = "
                     + A.dataExpr + "[(_nk_R / _nk_p) + (_nk_C / _nk_q) * _nk_m] * "
                     + B.dataExpr + "[(_nk_R % _nk_p) + (_nk_C % _nk_q) * _nk_p];");
                close();
                close();
                --indent_;
                line("}");
                types_.set(name, res);
                return;
            }
        }
        // Native cat(dim, A, B) with a LITERAL dim -> concatenate two 2-D matrices: dim==2 is
        // horizontal (M cols = A cols + B cols, a column-major buffer concat -- A's columns
        // then B's), dim==1 is vertical (M rows = A rows + B rows, a per-column interleave).
        // A runtime-dim 2-D result; both tiers via dimExpr; runtime dst companions set;
        // matching-dimension guard. v1: two DOUBLE matrix vars distinct from the dest.
        if (isArrayVar(name) && arrays_.at(name).isLocal && arrays_.at(name).ndRuntimeLocal
            && arrays_.at(name).ndDims.size() == 2 && rhs.type == NodeType::CALL
            && rhs.children.size() == 4 && rhs.children[0]->type == NodeType::IDENTIFIER
            && rhs.children[0]->strValue == "cat"
            && rhs.children[1]->type == NodeType::NUMBER_LITERAL
            && (rhs.children[1]->numValue == 1.0 || rhs.children[1]->numValue == 2.0)
            && rhs.children[2]->type == NodeType::IDENTIFIER && isArrayVar(rhs.children[2]->strValue)
            && rhs.children[3]->type == NodeType::IDENTIFIER && isArrayVar(rhs.children[3]->strValue)
            && rhs.children[2]->strValue != name && rhs.children[3]->strValue != name) {
            const ArrayInfo &A = arrays_.at(rhs.children[2]->strValue);
            const ArrayInfo &B = arrays_.at(rhs.children[3]->strValue);
            const bool aMat = A.is2D || (A.isND && A.ndDims.size() == 2);
            const bool bMat = B.is2D || (B.isND && B.ndDims.size() == 2);
            if (aMat && bMat && A.dtype == ValueType::DOUBLE && B.dtype == ValueType::DOUBLE) {
                const ArrayInfo    &M   = arrays_.at(name);
                const AbstractValue res = inferExpr(rhs, types_, reg_, classes_);
                const bool          horz = rhs.children[1]->numValue == 2.0;
                line("{");
                ++indent_;
                line("const std::size_t _nk_ar = " + dimExpr(A, 0) + ";");  // A rows
                line("const std::size_t _nk_ac = " + dimExpr(A, 1) + ";");  // A cols
                line("const std::size_t _nk_br = " + dimExpr(B, 0) + ";");  // B rows
                line("const std::size_t _nk_bc = " + dimExpr(B, 1) + ";");  // B cols
                if (horz) {  // cat(2,..): A cols then B cols; rows must agree
                    line("if (_nk_ar != _nk_br) throw std::out_of_range(\"numkit: cat dim-1 "
                         "sizes must agree\");");
                    line(M.ndDims[0] + " = _nk_ar;");
                    line(M.ndDims[1] + " = _nk_ac + _nk_bc;");
                    line(name + ".assign(_nk_ar * (_nk_ac + _nk_bc), 0.0);");
                    open("for (std::size_t _nk_k = 0; _nk_k < _nk_ar * _nk_ac; ++_nk_k)");
                    line(name + "[_nk_k] = " + A.dataExpr + "[_nk_k];");
                    close();
                    open("for (std::size_t _nk_k = 0; _nk_k < _nk_ar * _nk_bc; ++_nk_k)");
                    line(name + "[_nk_ar * _nk_ac + _nk_k] = " + B.dataExpr + "[_nk_k];");
                    close();
                } else {  // cat(1,..): A rows then B rows interleaved per column; cols agree
                    line("if (_nk_ac != _nk_bc) throw std::out_of_range(\"numkit: cat dim-2 "
                         "sizes must agree\");");
                    line("const std::size_t _nk_tr = _nk_ar + _nk_br;");
                    line(M.ndDims[0] + " = _nk_tr;");
                    line(M.ndDims[1] + " = _nk_ac;");
                    line(name + ".assign(_nk_tr * _nk_ac, 0.0);");
                    open("for (std::size_t _nk_j = 0; _nk_j < _nk_ac; ++_nk_j)");
                    open("for (std::size_t _nk_i = 0; _nk_i < _nk_ar; ++_nk_i)");
                    line(name + "[_nk_i + _nk_j * _nk_tr] = " + A.dataExpr + "[_nk_i + _nk_j * _nk_ar];");
                    close();
                    open("for (std::size_t _nk_i = 0; _nk_i < _nk_br; ++_nk_i)");
                    line(name + "[(_nk_ar + _nk_i) + _nk_j * _nk_tr] = " + B.dataExpr
                         + "[_nk_i + _nk_j * _nk_br];");
                    close();
                    close();
                }
                --indent_;
                line("}");
                types_.set(name, res);
                return;
            }
        }
        // Native cat(3, A, B, C, ...): stack N same-leading-size matrices/pages into a rank-3
        // array (phase N2 + N18 N-operand). Concat along the new TRAILING dim is a CONTIGUOUS
        // buffer append in column-major -- M = op0 pages ++ op1 pages ++ ... A 2-D operand is
        // one page; a rank-3 operand has size(.,3) pages. A rank-3 ndRuntimeLocal dest; dims
        // [m, n, sum(pages)]; per-operand page-size guard; running offset across operands. v1:
        // >=2 DOUBLE 2-D/rank-3 array vars distinct from the dest.
        if (isArrayVar(name) && arrays_.at(name).isLocal && arrays_.at(name).ndRuntimeLocal
            && arrays_.at(name).ndDims.size() == 3 && rhs.type == NodeType::CALL
            && rhs.children.size() >= 4 && rhs.children[0]->type == NodeType::IDENTIFIER
            && rhs.children[0]->strValue == "cat"
            && rhs.children[1]->type == NodeType::NUMBER_LITERAL
            && rhs.children[1]->numValue == 3.0) {
            const std::size_t nOps = rhs.children.size() - 2;
            bool              allOk = true;
            for (std::size_t i = 0; i < nOps && allOk; ++i) {
                const ASTNode &c = *rhs.children[2 + i];
                if (!(c.type == NodeType::IDENTIFIER && isArrayVar(c.strValue) && c.strValue != name)) {
                    allOk = false;
                    break;
                }
                const ArrayInfo &op = arrays_.at(c.strValue);
                if (!((op.is2D || (op.isND && (op.ndDims.size() == 2 || op.ndDims.size() == 3)))
                      && op.dtype == ValueType::DOUBLE))
                    allOk = false;
            }
            if (allOk) {
                const ArrayInfo    &M   = arrays_.at(name);
                const AbstractValue res = inferExpr(rhs, types_, reg_, classes_);
                const ArrayInfo    &A0  = arrays_.at(rhs.children[2]->strValue);
                line("{");
                ++indent_;
                line("const std::size_t _nk_ar = " + dimExpr(A0, 0) + ";");
                line("const std::size_t _nk_ac = " + dimExpr(A0, 1) + ";");
                line("const std::size_t _nk_pg = _nk_ar * _nk_ac;");  // one-page element count
                std::string totalPages;
                for (std::size_t i = 0; i < nOps; ++i) {
                    const ArrayInfo  &op = arrays_.at(rhs.children[2 + i]->strValue);
                    if (i > 0)
                        line("if (" + dimExpr(op, 0) + " != _nk_ar || " + dimExpr(op, 1)
                             + " != _nk_ac) throw std::out_of_range(\"numkit: cat(3) page sizes "
                               "must agree\");");
                    const std::string pi =
                        (op.isND && op.ndDims.size() == 3) ? dimExpr(op, 2) : std::string("1");
                    line("const std::size_t _nk_p" + std::to_string(i) + " = " + pi + ";");
                    totalPages += (i ? " + " : "") + ("_nk_p" + std::to_string(i));
                }
                line(M.ndDims[0] + " = _nk_ar;");
                line(M.ndDims[1] + " = _nk_ac;");
                line(M.ndDims[2] + " = " + totalPages + ";");
                line(name + ".assign(_nk_pg * (" + totalPages + "), 0.0);");
                line("std::size_t _nk_off = 0;");
                for (std::size_t i = 0; i < nOps; ++i) {
                    const ArrayInfo &op = arrays_.at(rhs.children[2 + i]->strValue);
                    line("{");
                    ++indent_;
                    line("const std::size_t _nk_n = _nk_pg * _nk_p" + std::to_string(i) + ";");
                    open("for (std::size_t _nk_k = 0; _nk_k < _nk_n; ++_nk_k)");
                    line(name + "[_nk_off + _nk_k] = " + op.dataExpr + "[_nk_k];");
                    close();
                    line("_nk_off += _nk_n;");
                    --indent_;
                    line("}");
                }
                --indent_;
                line("}");
                types_.set(name, res);
                return;
            }
        }
        // Native cat(4, A, B, C, ...): append N operands along dim 4 -> rank-4 (m x n x p x
        // sum(slabs); phase N15 + N19 N-operand). The mirror of N-operand cat(3) one rank up --
        // a contiguous trailing-dim buffer append (M = op0 slabs ++ op1 slabs ++ ...). A rank-3
        // operand is one slab (m*n*p elems); a rank-4 operand has size(.,4) slabs. Operands share
        // the leading dims m,n,p. Running offset across operands; per-operand leading-dim guard.
        // v1: >=2 DOUBLE rank-3/rank-4 array vars distinct from the dest.
        if (isArrayVar(name) && arrays_.at(name).isLocal && arrays_.at(name).ndRuntimeLocal
            && arrays_.at(name).ndDims.size() == 4 && rhs.type == NodeType::CALL
            && rhs.children.size() >= 4 && rhs.children[0]->type == NodeType::IDENTIFIER
            && rhs.children[0]->strValue == "cat"
            && rhs.children[1]->type == NodeType::NUMBER_LITERAL
            && rhs.children[1]->numValue == 4.0) {
            const std::size_t nOps = rhs.children.size() - 2;
            bool              allOk = true;
            for (std::size_t i = 0; i < nOps && allOk; ++i) {
                const ASTNode &c = *rhs.children[2 + i];
                if (!(c.type == NodeType::IDENTIFIER && isArrayVar(c.strValue) && c.strValue != name)) {
                    allOk = false;
                    break;
                }
                const ArrayInfo &op = arrays_.at(c.strValue);
                if (!(op.isND && (op.ndDims.size() == 3 || op.ndDims.size() == 4)
                      && op.dtype == ValueType::DOUBLE))
                    allOk = false;
            }
            if (allOk) {
                const ArrayInfo    &M   = arrays_.at(name);
                const AbstractValue res = inferExpr(rhs, types_, reg_, classes_);
                const ArrayInfo    &A0  = arrays_.at(rhs.children[2]->strValue);
                line("{");
                ++indent_;
                line("const std::size_t _nk_d0 = " + dimExpr(A0, 0) + ";");
                line("const std::size_t _nk_d1 = " + dimExpr(A0, 1) + ";");
                line("const std::size_t _nk_d2 = " + dimExpr(A0, 2) + ";");
                line("const std::size_t _nk_sl = _nk_d0 * _nk_d1 * _nk_d2;");  // one-slab elems
                std::string totalSlabs;
                for (std::size_t i = 0; i < nOps; ++i) {
                    const ArrayInfo &op = arrays_.at(rhs.children[2 + i]->strValue);
                    if (i > 0)
                        line("if (" + dimExpr(op, 0) + " != _nk_d0 || " + dimExpr(op, 1)
                             + " != _nk_d1 || " + dimExpr(op, 2)
                             + " != _nk_d2) throw std::out_of_range(\"numkit: cat(4) leading dims "
                               "must agree\");");
                    const std::string si =
                        op.ndDims.size() == 4 ? dimExpr(op, 3) : std::string("1");
                    line("const std::size_t _nk_s" + std::to_string(i) + " = " + si + ";");
                    totalSlabs += (i ? " + " : "") + ("_nk_s" + std::to_string(i));
                }
                line(M.ndDims[0] + " = _nk_d0;");
                line(M.ndDims[1] + " = _nk_d1;");
                line(M.ndDims[2] + " = _nk_d2;");
                line(M.ndDims[3] + " = " + totalSlabs + ";");
                line(name + ".assign(_nk_sl * (" + totalSlabs + "), 0.0);");
                line("std::size_t _nk_off = 0;");
                for (std::size_t i = 0; i < nOps; ++i) {
                    const ArrayInfo &op = arrays_.at(rhs.children[2 + i]->strValue);
                    line("{");
                    ++indent_;
                    line("const std::size_t _nk_n = _nk_sl * _nk_s" + std::to_string(i) + ";");
                    open("for (std::size_t _nk_k = 0; _nk_k < _nk_n; ++_nk_k)");
                    line(name + "[_nk_off + _nk_k] = " + op.dataExpr + "[_nk_k];");
                    close();
                    line("_nk_off += _nk_n;");
                    --indent_;
                    line("}");
                }
                --indent_;
                line("}");
                types_.set(name, res);
                return;
            }
        }
        // Native page-slice read B = A(:,:,k): extract page k of a rank-3 A as a 2-D m x n
        // matrix (phase N3). Column-major: page k (1-based) is the CONTIGUOUS block
        // A[(k-1)*m*n .. +m*n] -> a straight copy. B a runtime-dim 2-D ndRuntimeLocal; bounds-
        // checked k; `end` in the page index = size(A,3). v1: rank-3 DOUBLE A, two leading bare
        // colons + a scalar k; B distinct from A.
        if (isArrayVar(name) && arrays_.at(name).isLocal && arrays_.at(name).ndRuntimeLocal
            && arrays_.at(name).ndDims.size() == 2 && rhs.type == NodeType::CALL
            && rhs.children.size() == 4 && rhs.children[0]->type == NodeType::IDENTIFIER
            && isArrayVar(rhs.children[0]->strValue) && rhs.children[0]->strValue != name
            && arrays_.at(rhs.children[0]->strValue).isND
            && arrays_.at(rhs.children[0]->strValue).ndDims.size() == 3
            && arrays_.at(rhs.children[0]->strValue).dtype == ValueType::DOUBLE
            && rhs.children[1]->type == NodeType::COLON_EXPR && rhs.children[1]->children.empty()
            && rhs.children[2]->type == NodeType::COLON_EXPR && rhs.children[2]->children.empty()
            && rhs.children[3]->type != NodeType::COLON_EXPR) {
            const ArrayInfo    &A   = arrays_.at(rhs.children[0]->strValue);
            const ArrayInfo    &B   = arrays_.at(name);
            const AbstractValue res = inferExpr(rhs, types_, reg_, classes_);
            if (res.type.isConcrete() && !res.type.shape.isScalar()
                && inferExpr(*rhs.children[3], types_, reg_, classes_).type.shape.isScalar()) {
                const std::string m = dimExpr(A, 0), n = dimExpr(A, 1), p = dimExpr(A, 2);
                endStack_.push_back(p);  // `end` in the page index = size(A,3)
                const std::string k = emitExpr(*rhs.children[3]);
                endStack_.pop_back();
                line("{");
                ++indent_;
                line("const std::size_t _nk_m = " + m + ";");
                line("const std::size_t _nk_n = " + n + ";");
                line("const std::ptrdiff_t _nk_k = static_cast<std::ptrdiff_t>(" + k + ");");
                line("if (_nk_k < 1 || _nk_k > static_cast<std::ptrdiff_t>(" + p + "))");
                line("    throw std::out_of_range(\"numkit: page index out of bounds\");");
                line("const std::size_t _nk_pg = _nk_m * _nk_n;");
                line("const std::size_t _nk_off = static_cast<std::size_t>(_nk_k - 1) * _nk_pg;");
                line(B.ndDims[0] + " = _nk_m;");
                line(B.ndDims[1] + " = _nk_n;");
                line(name + ".assign(" + A.dataExpr + " + _nk_off, " + A.dataExpr
                     + " + _nk_off + _nk_pg);");
                --indent_;
                line("}");
                types_.set(name, res);
                return;
            }
        }
        // Native 2-D COLUMN-RANGE read B = A(:, j1:j2): extract columns j1..j2 of a 2-D A as a
        // runtime-dim 2-D rows x (j2-j1+1) sub-matrix (phase N22). Column-major: columns j1..j2
        // are the CONTIGUOUS block A[(j1-1)*rows .. j2*rows] -> a straight copy. B a runtime-dim
        // 2-D ndRuntimeLocal; bounds-checked range; `end` in the column index = cols. v1: 2-D
        // DOUBLE A (KnownDims or runtime), a leading bare colon + a step-1 range j1:j2, B distinct.
        if (isArrayVar(name) && arrays_.at(name).isLocal && arrays_.at(name).ndRuntimeLocal
            && arrays_.at(name).ndDims.size() == 2 && rhs.type == NodeType::CALL
            && rhs.children.size() == 3 && rhs.children[0]->type == NodeType::IDENTIFIER
            && isArrayVar(rhs.children[0]->strValue) && rhs.children[0]->strValue != name
            && (arrays_.at(rhs.children[0]->strValue).is2D
                || (arrays_.at(rhs.children[0]->strValue).isND
                    && arrays_.at(rhs.children[0]->strValue).ndDims.size() == 2))
            && arrays_.at(rhs.children[0]->strValue).dtype == ValueType::DOUBLE
            && rhs.children[1]->type == NodeType::COLON_EXPR && rhs.children[1]->children.empty()
            && rhs.children[2]->type == NodeType::COLON_EXPR && rhs.children[2]->children.size() == 2) {
            const ArrayInfo    &A   = arrays_.at(rhs.children[0]->strValue);
            const ArrayInfo    &B   = arrays_.at(name);
            const AbstractValue res = inferExpr(rhs, types_, reg_, classes_);
            if (res.type.isConcrete() && !res.type.shape.isScalar()) {
                const std::string rows = dimExpr(A, 0), cols = dimExpr(A, 1);
                const ASTNode    &rng  = *rhs.children[2];  // j1:j2
                endStack_.push_back(cols);                  // `end` in the column index = cols
                const std::string j1 = emitExpr(*rng.children[0]);
                const std::string j2 = emitExpr(*rng.children[1]);
                endStack_.pop_back();
                line("{");
                ++indent_;
                line("const std::size_t _nk_rows = " + rows + ";");
                line("const std::ptrdiff_t _nk_j1 = static_cast<std::ptrdiff_t>(" + j1 + ");");
                line("const std::ptrdiff_t _nk_j2 = static_cast<std::ptrdiff_t>(" + j2 + ");");
                line("if (_nk_j1 < 1 || _nk_j2 > static_cast<std::ptrdiff_t>(" + cols
                     + ") || _nk_j2 < _nk_j1)");
                line("    throw std::out_of_range(\"numkit: column range out of bounds\");");
                line("const std::size_t _nk_nc = static_cast<std::size_t>(_nk_j2 - _nk_j1 + 1);");
                line("const std::size_t _nk_off = static_cast<std::size_t>(_nk_j1 - 1) * _nk_rows;");
                line(B.ndDims[0] + " = _nk_rows;");
                line(B.ndDims[1] + " = _nk_nc;");
                line(name + ".assign(" + A.dataExpr + " + _nk_off, " + A.dataExpr
                     + " + _nk_off + _nk_nc * _nk_rows);");
                --indent_;
                line("}");
                types_.set(name, res);
                return;
            }
        }
        // Native 2-D ROW-RANGE read B = A(i1:i2, :): extract rows i1..i2 of a 2-D A as a runtime-
        // dim 2-D (i2-i1+1) x cols sub-matrix (phase N23). STRIDED (a row block is not contiguous
        // in column-major): for each column j the kept rows are the run A[(i1-1)+j*rows ..
        // i2+j*rows). B a runtime-dim 2-D ndRuntimeLocal; bounds-checked range; `end` in the row
        // index = rows. v1: 2-D DOUBLE A, a leading step-1 range i1:i2 + a trailing bare colon, B
        // distinct.
        if (isArrayVar(name) && arrays_.at(name).isLocal && arrays_.at(name).ndRuntimeLocal
            && arrays_.at(name).ndDims.size() == 2 && rhs.type == NodeType::CALL
            && rhs.children.size() == 3 && rhs.children[0]->type == NodeType::IDENTIFIER
            && isArrayVar(rhs.children[0]->strValue) && rhs.children[0]->strValue != name
            && (arrays_.at(rhs.children[0]->strValue).is2D
                || (arrays_.at(rhs.children[0]->strValue).isND
                    && arrays_.at(rhs.children[0]->strValue).ndDims.size() == 2))
            && arrays_.at(rhs.children[0]->strValue).dtype == ValueType::DOUBLE
            && rhs.children[1]->type == NodeType::COLON_EXPR && rhs.children[1]->children.size() == 2
            && rhs.children[2]->type == NodeType::COLON_EXPR && rhs.children[2]->children.empty()) {
            const ArrayInfo    &A   = arrays_.at(rhs.children[0]->strValue);
            const ArrayInfo    &B   = arrays_.at(name);
            const AbstractValue res = inferExpr(rhs, types_, reg_, classes_);
            if (res.type.isConcrete() && !res.type.shape.isScalar()) {
                const std::string rows = dimExpr(A, 0), cols = dimExpr(A, 1);
                const ASTNode    &rng  = *rhs.children[1];  // i1:i2
                endStack_.push_back(rows);                  // `end` in the row index = rows
                const std::string i1 = emitExpr(*rng.children[0]);
                const std::string i2 = emitExpr(*rng.children[1]);
                endStack_.pop_back();
                line("{");
                ++indent_;
                line("const std::size_t _nk_rows = " + rows + ";");
                line("const std::size_t _nk_cols = " + cols + ";");
                line("const std::ptrdiff_t _nk_i1 = static_cast<std::ptrdiff_t>(" + i1 + ");");
                line("const std::ptrdiff_t _nk_i2 = static_cast<std::ptrdiff_t>(" + i2 + ");");
                line("if (_nk_i1 < 1 || _nk_i2 > static_cast<std::ptrdiff_t>(_nk_rows) || _nk_i2 < "
                     "_nk_i1)");
                line("    throw std::out_of_range(\"numkit: row range out of bounds\");");
                line("const std::size_t _nk_nr = static_cast<std::size_t>(_nk_i2 - _nk_i1 + 1);");
                line("const std::size_t _nk_r0 = static_cast<std::size_t>(_nk_i1 - 1);");
                line(B.ndDims[0] + " = _nk_nr;");
                line(B.ndDims[1] + " = _nk_cols;");
                line(name + ".assign(_nk_nr * _nk_cols, 0.0);");
                open("for (std::size_t _nk_j = 0; _nk_j < _nk_cols; ++_nk_j)");
                open("for (std::size_t _nk_r = 0; _nk_r < _nk_nr; ++_nk_r)");
                line(name + "[_nk_r + _nk_j * _nk_nr] = " + A.dataExpr
                     + "[(_nk_r0 + _nk_r) + _nk_j * _nk_rows];");
                close();
                close();
                --indent_;
                line("}");
                types_.set(name, res);
                return;
            }
        }
        // Native 2-D BOTH-RANGE read B = A(i1:i2, j1:j2): extract the sub-block rows i1..i2 x
        // columns j1..j2 of a 2-D A as a runtime-dim 2-D (i2-i1+1) x (j2-j1+1) matrix (phase N24).
        // STRIDED: for each kept column cc = j1-1+c, copy the row run A[(i1-1)+cc*rows ..
        // i2+cc*rows) into B's column c. B a runtime-dim 2-D ndRuntimeLocal; both ranges bounds-
        // checked. v1: 2-D DOUBLE A, two step-1 ranges, B distinct.
        if (isArrayVar(name) && arrays_.at(name).isLocal && arrays_.at(name).ndRuntimeLocal
            && arrays_.at(name).ndDims.size() == 2 && rhs.type == NodeType::CALL
            && rhs.children.size() == 3 && rhs.children[0]->type == NodeType::IDENTIFIER
            && isArrayVar(rhs.children[0]->strValue) && rhs.children[0]->strValue != name
            && (arrays_.at(rhs.children[0]->strValue).is2D
                || (arrays_.at(rhs.children[0]->strValue).isND
                    && arrays_.at(rhs.children[0]->strValue).ndDims.size() == 2))
            && arrays_.at(rhs.children[0]->strValue).dtype == ValueType::DOUBLE
            && rhs.children[1]->type == NodeType::COLON_EXPR && rhs.children[1]->children.size() == 2
            && rhs.children[2]->type == NodeType::COLON_EXPR && rhs.children[2]->children.size() == 2) {
            const ArrayInfo    &A   = arrays_.at(rhs.children[0]->strValue);
            const ArrayInfo    &B   = arrays_.at(name);
            const AbstractValue res = inferExpr(rhs, types_, reg_, classes_);
            if (res.type.isConcrete() && !res.type.shape.isScalar()) {
                const std::string rows = dimExpr(A, 0), cols = dimExpr(A, 1);
                const ASTNode    &rr = *rhs.children[1];  // i1:i2
                const ASTNode    &cr = *rhs.children[2];  // j1:j2
                endStack_.push_back(rows);
                const std::string i1 = emitExpr(*rr.children[0]);
                const std::string i2 = emitExpr(*rr.children[1]);
                endStack_.pop_back();
                endStack_.push_back(cols);
                const std::string j1 = emitExpr(*cr.children[0]);
                const std::string j2 = emitExpr(*cr.children[1]);
                endStack_.pop_back();
                line("{");
                ++indent_;
                line("const std::size_t _nk_rows = " + rows + ";");
                line("const std::size_t _nk_cols = " + cols + ";");
                line("const std::ptrdiff_t _nk_i1 = static_cast<std::ptrdiff_t>(" + i1 + ");");
                line("const std::ptrdiff_t _nk_i2 = static_cast<std::ptrdiff_t>(" + i2 + ");");
                line("const std::ptrdiff_t _nk_j1 = static_cast<std::ptrdiff_t>(" + j1 + ");");
                line("const std::ptrdiff_t _nk_j2 = static_cast<std::ptrdiff_t>(" + j2 + ");");
                line("if (_nk_i1 < 1 || _nk_i2 > static_cast<std::ptrdiff_t>(_nk_rows) || _nk_i2 < "
                     "_nk_i1)");
                line("    throw std::out_of_range(\"numkit: row range out of bounds\");");
                line("if (_nk_j1 < 1 || _nk_j2 > static_cast<std::ptrdiff_t>(_nk_cols) || _nk_j2 < "
                     "_nk_j1)");
                line("    throw std::out_of_range(\"numkit: column range out of bounds\");");
                line("const std::size_t _nk_nr = static_cast<std::size_t>(_nk_i2 - _nk_i1 + 1);");
                line("const std::size_t _nk_nc = static_cast<std::size_t>(_nk_j2 - _nk_j1 + 1);");
                line("const std::size_t _nk_r0 = static_cast<std::size_t>(_nk_i1 - 1);");
                line("const std::size_t _nk_c0 = static_cast<std::size_t>(_nk_j1 - 1);");
                line(B.ndDims[0] + " = _nk_nr;");
                line(B.ndDims[1] + " = _nk_nc;");
                line(name + ".assign(_nk_nr * _nk_nc, 0.0);");
                open("for (std::size_t _nk_c = 0; _nk_c < _nk_nc; ++_nk_c)");
                open("for (std::size_t _nk_r = 0; _nk_r < _nk_nr; ++_nk_r)");
                line(name + "[_nk_r + _nk_c * _nk_nr] = " + A.dataExpr
                     + "[(_nk_r0 + _nk_r) + (_nk_c0 + _nk_c) * _nk_rows];");
                close();
                close();
                --indent_;
                line("}");
                types_.set(name, res);
                return;
            }
        }
        // Native trailing-RANGE read B = A(:,...,:,k1:k2): extract the contiguous trailing-dim
        // block k1..k2 of a rank-r A (r >= 3) as a rank-r sub-array (phase N7 r=3 page-range +
        // N21 r=4 slab-range). Column-major: the leading (r-1) dims form a "slab" of size
        // d0*..*d(r-2), and the trailing-dim values k1..k2 are the CONTIGUOUS block
        // A[(k1-1)*slab .. k2*slab] -> a straight copy. B a runtime-dim rank-r ndRuntimeLocal;
        // bounds-checked range; `end` in the trailing index = size(A,r). v1: rank-3/4 DOUBLE A,
        // (r-1) leading bare colons + a step-1 range k1:k2, B distinct from A.
        if (isArrayVar(name) && arrays_.at(name).isLocal && arrays_.at(name).ndRuntimeLocal
            && rhs.type == NodeType::CALL && rhs.children[0]->type == NodeType::IDENTIFIER
            && isArrayVar(rhs.children[0]->strValue) && rhs.children[0]->strValue != name
            && arrays_.at(rhs.children[0]->strValue).isND
            && arrays_.at(rhs.children[0]->strValue).ndDims.size() >= 3
            && arrays_.at(rhs.children[0]->strValue).ndDims.size() == arrays_.at(name).ndDims.size()
            && arrays_.at(rhs.children[0]->strValue).dtype == ValueType::DOUBLE
            && rhs.children.size() == arrays_.at(rhs.children[0]->strValue).ndDims.size() + 1) {
            const ArrayInfo  &A = arrays_.at(rhs.children[0]->strValue);
            const std::size_t r = A.ndDims.size();
            bool              ok = rhs.children[r]->type == NodeType::COLON_EXPR
                      && rhs.children[r]->children.size() == 2;
            for (std::size_t i = 1; i < r && ok; ++i)
                if (!(rhs.children[i]->type == NodeType::COLON_EXPR
                      && rhs.children[i]->children.empty()))
                    ok = false;
            if (ok) {
                const ArrayInfo    &B   = arrays_.at(name);
                const AbstractValue res = inferExpr(rhs, types_, reg_, classes_);
                if (res.type.isConcrete() && !res.type.shape.isScalar()) {
                    const std::string lastDim = dimExpr(A, r - 1);  // size along the range dim
                    const ASTNode    &rng     = *rhs.children[r];   // k1:k2
                    endStack_.push_back(lastDim);                   // `end` in the range = size(A,r)
                    const std::string k1 = emitExpr(*rng.children[0]);
                    const std::string k2 = emitExpr(*rng.children[1]);
                    endStack_.pop_back();
                    line("{");
                    ++indent_;
                    std::string slabExpr;
                    for (std::size_t i = 0; i + 1 < r; ++i) {  // leading r-1 dims form one slab
                        line("const std::size_t _nk_d" + std::to_string(i) + " = " + dimExpr(A, i)
                             + ";");
                        slabExpr += (i ? " * " : "") + ("_nk_d" + std::to_string(i));
                    }
                    line("const std::size_t _nk_slab = " + slabExpr + ";");
                    line("const std::ptrdiff_t _nk_k1 = static_cast<std::ptrdiff_t>(" + k1 + ");");
                    line("const std::ptrdiff_t _nk_k2 = static_cast<std::ptrdiff_t>(" + k2 + ");");
                    line("if (_nk_k1 < 1 || _nk_k2 > static_cast<std::ptrdiff_t>(" + lastDim
                         + ") || _nk_k2 < _nk_k1)");
                    line("    throw std::out_of_range(\"numkit: range out of bounds\");");
                    line("const std::size_t _nk_nb = static_cast<std::size_t>(_nk_k2 - _nk_k1 + 1);");
                    line("const std::size_t _nk_off = static_cast<std::size_t>(_nk_k1 - 1) * _nk_slab;");
                    for (std::size_t i = 0; i + 1 < r; ++i)
                        line(B.ndDims[i] + " = _nk_d" + std::to_string(i) + ";");
                    line(B.ndDims[r - 1] + " = _nk_nb;");
                    line(name + ".assign(" + A.dataExpr + " + _nk_off, " + A.dataExpr
                         + " + _nk_off + _nk_nb * _nk_slab);");
                    --indent_;
                    line("}");
                    types_.set(name, res);
                    return;
                }
            }
        }
        // Native A(i,:,:) read: a LEADING-scalar strided slice of a rank-3 A -> a rank-3
        // [1, n, p] sub-array (phase N8). The fixed row i is kept as a singleton first dim
        // (only TRAILING scalar dims drop). STRIDED (not contiguous): B(1,j,k) = A(i,j,k);
        // A flat = (i-1) + j*m + k*m*n, B flat (dims [1,n,p]) = j + k*n. B a runtime-dim rank-3
        // ndRuntimeLocal; bounds-checked i (end = size(A,1)). v1: rank-3 DOUBLE A, a scalar i +
        // two trailing bare colons, B distinct from A.
        if (isArrayVar(name) && arrays_.at(name).isLocal && arrays_.at(name).ndRuntimeLocal
            && arrays_.at(name).ndDims.size() == 3 && rhs.type == NodeType::CALL
            && rhs.children.size() == 4 && rhs.children[0]->type == NodeType::IDENTIFIER
            && isArrayVar(rhs.children[0]->strValue) && rhs.children[0]->strValue != name
            && arrays_.at(rhs.children[0]->strValue).isND
            && arrays_.at(rhs.children[0]->strValue).ndDims.size() == 3
            && arrays_.at(rhs.children[0]->strValue).dtype == ValueType::DOUBLE
            && rhs.children[1]->type != NodeType::COLON_EXPR
            && rhs.children[2]->type == NodeType::COLON_EXPR && rhs.children[2]->children.empty()
            && rhs.children[3]->type == NodeType::COLON_EXPR && rhs.children[3]->children.empty()) {
            const ArrayInfo    &A   = arrays_.at(rhs.children[0]->strValue);
            const ArrayInfo    &B   = arrays_.at(name);
            const AbstractValue res = inferExpr(rhs, types_, reg_, classes_);
            if (res.type.isConcrete() && !res.type.shape.isScalar()
                && inferExpr(*rhs.children[1], types_, reg_, classes_).type.shape.isScalar()) {
                const std::string m = dimExpr(A, 0), n = dimExpr(A, 1), p = dimExpr(A, 2);
                endStack_.push_back(m);  // `end` in the row index = size(A,1)
                const std::string i = emitExpr(*rhs.children[1]);
                endStack_.pop_back();
                line("{");
                ++indent_;
                line("const std::size_t _nk_m = " + m + ";");
                line("const std::size_t _nk_n = " + n + ";");
                line("const std::size_t _nk_p = " + p + ";");
                line("const std::ptrdiff_t _nk_i0 = static_cast<std::ptrdiff_t>(" + i + ") - 1;");
                line("if (_nk_i0 < 0 || _nk_i0 >= static_cast<std::ptrdiff_t>(_nk_m))");
                line("    throw std::out_of_range(\"numkit: row index out of bounds\");");
                line(B.ndDims[0] + " = 1;");
                line(B.ndDims[1] + " = _nk_n;");
                line(B.ndDims[2] + " = _nk_p;");
                line(name + ".assign(_nk_n * _nk_p, 0.0);");
                open("for (std::size_t _nk_k = 0; _nk_k < _nk_p; ++_nk_k)");
                open("for (std::size_t _nk_j = 0; _nk_j < _nk_n; ++_nk_j)");
                line(name + "[_nk_j + _nk_k * _nk_n] = " + A.dataExpr
                     + "[static_cast<std::size_t>(_nk_i0) + _nk_j * _nk_m + _nk_k * _nk_m * _nk_n];");
                close();
                close();
                --indent_;
                line("}");
                types_.set(name, res);
                return;
            }
        }
        // Native A(:,j,:) read: a MIDDLE-scalar strided slice of a rank-3 A -> a rank-3
        // [m, 1, p] sub-array (phase N9, the sibling of N8). The fixed col j is kept as a
        // singleton middle dim. STRIDED: B(i,1,k) = A(i,j,k); A flat = i + (j-1)*m + k*m*n,
        // B flat (dims [m,1,p]) = i + k*m. v1: rank-3 DOUBLE A, colon + scalar j + colon, B
        // distinct from A; bounds-checked j (end = size(A,2)).
        if (isArrayVar(name) && arrays_.at(name).isLocal && arrays_.at(name).ndRuntimeLocal
            && arrays_.at(name).ndDims.size() == 3 && rhs.type == NodeType::CALL
            && rhs.children.size() == 4 && rhs.children[0]->type == NodeType::IDENTIFIER
            && isArrayVar(rhs.children[0]->strValue) && rhs.children[0]->strValue != name
            && arrays_.at(rhs.children[0]->strValue).isND
            && arrays_.at(rhs.children[0]->strValue).ndDims.size() == 3
            && arrays_.at(rhs.children[0]->strValue).dtype == ValueType::DOUBLE
            && rhs.children[1]->type == NodeType::COLON_EXPR && rhs.children[1]->children.empty()
            && rhs.children[2]->type != NodeType::COLON_EXPR
            && rhs.children[3]->type == NodeType::COLON_EXPR && rhs.children[3]->children.empty()) {
            const ArrayInfo    &A   = arrays_.at(rhs.children[0]->strValue);
            const ArrayInfo    &B   = arrays_.at(name);
            const AbstractValue res = inferExpr(rhs, types_, reg_, classes_);
            if (res.type.isConcrete() && !res.type.shape.isScalar()
                && inferExpr(*rhs.children[2], types_, reg_, classes_).type.shape.isScalar()) {
                const std::string m = dimExpr(A, 0), n = dimExpr(A, 1), p = dimExpr(A, 2);
                endStack_.push_back(n);  // `end` in the col index = size(A,2)
                const std::string j = emitExpr(*rhs.children[2]);
                endStack_.pop_back();
                line("{");
                ++indent_;
                line("const std::size_t _nk_m = " + m + ";");
                line("const std::size_t _nk_n = " + n + ";");
                line("const std::size_t _nk_p = " + p + ";");
                line("const std::ptrdiff_t _nk_j0 = static_cast<std::ptrdiff_t>(" + j + ") - 1;");
                line("if (_nk_j0 < 0 || _nk_j0 >= static_cast<std::ptrdiff_t>(_nk_n))");
                line("    throw std::out_of_range(\"numkit: column index out of bounds\");");
                line(B.ndDims[0] + " = _nk_m;");
                line(B.ndDims[1] + " = 1;");
                line(B.ndDims[2] + " = _nk_p;");
                line(name + ".assign(_nk_m * _nk_p, 0.0);");
                open("for (std::size_t _nk_k = 0; _nk_k < _nk_p; ++_nk_k)");
                open("for (std::size_t _nk_i = 0; _nk_i < _nk_m; ++_nk_i)");
                line(name + "[_nk_i + _nk_k * _nk_m] = " + A.dataExpr
                     + "[_nk_i + static_cast<std::size_t>(_nk_j0) * _nk_m + _nk_k * _nk_m * _nk_n];");
                close();
                close();
                --indent_;
                line("}");
                types_.set(name, res);
                return;
            }
        }
        // Native A(i,j,:) read: a FIBER of a rank-3 A -> a rank-3 [1, 1, p] sub-array (phase
        // N10). Two fixed dims kept as singletons (the trailing colon is not droppable).
        // STRIDED: B(1,1,k) = A(i,j,k); A flat = (i-1) + (j-1)*m + k*m*n, B flat (dims [1,1,p])
        // = k. B a runtime-dim rank-3 ndRuntimeLocal; bounds-checked i,j. v1: rank-3 DOUBLE A,
        // scalar i + scalar j + trailing bare colon, B distinct from A.
        if (isArrayVar(name) && arrays_.at(name).isLocal && arrays_.at(name).ndRuntimeLocal
            && arrays_.at(name).ndDims.size() == 3 && rhs.type == NodeType::CALL
            && rhs.children.size() == 4 && rhs.children[0]->type == NodeType::IDENTIFIER
            && isArrayVar(rhs.children[0]->strValue) && rhs.children[0]->strValue != name
            && arrays_.at(rhs.children[0]->strValue).isND
            && arrays_.at(rhs.children[0]->strValue).ndDims.size() == 3
            && arrays_.at(rhs.children[0]->strValue).dtype == ValueType::DOUBLE
            && rhs.children[1]->type != NodeType::COLON_EXPR
            && rhs.children[2]->type != NodeType::COLON_EXPR
            && rhs.children[3]->type == NodeType::COLON_EXPR && rhs.children[3]->children.empty()) {
            const ArrayInfo    &A   = arrays_.at(rhs.children[0]->strValue);
            const ArrayInfo    &B   = arrays_.at(name);
            const AbstractValue res = inferExpr(rhs, types_, reg_, classes_);
            if (res.type.isConcrete() && !res.type.shape.isScalar()
                && inferExpr(*rhs.children[1], types_, reg_, classes_).type.shape.isScalar()
                && inferExpr(*rhs.children[2], types_, reg_, classes_).type.shape.isScalar()) {
                const std::string m = dimExpr(A, 0), n = dimExpr(A, 1), p = dimExpr(A, 2);
                endStack_.push_back(m);  // `end` in the row index = size(A,1)
                const std::string i = emitExpr(*rhs.children[1]);
                endStack_.pop_back();
                endStack_.push_back(n);  // `end` in the col index = size(A,2)
                const std::string j = emitExpr(*rhs.children[2]);
                endStack_.pop_back();
                line("{");
                ++indent_;
                line("const std::size_t _nk_m = " + m + ";");
                line("const std::size_t _nk_n = " + n + ";");
                line("const std::size_t _nk_p = " + p + ";");
                line("const std::ptrdiff_t _nk_i0 = static_cast<std::ptrdiff_t>(" + i + ") - 1;");
                line("const std::ptrdiff_t _nk_j0 = static_cast<std::ptrdiff_t>(" + j + ") - 1;");
                line("if (_nk_i0 < 0 || _nk_i0 >= static_cast<std::ptrdiff_t>(_nk_m)"
                     " || _nk_j0 < 0 || _nk_j0 >= static_cast<std::ptrdiff_t>(_nk_n))");
                line("    throw std::out_of_range(\"numkit: fiber index out of bounds\");");
                line(B.ndDims[0] + " = 1;");
                line(B.ndDims[1] + " = 1;");
                line(B.ndDims[2] + " = _nk_p;");
                line("const std::size_t _nk_b0 = static_cast<std::size_t>(_nk_i0)"
                     " + static_cast<std::size_t>(_nk_j0) * _nk_m;");  // (i-1)+(j-1)*m
                line(name + ".assign(_nk_p, 0.0);");
                open("for (std::size_t _nk_k = 0; _nk_k < _nk_p; ++_nk_k)");
                line(name + "[_nk_k] = " + A.dataExpr + "[_nk_b0 + _nk_k * _nk_m * _nk_n];");
                close();
                --indent_;
                line("}");
                types_.set(name, res);
                return;
            }
        }
        // Native A(i,:,k) read: a (scalar, colon, scalar) slice of a rank-3 A -> a 2-D [1, n]
        // ROW (phase N11). Row i of page k; the trailing scalar k drops the page dim, the
        // leading scalar i keeps a singleton first dim. STRIDED: B(1,j) = A(i,j,k); A flat =
        // (i-1) + j*m + (k-1)*m*n, B flat (dims [1,n]) = j. B a runtime-dim 2-D ndRuntimeLocal;
        // bounds-checked i,k. v1: rank-3 DOUBLE A, scalar + colon + scalar, B distinct from A.
        if (isArrayVar(name) && arrays_.at(name).isLocal && arrays_.at(name).ndRuntimeLocal
            && arrays_.at(name).ndDims.size() == 2 && rhs.type == NodeType::CALL
            && rhs.children.size() == 4 && rhs.children[0]->type == NodeType::IDENTIFIER
            && isArrayVar(rhs.children[0]->strValue) && rhs.children[0]->strValue != name
            && arrays_.at(rhs.children[0]->strValue).isND
            && arrays_.at(rhs.children[0]->strValue).ndDims.size() == 3
            && arrays_.at(rhs.children[0]->strValue).dtype == ValueType::DOUBLE
            && rhs.children[1]->type != NodeType::COLON_EXPR
            && rhs.children[2]->type == NodeType::COLON_EXPR && rhs.children[2]->children.empty()
            && rhs.children[3]->type != NodeType::COLON_EXPR) {
            const ArrayInfo    &A   = arrays_.at(rhs.children[0]->strValue);
            const ArrayInfo    &B   = arrays_.at(name);
            const AbstractValue res = inferExpr(rhs, types_, reg_, classes_);
            if (res.type.isConcrete() && !res.type.shape.isScalar()
                && inferExpr(*rhs.children[1], types_, reg_, classes_).type.shape.isScalar()
                && inferExpr(*rhs.children[3], types_, reg_, classes_).type.shape.isScalar()) {
                const std::string m = dimExpr(A, 0), n = dimExpr(A, 1), p = dimExpr(A, 2);
                endStack_.push_back(m);  // `end` in the row index = size(A,1)
                const std::string i = emitExpr(*rhs.children[1]);
                endStack_.pop_back();
                endStack_.push_back(p);  // `end` in the page index = size(A,3)
                const std::string k = emitExpr(*rhs.children[3]);
                endStack_.pop_back();
                line("{");
                ++indent_;
                line("const std::size_t _nk_m = " + m + ";");
                line("const std::size_t _nk_n = " + n + ";");
                line("const std::ptrdiff_t _nk_i0 = static_cast<std::ptrdiff_t>(" + i + ") - 1;");
                line("const std::ptrdiff_t _nk_k0 = static_cast<std::ptrdiff_t>(" + k + ") - 1;");
                line("if (_nk_i0 < 0 || _nk_i0 >= static_cast<std::ptrdiff_t>(_nk_m)"
                     " || _nk_k0 < 0 || _nk_k0 >= static_cast<std::ptrdiff_t>(" + p + "))");
                line("    throw std::out_of_range(\"numkit: slice index out of bounds\");");
                line(B.ndDims[0] + " = 1;");
                line(B.ndDims[1] + " = _nk_n;");
                line("const std::size_t _nk_b0 = static_cast<std::size_t>(_nk_i0)"
                     " + static_cast<std::size_t>(_nk_k0) * _nk_m * _nk_n;");  // (i-1)+(k-1)*m*n
                line(name + ".assign(_nk_n, 0.0);");
                open("for (std::size_t _nk_j = 0; _nk_j < _nk_n; ++_nk_j)");
                line(name + "[_nk_j] = " + A.dataExpr + "[_nk_b0 + _nk_j * _nk_m];");
                close();
                --indent_;
                line("}");
                types_.set(name, res);
                return;
            }
        }
        // Native A(:,j,k) read: a (colon, scalar, scalar) slice of a rank-3 A -> a 2-D [m, 1]
        // COLUMN (phase N12). Col j of page k; the trailing scalar k drops the page dim. Column-
        // major: this is the CONTIGUOUS block A[(j-1)*m + (k-1)*m*n .. +m] -> a straight copy
        // (unlike N11 which is strided). B a runtime-dim 2-D ndRuntimeLocal dims [m,1]; bounds-
        // checked j,k. v1: rank-3 DOUBLE A, colon + scalar + scalar, B distinct from A.
        if (isArrayVar(name) && arrays_.at(name).isLocal && arrays_.at(name).ndRuntimeLocal
            && arrays_.at(name).ndDims.size() == 2 && rhs.type == NodeType::CALL
            && rhs.children.size() == 4 && rhs.children[0]->type == NodeType::IDENTIFIER
            && isArrayVar(rhs.children[0]->strValue) && rhs.children[0]->strValue != name
            && arrays_.at(rhs.children[0]->strValue).isND
            && arrays_.at(rhs.children[0]->strValue).ndDims.size() == 3
            && arrays_.at(rhs.children[0]->strValue).dtype == ValueType::DOUBLE
            && rhs.children[1]->type == NodeType::COLON_EXPR && rhs.children[1]->children.empty()
            && rhs.children[2]->type != NodeType::COLON_EXPR
            && rhs.children[3]->type != NodeType::COLON_EXPR) {
            const ArrayInfo    &A   = arrays_.at(rhs.children[0]->strValue);
            const ArrayInfo    &B   = arrays_.at(name);
            const AbstractValue res = inferExpr(rhs, types_, reg_, classes_);
            if (res.type.isConcrete() && !res.type.shape.isScalar()
                && inferExpr(*rhs.children[2], types_, reg_, classes_).type.shape.isScalar()
                && inferExpr(*rhs.children[3], types_, reg_, classes_).type.shape.isScalar()) {
                const std::string m = dimExpr(A, 0), n = dimExpr(A, 1), p = dimExpr(A, 2);
                endStack_.push_back(n);  // `end` in the col index = size(A,2)
                const std::string j = emitExpr(*rhs.children[2]);
                endStack_.pop_back();
                endStack_.push_back(p);  // `end` in the page index = size(A,3)
                const std::string k = emitExpr(*rhs.children[3]);
                endStack_.pop_back();
                line("{");
                ++indent_;
                line("const std::size_t _nk_m = " + m + ";");
                line("const std::ptrdiff_t _nk_j0 = static_cast<std::ptrdiff_t>(" + j + ") - 1;");
                line("const std::ptrdiff_t _nk_k0 = static_cast<std::ptrdiff_t>(" + k + ") - 1;");
                line("if (_nk_j0 < 0 || _nk_j0 >= static_cast<std::ptrdiff_t>(" + n + ")"
                     " || _nk_k0 < 0 || _nk_k0 >= static_cast<std::ptrdiff_t>(" + p + "))");
                line("    throw std::out_of_range(\"numkit: slice index out of bounds\");");
                line(B.ndDims[0] + " = _nk_m;");
                line(B.ndDims[1] + " = 1;");
                line("const std::size_t _nk_off = static_cast<std::size_t>(_nk_j0) * _nk_m"
                     " + static_cast<std::size_t>(_nk_k0) * _nk_m * (" + n + ");");
                line(name + ".assign(" + A.dataExpr + " + _nk_off, " + A.dataExpr
                     + " + _nk_off + _nk_m);");
                --indent_;
                line("}");
                types_.set(name, res);
                return;
            }
        }
        // Native GENERAL scalar/colon slice B = A(s_0..s_{r-1}) for a rank r >= 4 A (phases
        // N8-N12 cover rank 3 per-pattern; this is the rank-4+ generalization). Each subscript
        // is a bare colon or a scalar (>=1 of each). Output dim per kept subscript: colon ->
        // size(A,k), scalar -> 1; trailing scalar dims drop (kept rank = lastColon+1, >=2).
        // STRIDED gather: A flat = base(from scalar dims) + sum over kept COLON dims of j_k *
        // As_k. v1: rank-4+ DOUBLE A, all bare-colon/scalar subscripts, B distinct from A.
        if (isArrayVar(name) && arrays_.at(name).isLocal && arrays_.at(name).ndRuntimeLocal
            && rhs.type == NodeType::CALL && rhs.children.size() >= 5
            && rhs.children[0]->type == NodeType::IDENTIFIER
            && isArrayVar(rhs.children[0]->strValue) && rhs.children[0]->strValue != name
            && arrays_.at(rhs.children[0]->strValue).isND
            && arrays_.at(rhs.children[0]->strValue).ndDims.size() >= 4
            && arrays_.at(rhs.children[0]->strValue).ndDims.size() == rhs.children.size() - 1
            && arrays_.at(rhs.children[0]->strValue).dtype == ValueType::DOUBLE) {
            const ArrayInfo  &A = arrays_.at(rhs.children[0]->strValue);
            const std::size_t r = A.ndDims.size();
            std::vector<bool> colon(r, false);
            int               lastCol = -1, scalarN = 0;
            bool              allCS = true;
            for (std::size_t k = 0; k < r; ++k) {
                const ASTNode &s     = *rhs.children[k + 1];
                const bool     isCol = s.type == NodeType::COLON_EXPR && s.children.empty();
                colon[k]             = isCol;
                if (isCol)
                    lastCol = static_cast<int>(k);
                else if (s.type != NodeType::COLON_EXPR)
                    ++scalarN;
                else {
                    allCS = false;  // a non-empty COLON (range) -> not handled here
                    break;
                }
            }
            const ArrayInfo    &B   = arrays_.at(name);
            const AbstractValue res = inferExpr(rhs, types_, reg_, classes_);
            bool scalarsOk = allCS && lastCol >= 0 && scalarN > 0 && res.type.isConcrete()
                             && !res.type.shape.isScalar();
            for (std::size_t k = 0; k < r && scalarsOk; ++k)
                if (!colon[k]
                    && !inferExpr(*rhs.children[k + 1], types_, reg_, classes_).type.shape.isScalar())
                    scalarsOk = false;
            const std::size_t keptRank =
                lastCol >= 0 ? (static_cast<std::size_t>(lastCol) + 1 < 2
                                    ? 2u
                                    : static_cast<std::size_t>(lastCol) + 1)
                             : 0;
            if (scalarsOk && B.ndDims.size() == keptRank) {
                line("{");
                ++indent_;
                line("const std::size_t _nk_As0 = 1;");  // A column-major strides
                for (std::size_t k = 1; k < r; ++k)
                    line("const std::size_t _nk_As" + std::to_string(k) + " = _nk_As"
                         + std::to_string(k - 1) + " * (" + dimExpr(A, k - 1) + ");");
                line("std::size_t _nk_base = 0;");  // fixed offset from the scalar subscripts
                for (std::size_t k = 0; k < r; ++k) {
                    if (colon[k]) continue;
                    endStack_.push_back(dimExpr(A, k));
                    const std::string sk = emitExpr(*rhs.children[k + 1]);
                    endStack_.pop_back();
                    line("{ const std::ptrdiff_t _nk_s = static_cast<std::ptrdiff_t>(" + sk + ") - 1;");
                    line("  if (_nk_s < 0 || _nk_s >= static_cast<std::ptrdiff_t>(" + dimExpr(A, k)
                         + ")) throw std::out_of_range(\"numkit: slice index out of bounds\");");
                    line("  _nk_base += static_cast<std::size_t>(_nk_s) * _nk_As" + std::to_string(k)
                         + "; }");
                }
                for (std::size_t kk = 0; kk < keptRank; ++kk)  // output (kept) dims
                    line("const std::size_t _nk_Bd" + std::to_string(kk) + " = "
                         + (colon[kk] ? dimExpr(A, kk) : std::string("1")) + ";");
                line("const std::size_t _nk_Bs0 = 1;");  // output strides
                for (std::size_t kk = 1; kk < keptRank; ++kk)
                    line("const std::size_t _nk_Bs" + std::to_string(kk) + " = _nk_Bs"
                         + std::to_string(kk - 1) + " * _nk_Bd" + std::to_string(kk - 1) + ";");
                std::string numel = "_nk_Bd0";
                for (std::size_t kk = 1; kk < keptRank; ++kk) numel += " * _nk_Bd" + std::to_string(kk);
                for (std::size_t kk = 0; kk < keptRank; ++kk)
                    line(B.ndDims[kk] + " = _nk_Bd" + std::to_string(kk) + ";");
                line(name + ".assign((" + numel + "), 0.0);");
                open("for (std::size_t _nk_o = 0; _nk_o < (" + numel + "); ++_nk_o)");
                line("std::size_t _nk_af = _nk_base;");
                for (std::size_t kk = 0; kk < keptRank; ++kk)
                    if (colon[kk])
                        line("_nk_af += ((_nk_o / _nk_Bs" + std::to_string(kk) + ") % _nk_Bd"
                             + std::to_string(kk) + ") * _nk_As" + std::to_string(kk) + ";");
                line(name + "[_nk_o] = " + A.dataExpr + "[_nk_af];");
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
        // Native unique(x) -> the sorted distinct values in a fresh 1-D LOCAL. Sort a
        // temp copy (NaN-last comparator) then push each element that differs from the
        // previous (consecutive-equal skip). NaN are KEPT (MATLAB: each NaN distinct)
        // because NaN != NaN is true. !bridge_ keeps the bridged array-result path as
        // the tier when the bridge is on. v1: a single 1-D DOUBLE array var.
        if (isArrayVar(name) && arrays_.at(name).isLocal && !bridge_ && !arrays_.at(name).is2D
            && !arrays_.at(name).isND && rhs.type == NodeType::CALL && rhs.children.size() == 2
            && rhs.children[0]->type == NodeType::IDENTIFIER && rhs.children[0]->strValue == "unique"
            && rhs.children[1]->type == NodeType::IDENTIFIER
            && isArrayVar(rhs.children[1]->strValue)
            && !arrays_.at(rhs.children[1]->strValue).is2D
            && !arrays_.at(rhs.children[1]->strValue).isND
            && arrays_.at(rhs.children[1]->strValue).dtype == ValueType::DOUBLE) {
            const ArrayInfo    &xa  = arrays_.at(rhs.children[1]->strValue);
            const AbstractValue res = inferExpr(rhs, types_, reg_, classes_);
            if (res.type.isConcrete() && !res.type.shape.isScalar()) {
                line("{");
                ++indent_;
                line("std::vector<double> _nk_t(" + xa.dataExpr + ", " + xa.dataExpr + " + "
                     + xa.lenVar + ");");
                line("std::sort(_nk_t.begin(), _nk_t.end(),");
                line("    [](double _a, double _b){ return _a < _b || (_b != _b && _a == _a); });");
                line(name + ".clear();");
                open("for (std::size_t _nk_i = 0; _nk_i < _nk_t.size(); ++_nk_i)");
                line("if (_nk_i == 0 || _nk_t[_nk_i] != _nk_t[_nk_i - 1]) " + name
                     + ".push_back(_nk_t[_nk_i]);");
                close();
                --indent_;
                line("}");
                types_.set(name, res);
                return;
            }
        }
        // Native polyval(p, x) with x a vector -> the polynomial p (coeffs highest
        // degree first) evaluated at each x[i] by Horner, a same-length-as-x 1-D
        // LOCAL: out[i] = ((p[0]*x[i] + p[1])*x[i] + ...) + p[np-1]. Exact (a fused
        // multiply-add chain) -> every tier. v1: p and x both 1-D DOUBLE array vars.
        if (isArrayVar(name) && arrays_.at(name).isLocal && !arrays_.at(name).is2D
            && !arrays_.at(name).isND && rhs.type == NodeType::CALL && rhs.children.size() == 3
            && rhs.children[0]->type == NodeType::IDENTIFIER
            && rhs.children[0]->strValue == "polyval"
            && rhs.children[1]->type == NodeType::IDENTIFIER && isArrayVar(rhs.children[1]->strValue)
            && rhs.children[2]->type == NodeType::IDENTIFIER && isArrayVar(rhs.children[2]->strValue)
            && !arrays_.at(rhs.children[1]->strValue).is2D
            && !arrays_.at(rhs.children[1]->strValue).isND
            && !arrays_.at(rhs.children[2]->strValue).is2D
            && !arrays_.at(rhs.children[2]->strValue).isND
            && arrays_.at(rhs.children[1]->strValue).dtype == ValueType::DOUBLE
            && arrays_.at(rhs.children[2]->strValue).dtype == ValueType::DOUBLE) {
            const ArrayInfo    &p   = arrays_.at(rhs.children[1]->strValue);
            const ArrayInfo    &x   = arrays_.at(rhs.children[2]->strValue);
            const AbstractValue res = inferExpr(rhs, types_, reg_, classes_);
            if (res.type.isConcrete() && !res.type.shape.isScalar()) {
                line("{");
                ++indent_;
                line(name + ".assign(" + x.lenVar + ", 0.0);");
                open("for (std::size_t _nk_i = 0; _nk_i < " + x.lenVar + "; ++_nk_i)");
                line("double _nk_acc = 0.0;");
                open("for (std::size_t _nk_k = 0; _nk_k < " + p.lenVar + "; ++_nk_k)");
                line("_nk_acc = _nk_acc * " + x.dataExpr + "[_nk_i] + " + p.dataExpr + "[_nk_k];");
                close();
                line(name + "[_nk_i] = _nk_acc;");
                close();
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
                    // A runtime-dim N-D LOCAL result (an ndRuntimeLocal, e.g. a
                    // runtime-dim 2-D matrix produced by C = A + B): copy the reference
                    // shape into the dst's OWN dim companions. The resize above only
                    // sizes the flat buffer; without this the companions stay 0 and the
                    // dst's later numel / N-D indexing read stale dims. (A KnownDims 2-D
                    // or 1-D dst has no companions -> this is a no-op for them.)
                    if (ai.isLocal && ai.ndRuntimeLocal) {
                        const ArrayInfo &ref = arrays_.at(*srcArrays.begin());
                        for (std::size_t k = 0; k < ai.ndDims.size(); ++k)
                            line(ai.ndDims[k] + " = " + ref.ndDims[k] + ";");
                    }
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

        // LOGICAL-INDEXING READ with an INLINE elementwise mask: `y = x(<expr>)` where <expr>
        // is a pure-elementwise LOGICAL self-mask over x (y = x(x>0), y = A(A>lo & A<hi), ...).
        // The sibling of the inline masked WRITE: the mask is FUSED into the filter loop (no
        // temp vector) -- elementCtx_ makes whole-x emit x[_nk_i] (flat), then for each flat
        // element i, if the mask holds, push x[i]. Works for a 1-D vector OR a 2-D / N-D matrix
        // source (filter is flat over the column-major buffer; the result is the selected
        // elements in MATLAB linear order -> bound on NUMEL). A self-mask (collectElementwise
        // srcArrays=={x}) keeps it in bounds and per-element-emittable; a READ never mutates x,
        // so no aliasing subtlety. y a 1-D array LOCAL (push_back). Placed after the VAR-mask
        // read so a pre-bound mask var takes that branch.
        if (isArrayVar(name) && arrays_.at(name).isLocal && !arrays_.at(name).is2D
            && !arrays_.at(name).isND && rhs.type == NodeType::CALL
            && rhs.children.size() == 2 && rhs.children[0]->type == NodeType::IDENTIFIER
            && isArrayVar(rhs.children[0]->strValue)
            && rhs.children[1]->type != NodeType::IDENTIFIER) {
            const AbstractValue   maskAV = inferExpr(*rhs.children[1], types_, reg_, classes_);
            std::set<std::string> maskArrays;
            const bool            pureEw = collectElementwise(*rhs.children[1], maskArrays);
            if (maskAV.type.isConcrete() && maskAV.type.dtype == ValueType::LOGICAL
                && !maskAV.type.shape.isScalar() && pureEw && maskArrays.size() == 1
                && *maskArrays.begin() == rhs.children[0]->strValue) {
                const ArrayInfo &bx = arrays_.at(rhs.children[0]->strValue);  // x (source)
                std::string      numel;  // flat element count (1-D len / 2-D rows*cols / N-D prod)
                if (bx.isND) {
                    numel = bx.ndDims[0];
                    for (std::size_t i = 1; i < bx.ndDims.size(); ++i) numel += " * " + bx.ndDims[i];
                } else if (bx.is2D) {
                    numel = bx.rowsVar + " * " + bx.colsVar;
                } else {
                    numel = bx.lenVar;
                }
                line("{");
                ++indent_;
                line(name + ".clear();");
                elementCtx_ = "_nk_i";  // whole x in the mask -> x[_nk_i] (flat)
                const std::string maskExpr = emitExpr(*rhs.children[1]);
                elementCtx_.clear();
                open("for (std::size_t _nk_i = 0; _nk_i < (" + numel + "); ++_nk_i)");
                line("if (" + maskExpr + ") " + name + ".push_back(" + bx.dataExpr + "[_nk_i]);");
                close();
                --indent_;
                line("}");
                types_.set(name, {InferredType::concrete(bx.dtype, Shape::rowVector()),
                                  ConstVal::unknown()});
                return;
            }
        }

        // NUMERIC GATHER READ: `y = x(idx)` where idx is a 1-D NUMERIC (DOUBLE) index vector
        // -> y[i] = x(idx[i]) 1-based, i.e. reindexing / selection. Result length = numel(idx).
        // Each index is range+integer checked (MATLAB errors on a non-positive-integer or out-
        // of-range subscript) BEFORE the 1-based->0-based read, so there is no UB (a NaN fails
        // `>= 1.0`) and the throw is faithful. y a fresh 1-D LOCAL distinct from x and idx (an
        // in-place x = x(idx) would need a temp -> refused, sound). Placed after the logical
        // reads (a LOGICAL idx takes those; this is the numeric sibling). v1: x + idx 1-D, idx
        // DOUBLE; a 2-D x / inline-literal index / an int-typed idx -> refused.
        if (isArrayVar(name) && arrays_.at(name).isLocal && !arrays_.at(name).is2D
            && !arrays_.at(name).isND && rhs.type == NodeType::CALL && rhs.children.size() == 2
            && rhs.children[0]->type == NodeType::IDENTIFIER && isArrayVar(rhs.children[0]->strValue)
            && !arrays_.at(rhs.children[0]->strValue).is2D && !arrays_.at(rhs.children[0]->strValue).isND
            && rhs.children[1]->type == NodeType::IDENTIFIER && isArrayVar(rhs.children[1]->strValue)
            && !arrays_.at(rhs.children[1]->strValue).is2D && !arrays_.at(rhs.children[1]->strValue).isND
            && arrays_.at(rhs.children[1]->strValue).dtype == ValueType::DOUBLE
            && rhs.children[0]->strValue != name && rhs.children[1]->strValue != name) {
            const ArrayInfo &bx = arrays_.at(rhs.children[0]->strValue);  // source
            const ArrayInfo &bi = arrays_.at(rhs.children[1]->strValue);  // numeric index vector
            line(name + ".resize(static_cast<std::size_t>(" + bi.lenVar + "));");
            open("for (std::size_t _nk_i = 0; _nk_i < " + bi.lenVar + "; ++_nk_i)");
            line("const double _nk_d = " + bi.dataExpr + "[_nk_i];");
            line("if (!(_nk_d >= 1.0 && _nk_d <= static_cast<double>(" + bx.lenVar
                 + ") && _nk_d == std::floor(_nk_d)))");
            line("    throw std::out_of_range(\"numkit: array index out of bounds\");");
            line(name + "[_nk_i] = " + bx.dataExpr + "[static_cast<std::size_t>(_nk_d) - 1];");
            close();
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
        // 2-D horzcat of columns: `M = [c1 c2 ... ck]` (single-row MATRIX_LITERAL, each
        // element a COLUMN-VECTOR var of a common length n) -> an n x k 2-D matrix, a
        // rank-2 ndRuntimeLocal. Column-major: column j is contiguous, M[i + j*n] =
        // c_j[i]. The k columns are distinct locals so the j-loop is unrolled; every
        // inner loop is bounded by the first column's length (the matrix's n). All
        // columns must share length n (precondition). v1: DOUBLE 1-D col vars.
        if (isArrayVar(name) && arrays_.at(name).isLocal && arrays_.at(name).ndRuntimeLocal
            && arrays_.at(name).ndDims.size() == 2 && rhs.type == NodeType::MATRIX_LITERAL
            && rhs.children.size() == 1 && rhs.children[0]
            && rhs.children[0]->children.size() >= 2) {
            bool allColVars = true;
            for (const auto &el : rhs.children[0]->children)
                if (!el || el->type != NodeType::IDENTIFIER || !isArrayVar(el->strValue)
                    || arrays_.at(el->strValue).is2D || arrays_.at(el->strValue).isND
                    || arrays_.at(el->strValue).dtype != ValueType::DOUBLE) {
                    allColVars = false;
                    break;
                }
            const AbstractValue rv = inferExpr(rhs, types_, reg_, classes_);
            if (allColVars && rv.type.isConcrete()) {
                const ArrayInfo  &M  = arrays_.at(name);
                const ArrayInfo  &c0 = arrays_.at(rhs.children[0]->children[0]->strValue);
                const std::string k  = std::to_string(rhs.children[0]->children.size());
                line("{");
                ++indent_;
                line("const std::size_t _nk_n = " + c0.lenVar + ";");  // matrix rows
                line(M.ndDims[0] + " = _nk_n;");
                line(M.ndDims[1] + " = " + k + ";");
                line(name + ".assign(_nk_n * " + k + ", 0.0);");
                for (std::size_t j = 0; j < rhs.children[0]->children.size(); ++j) {
                    const ArrayInfo &cj = arrays_.at(rhs.children[0]->children[j]->strValue);
                    open("for (std::size_t _nk_i = 0; _nk_i < _nk_n; ++_nk_i)");
                    line(name + "[_nk_i + " + std::to_string(j) + " * _nk_n] = "
                         + cj.dataExpr + "[_nk_i];");
                    close();
                }
                --indent_;
                line("}");
                types_.set(name, rv);
                return;
            }
        }
        // 2-D horzcat of MATRIX / COLUMN-VECTOR blocks: `M = [A B ...]` / `M = [A b]`
        // (augmented matrix) -- a single-row MATRIX_LITERAL whose every element is a 2-D
        // matrix var OR an n x 1 column vector, all of the same row count -> horizontal
        // concatenation, a rank-2 ndRuntimeLocal. In column-major each block's buffer IS
        // its columns in order, so the result is the buffers concatenated end to end: M
        // rows = block rows, M cols = sum of the blocks' cols (a column vector = 1 col).
        // dimExpr gives a matrix its rows/cols and a Col vector its (len, 1). A runtime
        // guard checks all blocks share the row count. v1: DOUBLE vars, >=2, none aliasing
        // the dest. (All-column-vector is the horzcat-of-columns case above.)
        if (isArrayVar(name) && arrays_.at(name).isLocal && arrays_.at(name).ndRuntimeLocal
            && arrays_.at(name).ndDims.size() == 2 && rhs.type == NodeType::MATRIX_LITERAL
            && rhs.children.size() == 1 && rhs.children[0]
            && rhs.children[0]->children.size() >= 2) {
            bool allBlockVars = true;
            for (const auto &el : rhs.children[0]->children)
                if (!el || el->type != NodeType::IDENTIFIER || !isArrayVar(el->strValue)
                    || el->strValue == name  // in-place horzcat -> fall through to refusal
                    || !(arrays_.at(el->strValue).is2D
                         || (arrays_.at(el->strValue).isND
                             && arrays_.at(el->strValue).ndDims.size() == 2)
                         || (!arrays_.at(el->strValue).is2D && !arrays_.at(el->strValue).isND
                             && arrays_.at(el->strValue).orient == VecOrient::Col))
                    || arrays_.at(el->strValue).dtype != ValueType::DOUBLE) {
                    allBlockVars = false;
                    break;
                }
            const AbstractValue rv = inferExpr(rhs, types_, reg_, classes_);
            if (allBlockVars && rv.type.isConcrete()) {
                const ArrayInfo  &M    = arrays_.at(name);
                const ArrayInfo  &op0  = arrays_.at(rhs.children[0]->children[0]->strValue);
                const std::string rows = dimExpr(op0, 0);
                std::string       totalCols;
                for (const auto &el : rhs.children[0]->children)
                    totalCols += (totalCols.empty() ? "" : " + ")
                                 + dimExpr(arrays_.at(el->strValue), 1);
                line("{");
                ++indent_;
                line(M.ndDims[0] + " = " + rows + ";");
                line(M.ndDims[1] + " = (" + totalCols + ");");
                for (std::size_t e = 1; e < rhs.children[0]->children.size(); ++e) {
                    const ArrayInfo &op = arrays_.at(rhs.children[0]->children[e]->strValue);
                    line("if (" + dimExpr(op, 0) + " != " + rows
                         + ") throw std::out_of_range(\"numkit: horzcat row dimensions must "
                           "agree\");");
                }
                line(name + ".resize(" + rows + " * (" + totalCols + "));");
                line("std::size_t _nk_off = 0;");
                for (const auto &el : rhs.children[0]->children) {
                    const ArrayInfo  &op = arrays_.at(el->strValue);
                    const std::string sz = "(" + dimExpr(op, 0) + " * " + dimExpr(op, 1) + ")";
                    open("for (std::size_t _nk_k = 0; _nk_k < " + sz + "; ++_nk_k)");
                    line(name + "[_nk_off + _nk_k] = " + op.dataExpr + "[_nk_k];");
                    close();
                    line("_nk_off += " + sz + ";");
                }
                --indent_;
                line("}");
                types_.set(name, rv);
                return;
            }
        }
        // 1-D vertcat of scalars: `v = [a; b; c]` (multi-row MATRIX_LITERAL, each row a
        // single scalar) -> a 1-D column LOCAL, pushing each row's scalar in order. The
        // column counterpart of the 1-D horzcat above. v1: scalar elements (a multi-
        // row stack of arrays/rows = a true 2-D vertcat, deferred).
        if (isArrayVar(name) && arrays_.at(name).isLocal && !arrays_.at(name).is2D
            && !arrays_.at(name).isND && rhs.type == NodeType::MATRIX_LITERAL
            && rhs.children.size() > 1) {
            bool allScalarRows = true;
            for (const auto &rowN : rhs.children)
                if (!rowN || rowN->children.size() != 1 || !rowN->children[0]
                    || !inferExpr(*rowN->children[0], types_, reg_, classes_).type.shape.isScalar()) {
                    allScalarRows = false;
                    break;
                }
            const AbstractValue rv = inferExpr(rhs, types_, reg_, classes_);
            if (allScalarRows && rv.type.isConcrete() && !rv.type.shape.isScalar()) {
                line("{");
                ++indent_;
                line(name + ".clear();");
                for (const auto &rowN : rhs.children)
                    line(name + ".push_back(" + emitExpr(*rowN->children[0]) + ");");
                --indent_;
                line("}");
                types_.set(name, rv);
                return;
            }
        }
        // 2-D vertcat of rows: `M = [r1; r2; ...; rk]` (multi-row MATRIX_LITERAL, each
        // row a single ROW-VECTOR var of a common length n) -> a k x n 2-D matrix, a
        // rank-2 ndRuntimeLocal. Column-major: M[i + j*k] = r_i[j]. The k rows are
        // distinct locals so the i-loop is unrolled; every inner loop is bounded by the
        // FIRST row's length (the matrix's n) so a write can never escape the buffer.
        // All rows must share length n (precondition; MATLAB errors otherwise). v1:
        // DOUBLE 1-D row vars.
        if (isArrayVar(name) && arrays_.at(name).isLocal && arrays_.at(name).ndRuntimeLocal
            && arrays_.at(name).ndDims.size() == 2 && rhs.type == NodeType::MATRIX_LITERAL
            && rhs.children.size() > 1) {
            bool allRowVars = true;
            for (const auto &rowN : rhs.children)
                if (!rowN || rowN->children.size() != 1 || !rowN->children[0]
                    || rowN->children[0]->type != NodeType::IDENTIFIER
                    || !isArrayVar(rowN->children[0]->strValue)
                    || arrays_.at(rowN->children[0]->strValue).is2D
                    || arrays_.at(rowN->children[0]->strValue).isND
                    || arrays_.at(rowN->children[0]->strValue).dtype != ValueType::DOUBLE) {
                    allRowVars = false;
                    break;
                }
            const AbstractValue rv = inferExpr(rhs, types_, reg_, classes_);
            if (allRowVars && rv.type.isConcrete()) {
                const ArrayInfo  &M  = arrays_.at(name);
                const ArrayInfo  &r0 = arrays_.at(rhs.children[0]->children[0]->strValue);
                const std::string k  = std::to_string(rhs.children.size());
                line("{");
                ++indent_;
                line("const std::size_t _nk_n = " + r0.lenVar + ";");  // matrix cols
                line(M.ndDims[0] + " = " + k + ";");                   // rows = k
                line(M.ndDims[1] + " = _nk_n;");
                line(name + ".assign(" + k + " * _nk_n, 0.0);");
                for (std::size_t i = 0; i < rhs.children.size(); ++i) {
                    const ArrayInfo &ri = arrays_.at(rhs.children[i]->children[0]->strValue);
                    open("for (std::size_t _nk_j = 0; _nk_j < _nk_n; ++_nk_j)");
                    line(name + "[" + std::to_string(i) + " + _nk_j * " + k + "] = "
                         + ri.dataExpr + "[_nk_j];");
                    close();
                }
                --indent_;
                line("}");
                types_.set(name, rv);
                return;
            }
        }
        // 2-D vertcat of MATRIX / ROW-VECTOR blocks: `M = [A; B; ...]` / `M = [A; r]`
        // (appending a row) -- a multi-row MATRIX_LITERAL whose every row is a single 2-D
        // matrix var OR a 1 x n row vector, all of the same column count -> vertical
        // concatenation, a rank-2 ndRuntimeLocal. M rows = sum of block rows (a row vector
        // = 1), M cols = shared. In column-major the blocks INTERLEAVE per column: for a
        // block at row-offset ro with pr rows, M[(ro+ii) + j*totalRows] = op[ii + j*pr]
        // (pr=1 for a row vector). dimExpr gives a Row vector its ("1", len). A runtime
        // guard checks the column counts agree. v1: DOUBLE vars, >=2 rows, none aliasing the
        // dest. (All-row-vector is the vertcat-of-rows case above.)
        if (isArrayVar(name) && arrays_.at(name).isLocal && arrays_.at(name).ndRuntimeLocal
            && arrays_.at(name).ndDims.size() == 2 && rhs.type == NodeType::MATRIX_LITERAL
            && rhs.children.size() > 1) {
            bool allMatRows = true;
            for (const auto &rowN : rhs.children)
                if (!rowN || rowN->children.size() != 1 || !rowN->children[0]
                    || rowN->children[0]->type != NodeType::IDENTIFIER
                    || !isArrayVar(rowN->children[0]->strValue)
                    || rowN->children[0]->strValue == name  // in-place -> fall through
                    || !(arrays_.at(rowN->children[0]->strValue).is2D
                         || (arrays_.at(rowN->children[0]->strValue).isND
                             && arrays_.at(rowN->children[0]->strValue).ndDims.size() == 2)
                         || (!arrays_.at(rowN->children[0]->strValue).is2D
                             && !arrays_.at(rowN->children[0]->strValue).isND
                             && arrays_.at(rowN->children[0]->strValue).orient == VecOrient::Row))
                    || arrays_.at(rowN->children[0]->strValue).dtype != ValueType::DOUBLE) {
                    allMatRows = false;
                    break;
                }
            const AbstractValue rv = inferExpr(rhs, types_, reg_, classes_);
            if (allMatRows && rv.type.isConcrete()) {
                const ArrayInfo  &M    = arrays_.at(name);
                const ArrayInfo  &op0  = arrays_.at(rhs.children[0]->children[0]->strValue);
                const std::string cols = dimExpr(op0, 1);
                std::string       totalRows;
                for (const auto &rowN : rhs.children)
                    totalRows += (totalRows.empty() ? "" : " + ")
                                 + dimExpr(arrays_.at(rowN->children[0]->strValue), 0);
                line("{");
                ++indent_;
                line("const std::size_t _nk_tr = (" + totalRows + ");");  // total rows
                line("const std::size_t _nk_nc = " + cols + ";");          // shared cols
                line(M.ndDims[0] + " = _nk_tr;");
                line(M.ndDims[1] + " = _nk_nc;");
                for (std::size_t e = 1; e < rhs.children.size(); ++e) {
                    const ArrayInfo &op = arrays_.at(rhs.children[e]->children[0]->strValue);
                    line("if (" + dimExpr(op, 1) + " != _nk_nc) throw std::out_of_range(\""
                         "numkit: vertcat column dimensions must agree\");");
                }
                line(name + ".resize(_nk_tr * _nk_nc);");
                line("std::size_t _nk_ro = 0;");
                for (const auto &rowN : rhs.children) {
                    const ArrayInfo &op = arrays_.at(rowN->children[0]->strValue);
                    line("{");
                    ++indent_;
                    line("const std::size_t _nk_pr = " + dimExpr(op, 0) + ";");
                    open("for (std::size_t _nk_j = 0; _nk_j < _nk_nc; ++_nk_j)");
                    open("for (std::size_t _nk_ii = 0; _nk_ii < _nk_pr; ++_nk_ii)");
                    line(name + "[(_nk_ro + _nk_ii) + _nk_j * _nk_tr] = "
                         + op.dataExpr + "[_nk_ii + _nk_j * _nk_pr];");
                    close();
                    close();
                    line("_nk_ro += _nk_pr;");
                    --indent_;
                    line("}");
                }
                --indent_;
                line("}");
                types_.set(name, rv);
                return;
            }
        }
        // BLOCK-matrix literal `M = [A B; C D]`: a multi-row MATRIX_LITERAL whose every row
        // is a HORZCAT of >=1 matrix vars (the >1-block rows the single-matrix vertcat above
        // does not match). Result (sum row-heights) x (common total cols), a rank-2
        // ndRuntimeLocal. Each block is copied column-major into its (rowOff, colOff) region;
        // runtime guards check the within-row block heights agree and each row's total width
        // matches the common width. v1: DOUBLE matrix vars, none aliasing the dest.
        if (isArrayVar(name) && arrays_.at(name).isLocal && arrays_.at(name).ndRuntimeLocal
            && arrays_.at(name).ndDims.size() == 2 && rhs.type == NodeType::MATRIX_LITERAL
            && rhs.children.size() > 1) {
            bool allBlocks = true;
            for (const auto &rowN : rhs.children) {
                if (!rowN || rowN->children.empty()) { allBlocks = false; break; }
                for (const auto &el : rowN->children)
                    if (!el || el->type != NodeType::IDENTIFIER || !isArrayVar(el->strValue)
                        || el->strValue == name
                        || !(arrays_.at(el->strValue).is2D
                             || (arrays_.at(el->strValue).isND
                                 && arrays_.at(el->strValue).ndDims.size() == 2))
                        || arrays_.at(el->strValue).dtype != ValueType::DOUBLE) {
                        allBlocks = false;
                        break;
                    }
                if (!allBlocks) break;
            }
            const AbstractValue rv = inferExpr(rhs, types_, reg_, classes_);
            if (allBlocks && rv.type.isConcrete()) {
                const ArrayInfo &M = arrays_.at(name);
                std::string      P, Q;  // P = sum of row heights; Q = row 0's total width
                for (const auto &rowN : rhs.children)
                    P += (P.empty() ? "" : " + ")
                         + dimExpr(arrays_.at(rowN->children[0]->strValue), 0);
                for (const auto &el : rhs.children[0]->children)
                    Q += (Q.empty() ? "" : " + ") + dimExpr(arrays_.at(el->strValue), 1);
                line("{");
                ++indent_;
                line("const std::size_t _nk_P = (" + P + ");");
                line("const std::size_t _nk_Q = (" + Q + ");");
                line(M.ndDims[0] + " = _nk_P;");
                line(M.ndDims[1] + " = _nk_Q;");
                line(name + ".assign(_nk_P * _nk_Q, 0.0);");
                line("std::size_t _nk_ro = 0;");
                for (const auto &rowN : rhs.children) {
                    line("{");
                    ++indent_;
                    line("const std::size_t _nk_rh = "
                         + dimExpr(arrays_.at(rowN->children[0]->strValue), 0) + ";");
                    line("std::size_t _nk_co = 0;");
                    for (const auto &el : rowN->children) {
                        const ArrayInfo &b = arrays_.at(el->strValue);
                        line("{");
                        ++indent_;
                        line("const std::size_t _nk_br = " + dimExpr(b, 0) + ";");
                        line("const std::size_t _nk_bc = " + dimExpr(b, 1) + ";");
                        line("if (_nk_br != _nk_rh) throw std::out_of_range(\"numkit: block row "
                             "heights must agree\");");
                        open("for (std::size_t _nk_j = 0; _nk_j < _nk_bc; ++_nk_j)");
                        open("for (std::size_t _nk_i = 0; _nk_i < _nk_br; ++_nk_i)");
                        line(name + "[(_nk_ro + _nk_i) + (_nk_co + _nk_j) * _nk_P] = "
                             + b.dataExpr + "[_nk_i + _nk_j * _nk_br];");
                        close();
                        close();
                        line("_nk_co += _nk_bc;");
                        --indent_;
                        line("}");
                    }
                    line("if (_nk_co != _nk_Q) throw std::out_of_range(\"numkit: block row widths "
                         "must agree\");");
                    line("_nk_ro += _nk_rh;");
                    --indent_;
                    line("}");
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

        // Whole-array COPY: `B = A` where A is an array VAR (param or local) -> a value
        // copy of A's flat buffer into B (column-major), plus B's dim companions for a
        // runtime-dim dst. Works for any rank (1-D / 2-D KnownDims / runtime-dim N-D); B
        // takes A's inferred type. v1: A distinct from B (a self-copy B=B is a no-op,
        // skipped). numel(A) = 1-D length / 2-D rows*cols / N-D product of dims.
        if (isArrayVar(name) && arrays_.at(name).isLocal && rhs.type == NodeType::IDENTIFIER
            && isArrayVar(rhs.strValue) && rhs.strValue != name) {
            const ArrayInfo    &A  = arrays_.at(rhs.strValue);
            const ArrayInfo    &B  = arrays_.at(name);
            const AbstractValue rv = inferExpr(rhs, types_, reg_, classes_);
            if (rv.type.isConcrete() && !rv.type.shape.isScalar()) {
                std::string numel;
                if (A.isND) {
                    numel = A.ndDims[0];
                    for (std::size_t i = 1; i < A.ndDims.size(); ++i) numel += " * " + A.ndDims[i];
                } else if (A.is2D) {
                    numel = A.rowsVar + " * " + A.colsVar;
                } else {
                    numel = A.lenVar;
                }
                line("{");
                ++indent_;
                if (B.isND && B.ndRuntimeLocal)  // runtime-dim dst: copy A's dims -> B's companions
                    for (std::size_t k = 0; k < B.ndDims.size(); ++k)
                        line(B.ndDims[k] + " = " + dimExpr(A, k) + ";");
                line(name + ".assign(" + A.dataExpr + ", " + A.dataExpr + " + (" + numel + "));");
                --indent_;
                line("}");
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
        // Plain struct: flatten `s.f = rhs` to a synthesized field-local (field-
        // flattening; no struct type), generalised to a NESTED chain s.a.b via the
        // chain helper. v1: a scalar (unboxable) field, or a 1-D array-var field.
        const std::string fld = base.type.isObject() ? std::string() : structFieldLocal(lhs);
        if (!fld.empty()) {
            const AbstractValue rv = inferExpr(rhs, types_, reg_, classes_);
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
    // [m, i] = max(x) / [m, i] = min(x): native argmax/argmin. m = the extremum
    // value, i = its 1-based index (the FIRST occurrence on ties, MATLAB). NaN is
    // ignored (the extremum of the non-NaNs); an all-NaN vector -> the value NaN at
    // index 1; the empty vector -> (NaN, 0) [MATLAB returns [], not representable as
    // a codegen scalar]. The update fires on the first element, on a strict cmp, or
    // when acc is NaN and the candidate is not (so the first non-NaN seeds it).
    // ALWAYS native (like [r,c]=size): maxMinMultiTransfer types m,i concrete for a
    // DOUBLE vector, so this emit must match in every tier (a matrix/non-double
    // operand stays Dynamic -> the bridged multi path below). BEFORE the user-fn
    // path. v1: a single 1-D DOUBLE array var, the two-output form.
    if (rhs.type == NodeType::CALL && rhs.children.size() == 2
        && rhs.children[0]->type == NodeType::IDENTIFIER
        && (rhs.children[0]->strValue == "max" || rhs.children[0]->strValue == "min")
        && rhs.children[1]->type == NodeType::IDENTIFIER
        && isArrayVar(rhs.children[1]->strValue) && s.returnNames.size() == 2
        && !arrays_.at(rhs.children[1]->strValue).is2D
        && !arrays_.at(rhs.children[1]->strValue).isND
        && arrays_.at(rhs.children[1]->strValue).dtype == ValueType::DOUBLE
        && !(ctx_ && ctx_->funcs && ctx_->funcs->has(rhs.children[0]->strValue))) {
        const std::string &rn0 = s.returnNames[0];  // value
        const std::string &rn1 = s.returnNames[1];  // 1-based index
        if (rn0.empty() || rn0 == "~" || rn1.empty() || rn1 == "~")
            unsupported("[m,i]=max/min with an ignored (~) output (v1)");
        if (isArrayVar(rn0) || isArrayVar(rn1))
            unsupported("[m,i]=max/min into a non-scalar target");
        const ArrayInfo  &xa  = arrays_.at(rhs.children[1]->strValue);
        const std::string cmp = rhs.children[0]->strValue == "max" ? ">" : "<";
        line("{");
        ++indent_;
        line("const std::size_t _nk_n = " + xa.lenVar + ";");
        open("if (_nk_n == 0)");
        line(rn0 + " = std::numeric_limits<double>::quiet_NaN(); " + rn1 + " = 0.0;");
        close();
        open("else");
        line("double _nk_acc = " + xa.dataExpr + "[0]; std::size_t _nk_idx = 1;");
        open("for (std::size_t _nk_i = 1; _nk_i < _nk_n; ++_nk_i)");
        line("const double _nk_v = " + xa.dataExpr + "[_nk_i];");
        line("if (_nk_v " + cmp + " _nk_acc || (_nk_acc != _nk_acc && _nk_v == _nk_v))");
        line("    { _nk_acc = _nk_v; _nk_idx = _nk_i + 1; }");
        close();
        line(rn0 + " = _nk_acc; " + rn1 + " = static_cast<double>(_nk_idx);");
        close();
        --indent_;
        line("}");
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
