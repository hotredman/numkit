// include/bytecode.hpp
#pragma once

#include <numkit/value/value.hpp>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace numkit {

enum class OpCode : uint8_t {
    // ── Data movement ────────────────────────────────────────
    LOAD_CONST,   // dst, constIdx          R[dst] = constants[constIdx]
    LOAD_EMPTY,   // dst                    R[dst] = empty
    LOAD_STRING,  // dst, strIdx            R[dst] = strings[strIdx]
    MOVE,         // dst, src               R[dst] = R[src]
    LOAD_END,     // dst, arrReg, dim       R[dst] = size(R[arrReg], dim)
    COLON_ALL,    // dst                    R[dst] = <colon-all marker>
    LOAD_NARGIN,  // [reserved] compiler injects nargin via varMap instead
    LOAD_NARGOUT, // [reserved] compiler injects nargout via varMap instead

    // ── Arithmetic ───────────────────────────────────────────
    ADD,   // dst, a, b              R[dst] = R[a] + R[b]
    SUB,   // dst, a, b              R[dst] = R[a] - R[b]
    MUL,   // dst, a, b              R[dst] = R[a] * R[b]
    RDIV,  // dst, a, b              R[dst] = R[a] / R[b]
    LDIV,  // dst, a, b              R[dst] = R[a] \ R[b]
    POW,   // dst, a, b              R[dst] = R[a] ^ R[b]
    NEG,   // dst, src               R[dst] = -R[src]
    UPLUS, // dst, src               R[dst] = +R[src]

    // ── Element-wise arithmetic ──────────────────────────────
    EMUL,  // dst, a, b              R[dst] = R[a] .* R[b]
    ERDIV, // dst, a, b              R[dst] = R[a] ./ R[b]
    ELDIV, // dst, a, b              R[dst] = R[a] .\ R[b]
    EPOW,  // dst, a, b              R[dst] = R[a] .^ R[b]

    // ── Comparison ───────────────────────────────────────────
    EQ, // dst, a, b              R[dst] = R[a] == R[b]
    NE, // dst, a, b              R[dst] = R[a] ~= R[b]
    LT, // dst, a, b              R[dst] = R[a] < R[b]
    GT, // dst, a, b              R[dst] = R[a] > R[b]
    LE, // dst, a, b              R[dst] = R[a] <= R[b]
    GE, // dst, a, b              R[dst] = R[a] >= R[b]

    // ── Logical ──────────────────────────────────────────────
    AND,    // dst, a, b              R[dst] = R[a] & R[b]
    OR,     // dst, a, b              R[dst] = R[a] | R[b]
    NOT,    // dst, src               R[dst] = ~R[src]
    AND_SC, // [reserved] && short-circuit — compiler uses JMP_FALSE chain instead
    OR_SC,  // [reserved] || short-circuit — compiler uses JMP_TRUE chain instead

    // ── Control flow ─────────────────────────────────────────
    JMP,       // offset(int16)          unconditional jump
    JMP_TRUE,  // reg, offset(int16)     jump if R[reg] != 0
    JMP_FALSE, // reg, offset(int16)     jump if R[reg] == 0

    // ── For-loop ─────────────────────────────────────────────
    FOR_INIT,       // var, range, endOffset  setup iterator from R[range]
    FOR_INIT_RANGE, // var, start, stop, endOffset, step  fused `for v = a:b` /
                    //                                    `for v = a:s:b` — skips
                    //                                    materialising the colon
                    //                                    range value (avoids the
                    //                                    8N-byte allocation for
                    //                                    `for i = 1:N` loops).
                    //                                    e=0xFF → implicit step=1.
    FOR_NEXT,       // var, backOffset        advance iterator, jump back or fall through

