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

    // TODO: indexed assign, field assign, cell assign
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

std::string Compiler::disassemble(const BytecodeChunk &chunk)
{
    std::ostringstream os;
    os << "=== " << chunk.name << " (regs=" << (int) chunk.numRegisters << ") ===\n";

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