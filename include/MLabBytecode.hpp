// include/MLabBytecode.hpp
#pragma once

#include "MLabValue.hpp"
#include <cstdint>
#include <string>
#include <vector>

namespace mlab {

enum class OpCode : uint8_t {
    // ── Data movement ────────────────────────────────────────
    LOAD_CONST,   // dst, constIdx          R[dst] = constants[constIdx]
    LOAD_EMPTY,   // dst                    R[dst] = empty
    LOAD_STRING,  // dst, strIdx            R[dst] = strings[strIdx]
    MOVE,         // dst, src               R[dst] = R[src]
    LOAD_END,     // dst, arrReg, dim       R[dst] = size(R[arrReg], dim)
    COLON_ALL,    // dst                    R[dst] = <colon-all marker>
    LOAD_NARGIN,  // dst                    R[dst] = nargin
    LOAD_NARGOUT, // dst                    R[dst] = nargout

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
    AND_SC, // dst, a, b, skipOffset  && short-circuit
    OR_SC,  // dst, a, b, skipOffset  || short-circuit

    // ── Control flow ─────────────────────────────────────────
    JMP,       // offset(int16)          unconditional jump
    JMP_TRUE,  // reg, offset(int16)     jump if R[reg] != 0
    JMP_FALSE, // reg, offset(int16)     jump if R[reg] == 0

    // ── For-loop ─────────────────────────────────────────────
    FOR_INIT, // var, range, endOffset  setup iterator from R[range]
    FOR_NEXT, // var, backOffset        advance iterator, jump back or fall through

    // ── Function calls ───────────────────────────────────────
    CALL,          // dst, funcIdx, base, nargs       R[dst] = func(R[base..base+nargs-1])
    CALL_MULTI,    // dstBase, funcIdx, argBase, nargs, e=nout
    CALL_BUILTIN,  // dst, builtinId, base, nargs     inline builtin (mod, sin, etc.)
    CALL_INDIRECT, // dst, fhReg, base, nargs         R[dst] = R[fhReg](R[base..base+nargs-1])

    // ── Array indexing ───────────────────────────────────────
    INDEX_GET,       // dst, arr, idx          R[dst] = R[arr](R[idx])         1D
    INDEX_GET_2D,    // dst, arr, row, col     R[dst] = R[arr](R[row], R[col]) 2D
    INDEX_GET_ND,    // dst, arr, base, ndims  R[dst] = R[arr](R[base]..R[base+ndims-1])
    INDEX_SET,       // arr, idx, val          R[arr](R[idx]) = R[val]         1D
    INDEX_SET_2D,    // arr, row, col, val     R[arr](R[row], R[col]) = R[val] 2D
    INDEX_SET_ND,    // arr, base, ndims, val  R[arr](R[base]..R[base+ndims-1]) = R[val]
    INDEX_DELETE,    // arr, idx               R[arr](R[idx]) = []             1D
    INDEX_DELETE_2D, // arr, row, col          R[arr](R[row], R[col]) = []     2D

    // ── Struct field access ──────────────────────────────────
    FIELD_GET,     // dst, obj, nameIdx      R[dst] = R[obj].fields[nameIdx]
    FIELD_SET,     // obj, nameIdx, val      R[obj].fields[nameIdx] = R[val]
    FIELD_GET_DYN, // dst, obj, nameReg      R[dst] = R[obj].(R[nameReg])
    FIELD_SET_DYN, // obj, nameReg, val      R[obj].(R[nameReg]) = R[val]

    // ── Cell array access ────────────────────────────────────
    CELL_GET,    // dst, cell, idx         R[dst] = R[cell]{R[idx]}        1D
    CELL_SET,    // cell, idx, val         R[cell]{R[idx]} = R[val]        1D
    CELL_GET_2D, // dst, cell, row, col    R[dst] = R[cell]{R[row], R[col]}
    CELL_SET_2D, // cell, row, col, val    R[cell]{R[row], R[col]} = R[val]

    // ── Transpose ────────────────────────────────────────────
    CTRANSPOSE, // dst, src               R[dst] = R[src]' (conjugate)
    TRANSPOSE,  // dst, src               R[dst] = R[src].' (non-conjugate)

    // ── Literals / construction ──────────────────────────────
    COLON,        // dst, start, stop       R[dst] = R[start]:R[stop]
    COLON3,       // dst, start, step, stop R[dst] = R[start]:R[step]:R[stop]
    HORZCAT,      // dst, base, count       R[dst] = [R[base], ..., R[base+count-1]]
    VERTCAT,      // dst, base, count       R[dst] = [R[base]; ...; R[base+count-1]]
    MATRIX_BUILD, // dst, base, nrows, ncols  build 2D from scalars
    CELL_LITERAL, // dst, base, count       {R[base]..R[base+count-1]}

    // ── Display ──────────────────────────────────────────────
    DISPLAY, // reg, nameIdx           print "name = value"

    // ── Return / flow signals ────────────────────────────────
    RET,       // reg                    return R[reg]
    RET_MULTI, // base, count            return R[base..base+count-1]
    RET_EMPTY, //                        return empty
    BREAK,     //                        break from loop
    CONTINUE,  //                        continue loop

    // ── Error handling ───────────────────────────────────────
    TRY_BEGIN, // catchOffset, exReg     setup try, on catch: R[exReg] = exception
    TRY_END,   //                        cleanup try block
    THROW,     // reg                    error(R[reg])

    // ── Scope ────────────────────────────────────────────────
    GLOBAL_DECL,     // nameIdx                declare global variable
    PERSISTENT_DECL, // nameIdx                declare persistent variable
    CLOSURE_MAKE,    // dst, funcIdx           R[dst] = @funcname or @(args) expr

    // ── Utility ──────────────────────────────────────────────
    NOP, //                        no-op (patching, alignment)

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
// BytecodeChunk: compiled function or script
// ============================================================
struct BytecodeChunk
{
    std::vector<Instruction> code;

    // Constant pools
    std::vector<MValue> constants;    // numeric/complex constants
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

    // Global variable names (declared with 'global' keyword)
    std::vector<std::string> globalNames;

    // Source mapping (for error reporting)
    std::vector<int> lineNumbers; // line number per instruction index
};

} // namespace mlab