    // ── Function calls ───────────────────────────────────────
    CALL,          // dst, argBase, nargs, funcIdx, e=nargout  R[dst] = func(R[base..base+nargs-1])
    CALL_MULTI,    // dstBase, funcIdx, argBase, nargs, e=nout
    CALL_BUILTIN,  // dst, builtinId, base, nargs     inline builtin (mod, sin, etc.)
    CALL_INDIRECT, // dst, fhReg, base, nargs         R[dst] = R[fhReg](R[base..base+nargs-1])
    // [a,b,…] = R[fhReg](R[base..]): multi-output indirect (handle-variable)
    // call. a=outBase, b=fhReg, c=argBase, d=nargs, e=nout. Mirrors
    // CALL_INDIRECT's handle resolution + CALL_MULTI's nout frame-push so a
    // stored function handle can return several outputs (e.g. fmincon nonlcon
    // `[c, ceq] = nonlcon(x)`).
    CALL_INDIRECT_MULTI,
    // Fused element-wise idiom (VM fusion). a=dst, b=operandBase, c=nOps,
    // d=ruleIdx (into engine.fusionRules()), e=skip. Gather R[base..base+nOps)
    // → rule.execute; on success R[dst]=result and skip the `e` following
    // fallback instructions; on decline fall through to them — the normally
    // compiled idiom, i.e. exact per-op semantics.
    FUSE_EWISE,

    // ── Array indexing ───────────────────────────────────────
    INDEX_GET,       // dst, arr, idx          R[dst] = R[arr](R[idx])         1D
    INDEX_GET_2D,    // dst, arr, row, col     R[dst] = R[arr](R[row], R[col]) 2D
    INDEX_GET_ND,    // dst, arr, base, ndims  R[dst] = R[arr](R[base]..R[base+ndims-1])
    INDEX_SET,       // arr, idx, val          R[arr](R[idx]) = R[val]         1D
    INDEX_SET_2D,    // arr, row, col, val     R[arr](R[row], R[col]) = R[val] 2D
    INDEX_SET_ND,    // arr, base, ndims, val  R[arr](R[base]..R[base+ndims-1]) = R[val]
    INDEX_DELETE,    // arr, idx               R[arr](R[idx]) = []             1D
    INDEX_DELETE_2D, // arr, row, col          R[arr](R[row], R[col]) = []     2D
    INDEX_DELETE_ND, // arr, base, ndims       R[arr](R[base]..R[base+ndims-1]) = []

    // ── Struct field access ──────────────────────────────────
    FIELD_GET,           // dst, obj, nameIdx      R[dst] = R[obj].fields[nameIdx]
    FIELD_GET_OR_CREATE, // dst, obj, nameIdx      like FIELD_GET but auto-creates struct/field
    FIELD_SET,           // obj, nameIdx, val      R[obj].fields[nameIdx] = R[val]
    FIELD_GET_DYN,       // dst, obj, nameReg      R[dst] = R[obj].(R[nameReg])
    FIELD_GET_OR_CREATE_DYN, // dst, obj, nameReg  like FIELD_GET_DYN but auto-creates
    FIELD_SET_DYN,       // obj, nameReg, val      R[obj].(R[nameReg]) = R[val]
    // Struct-array element field write: R[obj](R[idx]).field = R[val].
    // Auto-grows when idx exceeds current numel; creates a 1×0 struct
    // array if obj is unset / not a struct.
    STRUCT_ELEM_FIELD_SET, // a=obj, b=idxReg, c=valReg, d=nameIdx
    // obj.name(args): dotted call. a=dst, b=objReg, c=argBase, d=nameIdx,
    // e=nargs. If R[objReg] is an OBJECT whose class has method `name`,
    // dispatch it (self + args); otherwise fall back to FIELD_GET + the
    // CALL_INDIRECT machinery (struct-field func handle / index). Object
    // model — see OBJECT_MODEL.md §3.
    CALL_METHOD,
    // [a,b] = obj.name(args): dotted multi-output method call. a=outBase,
    // b=objReg, c=argBase, d=nameIdx, e=(nargs<<4)|nout (each nibble ≤15 —
    // methods with >15 args/outputs are not supported via this path).
    // Dispatches the class method with nout result slots written to
    // R[outBase..outBase+nout). Object model — see OBJECT_MODEL.md §3.
    CALL_METHOD_MULTI,
    // classdef superclass calls inside a method/ctor body (compiled into the
    // method chunk so the body runs on the VM). Both delegate to
    // Engine::superConstruct / superMethod.
    //   CALL_SUPER_CTOR:   `obj = obj@Base(args)` — a=dst, b=objReg (seed),
    //                      c=argBase, d=baseNameIdx, e=nargs.
    //   CALL_SUPER_METHOD: `[outs] = method@Base(obj, args)` — a=outBase,
    //                      b=argBase (args incl. obj at [0]), c=nargs,
    //                      d=idx of "Base>method", e=nout.
    CALL_SUPER_CTOR,
    CALL_SUPER_METHOD,
    // Struct-array element get/set as a whole scalar struct, used by the
    // general compound-lvalue store chain (`d(i).a.b = …`, `d(i,j,k).…`).
    // Subscripts live in R[base..base+nargs-1] (column-major, any rank).
    // GET auto-grows the array and returns a 1×1 struct (empty if the
    // slot was vacant); SET writes the scalar struct back into the
    // element. Coerce an unset/empty receiver to a struct array.
    STRUCT_ELEM_GET_OR_CREATE, // a=dst, b=obj, c=base, e=nargs
    STRUCT_ELEM_SET,           // a=obj, b=base, c=nargs, e=valReg

