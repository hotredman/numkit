// src/MLabCompiler.cpp
#include "MLabCompiler.hpp"
#include "MLabEngine.hpp"

#include <stdexcept>
#include <unordered_set>

namespace mlab {

// RAII guard for END_VAL compilation context
class IndexContextGuard
{
public:
    IndexContextGuard(Compiler &c, uint8_t arr, uint8_t ndims)
        : comp_(c)
        , savedArr_(c.indexContextArr_)
        , savedDim_(c.indexContextDim_)
        , savedNdims_(c.indexContextNdims_)
    {
        comp_.indexContextArr_ = arr;
        comp_.indexContextDim_ = 0;
        comp_.indexContextNdims_ = ndims;
    }
    ~IndexContextGuard()
    {
        comp_.indexContextArr_ = savedArr_;
        comp_.indexContextDim_ = savedDim_;
        comp_.indexContextNdims_ = savedNdims_;
    }
    void setDim(uint8_t d) { comp_.indexContextDim_ = d; }

    IndexContextGuard(const IndexContextGuard &) = delete;
    IndexContextGuard &operator=(const IndexContextGuard &) = delete;

private:
    Compiler &comp_;
    uint8_t savedArr_, savedDim_, savedNdims_;
};

Compiler::Compiler(Engine &engine)
    : engine_(engine)
{}

BytecodeChunk Compiler::compile(const ASTNode *ast, std::shared_ptr<const std::string> sourceCode)
{
    // Deferred clear from previous clear functions / clear all
    if (compiledFuncsDirty_) {
        compiledFuncs_.clear();
        compiledFuncsDirty_ = false;
    }

    chunk_ = BytecodeChunk{};
    chunk_.name = "<script>";
    chunk_.sourceCode = std::move(sourceCode);
    varRegisters_.clear();
    loopStack_.clear();
    constRegCache_.clear();
    scalarRegs_.reset();
    nextReg_ = 0;
    currentLoc_ = {};
    isTopLevel_ = true;

    // Pre-import: scan AST for identifiers that exist in workspaceEnv,
    // allocate registers and emit LOAD_CONST before main code.
    // This prevents imports inside loops from resetting each iteration.
    preImportGlobals(ast);

    uint8_t lastReg = compileNode(ast);
    emitA(OpCode::RET, lastReg);

    peepholeOptimize();

    chunk_.numRegisters = nextReg_;

    // Save variable→register mapping for environment export
    for (auto &[name, reg] : varRegisters_)
        chunk_.varMap.push_back({name, reg});

    isTopLevel_ = false;
    return std::move(chunk_);
}

// ============================================================
// Register allocation
// ============================================================

void Compiler::preImportGlobals(const ASTNode *ast)
{
    // Collect all identifiers referenced in the AST
    std::unordered_set<std::string> identifiers;
    collectAllIdentifiers(ast, identifiers);

    // For each identifier found in workspaceEnv, pre-allocate register and emit LOAD_CONST
    for (auto &name : identifiers) {
        if (varRegisters_.count(name))
            continue; // already allocated
        if (kBuiltinNames.count(name))
            continue; // builtins handled separately

        MValue *existing = engine_.getVariable(name);
        if (existing && !existing->isEmpty()) {
            uint8_t r = nextReg_++;
            varRegisters_[name] = r;
            int16_t idx = static_cast<int16_t>(chunk_.constants.size());
            chunk_.constants.push_back(*existing);
            emitAD(OpCode::LOAD_CONST, r, idx);
        }
    }
}

void Compiler::collectAllIdentifiers(const ASTNode *node, std::unordered_set<std::string> &out)
{
    if (!node)
        return;
    // Don't recurse into function definitions — they compile with separate scope
    if (node->type == NodeType::FUNCTION_DEF)
        return;
    if (node->type == NodeType::IDENTIFIER)
        out.insert(node->strValue);
    // Also collect from CALL target names
    if (node->type == NodeType::CALL && !node->children.empty()
        && node->children[0]->type == NodeType::IDENTIFIER)
        out.insert(node->children[0]->strValue);
    // Recurse
    for (auto &child : node->children)
        collectAllIdentifiers(child.get(), out);
    for (auto &[cond, body] : node->branches) {
        collectAllIdentifiers(cond.get(), out);
        collectAllIdentifiers(body.get(), out);
    }
    if (node->elseBranch)
        collectAllIdentifiers(node->elseBranch.get(), out);
    // Also strValue for for-loop variable, function names, etc.
    if (node->type == NodeType::FOR_STMT && !node->strValue.empty())
        out.insert(node->strValue);
}

uint8_t Compiler::varReg(const std::string &name)
{
    auto it = varRegisters_.find(name);
    if (it != varRegisters_.end())
        return it->second;
    uint8_t r = nextReg_++;
    varRegisters_[name] = r;

    // Import builtin constants (pi, inf, nan, etc.) regardless of context
    if (kBuiltinNames.count(name)) {
        MValue *existing = engine_.getVariable(name);
        if (existing && !existing->isEmpty()) {
            int16_t idx = static_cast<int16_t>(chunk_.constants.size());
            chunk_.constants.push_back(*existing);
            emitAD(OpCode::LOAD_CONST, r, idx);
        }
    }

    return r;
}

uint8_t Compiler::varRegRead(const std::string &name)
{
    // Check if variable is already known (local scope)
    auto it = varRegisters_.find(name);
    if (it != varRegisters_.end())
        return it->second;

    // Check workspaceEnv
    if (isTopLevel_ && !kBuiltinNames.count(name)) {
        MValue *existing = engine_.getVariable(name);
        if (existing)
            return varReg(name); // will import from workspaceEnv (including empty values)
    }

    // Check if it's a builtin constant
    if (kBuiltinNames.count(name))
        return varReg(name);

    // Check if it's a known function — emit zero-arg CALL
    // In MATLAB, bare function name in expression context is a call: t1 = toc
    if (engine_.hasFunction(name) || engine_.externalFuncs_.count(name)) {
        uint8_t argBase = nextReg_;
        uint8_t dst = tempReg();
        int16_t funcIdx = addStringConstant(name);
        emit(Instruction::make_abcde(OpCode::CALL, dst, argBase, 0, funcIdx, nargoutContext_));
        return dst;
    }

    // Unknown variable
    if (isTopLevel_) {
        // Top-level: throw to trigger TW fallback
        throw std::runtime_error("Undefined variable: " + name);
    }
    // Inside function: variable might be assigned later (e.g. via eval, global).
    // Allocate register — if still unset at runtime, emit error.
    uint8_t reg = varReg(name);
    // Emit runtime check: if R[reg] is unset, throw "Undefined variable: name"
    int16_t nameIdx = addStringConstant(name);
    emitAD(OpCode::ASSERT_DEF, reg, nameIdx);
    return reg;
}

uint8_t Compiler::tempReg()
{
    return nextReg_++;
}

// ============================================================
// Peephole optimization
// ============================================================

void Compiler::peepholeOptimize()
{
    // Peephole MOVE elimination disabled — needs more thorough liveness analysis
    // to avoid corrupting register state. The constant register cache in compileNumber()
    // already eliminates most redundant LOAD_CONST instructions.
    return;
    auto &code = chunk_.code;
    size_t n = code.size();
    if (n < 2)
        return;

    // Pass: eliminate MOVE a, b followed by instruction that reads a as single use.
    // Pattern: MOVE dst, src; OP ..., dst, ... → OP ..., src, ...  (NOP the MOVE)
    // Only safe when dst is a temp register not used elsewhere.

    // Build use count per register: count how many times each register is read as source
    std::vector<int> readCount(256, 0);
    std::vector<int> writeCount(256, 0);
    for (size_t i = 0; i < n; ++i) {
        auto &ins = code[i];
        switch (ins.op) {
        case OpCode::ADD: case OpCode::SUB: case OpCode::MUL: case OpCode::RDIV:
        case OpCode::LDIV: case OpCode::POW: case OpCode::EMUL: case OpCode::ERDIV:
        case OpCode::ELDIV: case OpCode::EPOW: case OpCode::EQ: case OpCode::NE:
        case OpCode::LT: case OpCode::GT: case OpCode::LE: case OpCode::GE:
        case OpCode::AND: case OpCode::OR:
            writeCount[ins.a]++;
            readCount[ins.b]++;
            readCount[ins.c]++;
            break;
        case OpCode::MOVE: case OpCode::NEG: case OpCode::UPLUS: case OpCode::NOT:
        case OpCode::CTRANSPOSE: case OpCode::TRANSPOSE:
            writeCount[ins.a]++;
            readCount[ins.b]++;
            break;
        case OpCode::CALL_BUILTIN:
            writeCount[ins.a]++;
            for (uint8_t j = 0; j < ins.c; ++j)
                readCount[ins.b + j]++;
            break;
        case OpCode::INDEX_SET_2D:
            readCount[ins.b]++;
            readCount[ins.c]++;
            readCount[ins.e]++;
            break;
        case OpCode::INDEX_GET_2D:
            writeCount[ins.a]++;
            readCount[ins.b]++;
            readCount[ins.c]++;
            readCount[ins.e]++;
            break;
        case OpCode::LOAD_CONST:
        case OpCode::LOAD_EMPTY:
        case OpCode::LOAD_STRING:
            writeCount[ins.a]++;
            break;
        case OpCode::COLON:
            writeCount[ins.a]++;
            readCount[ins.b]++;
            readCount[ins.c]++;
            break;
        case OpCode::COLON3:
            writeCount[ins.a]++;
            readCount[ins.b]++;
            readCount[ins.c]++;
            readCount[ins.e]++;
            break;
        case OpCode::HORZCAT:
        case OpCode::VERTCAT:
        case OpCode::CELL_LITERAL:
            writeCount[ins.a]++;
            for (uint8_t j = 0; j < ins.c; ++j)
                readCount[ins.b + j]++;
            break;
        case OpCode::CALL:
            writeCount[ins.a]++;
            for (uint8_t j = 0; j < ins.c; ++j)
                readCount[ins.b + j]++;
            break;
        case OpCode::RET:
        case OpCode::DISPLAY:
            readCount[ins.a]++;
            break;
        case OpCode::FOR_INIT:
            writeCount[ins.a]++;
            readCount[ins.b]++;
            break;
        case OpCode::FOR_NEXT:
            writeCount[ins.a]++;
            readCount[ins.a]++;
            break;
        case OpCode::JMP_TRUE:
        case OpCode::JMP_FALSE:
            readCount[ins.a]++;
            break;
        case OpCode::INDEX_GET:
            writeCount[ins.a]++;
            readCount[ins.b]++;
            readCount[ins.c]++;
            break;
        case OpCode::INDEX_SET:
            readCount[ins.a]++;
            readCount[ins.b]++;
            readCount[ins.c]++;
            break;
        case OpCode::FIELD_GET:
        case OpCode::FIELD_GET_OR_CREATE:
            writeCount[ins.a]++;
            readCount[ins.b]++;
            break;
        case OpCode::FIELD_SET:
            readCount[ins.a]++;
            readCount[ins.c]++;
            break;
        case OpCode::CELL_GET:
            writeCount[ins.a]++;
            readCount[ins.b]++;
            readCount[ins.c]++;
            break;
        case OpCode::CELL_SET:
            readCount[ins.a]++;
            readCount[ins.b]++;
            readCount[ins.c]++;
            break;
        case OpCode::ASSERT_DEF:
            readCount[ins.a]++;
            break;
        default:
            // Conservative: treat all register fields as both read and written
            readCount[ins.a] += 2;
            writeCount[ins.a] += 2;
            readCount[ins.b] += 2;
            readCount[ins.c] += 2;
            readCount[ins.e] += 2;
            break;
        }
    }

    // Eliminate MOVE dst, src where dst is written once and read once by the next instruction
    for (size_t i = 0; i + 1 < n; ++i) {
        auto &mv = code[i];
        if (mv.op != OpCode::MOVE)
            continue;
        uint8_t dst = mv.a, src = mv.b;
        // dst must be a temp: written exactly once (this MOVE) and read exactly once
        if (writeCount[dst] != 1 || readCount[dst] != 1)
            continue;
        // src must not be overwritten between this MOVE and its use
        auto &next = code[i + 1];
        // Replace dst with src in the next instruction
        bool replaced = false;
        switch (next.op) {
        case OpCode::CALL_BUILTIN:
            // CALL_BUILTIN a=dst_out, b=argBase, c=nargs — argBase is the MOVE dst
            if (next.b == dst && next.c == 1) { next.b = src; replaced = true; }
            break;
        case OpCode::ADD: case OpCode::SUB: case OpCode::MUL: case OpCode::RDIV:
        case OpCode::LDIV: case OpCode::POW: case OpCode::EMUL: case OpCode::ERDIV:
        case OpCode::ELDIV: case OpCode::EPOW: case OpCode::EQ: case OpCode::NE:
        case OpCode::LT: case OpCode::GT: case OpCode::LE: case OpCode::GE:
        case OpCode::AND: case OpCode::OR:
            if (next.b == dst) { next.b = src; replaced = true; }
            else if (next.c == dst) { next.c = src; replaced = true; }
            break;
        case OpCode::NEG: case OpCode::NOT: case OpCode::CTRANSPOSE:
        case OpCode::TRANSPOSE:
            if (next.b == dst) { next.b = src; replaced = true; }
            break;
        case OpCode::INDEX_SET_2D:
            if (next.e == dst) { next.e = src; replaced = true; }
            break;
        default:
            break;
        }
        if (replaced) {
            mv.op = OpCode::NOP; // eliminate the MOVE
        }
    }

    // Strip NOPs (adjust jump offsets accordingly)
    return; // TODO: NOP stripping has jump patching bugs, skip for now
    std::vector<size_t> newIndex(n); // old pos → new pos
    size_t out = 0;
    for (size_t i = 0; i < n; ++i) {
        newIndex[i] = out;
        if (code[i].op != OpCode::NOP)
            ++out;
    }
    // Don't strip if no NOPs were removed (avoid messing with source maps)
    if (out == n)
        return;

    // Patch jump offsets: JMP, JMP_TRUE, JMP_FALSE, FOR_INIT, FOR_NEXT
    for (size_t i = 0; i < n; ++i) {
        auto &ins = code[i];
        if (ins.op == OpCode::NOP)
            continue;
        int16_t off = ins.d;
        switch (ins.op) {
        case OpCode::JMP:
        case OpCode::JMP_TRUE:
        case OpCode::JMP_FALSE:
        case OpCode::FOR_INIT:
        case OpCode::TRY_BEGIN: {
            // d = relative offset from current instruction
            size_t target = static_cast<size_t>(static_cast<int>(i) + off);
            ins.d = static_cast<int16_t>(static_cast<int>(newIndex[target])
                                         - static_cast<int>(newIndex[i]));
            break;
        }
        case OpCode::FOR_NEXT: {
            size_t target = static_cast<size_t>(static_cast<int>(i) + off);
            ins.d = static_cast<int16_t>(static_cast<int>(newIndex[target])
                                         - static_cast<int>(newIndex[i]));
            break;
        }
        default:
            break;
        }
    }

    // Compact code and source map
    std::vector<Instruction> newCode;
    std::vector<SourceLoc> newSourceMap;
    newCode.reserve(out);
    newSourceMap.reserve(out);
    for (size_t i = 0; i < n; ++i) {
        if (code[i].op != OpCode::NOP) {
            newCode.push_back(code[i]);
            if (i < chunk_.sourceMap.size())
                newSourceMap.push_back(chunk_.sourceMap[i]);
        }
    }
    code = std::move(newCode);
    chunk_.sourceMap = std::move(newSourceMap);
}

// ============================================================
// Emit helpers
// ============================================================

void Compiler::emit(Instruction instr)
{
    chunk_.code.push_back(instr);
    chunk_.sourceMap.push_back(currentLoc_);
}

void Compiler::emitABC(OpCode op, uint8_t a, uint8_t b, uint8_t c)
{
    emit(Instruction::make_abc(op, a, b, c));
}

void Compiler::emitAB(OpCode op, uint8_t a, uint8_t b)
{
    emit(Instruction::make_abc(op, a, b, 0));
}

void Compiler::emitAD(OpCode op, uint8_t a, int16_t d)
{
    emit(Instruction::make_ad(op, a, d));
}

void Compiler::emitA(OpCode op, uint8_t a)
{
    emit(Instruction::make_a(op, a));
}

void Compiler::emitD(OpCode op, int16_t d)
{
    emit(Instruction::make_d(op, d));
}

void Compiler::emitNone(OpCode op)
{
    emit(Instruction::make_none(op));
}

size_t Compiler::currentPos() const
{
    return chunk_.code.size();
}

void Compiler::patchJump(size_t instrPos, int16_t offset)
{
    chunk_.code[instrPos].d = offset;
}

// ============================================================
// Constant pool
// ============================================================

int16_t Compiler::addConstant(double value)
{
    // Check if constant already exists
    for (size_t i = 0; i < chunk_.constants.size(); ++i) {
        if (chunk_.constants[i].isScalar() && chunk_.constants[i].type() == MType::DOUBLE
            && chunk_.constants[i].toScalar() == value) {
            return static_cast<int16_t>(i);
        }
    }
    int16_t idx = static_cast<int16_t>(chunk_.constants.size());
    chunk_.constants.push_back(MValue::scalar(value, &engine_.allocator_));
    return idx;
}

int16_t Compiler::addStringConstant(const std::string &s)
{
    for (size_t i = 0; i < chunk_.strings.size(); ++i) {
        if (chunk_.strings[i] == s)
            return static_cast<int16_t>(i);
    }
    int16_t idx = static_cast<int16_t>(chunk_.strings.size());
    chunk_.strings.push_back(s);
    return idx;
}

// ============================================================
// AST compilation
// ============================================================

uint8_t Compiler::compileNode(const ASTNode *node)
{
    if (!node)
        return 0;

    // Update current source location from AST node (if it has valid line info)
    if (node->line > 0) {
        currentLoc_.line = static_cast<uint16_t>(node->line);
        currentLoc_.col = static_cast<uint16_t>(node->col);
    }

    switch (node->type) {
    case NodeType::BLOCK:
        return compileBlock(node);
    case NodeType::NUMBER_LITERAL:
        return compileNumber(node);
    case NodeType::IMAG_LITERAL: {
        // 3i → complexScalar(0, 3)
        uint8_t dst = tempReg();
        int16_t idx = static_cast<int16_t>(chunk_.constants.size());
        chunk_.constants.push_back(MValue::complexScalar(0.0, node->numValue, nullptr));
        emitAD(OpCode::LOAD_CONST, dst, idx);
        return dst;
    }
    case NodeType::STRING_LITERAL:
        return compileString(node);
    case NodeType::DQSTRING_LITERAL: {
        uint8_t dst = tempReg();
        int16_t idx = static_cast<int16_t>(chunk_.constants.size());
        chunk_.constants.push_back(MValue::stringScalar(node->strValue, nullptr));
        emitAD(OpCode::LOAD_CONST, dst, idx);
        return dst;
    }
    case NodeType::BOOL_LITERAL:
        return compileBool(node);
    case NodeType::IDENTIFIER:
        return compileIdentifier(node);
    case NodeType::ASSIGN:
        return compileAssign(node);
    case NodeType::MULTI_ASSIGN:
        return compileMultiAssign(node);
    case NodeType::BINARY_OP:
        return compileBinaryOp(node);
    case NodeType::UNARY_OP:
        return compileUnaryOp(node);
    case NodeType::EXPR_STMT:
        return compileExprStmt(node);
    case NodeType::IF_STMT:
        return compileIf(node);
    case NodeType::SWITCH_STMT:
        return compileSwitch(node);
    case NodeType::TRY_STMT:
        return compileTryCatch(node);
    case NodeType::GLOBAL_STMT:
    case NodeType::PERSISTENT_STMT:
        return compileGlobalPersistent(node);
    case NodeType::FIELD_ACCESS:
        return compileFieldAccess(node);
    case NodeType::DYNAMIC_FIELD_ACCESS: {
        // s.(expr) — child[0]=obj, child[1]=field name expr
        uint8_t obj = compileNode(node->children[0].get());
        uint8_t nameReg = compileNode(node->children[1].get());
        uint8_t dst = tempReg();
        emitABC(OpCode::FIELD_GET_DYN, dst, obj, nameReg);
        return dst;
    }
    case NodeType::CELL_INDEX:
        return compileCellIndex(node);
    case NodeType::CELL_LITERAL:
        return compileCellLiteral(node);
    case NodeType::ANON_FUNC:
        return compileAnonFunc(node);
    case NodeType::WHILE_STMT:
        return compileWhile(node);
    case NodeType::BREAK_STMT:
        return compileBreak(node);
    case NodeType::CONTINUE_STMT:
        return compileContinue(node);
    case NodeType::FOR_STMT:
        return compileFor(node);
    case NodeType::COLON_EXPR:
        return compileColonExpr(node);
    case NodeType::MATRIX_LITERAL:
        return compileMatrixLiteral(node);
    case NodeType::INDEX:
        return compileIndexExpr(node);
    case NodeType::CALL:
        return compileCall(node);
    case NodeType::COMMAND_CALL:
        return compileCommandCall(node);
    case NodeType::FUNCTION_DEF:
        return compileFunctionDef(node);
    case NodeType::RETURN_STMT:
        return compileReturn(node);
    case NodeType::END_VAL: {
        // 'end' in indexing context — emit LOAD_END
        // a=dst, b=arrReg, c=dim, d=ndims
        uint8_t dst = tempReg();
        emit(Instruction::make_abcde(OpCode::LOAD_END,
                                     dst,
                                     indexContextArr_,
                                     indexContextDim_,
                                     static_cast<int16_t>(indexContextNdims_),
                                     0));
        return dst;
    }
    case NodeType::DELETE_ASSIGN:
        return compileDeleteAssign(node);
    default:
        throw std::runtime_error("Compiler: unsupported node type "
                                 + std::to_string(static_cast<int>(node->type)));
    }
}

uint8_t Compiler::compileBlock(const ASTNode *node)
{
    uint8_t last = 0;
    for (auto &child : node->children) {
        last = compileNode(child.get());
    }
    return last;
}

uint8_t Compiler::compileNumber(const ASTNode *node)
{
    int16_t idx = addConstant(node->numValue);
    // Reuse register if this constant was already loaded
    auto it = constRegCache_.find(idx);
    if (it != constRegCache_.end()) {
        scalarRegs_.set(it->second);
        return it->second;
    }
    uint8_t dst = tempReg();
    emitAD(OpCode::LOAD_CONST, dst, idx);
    constRegCache_[idx] = dst;
    scalarRegs_.set(dst);
    return dst;
}

uint8_t Compiler::compileString(const ASTNode *node)
{
    uint8_t dst = tempReg();
    int16_t idx = addStringConstant(node->strValue);
    emitAD(OpCode::LOAD_STRING, dst, idx);
    return dst;
}

uint8_t Compiler::compileBool(const ASTNode *node)
{
    uint8_t dst = tempReg();
    // Store as logical (tag-based, no heap allocation) for islogical() correctness
    int16_t idx = static_cast<int16_t>(chunk_.constants.size());
    chunk_.constants.push_back(MValue::logicalScalar(node->boolValue, nullptr));
    emitAD(OpCode::LOAD_CONST, dst, idx);
    return dst;
}

uint8_t Compiler::compileIdentifier(const ASTNode *node)
{
    return varRegRead(node->strValue);
}

uint8_t Compiler::compileAssign(const ASTNode *node)
{
    auto *lhs = node->children[0].get();
    auto *rhs = node->children[1].get();

    if (lhs->type == NodeType::IDENTIFIER) {
        uint8_t src = compileNode(rhs);

        // MOVE elimination: redirect last instruction to write directly to var register
        auto varIt = varRegisters_.find(lhs->strValue);
        bool canEliminate = (src == nextReg_ - 1 && !chunk_.code.empty()
                             && chunk_.code.back().a == src);
        if (canEliminate) {
            OpCode lastOp = chunk_.code.back().op;
            // Only redirect arithmetic/builtin ops (not LOAD_CONST — constRegCache)
            canEliminate = (lastOp >= OpCode::ADD && lastOp <= OpCode::UPLUS)
                           || (lastOp >= OpCode::EMUL && lastOp <= OpCode::EPOW)
                           || (lastOp >= OpCode::ADD_SS && lastOp <= OpCode::NEG_S)
                           || lastOp == OpCode::CALL_BUILTIN;
        }

        uint8_t dst;
        if (canEliminate && varIt == varRegisters_.end()) {
            // First assignment: adopt temp register as the variable
            varRegisters_[lhs->strValue] = src;
            dst = src;
            // No instruction emitted, no reclaim needed — src IS the variable now
        } else if (canEliminate && varIt != varRegisters_.end()) {
            // Existing variable: redirect last instruction to write to var register
            dst = varIt->second;
            chunk_.code.back().a = dst;
            nextReg_--;  // reclaim temp
            scalarRegs_.reset(src);
        } else {
            dst = varReg(lhs->strValue);
            if (src != dst)
                emitAB(OpCode::MOVE, dst, src);
        }
        // Propagate scalar tracking through assignment
        if (scalarRegs_.test(src))
            scalarRegs_.set(dst);
        else
            scalarRegs_.reset(dst);

        // Display if no semicolon
        if (!node->suppressOutput) {
            int16_t nameIdx = addStringConstant(lhs->strValue);
            emitAD(OpCode::DISPLAY, dst, nameIdx);
        }
        return dst;
    }

    if (lhs->type == NodeType::INDEX || lhs->type == NodeType::CALL) {
        return compileIndexAssign(node);
    }

    if (lhs->type == NodeType::FIELD_ACCESS) {
        return compileFieldAssign(node);
    }

    if (lhs->type == NodeType::DYNAMIC_FIELD_ACCESS) {
        // s.(expr) = val — child[0]=obj, child[1]=field name expr
        auto *objNode = lhs->children[0].get();
        if (objNode->type != NodeType::IDENTIFIER)
            throw std::runtime_error("Compiler: dynamic field assign requires identifier target");
        uint8_t obj = varReg(objNode->strValue);
        uint8_t val = compileNode(node->children[1].get());
        uint8_t nameReg = compileNode(lhs->children[1].get());
        emitABC(OpCode::FIELD_SET_DYN, obj, nameReg, val);
        if (!node->suppressOutput) {
            int16_t dispIdx = addStringConstant(objNode->strValue);
            emitAD(OpCode::DISPLAY, obj, dispIdx);
        }
        return obj;
    }

    if (lhs->type == NodeType::CELL_INDEX) {
        return compileCellAssign(node);
    }

    throw std::runtime_error("Compiler: unsupported assignment target");
}

uint8_t Compiler::compileMultiAssign(const ASTNode *node)
{
    // [a, b, c] = func(args)  OR  [a, b] = c{idx}
    // node->returnNames = ["a", "b", "c"]  (~ means ignore)
    // node->children[0] = CALL or CELL_INDEX node
    auto *rhsNode = node->children[0].get();
    size_t nout = node->returnNames.size();

    // Allocate destination registers for outputs
    std::vector<uint8_t> outRegs;
    for (auto &name : node->returnNames) {
        if (name == "~")
            outRegs.push_back(tempReg()); // throwaway
        else
            outRegs.push_back(varReg(name));
    }

    // Cell CSL: [a, b] = c{idx}
    if (rhsNode->type == NodeType::CELL_INDEX) {
        uint8_t cellReg = compileNode(rhsNode->children[0].get());
        uint8_t idxReg = compileNode(rhsNode->children[1].get());

        uint8_t outBase = nextReg_;
        for (size_t i = 0; i < nout; ++i)
            tempReg();

        emit(Instruction::make_abcde(OpCode::CELL_GET_MULTI,
                                     outBase, cellReg, idxReg, 0,
                                     static_cast<uint8_t>(nout)));

        for (size_t i = 0; i < nout; ++i)
            if (outRegs[i] != outBase + i)
                emitAB(OpCode::MOVE, outRegs[i], outBase + static_cast<uint8_t>(i));

        if (!node->suppressOutput) {
            for (size_t i = 0; i < node->returnNames.size(); ++i)
                if (node->returnNames[i] != "~") {
                    int16_t nameIdx = addStringConstant(node->returnNames[i]);
                    emitAD(OpCode::DISPLAY, outRegs[i], nameIdx);
                }
        }
        return outRegs.empty() ? 0 : outRegs[0];
    }

    auto *callNode = rhsNode;

    // Compile call arguments
    std::vector<uint8_t> argRegs;
    for (size_t i = 1; i < callNode->children.size(); ++i)
        argRegs.push_back(compileNode(callNode->children[i].get()));

    uint8_t argBase = nextReg_;
    for (size_t i = 0; i < argRegs.size(); ++i) {
        uint8_t slot = tempReg();
        if (argRegs[i] != slot)
            emitAB(OpCode::MOVE, slot, argRegs[i]);
    }

    // Allocate consecutive registers for output
    uint8_t outBase = nextReg_;
    for (size_t i = 0; i < nout; ++i)
        tempReg();

    const std::string &funcName = callNode->children[0]->strValue;
    int16_t funcIdx = addStringConstant(funcName);

    // CALL_MULTI: a=outBase, d=funcIdx, b=argBase, c=nargs, e=nout
    emit(Instruction::make_abcde(OpCode::CALL_MULTI,
                                 outBase,
                                 argBase,
                                 static_cast<uint8_t>(argRegs.size()),
                                 funcIdx,
                                 static_cast<uint8_t>(nout)));

    // Move outputs to destination registers
    for (size_t i = 0; i < nout; ++i) {
        if (outRegs[i] != outBase + i)
            emitAB(OpCode::MOVE, outRegs[i], outBase + static_cast<uint8_t>(i));
    }

    // Display if no semicolon
    if (!node->suppressOutput) {
        for (size_t i = 0; i < node->returnNames.size(); ++i) {
            if (node->returnNames[i] != "~") {
                int16_t nameIdx = addStringConstant(node->returnNames[i]);
                emitAD(OpCode::DISPLAY, outRegs[i], nameIdx);
            }
        }
    }

    return outRegs.empty() ? 0 : outRegs[0];
}

uint8_t Compiler::compileBinaryOp(const ASTNode *node)
{
    const std::string &op = node->strValue;

    // Short-circuit: && and || need special handling
    // (right operand should not be evaluated if left decides result)
    if (op == "&&") {
        uint8_t dst = tempReg();
        uint8_t left = compileNode(node->children[0].get());
        emitAB(OpCode::MOVE, dst, left);
        size_t jumpPos = currentPos();
        emitAD(OpCode::JMP_FALSE, dst, 0); // placeholder
        constRegCache_.clear(); scalarRegs_.reset(); // right operand may be skipped
        uint8_t right = compileNode(node->children[1].get());
        emitAB(OpCode::MOVE, dst, right);
        patchJump(jumpPos, static_cast<int16_t>(currentPos() - jumpPos));
        constRegCache_.clear(); scalarRegs_.reset(); // constants from skipped branch must not leak
        return dst;
    }
    if (op == "||") {
        uint8_t dst = tempReg();
        uint8_t left = compileNode(node->children[0].get());
        emitAB(OpCode::MOVE, dst, left);
        size_t jumpPos = currentPos();
        emitAD(OpCode::JMP_TRUE, dst, 0); // placeholder
        constRegCache_.clear(); scalarRegs_.reset(); // right operand may be skipped
        uint8_t right = compileNode(node->children[1].get());
        emitAB(OpCode::MOVE, dst, right);
        patchJump(jumpPos, static_cast<int16_t>(currentPos() - jumpPos));
        constRegCache_.clear(); scalarRegs_.reset(); // constants from skipped branch must not leak
        return dst;
    }

    // Constant folding: both operands are number literals → compute at compile time
    if (node->children[0]->type == NodeType::NUMBER_LITERAL
        && node->children[1]->type == NodeType::NUMBER_LITERAL) {
        double lv = node->children[0]->numValue;
        double rv = node->children[1]->numValue;
        double result;
        bool folded = true;
        if (op == "+")       result = lv + rv;
        else if (op == "-")  result = lv - rv;
        else if (op == "*")  result = lv * rv;
        else if (op == "/")  result = lv / rv;
        else if (op == "^")  result = std::pow(lv, rv);
        else if (op == ".*") result = lv * rv;
        else if (op == "./") result = lv / rv;
        else if (op == ".^") result = std::pow(lv, rv);
        else folded = false;
        if (folded) {
            int16_t idx = addConstant(result);
            auto it = constRegCache_.find(idx);
            if (it != constRegCache_.end()) {
                scalarRegs_.set(it->second);
                return it->second;
            }
            uint8_t dst = tempReg();
            emitAD(OpCode::LOAD_CONST, dst, idx);
            constRegCache_[idx] = dst;
            scalarRegs_.set(dst);
            return dst;
        }
    }

    uint8_t left = compileNode(node->children[0].get());
    uint8_t right = compileNode(node->children[1].get());
    uint8_t dst = tempReg();
    bool bothScalar = scalarRegs_.test(left) && scalarRegs_.test(right);

    OpCode opcode;
    if (op == "+")
        opcode = bothScalar ? OpCode::ADD_SS : OpCode::ADD;
    else if (op == "-")
        opcode = bothScalar ? OpCode::SUB_SS : OpCode::SUB;
    else if (op == "*")
        opcode = bothScalar ? OpCode::MUL_SS : OpCode::MUL;
    else if (op == "/")
        opcode = bothScalar ? OpCode::RDIV_SS : OpCode::RDIV;
    else if (op == "\\")
        opcode = OpCode::LDIV;
    else if (op == "^")
        opcode = bothScalar ? OpCode::POW_SS : OpCode::POW;
    else if (op == ".*")
        opcode = bothScalar ? OpCode::MUL_SS : OpCode::EMUL;
    else if (op == "./")
        opcode = bothScalar ? OpCode::RDIV_SS : OpCode::ERDIV;
    else if (op == ".\\")
        opcode = OpCode::ELDIV;
    else if (op == ".^")
        opcode = bothScalar ? OpCode::POW_SS : OpCode::EPOW;
    else if (op == "==")
        opcode = OpCode::EQ;
    else if (op == "~=")
        opcode = OpCode::NE;
    else if (op == "<")
        opcode = OpCode::LT;
    else if (op == ">")
        opcode = OpCode::GT;
    else if (op == "<=")
        opcode = OpCode::LE;
    else if (op == ">=")
        opcode = OpCode::GE;
    else if (op == "&")
        opcode = OpCode::AND;
    else if (op == "|")
        opcode = OpCode::OR;
    else
        throw std::runtime_error("Compiler: unsupported binary operator '" + op + "'");

    emitABC(opcode, dst, left, right);
    // Scalar-specialized ops produce scalar results
    if (bothScalar && (opcode == OpCode::ADD_SS || opcode == OpCode::SUB_SS
                       || opcode == OpCode::MUL_SS || opcode == OpCode::RDIV_SS
                       || opcode == OpCode::POW_SS))
        scalarRegs_.set(dst);
    return dst;
}

uint8_t Compiler::compileUnaryOp(const ASTNode *node)
{
    const std::string &op = node->strValue;

    // Constant folding: -<number> → load negated constant directly
    if (op == "-" && node->children[0]->type == NodeType::NUMBER_LITERAL) {
        int16_t idx = addConstant(-node->children[0]->numValue);
        auto it = constRegCache_.find(idx);
        if (it != constRegCache_.end()) {
            scalarRegs_.set(it->second);
            return it->second;
        }
        uint8_t dst = tempReg();
        emitAD(OpCode::LOAD_CONST, dst, idx);
        constRegCache_[idx] = dst;
        scalarRegs_.set(dst);
        return dst;
    }

    uint8_t src = compileNode(node->children[0].get());
    uint8_t dst = tempReg();

    if (op == "-") {
        if (scalarRegs_.test(src)) {
            emitAB(OpCode::NEG_S, dst, src);
            scalarRegs_.set(dst);
        } else {
            emitAB(OpCode::NEG, dst, src);
        }
    } else if (op == "+")
        emitAB(OpCode::UPLUS, dst, src);
    else if (op == "~")
        emitAB(OpCode::NOT, dst, src);
    else if (op == "'")
        emitAB(OpCode::CTRANSPOSE, dst, src);
    else if (op == ".'")
        emitAB(OpCode::TRANSPOSE, dst, src);
    else
        throw std::runtime_error("Compiler: unsupported unary operator '" + op + "'");

    return dst;
}

uint8_t Compiler::compileExprStmt(const ASTNode *node)
{
    auto *child = node->children[0].get();

    // Expression statements are nargout=0 context — functions should
    // not produce output values (tic, figure, fprintf, etc.)
    struct NargoutGuard
    {
        uint8_t &ref;
        uint8_t saved;
        NargoutGuard(uint8_t &r, uint8_t v)
            : ref(r)
            , saved(r)
        {
            ref = v;
        }
        ~NargoutGuard() { ref = saved; }
    } nargoutGuard(nargoutContext_, 0);

    // ── Bare zero-arg function call ──────────────────────────────
    // When the parser sees `clear` or `figure` alone on a line (followed
    // by a terminator), it produces EXPR_STMT → IDENTIFIER.  The parser's
    // isCommandStyleCall() only fires when there IS a following argument
    // token.  So bare no-arg commands land here.
    //
    // If the identifier is NOT a known variable but IS a registered
    // function (external or user-defined), compile it as CALL with 0 args
    // instead of treating it as a variable read.
    if (child->type == NodeType::IDENTIFIER) {
        const std::string &name = child->strValue;
        bool isKnownVar = varRegisters_.count(name) > 0;

        // Also check workspaceEnv for variables from previous eval() calls
        if (!isKnownVar && isTopLevel_ && !kBuiltinNames.count(name)) {
            MValue *existing = engine_.getVariable(name);
            if (existing && !existing->isEmpty())
                isKnownVar = true;
        }

        if (!isKnownVar && (engine_.externalFuncs_.count(name) || engine_.hasFunction(name))) {
            // Workspace builtins inside functions → opcodes
            if (!isTopLevel_) {
                if (name == "who") {
                    emitAB(OpCode::WHO, 0, 0);
                    return tempReg();
                }
                if (name == "whos") {
                    emitAB(OpCode::WHOS, 0, 0);
                    return tempReg();
                }
                if (name == "clear") {
                    // clear (no args) inside function — clear all locals
                    for (auto &[vname, reg] : varRegisters_) {
                        if (kBuiltinNames.count(vname) == 0)
                            emitA(OpCode::CLEAR_VAR, reg);
                    }
                    return tempReg();
                }
                if (name == "exist") {
                    // exist (no args) — error, but let it go to CALL
                }
            } else if (name == "clear") {
                // clear (no args) on top-level — clear all registers + call externalFunc
                for (auto &[vname, reg] : varRegisters_) {
                    if (kBuiltinNames.count(vname) == 0)
                        emitA(OpCode::CLEAR_VAR, reg);
                }
            }

            // Compile as zero-arg CALL (nargout=0 for statement context)
            uint8_t argBase = nextReg_;
            uint8_t dst = tempReg();
            int16_t funcIdx = addStringConstant(name);
            emit(Instruction::make_abcde(OpCode::CALL, dst, argBase, 0, funcIdx, 0));

            if (!node->suppressOutput) {
                int16_t nameIdx = addStringConstant("ans");
                emitAD(OpCode::DISPLAY, dst, nameIdx);
            }
            return dst;
        }
    }

    uint8_t reg = compileNode(child);

    // Display if no semicolon
    if (!node->suppressOutput) {
        int16_t nameIdx = addStringConstant("ans");
        emitAD(OpCode::DISPLAY, reg, nameIdx);
    }
    return reg;
}

// ============================================================
// Phase 2: Control flow
// ============================================================

uint8_t Compiler::compileIf(const ASTNode *node)
{
    constRegCache_.clear(); scalarRegs_.reset(); // branches may skip LOAD_CONST
    // branches: vector<pair<condition, body>>
    // elseBranch: optional else body
    //
    // Compiled as:
    //   eval cond1
    //   JMP_FALSE cond1 → L_next1
    //   body1
    //   JMP → L_end
    // L_next1:
    //   eval cond2
    //   JMP_FALSE cond2 → L_next2
    //   body2
    //   JMP → L_end
    // L_next2:
    //   else body
    // L_end:

    std::vector<size_t> endJumps; // positions of JMP → L_end

    for (size_t i = 0; i < node->branches.size(); ++i) {
        auto &[cond, body] = node->branches[i];

        // Clear cache: previous branch body may have been skipped
        constRegCache_.clear(); scalarRegs_.reset();
        // Compile condition
        uint8_t condReg = compileNode(cond.get());

        // JMP_FALSE to next branch (placeholder)
        size_t skipPos = currentPos();
        emitAD(OpCode::JMP_FALSE, condReg, 0);

        // Compile body
        compileNode(body.get());

        // JMP to end (skip remaining branches) — unless this is the last branch with no else
        if (i < node->branches.size() - 1 || node->elseBranch) {
            endJumps.push_back(currentPos());
            emitD(OpCode::JMP, 0); // placeholder
        }

        // Patch the JMP_FALSE to jump here (next branch)
        patchJump(skipPos, static_cast<int16_t>(currentPos() - skipPos));
    }

    // Compile else branch if present
    if (node->elseBranch) {
        constRegCache_.clear(); scalarRegs_.reset();
        compileNode(node->elseBranch.get());
    }

    // Emit NOP for the 'end' keyword so breakpoints on it fire
    size_t endNopPos = currentPos();
    if (node->endLine > 0) {
        currentLoc_ = {static_cast<uint16_t>(node->endLine), 1};
        emitNone(OpCode::NOP);
    }

    // Patch all end jumps to the NOP (all branches converge here)
    for (size_t pos : endJumps) {
        patchJump(pos, static_cast<int16_t>(endNopPos - pos));
    }

    constRegCache_.clear(); scalarRegs_.reset(); // don't know which branch ran
    return 0;
}

// ============================================================
// Switch/case
// ============================================================
uint8_t Compiler::compileSwitch(const ASTNode *node)
{
    constRegCache_.clear(); scalarRegs_.reset();
    // AST: children[0] = switch expression
    //      branches = vector<pair<case_expr, case_body>>
    //      elseBranch = otherwise body (optional)
    //
    // Compiled as chain of EQ + JMP_FALSE (like if/elseif):
    //   eval switch_val → rSV
    //   eval case1_val → rCV
    //   EQ rCmp, rSV, rCV
    //   JMP_FALSE rCmp → L_next1
    //   body1
    //   JMP → L_end
    // L_next1:
    //   eval case2_val → rCV
    //   EQ rCmp, rSV, rCV
    //   JMP_FALSE rCmp → L_next2
    //   body2
    //   JMP → L_end
    // L_next2:
    //   otherwise body
    // L_end:

    // Compile switch expression
    uint8_t switchReg = compileNode(node->children[0].get());

    // Temp register for comparison result
    uint8_t cmpReg = tempReg();

    std::vector<size_t> endJumps;

    for (size_t i = 0; i < node->branches.size(); ++i) {
        auto &[caseExpr, caseBody] = node->branches[i];

        if (caseExpr->type == NodeType::CELL_LITERAL) {
            // Cell case {val1, val2, ...}: match if switchReg == any element
            // Get cell elements: may be wrapped in a BLOCK child
            const std::vector<ASTNodePtr> *elements = &caseExpr->children;
            if (caseExpr->children.size() == 1 && caseExpr->children[0]->type == NodeType::BLOCK) {
                elements = &caseExpr->children[0]->children;
            }

            // Compile: cmpReg = (sv==el1) | (sv==el2) | ...
            uint8_t tmpCmp = tempReg();
            // Start with false
            int16_t zeroIdx = addConstant(0.0);
            emitAD(OpCode::LOAD_CONST, cmpReg, zeroIdx);

            for (size_t ei = 0; ei < elements->size(); ++ei) {
                uint8_t elReg = compileNode((*elements)[ei].get());
                emitABC(OpCode::EQ, tmpCmp, switchReg, elReg);
                emitABC(OpCode::OR, cmpReg, cmpReg, tmpCmp);
            }
        } else {
            // Simple case: scalar comparison
            uint8_t caseReg = compileNode(caseExpr.get());
            emitABC(OpCode::EQ, cmpReg, switchReg, caseReg);
        }

        // JMP_FALSE to next case
        size_t skipPos = currentPos();
        emitAD(OpCode::JMP_FALSE, cmpReg, 0);

        // Compile case body
        compileNode(caseBody.get());

        // JMP to end
        if (i < node->branches.size() - 1 || node->elseBranch) {
            endJumps.push_back(currentPos());
            emitD(OpCode::JMP, 0);
        }

        // Patch JMP_FALSE
        patchJump(skipPos, static_cast<int16_t>(currentPos() - skipPos));
    }

    // Compile otherwise
    if (node->elseBranch) {
        compileNode(node->elseBranch.get());
    }

    // Emit NOP for the 'end' keyword so breakpoints on it fire
    size_t endNopPos = currentPos();
    if (node->endLine > 0) {
        currentLoc_ = {static_cast<uint16_t>(node->endLine), 1};
        emitNone(OpCode::NOP);
    }

    // Patch end jumps
    for (size_t pos : endJumps) {
        patchJump(pos, static_cast<int16_t>(endNopPos - pos));
    }

    return 0;
}

// ============================================================
// Global / Persistent
// ============================================================
uint8_t Compiler::compileGlobalPersistent(const ASTNode *node)
{
    for (auto &name : node->paramNames) {
        varReg(name); // allocate register
        chunk_.globalNames.push_back(name);
    }
    return 0;
}

// ============================================================
// Try/catch
// ============================================================
uint8_t Compiler::compileTryCatch(const ASTNode *node)
{
    constRegCache_.clear(); scalarRegs_.reset();
    // AST: children[0] = try body
    //      children[1] = catch body (optional)
    //      strValue = catch variable name (e.g. "e")
    //
    // Compiled as:
    //   TRY_BEGIN catchOffset, exReg
    //   ... try body ...
    //   TRY_END
    //   JMP → L_end
    // L_catch:
    //   ... catch body ...
    // L_end:

    // Register for exception variable (if named)
    uint8_t exReg = 0;
    if (!node->strValue.empty()) {
        exReg = varReg(node->strValue);
    } else {
        exReg = tempReg();
    }

    // TRY_BEGIN: d = catchOffset (placeholder), a = exReg
    size_t tryBeginPos = currentPos();
    emitAD(OpCode::TRY_BEGIN, exReg, 0);

    // Compile try body
    compileNode(node->children[0].get());

    // TRY_END
    emitA(OpCode::TRY_END, 0);

    // JMP past catch body
    size_t endJmpPos = currentPos();
    emitD(OpCode::JMP, 0);

    // Patch TRY_BEGIN to jump here on exception
    patchJump(tryBeginPos, static_cast<int16_t>(currentPos() - tryBeginPos));

    // Compile catch body
    if (node->children.size() > 1) {
        compileNode(node->children[1].get());
    }

    // Emit NOP for the 'end' keyword so breakpoints on it fire
    size_t endNopPos = currentPos();
    if (node->endLine > 0) {
        currentLoc_ = {static_cast<uint16_t>(node->endLine), 1};
        emitNone(OpCode::NOP);
    }

    // Patch JMP to end (land on NOP)
    patchJump(endJmpPos, static_cast<int16_t>(endNopPos - endJmpPos));

    return 0;
}

uint8_t Compiler::compileWhile(const ASTNode *node)
{
    constRegCache_.clear(); scalarRegs_.reset();
    // children[0] = condition, children[1] = body
    //
    // L_start:
    //   eval condition
    //   JMP_FALSE → L_end
    //   body
    //   JMP → L_start
    // L_end:

    loopStack_.push_back(LoopContext{});

    size_t loopStart = currentPos();

    // Compile condition
    uint8_t condReg = compileNode(node->children[0].get());

    // JMP_FALSE to end (placeholder)
    size_t exitPos = currentPos();
    emitAD(OpCode::JMP_FALSE, condReg, 0);

    // Compile body
    compileNode(node->children[1].get());

    // Patch continue targets → jump to loopStart (re-evaluate condition)
    for (size_t pos : loopStack_.back().continuePatches) {
        patchJump(pos, static_cast<int16_t>(loopStart - pos));
    }

    // Emit source location for the 'end' keyword so breakpoints on it fire
    if (node->endLine > 0)
        currentLoc_ = {static_cast<uint16_t>(node->endLine), 1};

    // JMP back to start
    emitD(OpCode::JMP, static_cast<int16_t>(loopStart - currentPos()));

    // Patch JMP_FALSE exit
    patchJump(exitPos, static_cast<int16_t>(currentPos() - exitPos));

    // Patch break targets → jump to here (after loop)
    for (size_t pos : loopStack_.back().breakPatches) {
        patchJump(pos, static_cast<int16_t>(currentPos() - pos));
    }

    loopStack_.pop_back();
    constRegCache_.clear(); scalarRegs_.reset(); // body constants may not have executed
    return 0;
}

uint8_t Compiler::compileBreak(const ASTNode * /*node*/)
{
    if (loopStack_.empty())
        throw std::runtime_error("Compiler: break outside of loop");

    // For for-loops, need to pop forStack_ before jumping out.
    // Count how many for-loop levels we need to pop (usually 1 — the innermost)
    if (loopStack_.back().isForLoop) {
        // Emit FOR_NEXT with the loop's variable that will force-exit.
        // Actually, simpler: emit a NOP that VM interprets as forStack pop.
        // Use TRY_END as a safe NOP-like... no.
        // Just emit a JMP to FOR_NEXT instead of past it.
        // The FOR_NEXT will pop forStack when index >= count.
        // But we need to set index = count first...

        // Simplest: jump to a landing pad that pops forStack then jumps to end.
        // We'll patch both: first a forStack pop placeholder, then JMP.
        emitA(OpCode::NOP, 1); // NOP with a=1 means "pop forStack" — VM convention
    }

    loopStack_.back().breakPatches.push_back(currentPos());
    emitD(OpCode::JMP, 0); // placeholder — patched when loop ends
    return 0;
}

uint8_t Compiler::compileContinue(const ASTNode * /*node*/)
{
    if (loopStack_.empty())
        throw std::runtime_error("Compiler: continue outside of loop");
    loopStack_.back().continuePatches.push_back(currentPos());
    emitD(OpCode::JMP, 0); // placeholder — patched when loop ends
    return 0;
}

// ============================================================
// Phase 3: For-loop, colon, arrays, indexing
// ============================================================

uint8_t Compiler::compileFor(const ASTNode *node)
{
    // node->strValue = loop variable name
    // children[0] = range expression
    // children[1] = body
    //
    // Compiled as:
    //   eval range → rangeReg
    //   FOR_INIT varReg, rangeReg, endOffset(placeholder)
    // L_body:
    //   body
    // L_continue:
    //   FOR_NEXT varReg, backOffset  (jumps to L_body or falls through)
    // L_end:

    uint8_t rangeReg = compileNode(node->children[0].get());
    uint8_t vReg = varReg(node->strValue);

    loopStack_.push_back(LoopContext{{}, {}, true});

    size_t forInitPos = currentPos();
    // FOR_INIT: a=varReg, b=rangeReg, d=endOffset(placeholder)
    emit(Instruction::make_abd(OpCode::FOR_INIT, vReg, rangeReg, 0));
    // Mark loop variable as scalar only when range is a numeric colon expression
    // (logical/char arrays produce non-double-scalar loop values)
    if (node->children[0]->type == NodeType::COLON_EXPR)
        scalarRegs_.set(vReg);

    size_t bodyStart = currentPos();

    // Compile body
    compileNode(node->children[1].get());

    // Patch continue targets → jump to FOR_NEXT
    size_t forNextPos = currentPos();
    for (size_t pos : loopStack_.back().continuePatches) {
        patchJump(pos, static_cast<int16_t>(forNextPos - pos));
    }

    // Emit source location for the 'end' keyword so breakpoints on it fire
    if (node->endLine > 0)
        currentLoc_ = {static_cast<uint16_t>(node->endLine), 1};

    // FOR_NEXT: a=varReg, d=backOffset (negative, jumps to bodyStart)
    emitAD(OpCode::FOR_NEXT, vReg, static_cast<int16_t>(bodyStart - (currentPos() + 1)));
    // +1 because offset is relative to the FOR_NEXT instruction itself,
    // but ip will be at FOR_NEXT when it executes. We want ip+d = bodyStart.
    // Actually: ip += d then continue (no ++ip). So d = bodyStart - forNextPos.
    // Let me fix: FOR_NEXT is at forNextPos = currentPos()-1 (just emitted)
    // We want ip = bodyStart, so d = bodyStart - (currentPos()-1)
    // Rewrite:
    chunk_.code.back().d = static_cast<int16_t>(bodyStart - (currentPos() - 1));

    // Patch FOR_INIT endOffset → jump past FOR_NEXT
    patchJump(forInitPos, static_cast<int16_t>(currentPos() - forInitPos));

    // Patch break targets → jump to here (after loop)
    for (size_t pos : loopStack_.back().breakPatches) {
        patchJump(pos, static_cast<int16_t>(currentPos() - pos));
    }

    loopStack_.pop_back();
    constRegCache_.clear(); scalarRegs_.reset(); // body constants may not have executed (empty range)
    return 0;
}

uint8_t Compiler::compileColonExpr(const ASTNode *node)
{
    // No children = bare ":" (colon-all marker)
    if (node->children.empty()) {
        uint8_t dst = tempReg();
        emitA(OpCode::COLON_ALL, dst);
        return dst;
    }

    // 2 children: start:stop
    if (node->children.size() == 2) {
        uint8_t start = compileNode(node->children[0].get());
        uint8_t stop = compileNode(node->children[1].get());
        uint8_t dst = tempReg();
        emitABC(OpCode::COLON, dst, start, stop);
        return dst;
    }

    // 3 children: start:step:stop
    if (node->children.size() == 3) {
        uint8_t start = compileNode(node->children[0].get());
        uint8_t step = compileNode(node->children[1].get());
        uint8_t stop = compileNode(node->children[2].get());
        uint8_t dst = tempReg();
        // COLON3: dst, start, step, stop — need 4 regs
        // Use: a=dst, b=start, c=step, d is not suitable (int16), e=stop
        emit(Instruction::make_abcde(OpCode::COLON3, dst, start, step, 0, stop));
        return dst;
    }

    throw std::runtime_error("Compiler: invalid colon expression");
}

uint8_t Compiler::compileMatrixLiteral(const ASTNode *node)
{
    // node->children = rows
    // Each row's children = elements

    if (node->children.empty()) {
        uint8_t dst = tempReg();
        emitA(OpCode::LOAD_EMPTY, dst);
        return dst;
    }

    // Compile each row into a single register
    std::vector<uint8_t> rowRegs;
    for (auto &row : node->children) {
        if (row->children.empty()) {
            uint8_t dst = tempReg();
            emitA(OpCode::LOAD_EMPTY, dst);
            rowRegs.push_back(dst);
            continue;
        }

        if (row->children.size() == 1) {
            rowRegs.push_back(compileNode(row->children[0].get()));
            continue;
        }

        // Multi-element row: compile each, move into consecutive block, HORZCAT
        std::vector<uint8_t> elemRegs;
        for (auto &elem : row->children) {
            elemRegs.push_back(compileNode(elem.get()));
        }

        uint8_t hBase = nextReg_;
        for (size_t i = 0; i < elemRegs.size(); ++i) {
            uint8_t slot = tempReg();
            if (elemRegs[i] != slot) {
                emitAB(OpCode::MOVE, slot, elemRegs[i]);
            }
        }

        uint8_t dst = tempReg();
        emitABC(OpCode::HORZCAT, dst, hBase, static_cast<uint8_t>(elemRegs.size()));
        rowRegs.push_back(dst);
    }

    if (rowRegs.size() == 1) {
        return rowRegs[0];
    }

    // Multiple rows: move into consecutive block, VERTCAT
    uint8_t vBase = nextReg_;
    for (size_t i = 0; i < rowRegs.size(); ++i) {
        uint8_t slot = tempReg();
        if (rowRegs[i] != slot) {
            emitAB(OpCode::MOVE, slot, rowRegs[i]);
        }
    }

    uint8_t dst = tempReg();
    emitABC(OpCode::VERTCAT, dst, vBase, static_cast<uint8_t>(rowRegs.size()));
    return dst;
}

uint8_t Compiler::compileIndexExpr(const ASTNode *node)
{
    // For CALL nodes: children[0] = base identifier, children[1..] = index args
    // For INDEX nodes: strValue = name, children = index args

    std::string name;
    size_t nargs;
    size_t argOffset; // first index arg in children[]

    if (node->type == NodeType::CALL) {
        name = node->children[0]->strValue;
        nargs = node->children.size() - 1;
        argOffset = 1;
    } else {
        name = node->strValue;
        nargs = node->children.size();
        argOffset = 0;
    }

    uint8_t arr = varReg(name);

    if (nargs == 1) {
        IndexContextGuard guard(*this, arr, 1);
        uint8_t idx = compileNode(node->children[argOffset].get());
        uint8_t dst = tempReg();
        emitABC(OpCode::INDEX_GET, dst, arr, idx);
        return dst;
    }

    if (nargs == 2) {
        IndexContextGuard guard(*this, arr, 2);
        uint8_t row = compileNode(node->children[argOffset].get());
        guard.setDim(1);
        uint8_t col = compileNode(node->children[argOffset + 1].get());
        uint8_t dst = tempReg();
        emit(Instruction::make_abcde(OpCode::INDEX_GET_2D, dst, arr, row, 0, col));
        return dst;
    }

    // ND indexing (3D+): compile indices into consecutive registers
    {
        IndexContextGuard guard(*this, arr, static_cast<uint8_t>(nargs));
        std::vector<uint8_t> idxRegs;
        for (size_t i = 0; i < nargs; ++i) {
            idxRegs.push_back(compileNode(node->children[argOffset + i].get()));
            if (i + 1 < nargs)
                guard.setDim(static_cast<uint8_t>(i + 1));
        }
        uint8_t base = nextReg_;
        for (size_t i = 0; i < nargs; ++i) {
            uint8_t slot = tempReg();
            if (idxRegs[i] != slot)
                emitAB(OpCode::MOVE, slot, idxRegs[i]);
        }
        uint8_t dst = tempReg();
        emit(Instruction::make_abcde(OpCode::INDEX_GET_ND,
                                     dst,
                                     arr,
                                     base,
                                     0,
                                     static_cast<uint8_t>(nargs)));
        return dst;
    }
}

uint8_t Compiler::compileIndexAssign(const ASTNode *node)
{
    // node->children[0] = lhs (CALL or INDEX node)
    // node->children[1] = rhs
    auto *lhs = node->children[0].get();
    auto *rhs = node->children[1].get();

    // For CALL: children[0] = IDENTIFIER, children[1..] = indices
    // For INDEX: strValue = name, children = indices
    std::string name;
    size_t nargs;
    size_t argOffset;

    if (lhs->type == NodeType::CALL) {
        name = lhs->children[0]->strValue;
        nargs = lhs->children.size() - 1;
        argOffset = 1;
    } else {
        name = lhs->strValue;
        nargs = lhs->children.size();
        argOffset = 0;
    }

    uint8_t arr = varReg(name);
    uint8_t val = compileNode(rhs);

    {
        IndexContextGuard guard(*this, arr, static_cast<uint8_t>(nargs));

        if (nargs == 1) {
            uint8_t idx = compileNode(lhs->children[argOffset].get());
            emitABC(OpCode::INDEX_SET, arr, idx, val);
        } else if (nargs == 2) {
            uint8_t row = compileNode(lhs->children[argOffset].get());
            guard.setDim(1);
            uint8_t col = compileNode(lhs->children[argOffset + 1].get());
            emit(Instruction::make_abcde(OpCode::INDEX_SET_2D, arr, row, col, 0, val));
        } else {
            // ND indexed assign (3D+)
            std::vector<uint8_t> idxRegs;
            for (size_t i = 0; i < nargs; ++i) {
                idxRegs.push_back(compileNode(lhs->children[argOffset + i].get()));
                if (i + 1 < nargs)
                    guard.setDim(static_cast<uint8_t>(i + 1));
            }
            uint8_t base = nextReg_;
            for (size_t i = 0; i < nargs; ++i) {
                uint8_t slot = tempReg();
                if (idxRegs[i] != slot)
                    emitAB(OpCode::MOVE, slot, idxRegs[i]);
            }
            uint8_t safeVal = tempReg();
            if (val != safeVal)
                emitAB(OpCode::MOVE, safeVal, val);
            emit(Instruction::make_abcde(OpCode::INDEX_SET_ND,
                                         arr,
                                         base,
                                         static_cast<uint8_t>(nargs),
                                         0,
                                         safeVal));
        }
    } // guard restores index context

    if (!node->suppressOutput) {
        int16_t nameIdx = addStringConstant(name);
        emitAD(OpCode::DISPLAY, arr, nameIdx);
    }

    return arr;
}

// ============================================================
// Struct field access and assignment
// ============================================================

uint8_t Compiler::compileFieldAccess(const ASTNode *node)
{
    // node->children[0] = object expression
    // node->strValue = field name
    uint8_t obj = compileNode(node->children[0].get());
    uint8_t dst = tempReg();
    int16_t nameIdx = addStringConstant(node->strValue);
    emitABC(OpCode::FIELD_GET, dst, obj, 0);
    // Store nameIdx in d field
    chunk_.code.back().d = nameIdx;
    return dst;
}

uint8_t Compiler::compileFieldAssign(const ASTNode *node)
{
    auto *lhs = node->children[0].get(); // FIELD_ACCESS node
    auto *rhs = node->children[1].get();

    // Collect the field chain: s.a.b → ["b", "a"] (outermost first)
    // Walk from outermost FIELD_ACCESS inward to find root IDENTIFIER
    std::vector<std::string> fieldChain;
    const ASTNode *cur = lhs;
    while (cur->type == NodeType::FIELD_ACCESS) {
        fieldChain.push_back(cur->strValue);
        cur = cur->children[0].get();
    }
    if (cur->type != NodeType::IDENTIFIER)
        throw std::runtime_error("Compiler: invalid nested field assign target");

    // fieldChain is in reverse: ["b", "a"] for s.a.b
    // Root variable
    uint8_t rootReg = varReg(cur->strValue);
    uint8_t val = compileNode(rhs);

    if (fieldChain.size() == 1) {
        // Simple: s.x = val
        int16_t nameIdx = addStringConstant(fieldChain[0]);
        emitABC(OpCode::FIELD_SET, rootReg, val, 0);
        chunk_.code.back().d = nameIdx;
    } else {
        // Nested: s.a.b = val  →  fieldChain = ["b", "a"]
        // Step 1: FIELD_GET chain from root to second-to-last
        //   intermediates[0] = s (rootReg)
        //   intermediates[1] = FIELD_GET s."a" → t1
        //   For s.a.b.c: intermediates[2] = FIELD_GET t1."b" → t2
        std::vector<uint8_t> intermediates;
        intermediates.push_back(rootReg);

        // Walk from deepest to shallowest: fieldChain reversed = ["a", "b"]
        for (size_t i = fieldChain.size() - 1; i >= 1; --i) {
            uint8_t src = intermediates.back();
            uint8_t dst = tempReg();
            int16_t nameIdx = addStringConstant(fieldChain[i]);
            emitABC(OpCode::FIELD_GET_OR_CREATE, dst, src, 0);
            chunk_.code.back().d = nameIdx;
            intermediates.push_back(dst);
        }

        // Step 2: FIELD_SET on the deepest intermediate
        uint8_t deepest = intermediates.back();
        int16_t leafIdx = addStringConstant(fieldChain[0]);
        emitABC(OpCode::FIELD_SET, deepest, val, 0);
        chunk_.code.back().d = leafIdx;

        // Step 3: Write back chain (reverse order)
        for (size_t i = intermediates.size() - 1; i >= 1; --i) {
            uint8_t child = intermediates[i];
            uint8_t parent = intermediates[i - 1];
            // fieldChain index: fieldChain[intermediates.size() - i]
            size_t fi = fieldChain.size() - i;
            int16_t nameIdx = addStringConstant(fieldChain[fi]);
            emitABC(OpCode::FIELD_SET, parent, child, 0);
            chunk_.code.back().d = nameIdx;
        }
    }

    if (!node->suppressOutput) {
        int16_t dispIdx = addStringConstant(cur->strValue);
        emitAD(OpCode::DISPLAY, rootReg, dispIdx);
    }

    return rootReg;
}

// ============================================================
// Cell operations
// ============================================================

uint8_t Compiler::compileCellLiteral(const ASTNode *node)
{
    if (node->children.empty()) {
        uint8_t dst = tempReg();
        emitABC(OpCode::CELL_LITERAL, dst, 0, 0);
        return dst;
    }

    // Each child is a row (BLOCK node). Same structure as matrix literals.
    // {a, b; c, d} → 2 BLOCK children: BLOCK(a,b) and BLOCK(c,d) → 2x2 cell
    // {a, b, c}    → 1 BLOCK child: BLOCK(a,b,c) → 1x3 cell

    // Compile each row into a cell row
    std::vector<uint8_t> rowRegs;
    for (auto &row : node->children) {
        if (row->children.empty()) {
            uint8_t dst = tempReg();
            emitABC(OpCode::CELL_LITERAL, dst, 0, 0);
            rowRegs.push_back(dst);
            continue;
        }

        // Compile row elements into consecutive registers → CELL_LITERAL
        std::vector<uint8_t> elemRegs;
        for (auto &elem : row->children)
            elemRegs.push_back(compileNode(elem.get()));

        uint8_t base = nextReg_;
        for (size_t i = 0; i < elemRegs.size(); ++i) {
            uint8_t slot = tempReg();
            if (elemRegs[i] != slot)
                emitAB(OpCode::MOVE, slot, elemRegs[i]);
        }

        uint8_t dst = tempReg();
        emitABC(OpCode::CELL_LITERAL, dst, base, static_cast<uint8_t>(elemRegs.size()));
        rowRegs.push_back(dst);
    }

    if (rowRegs.size() == 1)
        return rowRegs[0];

    // Multiple rows: vertcat the cell rows
    uint8_t vBase = nextReg_;
    for (size_t i = 0; i < rowRegs.size(); ++i) {
        uint8_t slot = tempReg();
        if (rowRegs[i] != slot)
            emitAB(OpCode::MOVE, slot, rowRegs[i]);
    }

    uint8_t dst = tempReg();
    emitABC(OpCode::VERTCAT, dst, vBase, static_cast<uint8_t>(rowRegs.size()));
    return dst;
}

uint8_t Compiler::compileCellIndex(const ASTNode *node)
{
    // node->children[0] = cell variable
    // node->children[1..] = indices
    uint8_t cell = compileNode(node->children[0].get());
    size_t nidx = node->children.size() - 1;

    if (nidx == 1) {
        IndexContextGuard guard(*this, cell, 1);
        uint8_t idx = compileNode(node->children[1].get());
        uint8_t dst = tempReg();
        emitABC(OpCode::CELL_GET, dst, cell, idx);
        return dst;
    }
    if (nidx == 2) {
        IndexContextGuard guard(*this, cell, 2);
        uint8_t row = compileNode(node->children[1].get());
        guard.setDim(1);
        uint8_t col = compileNode(node->children[2].get());
        uint8_t dst = tempReg();
        emit(Instruction::make_abcde(OpCode::CELL_GET_2D, dst, cell, row, 0, col));
        return dst;
    }
    // ND cell indexing (3D+): use CELL_GET_ND — returns content, not sub-cell
    {
        IndexContextGuard guard(*this, cell, static_cast<uint8_t>(nidx));
        std::vector<uint8_t> idxRegs;
        for (size_t i = 0; i < nidx; ++i) {
            idxRegs.push_back(compileNode(node->children[1 + i].get()));
            if (i + 1 < nidx)
                guard.setDim(static_cast<uint8_t>(i + 1));
        }
        uint8_t base = nextReg_;
        for (size_t i = 0; i < nidx; ++i) {
            uint8_t slot = tempReg();
            if (idxRegs[i] != slot)
                emitAB(OpCode::MOVE, slot, idxRegs[i]);
        }
        uint8_t dst = tempReg();
        emit(Instruction::make_abcde(OpCode::CELL_GET_ND,
                                     dst,
                                     cell,
                                     base,
                                     0,
                                     static_cast<uint8_t>(nidx)));
        return dst;
    }
}

uint8_t Compiler::compileCellAssign(const ASTNode *node)
{
    auto *lhs = node->children[0].get(); // CELL_INDEX node
    auto *rhs = node->children[1].get();

    auto *target = lhs->children[0].get();
    if (target->type != NodeType::IDENTIFIER)
        throw std::runtime_error("Compiler: invalid cell assignment target");

    uint8_t cell = varReg(target->strValue);
    uint8_t val = compileNode(rhs);

    size_t nidx = lhs->children.size() - 1;
    if (nidx == 1) {
        IndexContextGuard guard(*this, cell, 1);
        uint8_t idx = compileNode(lhs->children[1].get());
        emitABC(OpCode::CELL_SET, cell, idx, val);
    } else if (nidx == 2) {
        IndexContextGuard guard(*this, cell, 2);
        uint8_t row = compileNode(lhs->children[1].get());
        guard.setDim(1);
        uint8_t col = compileNode(lhs->children[2].get());
        emit(Instruction::make_abcde(OpCode::CELL_SET_2D, cell, row, col, 0, val));
    } else {
        // ND cell assign
        IndexContextGuard guard(*this, cell, static_cast<uint8_t>(nidx));
        std::vector<uint8_t> idxRegs;
        for (size_t i = 0; i < nidx; ++i) {
            idxRegs.push_back(compileNode(lhs->children[1 + i].get()));
            if (i + 1 < nidx)
                guard.setDim(static_cast<uint8_t>(i + 1));
        }
        uint8_t base = nextReg_;
        for (size_t i = 0; i < nidx; ++i) {
            uint8_t slot = tempReg();
            if (idxRegs[i] != slot)
                emitAB(OpCode::MOVE, slot, idxRegs[i]);
        }
        uint8_t safeVal = tempReg();
        if (val != safeVal)
            emitAB(OpCode::MOVE, safeVal, val);
        emit(Instruction::make_abcde(OpCode::CELL_SET_ND,
                                     cell,
                                     base,
                                     static_cast<uint8_t>(nidx),
                                     0,
                                     safeVal));
    }

    if (!node->suppressOutput) {
        int16_t nameIdx = addStringConstant(target->strValue);
        emitAD(OpCode::DISPLAY, cell, nameIdx);
    }

    return cell;
}

// ============================================================
// Phase 4: Function calls
// ============================================================

uint8_t Compiler::compileCall(const ASTNode *node)
{
    auto *funcNode = node->children[0].get();

    // Non-identifier call target (e.g. anonymous func expression called directly)
    if (funcNode->type != NodeType::IDENTIFIER) {
        uint8_t fhReg = compileNode(funcNode);
        // Compile arguments
        std::vector<uint8_t> argRegs;
        for (size_t i = 1; i < node->children.size(); ++i)
            argRegs.push_back(compileNode(node->children[i].get()));
        uint8_t argBase = nextReg_;
        for (size_t i = 0; i < argRegs.size(); ++i) {
            uint8_t slot = tempReg();
            if (argRegs[i] != slot)
                emitAB(OpCode::MOVE, slot, argRegs[i]);
        }
        uint8_t dst = tempReg();
        emit(Instruction::make_abcde(OpCode::CALL_INDIRECT,
                                     dst,
                                     fhReg,
                                     argBase,
                                     0,
                                     static_cast<uint8_t>(argRegs.size())));
        return dst;
    }

    const std::string &name = funcNode->strValue;
    size_t nargs = node->children.size() - 1;

    // ── Workspace introspection builtins → opcodes inside functions ──
    if (!isTopLevel_) {
        // exist(expr) or exist(expr, filter) → EXIST_VAR dst, nameReg, filterReg
        if (name == "exist" && (nargs == 1 || nargs == 2)) {
            uint8_t nameReg = compileNode(node->children[1].get());
            uint8_t filterReg = 0;
            if (nargs == 2) {
                filterReg = compileNode(node->children[2].get());
            }
            uint8_t dst = tempReg();
            emitABC(OpCode::EXIST_VAR, dst, nameReg, filterReg);
            return dst;
        }
    }

    // clear(expr) works both inside functions and at top-level
    if (name == "clear") {
        for (size_t i = 1; i <= nargs; ++i) {
            auto *argNode = node->children[i].get();
            if (argNode->type == NodeType::STRING_LITERAL) {
                auto it = varRegisters_.find(argNode->strValue);
                if (it != varRegisters_.end() && kBuiltinNames.count(argNode->strValue) == 0)
                    emitA(OpCode::CLEAR_VAR, it->second);
            } else {
                uint8_t nameReg = compileNode(argNode);
                emitA(OpCode::CLEAR_DYN, nameReg);
            }
        }
        if (nargs == 0) {
            for (auto &[vname, reg] : varRegisters_) {
                if (kBuiltinNames.count(vname) == 0)
                    emitA(OpCode::CLEAR_VAR, reg);
            }
        }
        // On top-level, also call externalFunc for side effects
        if (isTopLevel_) {
            uint8_t argBase = nextReg_;
            for (size_t i = 1; i <= nargs; ++i) {
                uint8_t slot = tempReg();
                auto *argNode = node->children[i].get();
                uint8_t argReg = compileNode(argNode);
                if (argReg != slot)
                    emitAB(OpCode::MOVE, slot, argReg);
            }
            uint8_t dst = tempReg();
            int16_t funcIdx = addStringConstant(name);
            emit(Instruction::make_abcde(OpCode::CALL,
                                         dst,
                                         argBase,
                                         static_cast<uint8_t>(nargs),
                                         funcIdx,
                                         0));
            return dst;
        }
        return tempReg();
    }

    if (!isTopLevel_) {
        // who(expr...) / whos(expr...) → WHO/WHOS with args
        if (name == "who" || name == "whos") {
            OpCode op = (name == "who") ? OpCode::WHO : OpCode::WHOS;
            if (nargs == 0) {
                emitAB(op, 0, 0);
            } else {
                std::vector<uint8_t> argRegs;
                for (size_t i = 1; i <= nargs; ++i)
                    argRegs.push_back(compileNode(node->children[i].get()));
                uint8_t base = nextReg_;
                for (size_t i = 0; i < argRegs.size(); ++i) {
                    uint8_t slot = tempReg();
                    if (argRegs[i] != slot)
                        emitAB(OpCode::MOVE, slot, argRegs[i]);
                }
                emitAB(op, base, static_cast<uint8_t>(argRegs.size()));
            }
            return tempReg();
        }
    }

    // Check if it's a known variable (local or from workspaceEnv)
    // → could be array indexing OR func handle call — emit CALL_INDIRECT
    auto it = varRegisters_.find(name);
    bool isKnownVar = (it != varRegisters_.end());

    // Also check workspaceEnv for variables from previous eval() calls
    if (!isKnownVar && isTopLevel_ && !kBuiltinNames.count(name)) {
        MValue *existing = engine_.getVariable(name);
        if (existing && !existing->isEmpty()) {
            // Variable exists — check if it's a callable (funcHandle or closure cell)
            // Callable variables take priority over same-name external functions
            bool isCallable = existing->isFuncHandle()
                              || (existing->isCell() && existing->numel() >= 1
                                  && existing->cellAt(0).isFuncHandle());
            if (isCallable || (!engine_.externalFuncs_.count(name) && !engine_.hasFunction(name))) {
                isKnownVar = true;
            }
        }
    }

    if (isKnownVar) {
        uint8_t fhReg = varReg(name);

        IndexContextGuard guard(*this, fhReg, static_cast<uint8_t>(nargs));

        std::vector<uint8_t> argRegs;
        for (size_t i = 1; i < node->children.size(); ++i) {
            guard.setDim(static_cast<uint8_t>(i - 1));
            argRegs.push_back(compileNode(node->children[i].get()));
        }
        // guard restores here (but we can let it live until return — same scope)

        uint8_t argBase = nextReg_;
        for (size_t i = 0; i < argRegs.size(); ++i) {
            uint8_t slot = tempReg();
            if (argRegs[i] != slot)
                emitAB(OpCode::MOVE, slot, argRegs[i]);
        }
        uint8_t dst = tempReg();
        emit(Instruction::make_abcde(OpCode::CALL_INDIRECT,
                                     dst,
                                     fhReg,
                                     argBase,
                                     0,
                                     static_cast<uint8_t>(argRegs.size())));
        return dst;
    }

    // Fast path: 1-arg inline builtins — skip MOVE, pass arg register directly
    {
        int8_t builtinId = resolveBuiltinId(name, nargs);
        if (builtinId >= 0 && nargs == 1) {
            uint8_t arg = compileNode(node->children[1].get());
            uint8_t dst = tempReg();
            emit(Instruction::make_abcde(OpCode::CALL_BUILTIN,
                                         dst, arg, 1,
                                         static_cast<int16_t>(builtinId), 0));
            // Math builtins on scalar produce scalar
            if (scalarRegs_.test(arg) && builtinId <= 13)
                scalarRegs_.set(dst);
            return dst;
        }
    }

    // Compile arguments into consecutive registers
    std::vector<uint8_t> argRegs;
    for (size_t i = 1; i < node->children.size(); ++i) {
        argRegs.push_back(compileNode(node->children[i].get()));
    }

    uint8_t argBase = nextReg_;
    for (size_t i = 0; i < argRegs.size(); ++i) {
        uint8_t slot = tempReg();
        if (argRegs[i] != slot) {
            emitAB(OpCode::MOVE, slot, argRegs[i]);
        }
    }

    uint8_t dst = tempReg();

    // Try to resolve as inline scalar builtin (numeric ID)
    int8_t builtinId = resolveBuiltinId(name, nargs);
    if (builtinId >= 0) {
        emit(Instruction::make_abcde(OpCode::CALL_BUILTIN,
                                     dst,
                                     argBase,
                                     static_cast<uint8_t>(nargs),
                                     static_cast<int16_t>(builtinId),
                                     0));
        return dst;
    }

    // General CALL
    int16_t funcIdx = addStringConstant(name);
    emit(Instruction::make_abcde(OpCode::CALL,
                                 dst,
                                 argBase,
                                 static_cast<uint8_t>(nargs),
                                 funcIdx,
                                 nargoutContext_));
    return dst;
}

// ============================================================
// Command-style calls: clear all, hold on, grid on, etc.
//
// The parser generates COMMAND_CALL nodes with:
//   strValue   = function name
//   children[] = arguments (all STRING_LITERAL)
//
// We compile these as standard CALL opcodes with string args,
// which the VM dispatches through externalFuncs_ like any other call.
// ============================================================

uint8_t Compiler::compileCommandCall(const ASTNode *node)
{
    const std::string &name = node->strValue;
    size_t nargs = node->children.size();

    // ── clear x y z → CLEAR_VAR for each known variable ──
    if (name == "clear") {
        // Determine what kind of clear this is
        static const std::unordered_set<std::string> kClearFlags
            = {"all", "classes", "functions", "global", "-regexp", "import"};

        bool isFlag = nargs > 0 && kClearFlags.count(node->children[0]->strValue);
        bool isClearAll = nargs == 0
                          || (nargs > 0
                              && (node->children[0]->strValue == "all"
                                  || node->children[0]->strValue == "classes"));

        // Step 1: emit CLEAR_VAR opcodes for local registers
        if (isClearAll) {
            // clear / clear all / clear classes → clear all local registers
            for (auto &[vname, reg] : varRegisters_) {
                if (kBuiltinNames.count(vname) == 0)
                    emitA(OpCode::CLEAR_VAR, reg);
            }
        } else if (!isFlag) {
            // clear x y z → clear specific variables
            for (size_t i = 0; i < nargs; ++i) {
                const std::string &varName = node->children[i]->strValue;
                if (kBuiltinNames.count(varName) == 0) {
                    auto it = varRegisters_.find(varName);
                    if (it != varRegisters_.end())
                        emitA(OpCode::CLEAR_VAR, it->second);
                }
            }
        }
        // Flags (functions, global, -regexp, import) don't clear local registers

        // Step 2: call externalFunc for side effects on top level,
        // or for flags that need runtime handling (global, -regexp, etc.)
        if (isTopLevel_ || isFlag) {
            uint8_t argBase = nextReg_;
            for (size_t i = 0; i < nargs; ++i) {
                uint8_t slot = tempReg();
                int16_t strIdx = addStringConstant(node->children[i]->strValue);
                emitAD(OpCode::LOAD_STRING, slot, strIdx);
            }
            uint8_t dst = tempReg();
            int16_t funcIdx = addStringConstant(name);
            emit(Instruction::make_abcde(OpCode::CALL,
                                         dst,
                                         argBase,
                                         static_cast<uint8_t>(nargs),
                                         funcIdx,
                                         0));
            return dst;
        }

        return tempReg();
    }

    // ── who/whos inside function → opcode ──
    if (!isTopLevel_ && (name == "who" || name == "whos")) {
        // Check for unsupported flags — fall through to generic CALL
        if (nargs > 0 && node->children[0]->strValue == "-file") {
            // Fall through to generic command call (externalFunc warns)
        } else {
            OpCode op = (name == "who") ? OpCode::WHO : OpCode::WHOS;
            if (nargs == 0) {
                emitAB(op, 0, 0);
            } else {
                uint8_t base = nextReg_;
                for (size_t i = 0; i < nargs; ++i) {
                    uint8_t slot = tempReg();
                    int16_t strIdx = addStringConstant(node->children[i]->strValue);
                    emitAD(OpCode::LOAD_STRING, slot, strIdx);
                }
                emitAB(op, base, static_cast<uint8_t>(nargs));
            }
            return tempReg();
        }
    }

    // ── exist x [type] inside function → EXIST_VAR ──
    if (!isTopLevel_ && name == "exist" && (nargs == 1 || nargs == 2)) {
        uint8_t nameReg = tempReg();
        int16_t strIdx = addStringConstant(node->children[0]->strValue);
        emitAD(OpCode::LOAD_STRING, nameReg, strIdx);
        uint8_t filterReg = 0;
        if (nargs == 2) {
            filterReg = tempReg();
            int16_t fIdx = addStringConstant(node->children[1]->strValue);
            emitAD(OpCode::LOAD_STRING, filterReg, fIdx);
        }
        uint8_t dst = tempReg();
        emitABC(OpCode::EXIST_VAR, dst, nameReg, filterReg);
        return dst;
    }

    // ── Generic command call ──
    // Compile arguments: each child is a STRING_LITERAL
    uint8_t argBase = nextReg_;
    for (size_t i = 0; i < nargs; ++i) {
        uint8_t slot = tempReg();
        int16_t strIdx = addStringConstant(node->children[i]->strValue);
        emitAD(OpCode::LOAD_STRING, slot, strIdx);
    }

    uint8_t dst = tempReg();
    int16_t funcIdx = addStringConstant(name);
    emit(
        Instruction::make_abcde(OpCode::CALL, dst, argBase, static_cast<uint8_t>(nargs), funcIdx, 0));

    // Display result if no semicolon (rare for commands, but correct)
    if (!node->suppressOutput) {
        int16_t nameIdx = addStringConstant("ans");
        emitAD(OpCode::DISPLAY, dst, nameIdx);
    }

    return dst;
}

// Collect free variables in AST node — identifiers used but not in params/locals
void Compiler::collectFreeVars(const ASTNode *node,
                               const std::vector<std::string> &params,
                               std::vector<std::string> &freeVars)
{
    if (!node)
        return;

    if (node->type == NodeType::IDENTIFIER) {
        const std::string &name = node->strValue;
        // Skip if it's a parameter
        for (auto &p : params)
            if (p == name)
                return;
        // Skip if already collected
        for (auto &f : freeVars)
            if (f == name)
                return;
        // Skip __result__ (synthetic return var)
        if (name == "__result__")
            return;
        // Check if exists in outer scope
        if (varRegisters_.count(name)) {
            freeVars.push_back(name);
        }
        return;
    }

    // Recurse into children
    for (auto &child : node->children)
        collectFreeVars(child.get(), params, freeVars);
    for (auto &[a, b] : node->branches) {
        collectFreeVars(a.get(), params, freeVars);
        collectFreeVars(b.get(), params, freeVars);
    }
    if (node->elseBranch)
        collectFreeVars(node->elseBranch.get(), params, freeVars);
}

uint8_t Compiler::compileAnonFunc(const ASTNode *node)
{
    // Case 1: @funcname — handle to existing function
    if (!node->strValue.empty() && node->children.empty()) {
        uint8_t dst = tempReg();
        MValue fh = MValue::funcHandle(node->strValue, &engine_.allocator_);
        int16_t constIdx = static_cast<int16_t>(chunk_.constants.size());
        chunk_.constants.push_back(std::move(fh));
        emitAD(OpCode::LOAD_CONST, dst, constIdx);
        return dst;
    }

    // Case 2: @(params) expr — anonymous function with possible closure capture
    int id = anonCounter_++;
    std::string anonName = "__anon_" + std::to_string(id);

    // Build a synthetic function AST node
    auto funcNode = makeNode(NodeType::FUNCTION_DEF);
    funcNode->strValue = anonName;
    funcNode->paramNames = node->paramNames;
    funcNode->returnNames = {"__result__"};

    // Body: __result__ = expr
    auto bodyBlock = makeNode(NodeType::BLOCK);
    auto assignNode = makeNode(NodeType::ASSIGN);
    assignNode->suppressOutput = true;
    auto resultId = makeNode(NodeType::IDENTIFIER);
    resultId->strValue = "__result__";
    assignNode->children.push_back(std::move(resultId));
    assignNode->children.push_back(cloneNode(node->children[0].get()));
    bodyBlock->children.push_back(std::move(assignNode));
    funcNode->children.push_back(std::move(bodyBlock));

    // Find free variables in body that exist in outer scope but are not params
    std::vector<std::string> capturedNames;
    std::vector<uint8_t> capturedOuterRegs;
    collectFreeVars(node->children[0].get(), node->paramNames, capturedNames);

    // Add captured vars as extra hidden parameters
    for (auto &name : capturedNames) {
        auto it = varRegisters_.find(name);
        if (it != varRegisters_.end()) {
            capturedOuterRegs.push_back(it->second);
            funcNode->paramNames.push_back(name);
        }
    }

    // Compile as separate chunk
    BytecodeChunk funcChunk = compileFunction(funcNode.get());
    funcChunk.capturedRegisters = capturedOuterRegs;
    compiledFuncs_[anonName] = std::move(funcChunk);

    if (capturedOuterRegs.empty()) {
        // No captures — simple func handle
        uint8_t dst = tempReg();
        MValue fh = MValue::funcHandle(anonName, &engine_.allocator_);
        int16_t constIdx = static_cast<int16_t>(chunk_.constants.size());
        chunk_.constants.push_back(std::move(fh));
        emitAD(OpCode::LOAD_CONST, dst, constIdx);
        return dst;
    }

    // Has captures — create a cell: {funcHandle, cap1, cap2, ...}
    size_t totalSlots = 1 + capturedOuterRegs.size();

    // Compile elements: funcHandle, then captured values
    std::vector<uint8_t> elemRegs;
    // funcHandle constant
    uint8_t fhReg = tempReg();
    MValue fh = MValue::funcHandle(anonName, &engine_.allocator_);
    int16_t constIdx = static_cast<int16_t>(chunk_.constants.size());
    chunk_.constants.push_back(std::move(fh));
    emitAD(OpCode::LOAD_CONST, fhReg, constIdx);
    elemRegs.push_back(fhReg);

    for (uint8_t outerReg : capturedOuterRegs) {
        elemRegs.push_back(outerReg);
    }

    // Pack into consecutive registers for CELL_LITERAL
    uint8_t base = nextReg_;
    for (size_t i = 0; i < totalSlots; ++i) {
        uint8_t slot = tempReg();
        if (elemRegs[i] != slot)
            emitAB(OpCode::MOVE, slot, elemRegs[i]);
    }

    uint8_t dst = tempReg();
    emitABC(OpCode::CELL_LITERAL, dst, base, static_cast<uint8_t>(totalSlots));
    return dst;
}

uint8_t Compiler::compileFunctionDef(const ASTNode *node)
{
    // Compile function body into a separate BytecodeChunk
    // and store in compiledFuncs_ table
    BytecodeChunk funcChunk = compileFunction(node);
    compiledFuncs_[node->strValue] = std::move(funcChunk);

    // Also register in engine.userFuncs_ for TreeWalker fallback compatibility
    UserFunction uf;
    uf.name = node->strValue;
    uf.params = node->paramNames;
    uf.returns = node->returnNames;
    uf.body = std::shared_ptr<const ASTNode>(cloneNode(node->children[0].get()));
    uf.closureEnv = nullptr;
    engine_.userFuncs_[node->strValue] = std::move(uf);

    return 0;
}

BytecodeChunk Compiler::compileFunction(const ASTNode *funcDef,
                                        std::shared_ptr<const std::string> sourceCode)
{
    // Save current compilation state
    auto savedChunk = std::move(chunk_);
    auto savedVarRegs = std::move(varRegisters_);
    auto savedLoopStack = std::move(loopStack_);
    auto savedConstCache = std::move(constRegCache_);
    auto savedScalarRegs = scalarRegs_;
    uint8_t savedNextReg = nextReg_;
    bool savedTopLevel = isTopLevel_;
    SourceLoc savedLoc = currentLoc_;
    isTopLevel_ = false;

    // Start fresh for function — share source code with parent chunk if not provided
    chunk_ = BytecodeChunk{};
    chunk_.name = funcDef->strValue;
    chunk_.sourceCode = sourceCode ? std::move(sourceCode) : savedChunk.sourceCode;
    chunk_.paramNames = funcDef->paramNames;
    chunk_.returnNames = funcDef->returnNames;
    chunk_.numParams = static_cast<uint8_t>(funcDef->paramNames.size());
    chunk_.numReturns = static_cast<uint8_t>(funcDef->returnNames.size());
    varRegisters_.clear();
    loopStack_.clear();
    constRegCache_.clear();
    scalarRegs_.reset();
    nextReg_ = 0;

    // Allocate registers for parameters (they come pre-loaded by VM)
    for (auto &param : funcDef->paramNames) {
        varReg(param);
    }

    // Allocate registers for return variables
    // Registers default to empty — no initialization needed.
    // Skip allocation if return var is also a parameter (already loaded by caller)
    std::unordered_set<std::string> paramSet(funcDef->paramNames.begin(), funcDef->paramNames.end());
    for (auto &ret : funcDef->returnNames) {
        varReg(ret); // just allocate the register
    }

    // Always allocate nargin/nargout so they're visible in debugger
    varReg("nargin");
    varReg("nargout");

    // Compile body
    compileNode(funcDef->children[0].get());

    // Set source location to the 'end' keyword so breakpoints on it fire
    if (funcDef->endLine > 0)
        currentLoc_ = {static_cast<uint16_t>(funcDef->endLine), 1};

    // Emit return: collect return values
    if (funcDef->returnNames.size() == 1) {
        uint8_t retReg = varReg(funcDef->returnNames[0]);
        emitA(OpCode::RET, retReg);
    } else if (funcDef->returnNames.empty()) {
        emitNone(OpCode::RET_EMPTY);
    } else {
        // Multi-return: collect return regs into consecutive slots then RET_MULTI
        size_t nret = funcDef->returnNames.size();
        std::vector<uint8_t> retRegs;
        for (auto &name : funcDef->returnNames)
            retRegs.push_back(varReg(name));
        // Check if already consecutive
        bool consecutive = true;
        for (size_t i = 1; i < nret; ++i)
            if (retRegs[i] != retRegs[0] + i) { consecutive = false; break; }
        if (consecutive) {
            emitAB(OpCode::RET_MULTI, retRegs[0], static_cast<uint8_t>(nret));
        } else {
            uint8_t base = nextReg_;
            for (size_t i = 0; i < nret; ++i) {
                uint8_t slot = tempReg();
                if (retRegs[i] != slot)
                    emitAB(OpCode::MOVE, slot, retRegs[i]);
            }
            emitAB(OpCode::RET_MULTI, base, static_cast<uint8_t>(nret));
        }
    }

    chunk_.numRegisters = nextReg_;

    // Save variable→register mapping (needed for global var import/export)
    for (auto &[name, reg] : varRegisters_)
        chunk_.varMap.push_back({name, reg});

    BytecodeChunk result = std::move(chunk_);

    // Restore previous state
    chunk_ = std::move(savedChunk);
    varRegisters_ = std::move(savedVarRegs);
    loopStack_ = std::move(savedLoopStack);
    constRegCache_ = std::move(savedConstCache);
    scalarRegs_ = savedScalarRegs;
    nextReg_ = savedNextReg;
    isTopLevel_ = savedTopLevel;
    currentLoc_ = savedLoc;

    return result;
}

uint8_t Compiler::compileReturn(const ASTNode * /*node*/)
{
    if (chunk_.returnNames.size() <= 1) {
        if (!chunk_.returnNames.empty()) {
            uint8_t retReg = varReg(chunk_.returnNames[0]);
            emitA(OpCode::RET, retReg);
        } else {
            emitNone(OpCode::RET_EMPTY);
        }
    } else {
        // Multi-return: same logic as end-of-function
        size_t nret = chunk_.returnNames.size();
        std::vector<uint8_t> retRegs;
        for (auto &name : chunk_.returnNames)
            retRegs.push_back(varReg(name));
        bool consecutive = true;
        for (size_t i = 1; i < nret; ++i)
            if (retRegs[i] != retRegs[0] + i) { consecutive = false; break; }
        if (consecutive) {
            emitAB(OpCode::RET_MULTI, retRegs[0], static_cast<uint8_t>(nret));
        } else {
            uint8_t base = nextReg_;
            for (size_t i = 0; i < nret; ++i) {
                uint8_t slot = tempReg();
                if (retRegs[i] != slot)
                    emitAB(OpCode::MOVE, slot, retRegs[i]);
            }
            emitAB(OpCode::RET_MULTI, base, static_cast<uint8_t>(nret));
        }
    }
    return 0;
}

uint8_t Compiler::compileDeleteAssign(const ASTNode *node)
{
    // AST: children[0] = lhs (CALL node: children[0]=IDENTIFIER, children[1..]=indices)
    // Semantics: v(idx) = []  →  delete elements
    auto *lhs = node->children[0].get();
    if (lhs->type != NodeType::CALL || lhs->children.empty())
        throw std::runtime_error("Compiler: invalid delete assignment syntax");

    auto *target = lhs->children[0].get();
    if (target->type != NodeType::IDENTIFIER)
        throw std::runtime_error("Compiler: invalid delete target");

    uint8_t arr = varReg(target->strValue);
    size_t nargs = lhs->children.size() - 1;

    {
        IndexContextGuard guard(*this, arr, static_cast<uint8_t>(nargs));

        if (nargs == 1) {
            uint8_t idx = compileNode(lhs->children[1].get());
            emitAB(OpCode::INDEX_DELETE, arr, idx);
        } else if (nargs == 2) {
            uint8_t row = compileNode(lhs->children[1].get());
            guard.setDim(1);
            uint8_t col = compileNode(lhs->children[2].get());
            emitABC(OpCode::INDEX_DELETE_2D, arr, row, col);
        } else {
            // ND delete (3D+): compile indices into consecutive registers
            std::vector<uint8_t> idxRegs;
            for (size_t i = 0; i < nargs; ++i) {
                idxRegs.push_back(compileNode(lhs->children[1 + i].get()));
                if (i + 1 < nargs)
                    guard.setDim(static_cast<uint8_t>(i + 1));
            }
            uint8_t base = nextReg_;
            for (size_t i = 0; i < nargs; ++i) {
                uint8_t slot = tempReg();
                if (idxRegs[i] != slot)
                    emitAB(OpCode::MOVE, slot, idxRegs[i]);
            }
            emitABC(OpCode::INDEX_DELETE_ND, arr, base, static_cast<uint8_t>(nargs));
        }
    }

    return arr;
}

// ============================================================
// Builtin ID resolution (compile-time)
// ============================================================

// Must match the switch in VM::execute CALL_BUILTIN handler
int8_t Compiler::resolveBuiltinId(const std::string &name, size_t nargs)
{
    if (nargs == 1) {
        if (name == "abs")
            return 0;
        if (name == "floor")
            return 1;
        if (name == "ceil")
            return 2;
        if (name == "round")
            return 3;
        if (name == "fix")
            return 4;
        if (name == "sqrt")
            return 5;
        if (name == "exp")
            return 6;
        if (name == "log")
            return 7;
        if (name == "log2")
            return 8;
        if (name == "log10")
            return 9;
        if (name == "sin")
            return 10;
        if (name == "cos")
            return 11;
        if (name == "tan")
            return 12;
        if (name == "sign")
            return 13;
        if (name == "isnan")
            return 14;
        if (name == "isinf")
            return 15;
    }
    if (nargs == 2) {
        if (name == "mod")
            return 20;
        if (name == "rem")
            return 21;
        if (name == "max")
            return 22;
        if (name == "min")
            return 23;
        if (name == "pow")
            return 24;
        if (name == "atan2")
            return 25;
    }
    return -1; // not a known inline builtin
}

std::string Compiler::disassemble(const BytecodeChunk &chunk)
{
    std::ostringstream os;
    os << "=== " << chunk.name << " (regs=" << (int) chunk.numRegisters
       << " params=" << (int) chunk.numParams << " returns=" << (int) chunk.numReturns << ") ===\n";

    auto opName = [](OpCode op) -> const char * {
        switch (op) {
        case OpCode::LOAD_CONST:
            return "LOAD_CONST";
        case OpCode::LOAD_EMPTY:
            return "LOAD_EMPTY";
        case OpCode::LOAD_STRING:
            return "LOAD_STRING";
        case OpCode::MOVE:
            return "MOVE";
        case OpCode::ADD:
            return "ADD";
        case OpCode::SUB:
            return "SUB";
        case OpCode::MUL:
            return "MUL";
        case OpCode::RDIV:
            return "RDIV";
        case OpCode::LDIV:
            return "LDIV";
        case OpCode::POW:
            return "POW";
        case OpCode::NEG:
            return "NEG";
        case OpCode::UPLUS:
            return "UPLUS";
        case OpCode::NOT:
            return "NOT";
        case OpCode::EMUL:
            return "EMUL";
        case OpCode::ERDIV:
            return "ERDIV";
        case OpCode::ELDIV:
            return "ELDIV";
        case OpCode::EPOW:
            return "EPOW";
        case OpCode::EQ:
            return "EQ";
        case OpCode::NE:
            return "NE";
        case OpCode::LT:
            return "LT";
        case OpCode::GT:
            return "GT";
        case OpCode::LE:
            return "LE";
        case OpCode::GE:
            return "GE";
        case OpCode::AND:
            return "AND";
        case OpCode::OR:
            return "OR";
        case OpCode::JMP:
            return "JMP";
        case OpCode::JMP_TRUE:
            return "JMP_TRUE";
        case OpCode::JMP_FALSE:
            return "JMP_FALSE";
        case OpCode::FOR_INIT:
            return "FOR_INIT";
        case OpCode::FOR_NEXT:
            return "FOR_NEXT";
        case OpCode::COLON:
            return "COLON";
        case OpCode::COLON3:
            return "COLON3";
        case OpCode::COLON_ALL:
            return "COLON_ALL";
        case OpCode::HORZCAT:
            return "HORZCAT";
        case OpCode::VERTCAT:
            return "VERTCAT";
        case OpCode::INDEX_GET:
            return "INDEX_GET";
        case OpCode::INDEX_GET_2D:
            return "INDEX_GET_2D";
        case OpCode::INDEX_SET:
            return "INDEX_SET";
        case OpCode::INDEX_SET_2D:
            return "INDEX_SET_2D";
        case OpCode::INDEX_GET_ND:
            return "INDEX_GET_ND";
        case OpCode::INDEX_SET_ND:
            return "INDEX_SET_ND";
        case OpCode::INDEX_DELETE:
            return "INDEX_DELETE";
        case OpCode::INDEX_DELETE_2D:
            return "INDEX_DELETE_2D";
        case OpCode::INDEX_DELETE_ND:
            return "INDEX_DELETE_ND";
        case OpCode::LOAD_END:
            return "LOAD_END";
        case OpCode::CALL:
            return "CALL";
        case OpCode::CALL_BUILTIN:
            return "CALL_BUILTIN";
        case OpCode::CTRANSPOSE:
            return "CTRANSPOSE";
        case OpCode::TRANSPOSE:
            return "TRANSPOSE";
        case OpCode::DISPLAY:
            return "DISPLAY";
        case OpCode::RET:
            return "RET";
        case OpCode::RET_EMPTY:
            return "RET_EMPTY";
        case OpCode::HALT:
            return "HALT";
        case OpCode::NOP:
            return "NOP";
        case OpCode::ASSERT_DEF:
            return "ASSERT_DEF";
        case OpCode::ADD_SS:
            return "ADD_SS";
        case OpCode::SUB_SS:
            return "SUB_SS";
        case OpCode::MUL_SS:
            return "MUL_SS";
        case OpCode::RDIV_SS:
            return "RDIV_SS";
        case OpCode::POW_SS:
            return "POW_SS";
        case OpCode::NEG_S:
            return "NEG_S";
        case OpCode::CLEAR_VAR:
            return "CLEAR_VAR";
        case OpCode::CLEAR_DYN:
            return "CLEAR_DYN";
        case OpCode::EXIST_VAR:
            return "EXIST_VAR";
        case OpCode::WHO:
            return "WHO";
        case OpCode::WHOS:
            return "WHOS";
        case OpCode::FIELD_GET:
            return "FIELD_GET";
        case OpCode::FIELD_GET_OR_CREATE:
            return "FIELD_GET_OR_CREATE";
        case OpCode::FIELD_SET:
            return "FIELD_SET";
        case OpCode::FIELD_GET_DYN:
            return "FIELD_GET_DYN";
        case OpCode::FIELD_SET_DYN:
            return "FIELD_SET_DYN";
        case OpCode::CELL_LITERAL:
            return "CELL_LITERAL";
        case OpCode::CELL_GET:
            return "CELL_GET";
        case OpCode::CELL_GET_2D:
            return "CELL_GET_2D";
        case OpCode::CELL_SET:
            return "CELL_SET";
        case OpCode::CELL_SET_2D:
            return "CELL_SET_2D";
        case OpCode::CELL_GET_MULTI:
            return "CELL_GET_MULTI";
        case OpCode::CELL_GET_ND:
            return "CELL_GET_ND";
        case OpCode::CELL_SET_ND:
            return "CELL_SET_ND";
        case OpCode::TRY_BEGIN:
            return "TRY_BEGIN";
        case OpCode::TRY_END:
            return "TRY_END";
        case OpCode::THROW:
            return "THROW";
        case OpCode::CALL_INDIRECT:
            return "CALL_INDIRECT";
        case OpCode::CLOSURE_MAKE:
            return "CLOSURE_MAKE";
        case OpCode::CALL_MULTI:
            return "CALL_MULTI";
        case OpCode::AND_SC:
            return "AND_SC";
        case OpCode::OR_SC:
            return "OR_SC";
        case OpCode::LOAD_NARGIN:
            return "LOAD_NARGIN";
        case OpCode::LOAD_NARGOUT:
            return "LOAD_NARGOUT";
        case OpCode::MATRIX_BUILD:
            return "MATRIX_BUILD";
        case OpCode::BREAK:
            return "BREAK";
        case OpCode::CONTINUE:
            return "CONTINUE";
        case OpCode::GLOBAL_DECL:
            return "GLOBAL_DECL";
        case OpCode::PERSISTENT_DECL:
            return "PERSISTENT_DECL";
        default:
            return "???";
        }
    };

    for (size_t i = 0; i < chunk.code.size(); ++i) {
        const auto &ins = chunk.code[i];

        // Show source location if available
        if (i < chunk.sourceMap.size() && chunk.sourceMap[i].line > 0) {
            os << "  L" << chunk.sourceMap[i].line << ":" << chunk.sourceMap[i].col << "\t";
        } else {
            os << "  \t";
        }

        os << "[" << i << "] " << opName(ins.op) << " a=" << (int) ins.a << " b=" << (int) ins.b
           << " c=" << (int) ins.c << " d=" << ins.d << " e=" << (int) ins.e;

        if (ins.op == OpCode::LOAD_CONST && ins.d < (int16_t) chunk.constants.size()) {
            if (ins.d >= 0 && ins.d < (int16_t) chunk.constants.size()) {
                auto &cv = chunk.constants[ins.d];
                if (cv.isDoubleScalar())
                    os << "  ; val=" << cv.scalarVal();
                else
                    os << "  ; " << cv.debugString();
            }
        }
        os << "\n";
    }

    return os.str();
}

} // namespace mlab