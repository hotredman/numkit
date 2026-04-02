// src/MLabCompiler.cpp
#include "MLabCompiler.hpp"
#include "MLabEngine.hpp"

#include <stdexcept>

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

    uint8_t lastReg = compileNode(ast);
    emitA(OpCode::RET, lastReg);

    chunk_.numRegisters = nextReg_;
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
    return r;
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
    case NodeType::FIELD_ACCESS:
        return compileFieldAccess(node);
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
    return varReg(node->strValue);
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

    throw std::runtime_error("Compiler: unsupported assignment target");
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
    uint8_t reg = compileNode(node->children[0].get());

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

    loopStack_.push_back(LoopContext{});

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

    throw std::runtime_error("Compiler: ND indexing not yet supported");
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
        throw std::runtime_error("Compiler: ND indexed assign not yet supported");
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
// Phase 4: Function calls
// ============================================================

uint8_t Compiler::compileCall(const ASTNode *node)
{
    const std::string &name = node->children[0]->strValue;
    size_t nargs = node->children.size() - 1;

    // Check if it's a known variable → array indexing
    auto it = varRegisters_.find(name);
    if (it != varRegisters_.end()) {
        return compileIndexExpr(node);
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
        // CALL_BUILTIN: dst=a, builtinId=d, argBase=b, nargs=c
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

uint8_t Compiler::compileFunctionDef(const ASTNode *node)
{
    // Compile function body into a separate BytecodeChunk
    // and store in compiledFuncs_ table

    BytecodeChunk funcChunk = compileFunction(node);
    compiledFuncs_[node->strValue] = std::move(funcChunk);
    return 0;
}

BytecodeChunk Compiler::compileFunction(const ASTNode *funcDef)
{
    // Save current compilation state
    auto savedChunk = std::move(chunk_);
    auto savedVarRegs = std::move(varRegisters_);
    auto savedLoopStack = std::move(loopStack_);
    uint8_t savedNextReg = nextReg_;

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
    for (auto &ret : funcDef->returnNames) {
        uint8_t r = varReg(ret);
        // Initialize return vars to empty
        emitA(OpCode::LOAD_EMPTY, r);
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
    BytecodeChunk result = std::move(chunk_);

    // Restore previous state
    chunk_ = std::move(savedChunk);
    varRegisters_ = std::move(savedVarRegs);
    loopStack_ = std::move(savedLoopStack);
    nextReg_ = savedNextReg;

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
        default:
            return "???";
        }
    };

    for (size_t i = 0; i < chunk.code.size(); ++i) {
        const auto &ins = chunk.code[i];
        os << "  [" << i << "] " << opName(ins.op) << " a=" << (int) ins.a << " b=" << (int) ins.b
           << " c=" << (int) ins.c << " d=" << ins.d << " e=" << (int) ins.e;

        if (ins.op == OpCode::LOAD_CONST && ins.d < (int16_t) chunk.constants.size()) {
            os << "  ; val=" << chunk.constants[ins.d].toScalar();
        }
        os << "\n";
    }

    return os.str();
}

} // namespace mlab