    // ── Cell array access ────────────────────────────────────
    CELL_GET,      // dst, cell, idx         R[dst] = R[cell]{R[idx]}        1D
    // Like CELL_GET but for the compound-lvalue store chain: coerces an
    // unset/empty receiver to a cell and auto-grows to fit the subscripts
    // in R[base..base+nargs-1] (any rank), returning the content slot.
    CELL_GET_OR_CREATE, // a=dst, b=cell, c=base, e=nargs
    CELL_SET,      // cell, idx, val         R[cell]{R[idx]} = R[val]        1D
    CELL_GET_2D,   // dst, cell, row, col    R[dst] = R[cell]{R[row], R[col]}
    CELL_SET_2D,   // cell, row, col, val    R[cell]{R[row], R[col]} = R[val]
    CELL_GET_MULTI,// outBase, cell, idx, nout  R[outBase..+nout] = R[cell]{R[idx]}
    CELL_GET_ND,   // dst, cell, base, ndims   R[dst] = R[cell]{R[base]..R[base+ndims-1]}
    CELL_SET_ND,   // cell, base, ndims, val   R[cell]{R[base]..R[base+ndims-1]} = R[val]

    // ── Transpose ────────────────────────────────────────────
    CTRANSPOSE, // dst, src               R[dst] = R[src]' (conjugate)
    TRANSPOSE,  // dst, src               R[dst] = R[src].' (non-conjugate)

    // ── Literals / construction ──────────────────────────────
    COLON,        // dst, start, stop       R[dst] = R[start]:R[stop]
    COLON3,       // dst, start, step, stop R[dst] = R[start]:R[step]:R[stop]
    HORZCAT,      // dst, base, count       R[dst] = [R[base], ..., R[base+count-1]]
    HORZCAT_APPEND,// dst, val              R[dst] = [R[dst], R[val]]   amortised O(1)
    // Comma-separated-list expansion: R[dst] = [R[dst], structArr(0).fname, ..., structArr(N-1).fname]
    HORZCAT_APPEND_CSL, // a=dst, b=structArrReg, d=nameIdx
                  //                        when dst is a row vector / empty and val is a real
                  //                        scalar; falls back to a 2-elem horzcat otherwise
    // Cell comma-separated-list: R[dst] = [R[dst], cell{sub}...] -- the selected cell
    // contents (a=dst in/out, b=cellReg, c=subReg holding the ':' colon marker / vector
    // / scalar; resolveIndices over the cell numel). For [c{:}] / [c{vec}] in a literal.
    HORZCAT_APPEND_CELL_CSL, // a=dst, b=cellReg, c=subReg
    VERTCAT,      // dst, base, count       R[dst] = [R[base]; ...; R[base+count-1]]
    MATRIX_BUILD, // [reserved] compiler uses HORZCAT/VERTCAT instead
    CELL_LITERAL, // dst, base, count       {R[base]..R[base+count-1]}

    // ── Display ──────────────────────────────────────────────
    DISPLAY, // reg, nameIdx           print "name = value"

