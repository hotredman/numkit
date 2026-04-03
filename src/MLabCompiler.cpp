// src/MLabCompiler.cpp
#include "MLabCompiler.hpp"
#include "MLabEngine.hpp"

#include <stdexcept>
#include <unordered_set>

namespace mlab {

Compiler::Compiler(Engine &engine)
    : engine_(engine)
{}

BytecodeChunk Compiler::compile(const ASTNode *ast)
{
    chunk_ = BytecodeChunk{};
    chunk_.name = "<script>";
    varRegisters_.clear();
    loopStack_.clear();
    nextReg_ = 0;
    isTopLevel_ = true;

    uint8_t lastReg = compileNode(ast);
    emitA(OpCode::RET, lastReg);

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

uint8_t Compiler::varReg(const std::string &name)
{
    auto it = varRegisters_.find(name);
    if (it != varRegisters_.end())
        return it->second;
    uint8_t r = nextReg_++;
    varRegisters_[name] = r;

    // Import variable from globalEnv if it exists
    // For top-level scripts: import all non-builtin variables
    // For function bodies: import only builtin constants (inf, nan, pi, etc.)
    bool shouldImport = false;
    if (isTopLevel_ && chunk_.name == "<script>")
        shouldImport = true;
    else if (kBuiltinNames.count(name))
        shouldImport = true;

    if (shouldImport) {
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

    // Check globalEnv
    if (isTopLevel_ && !kBuiltinNames.count(name)) {
        MValue *existing = engine_.getVariable(name);
        if (existing && !existing->isEmpty())
            return varReg(name); // will import from globalEnv
    }

    // Check if it's a builtin constant
    if (kBuiltinNames.count(name))
        return varReg(name);

    // Check if it's a known function — don't throw, let CALL handle it
    if (engine_.hasFunction(name) || engine_.externalFuncs_.count(name))
        return varReg(name);

    // Unknown variable — throw to trigger TW fallback
    throw std::runtime_error("Undefined variable: " + name);
}

uint8_t Compiler::tempReg()
{
    return nextReg_++;
}

// ============================================================
// Emit helpers
// ============================================================

void Compiler::emit(Instruction instr)
{
    chunk_.code.push_back(instr);
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

    switch (node->type) {
    case NodeType::BLOCK:
        return compileBlock(node);
    case NodeType::NUMBER_LITERAL:
        return compileNumber(node);
    case NodeType::STRING_LITERAL:
        return compileString(node);
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
    case NodeType::FUNCTION_DEF:
        return compileFunctionDef(node);
    case NodeType::RETURN_STMT:
        return compileReturn(node);
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
    uint8_t dst = tempReg();
    int16_t idx = addConstant(node->numValue);
    emitAD(OpCode::LOAD_CONST, dst, idx);
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
    int16_t idx = addConstant(node->boolValue ? 1.0 : 0.0);
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
        uint8_t dst = varReg(lhs->strValue);
        if (src != dst) {
            emitAB(OpCode::MOVE, dst, src);
        }

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

    if (lhs->type == NodeType::CELL_INDEX) {
        return compileCellAssign(node);
    }

    throw std::runtime_error("Compiler: unsupported assignment target");
}

uint8_t Compiler::compileMultiAssign(const ASTNode *node)
{
    // [a, b, c] = func(args)
    // node->returnNames = ["a", "b", "c"]  (~ means ignore)
    // node->children[0] = CALL node
    auto *callNode = node->children[0].get();
    size_t nout = node->returnNames.size();

    // Allocate destination registers for outputs
    std::vector<uint8_t> outRegs;
    for (auto &name : node->returnNames) {
        if (name == "~")
            outRegs.push_back(tempReg()); // throwaway
        else
            outRegs.push_back(varReg(name));
    }

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
        uint8_t right = compileNode(node->children[1].get());
        emitAB(OpCode::MOVE, dst, right);
        patchJump(jumpPos, static_cast<int16_t>(currentPos() - jumpPos));
        return dst;
    }
    if (op == "||") {
        uint8_t dst = tempReg();
        uint8_t left = compileNode(node->children[0].get());
        emitAB(OpCode::MOVE, dst, left);
        size_t jumpPos = currentPos();
        emitAD(OpCode::JMP_TRUE, dst, 0); // placeholder
        uint8_t right = compileNode(node->children[1].get());
        emitAB(OpCode::MOVE, dst, right);
        patchJump(jumpPos, static_cast<int16_t>(currentPos() - jumpPos));
        return dst;
    }

    uint8_t left = compileNode(node->children[0].get());
    uint8_t right = compileNode(node->children[1].get());
    uint8_t dst = tempReg();

    OpCode opcode;
    if (op == "+")
        opcode = OpCode::ADD;
    else if (op == "-")
        opcode = OpCode::SUB;
    else if (op == "*")
        opcode = OpCode::MUL;
    else if (op == "/")
        opcode = OpCode::RDIV;
    else if (op == "\\")
        opcode = OpCode::LDIV;
    else if (op == "^")
        opcode = OpCode::POW;
    else if (op == ".*")
        opcode = OpCode::EMUL;
    else if (op == "./")
        opcode = OpCode::ERDIV;
    else if (op == ".\\")
        opcode = OpCode::ELDIV;
    else if (op == ".^")
        opcode = OpCode::EPOW;
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
    return dst;
}

uint8_t Compiler::compileUnaryOp(const ASTNode *node)
{
    const std::string &op = node->strValue;
    uint8_t src = compileNode(node->children[0].get());
    uint8_t dst = tempReg();

    if (op == "-")
        emitAB(OpCode::NEG, dst, src);
    else if (op == "+")
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
    // Check for bare builtin commands that VM can't handle
    // (e.g. 'clear', 'who', 'whos', 'clc') — force fallback to TreeWalker
    auto *child = node->children[0].get();
    if (child->type == NodeType::IDENTIFIER) {
        static const std::unordered_set<std::string> kBuiltinCommands
            = {"clear",  "who",     "whos",  "clc",    "clf",    "close",  "hold",   "grid",
               "figure", "subplot", "title", "xlabel", "ylabel", "zlabel", "legend", "axis",
               "format", "diary",   "cd",    "pwd",    "ls",     "dir",    "tic",    "toc"};
        if (kBuiltinCommands.count(child->strValue))
            throw std::runtime_error("Compiler: builtin command '" + child->strValue
                                     + "' requires TreeWalker");
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
        compileNode(node->elseBranch.get());
    }

    // Patch all end jumps to here
    for (size_t pos : endJumps) {
        patchJump(pos, static_cast<int16_t>(currentPos() - pos));
    }

    return 0;
}

// ============================================================
// Switch/case
// ============================================================
uint8_t Compiler::compileSwitch(const ASTNode *node)
{
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

        // Compile case expression
        uint8_t caseReg = compileNode(caseExpr.get());

        // Compare: cmpReg = (switchReg == caseReg)
        emitABC(OpCode::EQ, cmpReg, switchReg, caseReg);

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

    // Patch end jumps
    for (size_t pos : endJumps) {
        patchJump(pos, static_cast<int16_t>(currentPos() - pos));
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

    // Patch JMP to end
    patchJump(endJmpPos, static_cast<int16_t>(currentPos() - endJmpPos));

    return 0;
}

uint8_t Compiler::compileWhile(const ASTNode *node)
{
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

    // JMP back to start
    emitD(OpCode::JMP, static_cast<int16_t>(loopStart - currentPos()));

    // Patch JMP_FALSE exit
    patchJump(exitPos, static_cast<int16_t>(currentPos() - exitPos));

    // Patch break targets → jump to here (after loop)
    for (size_t pos : loopStack_.back().breakPatches) {
        patchJump(pos, static_cast<int16_t>(currentPos() - pos));
    }

    loopStack_.pop_back();
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

    size_t bodyStart = currentPos();

    // Compile body
    compileNode(node->children[1].get());

    // Patch continue targets → jump to FOR_NEXT
    size_t forNextPos = currentPos();
    for (size_t pos : loopStack_.back().continuePatches) {
        patchJump(pos, static_cast<int16_t>(forNextPos - pos));
    }

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
        uint8_t idx = compileNode(node->children[argOffset].get());
        uint8_t dst = tempReg();
        emitABC(OpCode::INDEX_GET, dst, arr, idx);
        return dst;
    }

    if (nargs == 2) {
        uint8_t row = compileNode(node->children[argOffset].get());
        uint8_t col = compileNode(node->children[argOffset + 1].get());
        uint8_t dst = tempReg();
        emit(Instruction::make_abcde(OpCode::INDEX_GET_2D, dst, arr, row, 0, col));
        return dst;
    }

    // ND indexing (3D+): compile indices into consecutive registers
    {
        std::vector<uint8_t> idxRegs;
        for (size_t i = 0; i < nargs; ++i)
            idxRegs.push_back(compileNode(node->children[argOffset + i].get()));
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

    if (nargs == 1) {
        uint8_t idx = compileNode(lhs->children[argOffset].get());
        emitABC(OpCode::INDEX_SET, arr, idx, val);
    } else if (nargs == 2) {
        uint8_t row = compileNode(lhs->children[argOffset].get());
        uint8_t col = compileNode(lhs->children[argOffset + 1].get());
        emit(Instruction::make_abcde(OpCode::INDEX_SET_2D, arr, row, col, 0, val));
    } else {
        // ND indexed assign (3D+)
        // Compile indices first
        std::vector<uint8_t> idxRegs;
        for (size_t i = 0; i < nargs; ++i)
            idxRegs.push_back(compileNode(lhs->children[argOffset + i].get()));
        uint8_t base = nextReg_;
        for (size_t i = 0; i < nargs; ++i) {
            uint8_t slot = tempReg();
            if (idxRegs[i] != slot)
                emitAB(OpCode::MOVE, slot, idxRegs[i]);
        }
        // Ensure val is in a safe register (not overlapping with index slots)
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

    // lhs->children[0] = object (IDENTIFIER)
    // lhs->strValue = field name
    auto *objNode = lhs->children[0].get();
    if (objNode->type != NodeType::IDENTIFIER)
        throw std::runtime_error("Compiler: nested field assign not yet supported");

    uint8_t obj = varReg(objNode->strValue);

    // If variable doesn't exist yet, initialize as struct
    // (VM will create struct on first FIELD_SET to empty)

    uint8_t val = compileNode(rhs);
    int16_t nameIdx = addStringConstant(lhs->strValue);
    emitABC(OpCode::FIELD_SET, obj, val, 0);
    chunk_.code.back().d = nameIdx;

    if (!node->suppressOutput) {
        int16_t dispIdx = addStringConstant(objNode->strValue);
        emitAD(OpCode::DISPLAY, obj, dispIdx);
    }

    return obj;
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

    // Check if children are row-blocks (2D cell) or direct elements (1D cell)
    // Parser wraps {a, b, c} as CELL_LITERAL with one BLOCK child containing a,b,c
    const std::vector<ASTNodePtr> *elements = &node->children;
    if (node->children.size() == 1 && node->children[0]->type == NodeType::BLOCK) {
        elements = &node->children[0]->children;
    }

    size_t count = elements->size();

    // Compile all elements first
    std::vector<uint8_t> elemRegs;
    elemRegs.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        elemRegs.push_back(compileNode((*elements)[i].get()));
    }

    // Allocate consecutive registers and MOVE elements into them
    uint8_t base = nextReg_;
    for (size_t i = 0; i < count; ++i) {
        uint8_t slot = tempReg();
        if (elemRegs[i] != slot)
            emitAB(OpCode::MOVE, slot, elemRegs[i]);
    }

    uint8_t dst = tempReg();
    emitABC(OpCode::CELL_LITERAL, dst, base, (uint8_t) count);
    return dst;
}

uint8_t Compiler::compileCellIndex(const ASTNode *node)
{
    // node->children[0] = cell variable
    // node->children[1..] = indices
    uint8_t cell = compileNode(node->children[0].get());
    size_t nidx = node->children.size() - 1;

    if (nidx == 1) {
        uint8_t idx = compileNode(node->children[1].get());
        uint8_t dst = tempReg();
        emitABC(OpCode::CELL_GET, dst, cell, idx);
        return dst;
    }
    if (nidx == 2) {
        uint8_t row = compileNode(node->children[1].get());
        uint8_t col = compileNode(node->children[2].get());
        uint8_t dst = tempReg();
        emit(Instruction::make_abcde(OpCode::CELL_GET_2D, dst, cell, row, 0, col));
        return dst;
    }
    // ND cell indexing (3D+): use INDEX_GET_ND — VM checks type at runtime
    {
        std::vector<uint8_t> idxRegs;
        for (size_t i = 0; i < nidx; ++i)
            idxRegs.push_back(compileNode(node->children[1 + i].get()));
        uint8_t base = nextReg_;
        for (size_t i = 0; i < nidx; ++i) {
            uint8_t slot = tempReg();
            if (idxRegs[i] != slot)
                emitAB(OpCode::MOVE, slot, idxRegs[i]);
        }
        uint8_t dst = tempReg();
        emit(Instruction::make_abcde(OpCode::INDEX_GET_ND,
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
        uint8_t idx = compileNode(lhs->children[1].get());
        emitABC(OpCode::CELL_SET, cell, idx, val);
    } else if (nidx == 2) {
        uint8_t row = compileNode(lhs->children[1].get());
        uint8_t col = compileNode(lhs->children[2].get());
        emit(Instruction::make_abcde(OpCode::CELL_SET_2D, cell, row, col, 0, val));
    } else {
        // ND cell assign
        std::vector<uint8_t> idxRegs;
        for (size_t i = 0; i < nidx; ++i)
            idxRegs.push_back(compileNode(lhs->children[1 + i].get()));
        uint8_t base = nextReg_;
        for (size_t i = 0; i < nidx; ++i) {
            uint8_t slot = tempReg();
            if (idxRegs[i] != slot)
                emitAB(OpCode::MOVE, slot, idxRegs[i]);
        }
        uint8_t safeVal = tempReg();
        if (val != safeVal)
            emitAB(OpCode::MOVE, safeVal, val);
        emit(Instruction::make_abcde(OpCode::INDEX_SET_ND,
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

    // Check if it's a known variable (local or from globalEnv)
    // → could be array indexing OR func handle call — emit CALL_INDIRECT
    auto it = varRegisters_.find(name);
    bool isKnownVar = (it != varRegisters_.end());

    // Also check globalEnv for variables from previous eval() calls
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
        uint8_t fhReg = varReg(name); // will import from globalEnv if needed
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
    emit(
        Instruction::make_abcde(OpCode::CALL, dst, argBase, static_cast<uint8_t>(nargs), funcIdx, 0));
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

BytecodeChunk Compiler::compileFunction(const ASTNode *funcDef)
{
    // Save current compilation state
    auto savedChunk = std::move(chunk_);
    auto savedVarRegs = std::move(varRegisters_);
    auto savedLoopStack = std::move(loopStack_);
    uint8_t savedNextReg = nextReg_;
    bool savedTopLevel = isTopLevel_;
    isTopLevel_ = false;

    // Start fresh for function
    chunk_ = BytecodeChunk{};
    chunk_.name = funcDef->strValue;
    chunk_.paramNames = funcDef->paramNames;
    chunk_.returnNames = funcDef->returnNames;
    chunk_.numParams = static_cast<uint8_t>(funcDef->paramNames.size());
    chunk_.numReturns = static_cast<uint8_t>(funcDef->returnNames.size());
    varRegisters_.clear();
    loopStack_.clear();
    nextReg_ = 0;

    // Allocate registers for parameters (they come pre-loaded by VM)
    for (auto &param : funcDef->paramNames) {
        varReg(param);
    }

    // Allocate registers for return variables
    // Skip LOAD_EMPTY if return var is also a parameter (already loaded by caller)
    std::unordered_set<std::string> paramSet(funcDef->paramNames.begin(), funcDef->paramNames.end());
    for (auto &ret : funcDef->returnNames) {
        uint8_t r = varReg(ret);
        if (!paramSet.count(ret)) {
            emitA(OpCode::LOAD_EMPTY, r);
        }
    }

    // Compile body
    compileNode(funcDef->children[0].get());

    // Emit return: collect return values
    if (funcDef->returnNames.size() == 1) {
        uint8_t retReg = varReg(funcDef->returnNames[0]);
        emitA(OpCode::RET, retReg);
    } else if (funcDef->returnNames.empty()) {
        emitNone(OpCode::RET_EMPTY);
    } else {
        // Multi-return: RET_MULTI base, count
        // Return vars are in their registers
        uint8_t base = varReg(funcDef->returnNames[0]);
        emitAB(OpCode::RET_MULTI, base, static_cast<uint8_t>(funcDef->returnNames.size()));
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
    nextReg_ = savedNextReg;
    isTopLevel_ = savedTopLevel;

    return result;
}

uint8_t Compiler::compileReturn(const ASTNode * /*node*/)
{
    // In a function context, return the return variables
    // For now, emit RET with first return var if known, else RET_EMPTY
    if (!chunk_.returnNames.empty()) {
        uint8_t retReg = varReg(chunk_.returnNames[0]);
        emitA(OpCode::RET, retReg);
    } else {
        emitNone(OpCode::RET_EMPTY);
    }
    return 0;
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
        case OpCode::FIELD_GET:
            return "FIELD_GET";
        case OpCode::FIELD_SET:
            return "FIELD_SET";
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
        default:
            return "???";
        }
    };

    for (size_t i = 0; i < chunk.code.size(); ++i) {
        const auto &ins = chunk.code[i];
        os << "  [" << i << "] " << opName(ins.op) << " a=" << (int) ins.a << " b=" << (int) ins.b
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