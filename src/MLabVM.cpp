// src/MLabVM.cpp
#include "MLabVM.hpp"
#include "MLabEngine.hpp"

#include <cmath>
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace mlab {

VM::VM(Engine &engine)
    : engine_(engine)
{}

MValue VM::execute(const BytecodeChunk &chunk)
{
    registers_.resize(chunk.numRegisters);
    for (auto &r : registers_)
        r = MValue::empty();

    const Instruction *ip = chunk.code.data();
    const Instruction *end = ip + chunk.code.size();
    MValue lastResult = MValue::empty();

    while (ip < end) {
        const Instruction &instr = *ip;

        switch (instr.op) {
        // ── Data movement ────────────────────────────────────
        case OpCode::LOAD_CONST:
            registers_[instr.a] = chunk.constants[instr.d];
            break;

        case OpCode::LOAD_EMPTY:
            registers_[instr.a] = MValue::empty();
            break;

        case OpCode::LOAD_STRING:
            registers_[instr.a] = MValue::fromString(chunk.strings[instr.d], &engine_.allocator_);
            break;

        case OpCode::MOVE:
            registers_[instr.a] = registers_[instr.b];
            break;

        // ── Arithmetic ───────────────────────────────────────
        case OpCode::ADD:
        case OpCode::SUB:
        case OpCode::MUL:
        case OpCode::RDIV:
        case OpCode::LDIV:
        case OpCode::POW:
        case OpCode::EMUL:
        case OpCode::ERDIV:
        case OpCode::ELDIV:
        case OpCode::EPOW:
            executeBinaryOp(instr.op, registers_[instr.a], registers_[instr.b], registers_[instr.c]);
            break;

        // ── Comparison ───────────────────────────────────────
        case OpCode::EQ:
        case OpCode::NE:
        case OpCode::LT:
        case OpCode::GT:
        case OpCode::LE:
        case OpCode::GE:
            executeBinaryOp(instr.op, registers_[instr.a], registers_[instr.b], registers_[instr.c]);
            break;

        // ── Logical ──────────────────────────────────────────
        case OpCode::AND:
        case OpCode::OR:
            executeBinaryOp(instr.op, registers_[instr.a], registers_[instr.b], registers_[instr.c]);
            break;

        // ── Unary ────────────────────────────────────────────
        case OpCode::NEG:
        case OpCode::UPLUS:
        case OpCode::NOT:
        case OpCode::CTRANSPOSE:
        case OpCode::TRANSPOSE:
            executeUnaryOp(instr.op, registers_[instr.a], registers_[instr.b]);
            break;

        // ── Control flow ─────────────────────────────────────
        case OpCode::JMP:
            ip += instr.d;
            continue; // skip ip++ at bottom

        case OpCode::JMP_TRUE:
            if (registers_[instr.a].toBool()) {
                ip += instr.d;
                continue;
            }
            break;

        case OpCode::JMP_FALSE:
            if (!registers_[instr.a].toBool()) {
                ip += instr.d;
                continue;
            }
            break;

        // ── Display ──────────────────────────────────────────
        case OpCode::DISPLAY: {
            const std::string &name = chunk.strings[instr.d];
            const MValue &val = registers_[instr.a];
            // Format output similar to MATLAB
            std::ostringstream os;
            if (val.isScalar() && val.type() == MType::DOUBLE) {
                os << name << " = " << val.toScalar() << "\n\n";
            } else if (val.isChar()) {
                os << name << " = '" << val.toString() << "'\n\n";
            } else if (val.isEmpty()) {
                os << name << " = []\n\n";
            } else {
                os << name << " = " << val.debugString() << "\n\n";
            }
            engine_.outputText(os.str());
            break;
        }

        // ── Return ───────────────────────────────────────────
        case OpCode::RET:
            return registers_[instr.a];

        case OpCode::RET_EMPTY:
            return MValue::empty();

        // ── End ──────────────────────────────────────────────
        case OpCode::HALT:
            // Return last assigned value (for scripts)
            return lastResult;

        case OpCode::NOP:
            break;

        default:
            throw std::runtime_error("VM: unimplemented opcode "
                                     + std::to_string(static_cast<int>(instr.op)));
        }

        // Track last result for script return value
        if (instr.op == OpCode::MOVE || instr.op == OpCode::LOAD_CONST) {
            lastResult = registers_[instr.a];
        }

        ++ip;
    }

    return lastResult;
}

// ============================================================
// Binary operations
// ============================================================

void VM::executeBinaryOp(OpCode op, MValue &dst, const MValue &a, const MValue &b)
{
    // Scalar fast path
    if (a.isScalar() && a.type() == MType::DOUBLE && b.isScalar() && b.type() == MType::DOUBLE) {
        double av = a.toScalar();
        double bv = b.toScalar();
        double result;

        switch (op) {
        case OpCode::ADD:
            result = av + bv;
            break;
        case OpCode::SUB:
            result = av - bv;
            break;
        case OpCode::MUL:
            result = av * bv;
            break;
        case OpCode::RDIV:
            result = av / bv;
            break;
        case OpCode::LDIV:
            result = bv / av;
            break;
        case OpCode::POW:
            result = std::pow(av, bv);
            break;
        case OpCode::EMUL:
            result = av * bv;
            break;
        case OpCode::ERDIV:
            result = av / bv;
            break;
        case OpCode::ELDIV:
            result = bv / av;
            break;
        case OpCode::EPOW:
            result = std::pow(av, bv);
            break;
        case OpCode::EQ:
            result = (av == bv) ? 1.0 : 0.0;
            break;
        case OpCode::NE:
            result = (av != bv) ? 1.0 : 0.0;
            break;
        case OpCode::LT:
            result = (av < bv) ? 1.0 : 0.0;
            break;
        case OpCode::GT:
            result = (av > bv) ? 1.0 : 0.0;
            break;
        case OpCode::LE:
            result = (av <= bv) ? 1.0 : 0.0;
            break;
        case OpCode::GE:
            result = (av >= bv) ? 1.0 : 0.0;
            break;
        case OpCode::AND:
            result = (av != 0.0 && bv != 0.0) ? 1.0 : 0.0;
            break;
        case OpCode::OR:
            result = (av != 0.0 || bv != 0.0) ? 1.0 : 0.0;
            break;
        default:
            throw std::runtime_error("VM: unknown binary opcode");
        }

        dst = MValue::scalar(result, &engine_.allocator_);
        return;
    }

    // Array fallback — delegate to Engine's registered binary ops
    const char *opStr = nullptr;
    switch (op) {
    case OpCode::ADD:
        opStr = "+";
        break;
    case OpCode::SUB:
        opStr = "-";
        break;
    case OpCode::MUL:
        opStr = "*";
        break;
    case OpCode::RDIV:
        opStr = "/";
        break;
    case OpCode::LDIV:
        opStr = "\\";
        break;
    case OpCode::POW:
        opStr = "^";
        break;
    case OpCode::EMUL:
        opStr = ".*";
        break;
    case OpCode::ERDIV:
        opStr = "./";
        break;
    case OpCode::ELDIV:
        opStr = ".\\";
        break;
    case OpCode::EPOW:
        opStr = ".^";
        break;
    case OpCode::EQ:
        opStr = "==";
        break;
    case OpCode::NE:
        opStr = "~=";
        break;
    case OpCode::LT:
        opStr = "<";
        break;
    case OpCode::GT:
        opStr = ">";
        break;
    case OpCode::LE:
        opStr = "<=";
        break;
    case OpCode::GE:
        opStr = ">=";
        break;
    case OpCode::AND:
        opStr = "&";
        break;
    case OpCode::OR:
        opStr = "|";
        break;
    default:
        break;
    }

    if (opStr) {
        auto it = engine_.binaryOps_.find(opStr);
        if (it != engine_.binaryOps_.end()) {
            dst = it->second(a, b);
            return;
        }
    }

    throw std::runtime_error("VM: unsupported binary operation on arrays");
}

// ============================================================
// Unary operations
// ============================================================

void VM::executeUnaryOp(OpCode op, MValue &dst, const MValue &src)
{
    // Scalar fast path
    if (src.isScalar() && src.type() == MType::DOUBLE) {
        double v = src.toScalar();
        switch (op) {
        case OpCode::NEG:
            dst = MValue::scalar(-v, &engine_.allocator_);
            return;
        case OpCode::UPLUS:
            dst = src;
            return;
        case OpCode::NOT:
            dst = MValue::scalar(v == 0.0 ? 1.0 : 0.0, &engine_.allocator_);
            return;
        default:
            break;
        }
    }

    // Array fallback
    const char *opStr = nullptr;
    switch (op) {
    case OpCode::NEG:
        opStr = "-";
        break;
    case OpCode::UPLUS:
        opStr = "+";
        break;
    case OpCode::NOT:
        opStr = "~";
        break;
    case OpCode::CTRANSPOSE:
        opStr = "'";
        break;
    case OpCode::TRANSPOSE:
        opStr = ".'";
        break;
    default:
        break;
    }

    if (opStr) {
        auto it = engine_.unaryOps_.find(opStr);
        if (it != engine_.unaryOps_.end()) {
            dst = it->second(src);
            return;
        }
    }

    throw std::runtime_error("VM: unsupported unary operation");
}

} // namespace mlab