    // ── Return / flow signals ────────────────────────────────
    RET,       // reg                    return R[reg]
    RET_MULTI, // base, count            return R[base..base+count-1]
    RET_EMPTY, //                        return empty
    // varargout return: a=fixedBase, b=numFixed, c=varargoutReg. Returns the
    // numFixed leading fixed outputs (R[fixedBase..]) followed by the elements
    // of the varargout cell R[varargoutReg] — a DYNAMIC return count
    // (numFixed + numel(cell)), so a `function varargout = f(...)` can return
    // however many values the caller's nargout asks for.
    RET_VARARGOUT,
    BREAK,     // [reserved] compiler uses JMP + NOP(flag) instead
    CONTINUE,  // [reserved] compiler uses JMP + NOP(flag) instead

    // ── Error handling ───────────────────────────────────────
    TRY_BEGIN, // catchOffset, exReg     setup try, on catch: R[exReg] = exception
    TRY_END,   //                        cleanup try block
    THROW,     // reg                    error(R[reg])

    // ── Scope ────────────────────────────────────────────────
    GLOBAL_DECL,     // [reserved] compiler writes to chunk.globalNames instead
    PERSISTENT_DECL, // [reserved] compiler writes to chunk.globalNames instead
    CLOSURE_MAKE,    // [reserved] compiler uses cell-packing {funcHandle, captures} instead
    CLEAR_VAR,       // reg                    R[reg] = empty (clear variable)
    CLEAR_DYN,       // nameReg                lookup name in varMap, clear register
    EXIST_VAR,       // dst, nameReg, filterReg  check name in varMap/functions → 0/1/5
    //                           filterReg=0: no filter; else R[filterReg] = 'var'/'builtin'
    WHO,  // base, count            list variables (count=0: all, else R[base..base+count-1])
    WHOS, // base, count            list variables with details (same)

    // ── Utility ──────────────────────────────────────────────
    NOP,        //                        no-op (patching, alignment)
    ASSERT_DEF, // reg, nameIdx           throw if R[reg] is unset (undefined variable)

    // ── Scalar-specialized (compiler guarantees double-scalar operands) ──
    ADD_SS,  // dst, a, b   R[dst] = R[a].scalar + R[b].scalar  (no type check)
    SUB_SS,  // dst, a, b
    MUL_SS,  // dst, a, b
    RDIV_SS, // dst, a, b
    POW_SS,  // dst, a, b   (includes integer-exponent fast paths)
    NEG_S,   // dst, src    R[dst] = -R[src].scalar

    // ── Fused multiply-add/sub (loop opt #2): R[a] = R[b] ± R[c]*R[e] ──
    // Collapses sum-of-products (MAC) chains — filters, polynomials, dot
    // products — into one opcode. Fast path is all-scalar (two-step double:
    // prod = c*e; a = b ± prod, so bit-identical to the unfused MUL+ADD/SUB).
    // Non-scalar operands fall back to the matching product then sum/diff.
    MULADD,  // a=dst, b=acc, c, e   R[a] = R[b] + R[c]*R[e]   (* matmul fallback)
    MULSUB,  // a=dst, b=acc, c, e   R[a] = R[b] - R[c]*R[e]
    EMULADD, // a=dst, b=acc, c, e   R[a] = R[b] + R[c].*R[e]
    EMULSUB, // a=dst, b=acc, c, e   R[a] = R[b] - R[c].*R[e]

    // ── End marker ───────────────────────────────────────────
    HALT, //                        stop execution
};

// ============================================================
// Instruction: fixed 8-byte encoding
// ============================================================
//
//  Byte:  0       1       2       3       4       5       6       7
//        [opcode] [  a  ] [  b  ] [  c  ] [    d (int16)   ] [  e  ]
//
//  a, b, c, e — register indices (0-255) or small immediates
//  d          — signed 16-bit: jump offset, constant/string/func index
//
struct Instruction
{
    OpCode op;
    uint8_t a; // dst / reg
    uint8_t b; // src1 / base
    uint8_t c; // src2 / nargs
    int16_t d; // offset / constIdx / funcIdx / nameIdx
    uint8_t e; // 5th operand (nout for CALL_MULTI, etc.)

    Instruction()
        : op(OpCode::HALT)
        , a(0)
        , b(0)
        , c(0)
        , d(0)
        , e(0)
    {}

    static Instruction make_abcde(OpCode op, uint8_t a, uint8_t b, uint8_t c, int16_t d, uint8_t e)
    {
        Instruction i;
        i.op = op;
        i.a = a;
        i.b = b;
        i.c = c;
        i.d = d;
        i.e = e;
        return i;
    }
    static Instruction make_abc(OpCode op, uint8_t a, uint8_t b, uint8_t c)
    {
        Instruction i;
        i.op = op;
        i.a = a;
        i.b = b;
        i.c = c;
        return i;
    }
    static Instruction make_abd(OpCode op, uint8_t a, uint8_t b, int16_t d)
    {
        Instruction i;
        i.op = op;
        i.a = a;
        i.b = b;
        i.d = d;
        return i;
    }
    static Instruction make_ad(OpCode op, uint8_t a, int16_t d)
    {
        Instruction i;
        i.op = op;
        i.a = a;
        i.d = d;
        return i;
    }
    static Instruction make_a(OpCode op, uint8_t a)
    {
        Instruction i;
        i.op = op;
        i.a = a;
        return i;
    }
    static Instruction make_d(OpCode op, int16_t d)
    {
        Instruction i;
        i.op = op;
        i.d = d;
        return i;
    }
    static Instruction make_none(OpCode op)
    {
        Instruction i;
        i.op = op;
        return i;
    }
};

static_assert(sizeof(Instruction) == 8, "Instruction must be 8 bytes");

// ============================================================
// Source location for debugging / error reporting
// ============================================================
struct SourceLoc
{
    uint16_t line = 0;
    uint16_t col = 0;
};

// ============================================================
// BytecodeChunk: compiled function or script
// ============================================================
struct BytecodeChunk
{
    std::vector<Instruction> code;

    // Constant pools
    std::vector<Value> constants;    // numeric/complex constants
    std::vector<std::string> strings; // string constants + field/variable/function names

    // Metadata
    std::string name;         // function name or "<script>"
    uint8_t numRegisters = 0; // total registers needed
    uint8_t numParams = 0;
    uint8_t numReturns = 0;
    std::vector<std::string> paramNames;
    std::vector<std::string> returnNames;

    // Closure support: indices of captured variables from parent scope
    std::vector<uint8_t> capturedRegisters;

    // Variable name → register mapping (for exporting to environment after execution)
    std::vector<std::pair<std::string, uint8_t>> varMap;

    // Names of variables the compiler emits a write for (STORE, param intro,
    // for-loop var, catch var, global/persistent decl, etc). Reads alone do
    // not populate this set. Used by DebugWorkspace::names() to decide if a
    // built-in (pi, eps, …) should be visible — MATLAB only lists built-ins
    // in `whos` once they've been shadowed by an assignment.
    std::unordered_set<std::string> assignedVars;

    // Global variable names this chunk routes through globalsEnv_: this chunk's
    // own `global X` declarations PLUS the base workspace's inherited globals
    // (seeded at compile so `gv = 5` without a re-declaration still writes the
    // global). Used by the import/export paths.
    std::vector<std::string> globalNames;

    // Just this chunk's OWN top-level `global X` declarations (a subset of
    // globalNames, excluding the inherited seed). Engine::runOneChunk uses it to
    // record base-workspace membership and seed a freshly declared global as [].
    // Excluding the inherited seed is what keeps `clear global g` from being
    // resurrected by a later chunk that merely inherited g.
    std::vector<std::string> ownGlobalDecls;

    // Per-CALL-site arg names — populated by the compiler for each
    // CALL / CALL_MULTI it emits. Key = index into `code` of the CALL
    // instruction. Value = list of arg names; entry i is the name of
    // the i-th arg if it was a bare identifier at the call site,
    // empty string otherwise. Used by `inputname(k)` to introspect
    // the caller's call site. Sparse on purpose — only populated for
    // calls that have at least one bare-identifier arg.
    std::unordered_map<uint32_t, std::vector<std::string>> callSiteArgNames;

    // Source mapping (parallel to code, same size — one entry per instruction)
    std::vector<SourceLoc> sourceMap;

    // Original source text (shared across chunks compiled from the same source)
    std::shared_ptr<const std::string> sourceCode;
};

} // namespace numkit